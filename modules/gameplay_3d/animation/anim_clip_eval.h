#ifndef DSE_ANIM_CLIP_EVAL_H
#define DSE_ANIM_CLIP_EVAL_H

#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace gameplay3d {
namespace anim_util {

inline AssetManager& RequireAssetManager(AssetManager* am) {
    if (am) return *am;
    throw std::runtime_error("Animation system requires an injected AssetManager");
}

inline bool IsValidAnimHeader(const asset::compiler::AnimHeader* h) {
    return h && h->magic[0] == 'D' && h->magic[1] == 'S' &&
           h->magic[2] == 'E' && h->magic[3] == 'A' && h->duration >= 0.0f;
}

template<typename T>
T Interpolate(const std::vector<float>& times, const std::vector<T>& values, float t) {
    if (times.empty() || values.empty()) return T();
    const size_t n = std::min(times.size(), values.size());
    if (n == 1) return values[0];
    if (t <= times[0]) return values[0];
    if (t >= times[n - 1]) return values[n - 1];

    size_t p0 = n - 2;
    for (size_t i = 0; i + 1 < n; ++i) {
        if (t < times[i + 1]) { p0 = i; break; }
    }
    float dt = times[p0 + 1] - times[p0];
    if (std::abs(dt) <= 1e-6f) return values[p0];
    float f = std::clamp((t - times[p0]) / dt, 0.0f, 1.0f);
    if constexpr (std::is_same_v<T, glm::quat>) return glm::slerp(values[p0], values[p0 + 1], f);
    else return glm::mix(values[p0], values[p0 + 1], f);
}

inline float AdvanceClipTime(float current, float delta_time, float speed, float dur, bool loop) {
    current += delta_time * speed;
    if (dur <= 0.0f) return 0.0f;
    if (current > dur) return loop ? std::fmod(current, dur) : dur;
    if (current < 0.0f) {
        current = loop ? dur + std::fmod(current, dur) : 0.0f;
        if (current >= dur) current = 0.0f;
    }
    return current;
}

struct AnimSampleBuffer {
    std::vector<glm::vec3> positions;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;
    std::vector<bool> touched;

    explicit AnimSampleBuffer(uint32_t bone_count)
        : positions(bone_count, glm::vec3(0.0f))
        , rotations(bone_count, glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
        , scales(bone_count, glm::vec3(1.0f))
        , touched(bone_count, false) {}
};

inline bool EvaluateClip(
    AssetManager& am,
    const std::string& anim_path,
    float current_time,
    const std::unordered_map<std::string, int>& bone_name_to_index,
    uint32_t bone_count,
    AnimSampleBuffer& sample,
    float& out_duration)
{
    if (anim_path.empty()) return false;
    auto danim = am.LoadDanim(anim_path);
    if (!danim || danim->GetData().empty()) return false;

    const uint8_t* data = danim->GetData().data();
    const size_t data_size = danim->GetData().size();
    const auto* header = reinterpret_cast<const asset::compiler::AnimHeader*>(data);
    if (!IsValidAnimHeader(header)) return false;
    out_duration = header->duration;
    const auto* channels = reinterpret_cast<const asset::compiler::AnimChannelDesc*>(
        data + sizeof(asset::compiler::AnimHeader));

    // Parse v2 channel name table for cross-skeleton remapping
    std::vector<std::string> channel_names;
    if (header->version >= 2 && !bone_name_to_index.empty()) {
        const uint8_t* nt_ptr = data + sizeof(asset::compiler::AnimHeader) +
            header->channel_count * sizeof(asset::compiler::AnimChannelDesc);
        if (nt_ptr + sizeof(uint32_t) <= data + data_size) {
            uint32_t nt_total = 0;
            std::memcpy(&nt_total, nt_ptr, sizeof(uint32_t));
            const uint8_t* np = nt_ptr + sizeof(uint32_t);
            const uint8_t* ne = nt_ptr + nt_total;
            if (ne > data + data_size) ne = data + data_size;
            channel_names.reserve(header->channel_count);
            for (uint32_t ci = 0; ci < header->channel_count && np + 2 <= ne; ++ci) {
                uint16_t nl = static_cast<uint16_t>(np[0] | (np[1] << 8));
                np += 2;
                if (np + nl > ne) break;
                channel_names.emplace_back(reinterpret_cast<const char*>(np), nl);
                np += nl;
            }
        }
    }

    for (uint32_t i = 0; i < header->channel_count; ++i) {
        const auto& ch = channels[i];
        int target_bone = ch.target_node_index;
        if (i < static_cast<uint32_t>(channel_names.size()) && !channel_names[i].empty()) {
            auto it = bone_name_to_index.find(channel_names[i]);
            if (it != bone_name_to_index.end()) target_bone = it->second;
            else continue;
        }
        if (target_bone < 0 || target_bone >= static_cast<int>(bone_count)) continue;

        if (ch.time_offset + ch.position_key_count * sizeof(float) > data_size) continue;
        if (ch.position_offset + ch.position_key_count * sizeof(glm::vec3) > data_size) continue;
        if (ch.rotation_offset + ch.rotation_key_count * sizeof(glm::quat) > data_size) continue;
        if (ch.scale_offset + ch.scale_key_count * sizeof(glm::vec3) > data_size) continue;

        auto BuildKeyTimes = [&](uint32_t key_count) -> std::vector<float> {
            if (key_count == 0) return {};
            std::vector<float> r(key_count);
            std::memcpy(r.data(), data + ch.time_offset, key_count * sizeof(float));
            return r;
        };

        if (ch.position_key_count > 0) {
            auto times = BuildKeyTimes(ch.position_key_count);
            std::vector<glm::vec3> vals(ch.position_key_count);
            std::memcpy(vals.data(), data + ch.position_offset, ch.position_key_count * sizeof(glm::vec3));
            sample.positions[target_bone] = Interpolate<glm::vec3>(times, vals, current_time);
            sample.touched[target_bone] = true;
        }
        if (ch.rotation_key_count > 0) {
            auto times = BuildKeyTimes(ch.rotation_key_count);
            std::vector<glm::quat> vals(ch.rotation_key_count);
            std::memcpy(vals.data(), data + ch.rotation_offset, ch.rotation_key_count * sizeof(glm::quat));
            sample.rotations[target_bone] = Interpolate<glm::quat>(times, vals, current_time);
            sample.touched[target_bone] = true;
        }
        if (ch.scale_key_count > 0) {
            auto times = BuildKeyTimes(ch.scale_key_count);
            std::vector<glm::vec3> vals(ch.scale_key_count);
            std::memcpy(vals.data(), data + ch.scale_offset, ch.scale_key_count * sizeof(glm::vec3));
            sample.scales[target_bone] = Interpolate<glm::vec3>(times, vals, current_time);
            sample.touched[target_bone] = true;
        }
    }
    return true;
}

} // namespace anim_util
} // namespace gameplay3d
} // namespace dse

#endif // DSE_ANIM_CLIP_EVAL_H
