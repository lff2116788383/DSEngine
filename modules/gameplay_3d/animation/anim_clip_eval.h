#ifndef DSE_ANIM_CLIP_EVAL_H
#define DSE_ANIM_CLIP_EVAL_H

#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include "engine/ecs/components_3d.h"
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

// Compute world-space transforms for all bones from pose_buffer.
// This is a shared utility for IK systems.
inline void ComputeBoneGlobals(
    const Animator3DComponent::PoseBuffer& pb,
    const Animator3DComponent::SkeletalCache& cache,
    std::vector<glm::mat4>& out_globals)
{
    const uint32_t bone_count = cache.bone_count;
    out_globals.resize(bone_count);

    // Build local matrices
    std::vector<glm::mat4> locals(bone_count);
    for (uint32_t i = 0; i < bone_count; ++i) {
        if (pb.touched[i]) {
            locals[i] = glm::translate(glm::mat4(1.0f), pb.positions[i])
                * glm::mat4_cast(pb.rotations[i])
                * glm::scale(glm::mat4(1.0f), pb.scales[i]);
        } else {
            locals[i] = cache.local_bind_poses[i];
        }
    }

    // Propagate globals (iterative for arbitrary order)
    std::vector<bool> computed(bone_count, false);
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = cache.parent_indices[i];
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            out_globals[i] = locals[i];
            computed[i] = true;
        }
    }
    for (uint32_t pass = 0; pass < bone_count; ++pass) {
        bool all_done = true;
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (computed[i]) continue;
            int pi = cache.parent_indices[i];
            if (computed[pi]) {
                out_globals[i] = out_globals[pi] * locals[i];
                computed[i] = true;
            } else {
                all_done = false;
            }
        }
        if (all_done) break;
    }
}

// Compute world-space positions for a chain of bones.
inline void ComputeChainPositions(
    const std::vector<int>& chain_indices,
    const Animator3DComponent::PoseBuffer& pb,
    const Animator3DComponent::SkeletalCache& cache,
    std::vector<glm::vec3>& out_positions,
    std::vector<glm::mat4>& out_globals)
{
    ComputeBoneGlobals(pb, cache, out_globals);

    out_positions.resize(chain_indices.size());
    for (size_t i = 0; i < chain_indices.size(); ++i) {
        out_positions[i] = glm::vec3(out_globals[chain_indices[i]][3]);
    }
}

// FABRIK forward-backward reaching IK.
// Operates entirely on the provided positions[] array (caller's coordinate space).
// pole_vector: optional bend-direction hint (pass glm::vec3(0) to ignore).
inline void SolveFABRIK(
    std::vector<glm::vec3>& positions,
    const glm::vec3& target,
    const glm::vec3& pole_vector,
    int max_iterations,
    float tolerance)
{
    const size_t n = positions.size();
    if (n < 2) return;

    std::vector<float> lengths(n - 1);
    float total_length = 0.0f;
    for (size_t i = 0; i < n - 1; ++i) {
        lengths[i] = glm::distance(positions[i], positions[i + 1]);
        total_length += lengths[i];
    }

    const glm::vec3 root_pos = positions[0];

    // Unreachable: stretch toward target
    if (glm::distance(root_pos, target) >= total_length) {
        glm::vec3 dir = glm::normalize(target - root_pos);
        for (size_t i = 1; i < n; ++i)
            positions[i] = positions[i - 1] + dir * lengths[i - 1];
        return;
    }

    for (int iter = 0; iter < max_iterations; ++iter) {
        if (glm::distance(positions[n - 1], target) < tolerance) break;

        // Forward pass: tip → root
        positions[n - 1] = target;
        for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
            glm::vec3 d = positions[i] - positions[i + 1];
            float len = glm::length(d);
            positions[i] = positions[i + 1] + (len > 1e-6f ? d / len : glm::vec3(0, 1, 0)) * lengths[i];
        }

        // Backward pass: root → tip
        positions[0] = root_pos;
        for (size_t i = 0; i < n - 1; ++i) {
            glm::vec3 d = positions[i + 1] - positions[i];
            float len = glm::length(d);
            positions[i + 1] = positions[i] + (len > 1e-6f ? d / len : glm::vec3(0, 1, 0)) * lengths[i];
        }

        // Pole vector constraint: nudge middle joints toward bend direction
        if (n > 2 && glm::length(pole_vector) > 1e-6f) {
            for (size_t i = 1; i < n - 1; ++i) {
                glm::vec3 line = positions[n - 1] - positions[0];
                float line_len = glm::length(line);
                if (line_len < 1e-6f) continue;
                glm::vec3 line_dir = line / line_len;
                float t = glm::dot(positions[i] - positions[0], line_dir);
                glm::vec3 projected = positions[0] + line_dir * t;
                if (glm::length(positions[i] - projected) < lengths[i - 1] * 0.01f) {
                    glm::vec3 pole_perp = pole_vector - glm::dot(pole_vector, line_dir) * line_dir;
                    float pole_len = glm::length(pole_perp);
                    if (pole_len > 1e-6f) {
                        positions[i] = projected + (pole_perp / pole_len) * lengths[i - 1] * 0.1f;
                        glm::vec3 d = positions[i] - positions[i - 1];
                        float l = glm::length(d);
                        if (l > 1e-6f) positions[i] = positions[i - 1] + (d / l) * lengths[i - 1];
                    }
                }
            }
        }
    }
}

// Write FABRIK result positions back to pose_buffer as blended local rotations.
// globals_before: full per-bone global transforms before IK was applied.
// Updates all chain bones (root to tip-1) and propagates incrementally.
inline void WriteBackFABRIKResult(
    const std::vector<int>& chain_indices,
    const std::vector<glm::vec3>& ik_positions,
    const std::vector<glm::mat4>& globals_before,
    Animator3DComponent::PoseBuffer& pb,
    const Animator3DComponent::SkeletalCache& cache,
    float weight)
{
    if (chain_indices.size() < 2) return;

    std::vector<glm::mat4> updated_globals = globals_before;
    const size_t n = chain_indices.size();

    for (size_t ci = 0; ci < n - 1; ++ci) {
        int bone_idx  = chain_indices[ci];
        int child_idx = chain_indices[ci + 1];

        glm::vec3 cur_dir = glm::vec3(updated_globals[child_idx][3]) - glm::vec3(updated_globals[bone_idx][3]);
        float cur_len = glm::length(cur_dir);
        if (cur_len < 1e-6f) continue;
        cur_dir /= cur_len;

        glm::vec3 ik_dir = ik_positions[ci + 1] - ik_positions[ci];
        float ik_len = glm::length(ik_dir);
        if (ik_len < 1e-6f) continue;
        ik_dir /= ik_len;

        glm::quat delta_world    = glm::rotation(cur_dir, ik_dir);
        glm::quat bone_world_rot = glm::normalize(glm::quat_cast(glm::mat3(updated_globals[bone_idx])));
        glm::quat new_world_rot  = delta_world * bone_world_rot;

        int parent_idx = cache.parent_indices[bone_idx];
        glm::quat parent_world_rot = (parent_idx >= 0 && parent_idx < static_cast<int>(cache.bone_count))
            ? glm::normalize(glm::quat_cast(glm::mat3(updated_globals[parent_idx])))
            : glm::quat(1, 0, 0, 0);

        glm::quat new_local = glm::inverse(parent_world_rot) * new_world_rot;
        pb.rotations[bone_idx] = glm::normalize(glm::slerp(pb.rotations[bone_idx], new_local, weight));
        pb.touched[bone_idx] = true;

        // Propagate updated global for this bone and its child
        glm::mat4 parent_global = (parent_idx >= 0 && parent_idx < static_cast<int>(cache.bone_count))
            ? updated_globals[parent_idx] : glm::mat4(1.0f);
        glm::mat4 new_local_mat = glm::translate(glm::mat4(1.0f), pb.positions[bone_idx])
            * glm::mat4_cast(pb.rotations[bone_idx])
            * glm::scale(glm::mat4(1.0f), pb.scales[bone_idx]);
        updated_globals[bone_idx] = parent_global * new_local_mat;

        glm::mat4 child_local = pb.touched[child_idx]
            ? glm::translate(glm::mat4(1.0f), pb.positions[child_idx])
              * glm::mat4_cast(pb.rotations[child_idx])
              * glm::scale(glm::mat4(1.0f), pb.scales[child_idx])
            : cache.local_bind_poses[child_idx];
        updated_globals[child_idx] = updated_globals[bone_idx] * child_local;
    }
}

} // namespace anim_util
} // namespace gameplay3d
} // namespace dse

#endif // DSE_ANIM_CLIP_EVAL_H
