// 动画压缩（.danim v3）单元测试。
//
// 覆盖：
//   - 旋转 smallest-three 48bit 量化 round-trip 误差界
//   - 位置/缩放 3×16bit 定点量化 round-trip 误差界
//   - 关键帧抽取（线性段塌缩为端点、静止轨塌缩为单帧、尖角保留）
//   - BuildDanimV3 → SampleChannelV3 端到端：在关键帧时刻采样误差 < ε
//   - 容器越界保护

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "engine/assets/compiler/anim_compress.h"
#include "engine/assets/compiler/raw_scene_data.h"

using namespace dse::asset::compiler;

namespace {

float QuatAngle(const glm::quat& a, const glm::quat& b) {
    float d = std::fabs(glm::dot(glm::normalize(a), glm::normalize(b)));
    d = std::min(1.0f, std::max(0.0f, d));
    return 2.0f * std::acos(d);
}

}  // namespace

// ── 量化 round-trip ─────────────────────────────────────────────────────────

TEST(AnimQuant, QuatRoundTripWithinTolerance) {
    const glm::quat samples[] = {
        glm::quat(1, 0, 0, 0),
        glm::normalize(glm::quat(0.5f, 0.5f, 0.5f, 0.5f)),
        glm::angleAxis(glm::radians(37.0f), glm::normalize(glm::vec3(1, 2, 3))),
        glm::angleAxis(glm::radians(160.0f), glm::normalize(glm::vec3(-1, 0.3f, 0.7f))),
        glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 1, 0)),
    };
    for (const auto& q : samples) {
        uint8_t packed[6];
        PackQuat48(q, packed);
        glm::quat r = UnpackQuat48(packed);
        // 15bit/分量 → 角误差应远小于 1 度。
        EXPECT_LT(QuatAngle(q, r), glm::radians(0.2f)) << "quat quant error too large";
    }
}

TEST(AnimQuant, Vec3RoundTripWithinTolerance) {
    const glm::vec3 mn(-2.0f, 0.0f, -10.0f);
    const glm::vec3 mx(3.0f, 1.0f, 5.0f);
    const glm::vec3 ext = mx - mn;
    const glm::vec3 samples[] = {
        mn, mx, glm::vec3(0.0f, 0.5f, -3.0f), glm::vec3(2.999f, 0.001f, 4.5f),
    };
    for (const auto& v : samples) {
        uint8_t packed[6];
        PackVec3_16(v, mn, ext, packed);
        glm::vec3 r = UnpackVec3_16(packed, mn, ext);
        // 16bit 在 extent 上 → 每分量误差 < extent/65535 * 1.5
        for (int i = 0; i < 3; ++i)
            EXPECT_LT(std::fabs(r[i] - v[i]), ext[i] / 65535.0f * 2.0f + 1e-5f);
    }
}

TEST(AnimQuant, Vec3ZeroExtentIsStable) {
    const glm::vec3 mn(1.0f, 2.0f, 3.0f);
    const glm::vec3 ext(0.0f);
    uint8_t packed[6];
    PackVec3_16(glm::vec3(1.0f, 2.0f, 3.0f), mn, ext, packed);
    glm::vec3 r = UnpackVec3_16(packed, mn, ext);
    EXPECT_FLOAT_EQ(r.x, 1.0f);
    EXPECT_FLOAT_EQ(r.y, 2.0f);
    EXPECT_FLOAT_EQ(r.z, 3.0f);
}

// ── 关键帧抽取 ──────────────────────────────────────────────────────────────

TEST(AnimReduce, LinearRampCollapsesToEndpoints) {
    std::vector<float> t;
    std::vector<glm::vec3> v;
    for (int i = 0; i <= 10; ++i) {
        t.push_back(static_cast<float>(i));
        v.push_back(glm::vec3(static_cast<float>(i) * 0.5f, 0.0f, 0.0f));  // 完美线性
    }
    std::vector<float> ot;
    std::vector<glm::vec3> ov;
    ReduceVec3Track(t, v, 1e-4f, ot, ov);
    EXPECT_EQ(ot.size(), 2u);  // 仅保留首尾
    EXPECT_EQ(ov.size(), 2u);
    EXPECT_FLOAT_EQ(ov.front().x, 0.0f);
    EXPECT_FLOAT_EQ(ov.back().x, 5.0f);
}

TEST(AnimReduce, StaticTrackCollapsesToSingleKey) {
    std::vector<float> t = {0, 1, 2, 3, 4};
    std::vector<glm::vec3> v(5, glm::vec3(7.0f, -1.0f, 2.0f));
    std::vector<float> ot;
    std::vector<glm::vec3> ov;
    ReduceVec3Track(t, v, 1e-4f, ot, ov);
    EXPECT_EQ(ot.size(), 1u);
    EXPECT_EQ(ov.size(), 1u);
}

TEST(AnimReduce, SharpCornerIsPreserved) {
    // 三角形：0 → 1 → 0，中间帧不可由端点线性还原，必须保留。
    std::vector<float> t = {0, 1, 2};
    std::vector<glm::vec3> v = {glm::vec3(0), glm::vec3(1, 0, 0), glm::vec3(0)};
    std::vector<float> ot;
    std::vector<glm::vec3> ov;
    ReduceVec3Track(t, v, 1e-3f, ot, ov);
    EXPECT_EQ(ot.size(), 3u);
}

TEST(AnimReduce, StaticQuatTrackCollapses) {
    std::vector<float> t = {0, 1, 2, 3};
    std::vector<glm::quat> v(4, glm::normalize(glm::quat(0.5f, 0.5f, 0.5f, 0.5f)));
    std::vector<float> ot;
    std::vector<glm::quat> ov;
    ReduceQuatTrack(t, v, glm::radians(0.1f), ot, ov);
    EXPECT_EQ(ot.size(), 1u);
}

// ── 端到端：BuildDanimV3 → SampleChannelV3 ──────────────────────────────────

namespace {

RawAnimation MakeWalkAnim() {
    RawAnimation anim;
    anim.name = "walk";
    anim.duration = 2.0f;

    RawAnimationChannel ch;
    ch.target_node_index = 0;
    ch.target_node_name = "hips";
    for (int i = 0; i <= 8; ++i) {
        float t = static_cast<float>(i) * 0.25f;
        ch.time_keys.push_back(t);
        ch.position_keys.push_back(glm::vec3(std::sin(t), t * 0.1f, std::cos(t)));
        ch.rotation_keys.push_back(glm::angleAxis(t, glm::normalize(glm::vec3(0, 1, 0))));
        ch.scale_keys.push_back(glm::vec3(1.0f + 0.05f * std::sin(t)));
    }
    anim.channels.push_back(ch);
    return anim;
}

}  // namespace

TEST(AnimV3RoundTrip, SamplesMatchSourceWithinTolerance) {
    RawAnimation anim = MakeWalkAnim();
    AnimCompressOptions opts;  // quantize + reduce
    opts.reduce_keyframes = false;  // 不抽取，直接验证量化+插值精度

    std::vector<uint8_t> blob = BuildDanimV3(anim, opts);
    ASSERT_GE(blob.size(), sizeof(AnimHeader) + sizeof(AnimChannelDescV3));

    const auto* header = reinterpret_cast<const AnimHeader*>(blob.data());
    EXPECT_EQ(header->magic[0], 'D');
    EXPECT_EQ(header->magic[3], 'A');
    EXPECT_EQ(header->version, kDanimVersionQuantized);
    EXPECT_EQ(header->channel_count, 1u);
    EXPECT_FLOAT_EQ(header->duration, anim.duration);

    const auto* desc = reinterpret_cast<const AnimChannelDescV3*>(
        blob.data() + sizeof(AnimHeader));

    const auto& ch = anim.channels[0];
    for (size_t i = 0; i < ch.time_keys.size(); ++i) {
        float t = ch.time_keys[i];
        ChannelSampleV3 s = SampleChannelV3(blob.data(), blob.size(), *desc, t);
        ASSERT_TRUE(s.has_position);
        ASSERT_TRUE(s.has_rotation);
        ASSERT_TRUE(s.has_scale);
        EXPECT_LT(glm::length(s.position - ch.position_keys[i]), 1e-3f);
        EXPECT_LT(QuatAngle(s.rotation, ch.rotation_keys[i]), glm::radians(0.5f));
        EXPECT_LT(glm::length(s.scale - ch.scale_keys[i]), 1e-3f);
    }
}

TEST(AnimV3RoundTrip, ReducedClipStaysWithinErrorBudget) {
    RawAnimation anim = MakeWalkAnim();
    AnimCompressOptions opts;
    opts.reduce_keyframes = true;
    opts.position_epsilon = 1e-2f;
    opts.scale_epsilon = 1e-2f;
    opts.rotation_epsilon_deg = 1.0f;

    std::vector<uint8_t> blob = BuildDanimV3(anim, opts);
    const auto* desc = reinterpret_cast<const AnimChannelDescV3*>(
        blob.data() + sizeof(AnimHeader));

    // 抽取后帧数应 <= 原始帧数。
    EXPECT_LE(desc->position_key_count, anim.channels[0].position_keys.size());

    // 在原始关键帧时刻采样，误差应在预算 + 量化噪声内。
    const auto& ch = anim.channels[0];
    for (size_t i = 0; i < ch.time_keys.size(); ++i) {
        float t = ch.time_keys[i];
        ChannelSampleV3 s = SampleChannelV3(blob.data(), blob.size(), *desc, t);
        EXPECT_LT(glm::length(s.position - ch.position_keys[i]), 5e-2f);
        EXPECT_LT(QuatAngle(s.rotation, ch.rotation_keys[i]), glm::radians(3.0f));
    }
}

TEST(AnimV3RoundTrip, OutOfRangeTimeClampsToEndpoints) {
    RawAnimation anim = MakeWalkAnim();
    AnimCompressOptions opts;
    opts.reduce_keyframes = false;
    std::vector<uint8_t> blob = BuildDanimV3(anim, opts);
    const auto* desc = reinterpret_cast<const AnimChannelDescV3*>(
        blob.data() + sizeof(AnimHeader));

    ChannelSampleV3 before = SampleChannelV3(blob.data(), blob.size(), *desc, -10.0f);
    ChannelSampleV3 after = SampleChannelV3(blob.data(), blob.size(), *desc, 1000.0f);
    const auto& ch = anim.channels[0];
    EXPECT_LT(glm::length(before.position - ch.position_keys.front()), 1e-3f);
    EXPECT_LT(glm::length(after.position - ch.position_keys.back()), 1e-3f);
}

TEST(AnimV3RoundTrip, TruncatedBufferIsRejectedGracefully) {
    RawAnimation anim = MakeWalkAnim();
    AnimCompressOptions opts;
    std::vector<uint8_t> blob = BuildDanimV3(anim, opts);
    const auto* desc = reinterpret_cast<const AnimChannelDescV3*>(
        blob.data() + sizeof(AnimHeader));
    // 截断到只剩 header+desc：偏移越界，采样应不读越界数据（has_* 为 false）。
    size_t truncated = sizeof(AnimHeader) + sizeof(AnimChannelDescV3);
    ChannelSampleV3 s = SampleChannelV3(blob.data(), truncated, *desc, 0.5f);
    EXPECT_FALSE(s.has_position);
    EXPECT_FALSE(s.has_rotation);
    EXPECT_FALSE(s.has_scale);
}
