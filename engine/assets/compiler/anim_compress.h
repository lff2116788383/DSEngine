#pragma once

// 动画压缩（.danim v3）：关键帧量化 + 关键帧抽取。
//
// 设计要点：
//   - 容器仍以 AnimHeader（magic "DSEA"）打头，version=3，duration/channel_count 字段
//     与 v2 完全一致 —— 因此只读取 header 的代码（animator_system 取 duration）无需改动。
//   - v3 用 AnimChannelDescV3 取代 v2 的 AnimChannelDesc：每条轨（position/rotation/scale）
//     拥有独立的时间数组与量化范围，以支持各轨独立抽取。
//   - 旋转用 smallest-three 48bit（6 字节）量化；位置/缩放按轨 bbox 归一化为 3×16bit（6 字节）。
//   - 时间保持 float32（抽取后关键帧已很少，时间数组占比小且需精确定位区间）。
//   - 运行时只新增一条 v3 采样路径；v2 文件继续走旧路径，完全向后兼容。
//
// 头文件只含内联/模板，便于 cook 端（importer）、运行时（anim_clip_eval）与单测共用，
// 不引入新的编译单元。

#include "engine/assets/compiler/raw_scene_data.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace dse {
namespace asset {
namespace compiler {

constexpr uint32_t kDanimVersionQuantized = 3;

#pragma pack(push, 1)
// v3 每通道描述：三条轨各自独立的时间/数据偏移与计数，外加位置/缩放的量化范围。
struct AnimChannelDescV3 {
    int32_t  target_node_index = -1;
    uint32_t position_key_count = 0;
    uint32_t rotation_key_count = 0;
    uint32_t scale_key_count = 0;

    uint64_t pos_time_offset = 0;    // float[position_key_count]
    uint64_t pos_offset = 0;         // 6 字节/帧（PackVec3_16）× position_key_count
    uint64_t rot_time_offset = 0;    // float[rotation_key_count]
    uint64_t rot_offset = 0;         // 6 字节/帧（PackQuat48）× rotation_key_count
    uint64_t scale_time_offset = 0;  // float[scale_key_count]
    uint64_t scale_offset = 0;       // 6 字节/帧（PackVec3_16）× scale_key_count

    float pos_min[3] = {0, 0, 0};
    float pos_extent[3] = {0, 0, 0};
    float scale_min[3] = {0, 0, 0};
    float scale_extent[3] = {0, 0, 0};
};
#pragma pack(pop)

constexpr uint32_t kQuantVec3Bytes = 6;   // 3 × uint16
constexpr uint32_t kQuantQuatBytes = 6;   // smallest-three 48bit

// ── 量化选项（cook 期） ─────────────────────────────────────────────────────
struct AnimCompressOptions {
    bool  quantize = true;          // 写 v3（false → 退回 v2）
    bool  reduce_keyframes = true;  // 误差阈值抽取
    float position_epsilon = 1e-4f; // 位置/缩放抽取误差（世界单位）
    float scale_epsilon = 1e-4f;
    float rotation_epsilon_deg = 0.1f; // 旋转抽取误差（角度）
};

// ── 量化 / 反量化 ───────────────────────────────────────────────────────────

// smallest-three：丢弃绝对值最大的分量（用 2bit 记录其索引、整体取正号），
// 其余 3 个分量落在 [-1/√2, 1/√2]，各以 15bit 存储，打包进 6 字节。
inline void PackQuat48(glm::quat q, uint8_t out[6]) {
    float n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (n < 1e-12f) { q = glm::quat(1, 0, 0, 0); n = 1.0f; }
    float c[4] = { q.x / n, q.y / n, q.z / n, q.w / n };

    int max_idx = 0;
    for (int i = 1; i < 4; ++i)
        if (std::fabs(c[i]) > std::fabs(c[max_idx])) max_idx = i;
    if (c[max_idx] < 0.0f) { for (float& v : c) v = -v; }  // 使最大分量为正

    constexpr float kInvSqrt2 = 0.70710678118654752440f;
    uint16_t packed[3];
    int j = 0;
    for (int i = 0; i < 4; ++i) {
        if (i == max_idx) continue;
        float v = std::clamp(c[i] / kInvSqrt2, -1.0f, 1.0f); // → [-1,1]
        packed[j++] = static_cast<uint16_t>(std::lround((v * 0.5f + 0.5f) * 32767.0f)) & 0x7FFF;
    }

    uint64_t bits = static_cast<uint64_t>(max_idx & 0x3);
    bits |= static_cast<uint64_t>(packed[0]) << 2;
    bits |= static_cast<uint64_t>(packed[1]) << 17;
    bits |= static_cast<uint64_t>(packed[2]) << 32;
    for (int i = 0; i < 6; ++i) out[i] = static_cast<uint8_t>((bits >> (i * 8)) & 0xFF);
}

inline glm::quat UnpackQuat48(const uint8_t in[6]) {
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) bits |= static_cast<uint64_t>(in[i]) << (i * 8);

    const int max_idx = static_cast<int>(bits & 0x3);
    const uint16_t packed[3] = {
        static_cast<uint16_t>((bits >> 2) & 0x7FFF),
        static_cast<uint16_t>((bits >> 17) & 0x7FFF),
        static_cast<uint16_t>((bits >> 32) & 0x7FFF),
    };

    constexpr float kInvSqrt2 = 0.70710678118654752440f;
    float c[4];
    int j = 0;
    float sum_sq = 0.0f;
    for (int i = 0; i < 4; ++i) {
        if (i == max_idx) continue;
        float v = (static_cast<float>(packed[j++]) / 32767.0f) * 2.0f - 1.0f;
        c[i] = v * kInvSqrt2;
        sum_sq += c[i] * c[i];
    }
    c[max_idx] = std::sqrt(std::max(0.0f, 1.0f - sum_sq));
    glm::quat q(c[3], c[0], c[1], c[2]); // (w, x, y, z)
    return glm::normalize(q);
}

inline void PackVec3_16(const glm::vec3& v, const glm::vec3& mn, const glm::vec3& ext, uint8_t out[6]) {
    for (int i = 0; i < 3; ++i) {
        float t = ext[i] > 1e-12f ? std::clamp((v[i] - mn[i]) / ext[i], 0.0f, 1.0f) : 0.0f;
        uint16_t q = static_cast<uint16_t>(std::lround(t * 65535.0f));
        out[i * 2 + 0] = static_cast<uint8_t>(q & 0xFF);
        out[i * 2 + 1] = static_cast<uint8_t>((q >> 8) & 0xFF);
    }
}

inline glm::vec3 UnpackVec3_16(const uint8_t in[6], const glm::vec3& mn, const glm::vec3& ext) {
    glm::vec3 v;
    for (int i = 0; i < 3; ++i) {
        uint16_t q = static_cast<uint16_t>(in[i * 2 + 0] | (in[i * 2 + 1] << 8));
        v[i] = mn[i] + (static_cast<float>(q) / 65535.0f) * ext[i];
    }
    return v;
}

// ── 关键帧抽取（RDP 式：误差超阈值才保留中间帧） ────────────────────────────
// err(a, b) 返回两值差异；lerp(a, b, f) 线性/球面插值。静止轨塌缩为单帧。
template <typename T, typename ErrFn, typename LerpFn>
void ReduceTrack(const std::vector<float>& times, const std::vector<T>& vals,
                 float epsilon, ErrFn err, LerpFn lerp,
                 std::vector<float>& out_times, std::vector<T>& out_vals) {
    out_times.clear();
    out_vals.clear();
    const size_t n = std::min(times.size(), vals.size());
    if (n == 0) return;
    if (n == 1) { out_times.push_back(times[0]); out_vals.push_back(vals[0]); return; }

    // 静止轨：所有值都接近首值 → 单帧
    bool stationary = true;
    for (size_t i = 1; i < n; ++i) {
        if (err(vals[i], vals[0]) > epsilon) { stationary = false; break; }
    }
    if (stationary) { out_times.push_back(times[0]); out_vals.push_back(vals[0]); return; }

    std::vector<uint8_t> keep(n, 0);
    keep[0] = 1;
    keep[n - 1] = 1;

    // 迭代式 RDP，用显式栈避免深递归。
    std::vector<std::pair<size_t, size_t>> stack;
    stack.emplace_back(0, n - 1);
    while (!stack.empty()) {
        auto [lo, hi] = stack.back();
        stack.pop_back();
        if (hi <= lo + 1) continue;

        float span = times[hi] - times[lo];
        size_t worst = lo;
        float worst_err = 0.0f;
        for (size_t i = lo + 1; i < hi; ++i) {
            float f = span > 1e-9f ? (times[i] - times[lo]) / span : 0.0f;
            T interp = lerp(vals[lo], vals[hi], f);
            float e = err(vals[i], interp);
            if (e > worst_err) { worst_err = e; worst = i; }
        }
        if (worst_err > epsilon) {
            keep[worst] = 1;
            stack.emplace_back(lo, worst);
            stack.emplace_back(worst, hi);
        }
    }

    for (size_t i = 0; i < n; ++i) {
        if (keep[i]) { out_times.push_back(times[i]); out_vals.push_back(vals[i]); }
    }
}

inline void ReduceVec3Track(const std::vector<float>& times, const std::vector<glm::vec3>& vals,
                            float epsilon, std::vector<float>& ot, std::vector<glm::vec3>& ov) {
    ReduceTrack<glm::vec3>(times, vals, epsilon,
        [](const glm::vec3& a, const glm::vec3& b) { return glm::length(a - b); },
        [](const glm::vec3& a, const glm::vec3& b, float f) { return glm::mix(a, b, f); },
        ot, ov);
}

inline void ReduceQuatTrack(const std::vector<float>& times, const std::vector<glm::quat>& vals,
                            float epsilon_rad, std::vector<float>& ot, std::vector<glm::quat>& ov) {
    ReduceTrack<glm::quat>(times, vals, epsilon_rad,
        [](const glm::quat& a, const glm::quat& b) {
            float d = std::fabs(glm::dot(glm::normalize(a), glm::normalize(b)));
            d = std::clamp(d, 0.0f, 1.0f);
            return 2.0f * std::acos(d); // 两旋转夹角（弧度）
        },
        [](const glm::quat& a, const glm::quat& b, float f) { return glm::slerp(a, b, f); },
        ot, ov);
}

// ── v3 区间插值（运行时 + 单测共用） ───────────────────────────────────────
inline glm::vec3 SampleQuantVec3(const float* times, const uint8_t* packed, uint32_t n,
                                 const glm::vec3& mn, const glm::vec3& ext, float t) {
    if (n == 0) return glm::vec3(0.0f);
    if (n == 1 || t <= times[0]) return UnpackVec3_16(packed, mn, ext);
    if (t >= times[n - 1]) return UnpackVec3_16(packed + (n - 1) * kQuantVec3Bytes, mn, ext);
    uint32_t p0 = n - 2;
    for (uint32_t i = 0; i + 1 < n; ++i) { if (t < times[i + 1]) { p0 = i; break; } }
    float dt = times[p0 + 1] - times[p0];
    float f = dt > 1e-6f ? std::clamp((t - times[p0]) / dt, 0.0f, 1.0f) : 0.0f;
    glm::vec3 a = UnpackVec3_16(packed + p0 * kQuantVec3Bytes, mn, ext);
    glm::vec3 b = UnpackVec3_16(packed + (p0 + 1) * kQuantVec3Bytes, mn, ext);
    return glm::mix(a, b, f);
}

inline glm::quat SampleQuantQuat(const float* times, const uint8_t* packed, uint32_t n, float t) {
    if (n == 0) return glm::quat(1, 0, 0, 0);
    if (n == 1 || t <= times[0]) return UnpackQuat48(packed);
    if (t >= times[n - 1]) return UnpackQuat48(packed + (n - 1) * kQuantQuatBytes);
    uint32_t p0 = n - 2;
    for (uint32_t i = 0; i + 1 < n; ++i) { if (t < times[i + 1]) { p0 = i; break; } }
    float dt = times[p0 + 1] - times[p0];
    float f = dt > 1e-6f ? std::clamp((t - times[p0]) / dt, 0.0f, 1.0f) : 0.0f;
    glm::quat a = UnpackQuat48(packed + p0 * kQuantQuatBytes);
    glm::quat b = UnpackQuat48(packed + (p0 + 1) * kQuantQuatBytes);
    return glm::slerp(a, b, f);
}

// 解码单条 v3 通道在时刻 t 的采样（带越界保护）。
struct ChannelSampleV3 {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1, 0, 0, 0);
    glm::vec3 scale = glm::vec3(1.0f);
    bool has_position = false;
    bool has_rotation = false;
    bool has_scale = false;
};

inline ChannelSampleV3 SampleChannelV3(const uint8_t* data, size_t size,
                                       const AnimChannelDescV3& d, float t) {
    ChannelSampleV3 out;
    auto in_bounds = [&](uint64_t off, uint64_t bytes) {
        return off + bytes <= static_cast<uint64_t>(size);
    };

    if (d.position_key_count > 0 &&
        in_bounds(d.pos_time_offset, static_cast<uint64_t>(d.position_key_count) * sizeof(float)) &&
        in_bounds(d.pos_offset, static_cast<uint64_t>(d.position_key_count) * kQuantVec3Bytes)) {
        const glm::vec3 mn(d.pos_min[0], d.pos_min[1], d.pos_min[2]);
        const glm::vec3 ext(d.pos_extent[0], d.pos_extent[1], d.pos_extent[2]);
        out.position = SampleQuantVec3(
            reinterpret_cast<const float*>(data + d.pos_time_offset),
            data + d.pos_offset, d.position_key_count, mn, ext, t);
        out.has_position = true;
    }
    if (d.rotation_key_count > 0 &&
        in_bounds(d.rot_time_offset, static_cast<uint64_t>(d.rotation_key_count) * sizeof(float)) &&
        in_bounds(d.rot_offset, static_cast<uint64_t>(d.rotation_key_count) * kQuantQuatBytes)) {
        out.rotation = SampleQuantQuat(
            reinterpret_cast<const float*>(data + d.rot_time_offset),
            data + d.rot_offset, d.rotation_key_count, t);
        out.has_rotation = true;
    }
    if (d.scale_key_count > 0 &&
        in_bounds(d.scale_time_offset, static_cast<uint64_t>(d.scale_key_count) * sizeof(float)) &&
        in_bounds(d.scale_offset, static_cast<uint64_t>(d.scale_key_count) * kQuantVec3Bytes)) {
        const glm::vec3 mn(d.scale_min[0], d.scale_min[1], d.scale_min[2]);
        const glm::vec3 ext(d.scale_extent[0], d.scale_extent[1], d.scale_extent[2]);
        out.scale = SampleQuantVec3(
            reinterpret_cast<const float*>(data + d.scale_time_offset),
            data + d.scale_offset, d.scale_key_count, mn, ext, t);
        out.has_scale = true;
    }
    return out;
}

// ── v3 字节流构建（cook 期 + 单测共用） ─────────────────────────────────────
// 布局：AnimHeader | AnimChannelDescV3[ch] | name_table | [各轨时间/数据交错追加]
inline std::vector<uint8_t> BuildDanimV3(const RawAnimation& anim, const AnimCompressOptions& opts) {
    const float rot_eps = opts.reduce_keyframes
        ? opts.rotation_epsilon_deg * 3.14159265358979323846f / 180.0f : 0.0f;
    const float pos_eps = opts.reduce_keyframes ? opts.position_epsilon : 0.0f;
    const float scale_eps = opts.reduce_keyframes ? opts.scale_epsilon : 0.0f;

    struct ChannelData {
        AnimChannelDescV3 desc;
        std::vector<float> pos_times, rot_times, scale_times;
        std::vector<uint8_t> pos_blob, rot_blob, scale_blob;
    };
    std::vector<ChannelData> chans;
    chans.reserve(anim.channels.size());

    for (const auto& ch : anim.channels) {
        ChannelData cd;
        cd.desc.target_node_index = ch.target_node_index;

        // 位置轨
        if (!ch.position_keys.empty()) {
            std::vector<float> rt; std::vector<glm::vec3> rv;
            if (opts.reduce_keyframes)
                ReduceVec3Track(ch.time_keys, ch.position_keys, pos_eps, rt, rv);
            else {
                size_t m = std::min(ch.time_keys.size(), ch.position_keys.size());
                rt.assign(ch.time_keys.begin(), ch.time_keys.begin() + m);
                rv.assign(ch.position_keys.begin(), ch.position_keys.begin() + m);
            }
            glm::vec3 mn(std::numeric_limits<float>::max());
            glm::vec3 mx(std::numeric_limits<float>::lowest());
            for (const auto& v : rv) { mn = glm::min(mn, v); mx = glm::max(mx, v); }
            if (rv.empty()) { mn = glm::vec3(0); mx = glm::vec3(0); }
            glm::vec3 ext = mx - mn;
            cd.desc.position_key_count = static_cast<uint32_t>(rv.size());
            for (int i = 0; i < 3; ++i) { cd.desc.pos_min[i] = mn[i]; cd.desc.pos_extent[i] = ext[i]; }
            cd.pos_times = rt;
            cd.pos_blob.resize(rv.size() * kQuantVec3Bytes);
            for (size_t i = 0; i < rv.size(); ++i)
                PackVec3_16(rv[i], mn, ext, cd.pos_blob.data() + i * kQuantVec3Bytes);
        }

        // 旋转轨
        if (!ch.rotation_keys.empty()) {
            std::vector<float> rt; std::vector<glm::quat> rv;
            if (opts.reduce_keyframes)
                ReduceQuatTrack(ch.time_keys, ch.rotation_keys, rot_eps, rt, rv);
            else {
                size_t m = std::min(ch.time_keys.size(), ch.rotation_keys.size());
                rt.assign(ch.time_keys.begin(), ch.time_keys.begin() + m);
                rv.assign(ch.rotation_keys.begin(), ch.rotation_keys.begin() + m);
            }
            cd.desc.rotation_key_count = static_cast<uint32_t>(rv.size());
            cd.rot_times = rt;
            cd.rot_blob.resize(rv.size() * kQuantQuatBytes);
            for (size_t i = 0; i < rv.size(); ++i)
                PackQuat48(rv[i], cd.rot_blob.data() + i * kQuantQuatBytes);
        }

        // 缩放轨
        if (!ch.scale_keys.empty()) {
            std::vector<float> rt; std::vector<glm::vec3> rv;
            if (opts.reduce_keyframes)
                ReduceVec3Track(ch.time_keys, ch.scale_keys, scale_eps, rt, rv);
            else {
                size_t m = std::min(ch.time_keys.size(), ch.scale_keys.size());
                rt.assign(ch.time_keys.begin(), ch.time_keys.begin() + m);
                rv.assign(ch.scale_keys.begin(), ch.scale_keys.begin() + m);
            }
            glm::vec3 mn(std::numeric_limits<float>::max());
            glm::vec3 mx(std::numeric_limits<float>::lowest());
            for (const auto& v : rv) { mn = glm::min(mn, v); mx = glm::max(mx, v); }
            if (rv.empty()) { mn = glm::vec3(0); mx = glm::vec3(0); }
            glm::vec3 ext = mx - mn;
            cd.desc.scale_key_count = static_cast<uint32_t>(rv.size());
            for (int i = 0; i < 3; ++i) { cd.desc.scale_min[i] = mn[i]; cd.desc.scale_extent[i] = ext[i]; }
            cd.scale_times = rt;
            cd.scale_blob.resize(rv.size() * kQuantVec3Bytes);
            for (size_t i = 0; i < rv.size(); ++i)
                PackVec3_16(rv[i], mn, ext, cd.scale_blob.data() + i * kQuantVec3Bytes);
        }

        chans.push_back(std::move(cd));
    }

    // name table（与 v2 同格式：uint32 total_size + 每通道 uint16 len + chars，8 字节对齐）
    std::vector<uint8_t> name_table;
    name_table.resize(sizeof(uint32_t), 0);
    for (const auto& ch : anim.channels) {
        const std::string& name = ch.target_node_name;
        uint16_t len = static_cast<uint16_t>(name.size());
        name_table.push_back(static_cast<uint8_t>(len & 0xFF));
        name_table.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        name_table.insert(name_table.end(), name.begin(), name.end());
    }
    while (name_table.size() % 8 != 0) name_table.push_back(0);
    {
        uint32_t total = static_cast<uint32_t>(name_table.size());
        std::memcpy(name_table.data(), &total, sizeof(uint32_t));
    }

    const uint64_t header_bytes = sizeof(AnimHeader);
    const uint64_t descs_bytes = chans.size() * sizeof(AnimChannelDescV3);
    uint64_t cursor = header_bytes + descs_bytes + name_table.size();

    // 依次分配各轨偏移：pos_times, pos_blob, rot_times, rot_blob, scale_times, scale_blob
    for (auto& cd : chans) {
        if (cd.desc.position_key_count > 0) {
            cd.desc.pos_time_offset = cursor; cursor += cd.pos_times.size() * sizeof(float);
            cd.desc.pos_offset = cursor;      cursor += cd.pos_blob.size();
        }
        if (cd.desc.rotation_key_count > 0) {
            cd.desc.rot_time_offset = cursor; cursor += cd.rot_times.size() * sizeof(float);
            cd.desc.rot_offset = cursor;      cursor += cd.rot_blob.size();
        }
        if (cd.desc.scale_key_count > 0) {
            cd.desc.scale_time_offset = cursor; cursor += cd.scale_times.size() * sizeof(float);
            cd.desc.scale_offset = cursor;      cursor += cd.scale_blob.size();
        }
    }

    std::vector<uint8_t> out;
    out.reserve(cursor);
    auto append = [&](const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    };

    AnimHeader header;
    header.version = kDanimVersionQuantized;
    header.duration = anim.duration;
    header.channel_count = static_cast<uint32_t>(chans.size());
    append(&header, sizeof(header));
    for (const auto& cd : chans) append(&cd.desc, sizeof(AnimChannelDescV3));
    append(name_table.data(), name_table.size());
    for (const auto& cd : chans) {
        if (cd.desc.position_key_count > 0) {
            append(cd.pos_times.data(), cd.pos_times.size() * sizeof(float));
            append(cd.pos_blob.data(), cd.pos_blob.size());
        }
        if (cd.desc.rotation_key_count > 0) {
            append(cd.rot_times.data(), cd.rot_times.size() * sizeof(float));
            append(cd.rot_blob.data(), cd.rot_blob.size());
        }
        if (cd.desc.scale_key_count > 0) {
            append(cd.scale_times.data(), cd.scale_times.size() * sizeof(float));
            append(cd.scale_blob.data(), cd.scale_blob.size());
        }
    }
    return out;
}

}  // namespace compiler
}  // namespace asset
}  // namespace dse
