#include "modules/gameplay_3d/animation/animator_system.h"
#include "engine/base/debug.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace dse {
namespace gameplay3d {

namespace {

AssetManager& RequireAssetManager(AssetManager* asset_manager) {
    if (asset_manager != nullptr) {
        return *asset_manager;
    }
    throw std::runtime_error("AnimatorSystem requires an injected AssetManager");
}

bool IsValidAnimHeader(const asset::compiler::AnimHeader* header) {
    return header != nullptr &&
        header->magic[0] == 'D' &&
        header->magic[1] == 'S' &&
        header->magic[2] == 'E' &&
        header->magic[3] == 'A' &&
        header->duration >= 0.0f;
}

template<typename T>
T Interpolate(const std::vector<float>& times, const std::vector<T>& values, float current_time) {
    if (times.empty() || values.empty()) return T();

    const size_t key_count = std::min(times.size(), values.size());
    if (key_count == 0) return T();
    if (key_count == 1) return values[0];

    if (current_time <= times[0]) {
        return values[0];
    }
    if (current_time >= times[key_count - 1]) {
        return values[key_count - 1];
    }

    size_t p0 = key_count - 2;
    for (size_t i = 0; i + 1 < key_count; ++i) {
        if (current_time < times[i + 1]) {
            p0 = i;
            break;
        }
    }
    const size_t p1 = p0 + 1;

    const float t0 = times[p0];
    const float t1 = times[p1];
    if (std::abs(t1 - t0) <= 1e-6f) {
        return values[p0];
    }

    float factor = (current_time - t0) / (t1 - t0);
    factor = std::clamp(factor, 0.0f, 1.0f);

    if constexpr (std::is_same_v<T, glm::quat>) {
        return glm::slerp(values[p0], values[p1], factor);
    } else {
        return glm::mix(values[p0], values[p1], factor);
    }
}

std::vector<float> BuildKeyTimes(const uint8_t* data, const asset::compiler::AnimChannelDesc& ch, uint32_t key_count) {
    if (key_count == 0) {
        return {};
    }

    std::vector<float> result(key_count);
    std::memcpy(result.data(), data + ch.time_offset, key_count * sizeof(float));
    return result;
}

float AdvanceClipTime(float current_time, float delta_time, float speed, float duration, bool loop) {
    current_time += delta_time * speed;
    if (duration <= 0.0f) {
        return 0.0f;
    }
    if (current_time > duration) {
        if (loop) {
            current_time = std::fmod(current_time, duration);
        } else {
            current_time = duration;
        }
    } else if (current_time < 0.0f) {
        current_time = loop ? duration + std::fmod(current_time, duration) : 0.0f;
        if (current_time >= duration) {
            current_time = 0.0f;
        }
    }
    return current_time;
}

struct SampleBuffer {
    std::vector<glm::vec3> positions;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;
    std::vector<bool> touched;

    explicit SampleBuffer(uint32_t bone_count)
        : positions(bone_count, glm::vec3(0.0f))
        , rotations(bone_count, glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
        , scales(bone_count, glm::vec3(1.0f))
        , touched(bone_count, false) {}
};

void ApplySamplesToLocalTransforms(const SampleBuffer& sample, std::vector<glm::mat4>& local_transforms, uint32_t bone_count) {
    for (uint32_t i = 0; i < bone_count; ++i) {
        if (!sample.touched[i]) continue;  // keep bind pose for bones without animation data
        local_transforms[i] = glm::translate(glm::mat4(1.0f), sample.positions[i]) * glm::mat4_cast(sample.rotations[i]) * glm::scale(glm::mat4(1.0f), sample.scales[i]);
    }
}

}

AssetManager* AnimatorSystem::asset_manager_ = nullptr;

void AnimatorSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

void AnimatorSystem::Update(World& world, float delta_time) {
    auto view = world.registry().view<Animator3DComponent>();
    if (view.empty()) return;
    
    AssetManager* asset_manager_ptr = nullptr;
    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        if (!animator.enabled) {
            continue;
        }
        
        if (animator.dskel_path.empty()) {
            continue;
        }

        // 延迟获取 AssetManager：仅在首次遇到真正需要加载资源的实体时校验
        if (!asset_manager_ptr) {
            asset_manager_ptr = &RequireAssetManager(asset_manager_);
        }
        auto& asset_manager = *asset_manager_ptr;

        auto dskel = asset_manager.LoadDskel(animator.dskel_path);
        if (!dskel || dskel->GetData().empty()) {
            continue;
        }

        const uint8_t* skel_data = dskel->GetData().data();
        const asset::compiler::SkelHeader* skel_header = reinterpret_cast<const asset::compiler::SkelHeader*>(skel_data);
        if (skel_header->magic[0] != 'D' || skel_header->magic[1] != 'S' || skel_header->magic[2] != 'E' || skel_header->magic[3] != 'S') {
            continue;
        }

        const asset::compiler::BoneDesc* bones = reinterpret_cast<const asset::compiler::BoneDesc*>(skel_data + sizeof(asset::compiler::SkelHeader));
        uint32_t bone_count = std::min(skel_header->bone_count, static_cast<uint32_t>(MAX_BONES));

        // Parse dskel v2 bone name table for cross-skeleton remapping
        std::unordered_map<std::string, int> bone_name_to_index;
        if (skel_header->version >= 2) {
            const uint8_t* name_ptr = skel_data + sizeof(asset::compiler::SkelHeader) + bone_count * sizeof(asset::compiler::BoneDesc);
            const uint8_t* skel_end = skel_data + dskel->GetData().size();
            for (uint32_t bi = 0; bi < bone_count && name_ptr + 2 <= skel_end; ++bi) {
                uint16_t name_len = static_cast<uint16_t>(name_ptr[0] | (name_ptr[1] << 8));
                name_ptr += 2;
                if (name_ptr + name_len > skel_end) break;
                std::string bname(reinterpret_cast<const char*>(name_ptr), name_len);
                name_ptr += name_len;
                bone_name_to_index[bname] = static_cast<int>(bi);
            }
        }

        if (animator.final_bone_matrices.size() < bone_count) {
            animator.final_bone_matrices.resize(bone_count, glm::mat4(1.0f));
            // 首次 resize 时输出诊断：确认骨骼数，供 verify_lua_3d_demos.py 检测 final_bones=48
            static int vse1522_animator_first_log_count = 0;
            if (vse1522_animator_first_log_count < 2) {
                DEBUG_LOG_INFO("[3D][VSE15.22] animator_system_first_update entity={} dskel_path={} bone_count={} final_bones={} note=animator_system_confirms_bone_count",
                    static_cast<unsigned int>(entity), animator.dskel_path, bone_count, animator.final_bone_matrices.size());
                ++vse1522_animator_first_log_count;
            }
        }

        // 1. Compute bind-pose global transforms from local_transforms (node tree basis).
        //    These serve as the reference for the "relative deformation" approach:
        //    final_bone_matrix = anim_global * inv(bind_global)
        //    This avoids using inverse_bind_matrix which may be inconsistent with
        //    local_transform when the FBX has an Armature node transform.
        //    NOTE: Bones may NOT be in topological order (e.g., Mixamo has children before
        //    parents). We use iterative propagation to handle arbitrary ordering.
        std::vector<glm::mat4> bind_globals(bone_count);
        std::vector<bool> computed(bone_count, false);
        // First pass: identify roots
        for (uint32_t i = 0; i < bone_count; ++i) {
            int parent_index = bones[i].parent_index;
            if (parent_index < 0 || parent_index >= static_cast<int>(bone_count)) {
                bind_globals[i] = bones[i].local_transform;
                computed[i] = true;
            }
        }
        // Iterative passes until all computed
        for (uint32_t pass = 0; pass < bone_count; ++pass) {
            bool all_done = true;
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (computed[i]) continue;
                int parent_index = bones[i].parent_index;
                if (computed[parent_index]) {
                    bind_globals[i] = bind_globals[parent_index] * bones[i].local_transform;
                    computed[i] = true;
                } else {
                    all_done = false;
                }
            }
            if (all_done) break;
        }

        // Initialize animated local transforms with bind pose
        std::vector<glm::mat4> local_transforms(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i) {
            local_transforms[i] = bones[i].local_transform;
        }
        
        auto EvaluateClip = [&](const std::string& anim_path, float current_time, SampleBuffer& sample, float& out_duration) -> bool {
            if (anim_path.empty()) return false;

            auto danim = asset_manager.LoadDanim(anim_path);
            if (!danim || danim->GetData().empty()) return false;
            
            const uint8_t* data = danim->GetData().data();
            const size_t data_size = danim->GetData().size();
            const asset::compiler::AnimHeader* header = reinterpret_cast<const asset::compiler::AnimHeader*>(data);
            if (!IsValidAnimHeader(header)) return false;
            out_duration = header->duration;
            const asset::compiler::AnimChannelDesc* channels = reinterpret_cast<const asset::compiler::AnimChannelDesc*>(data + sizeof(asset::compiler::AnimHeader));

            // Parse danim v2 channel name table for cross-skeleton remapping
            std::vector<std::string> channel_names;
            if (header->version >= 2 && !bone_name_to_index.empty()) {
                const uint8_t* nt_ptr = data + sizeof(asset::compiler::AnimHeader) + header->channel_count * sizeof(asset::compiler::AnimChannelDesc);
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

                // Resolve target bone index: prefer name-based remap, fall back to index
                int target_bone = ch.target_node_index;
                if (i < static_cast<uint32_t>(channel_names.size()) && !channel_names[i].empty()) {
                    auto name_it = bone_name_to_index.find(channel_names[i]);
                    if (name_it != bone_name_to_index.end()) {
                        target_bone = name_it->second;
                    } else {
                        continue;  // bone not found in target skeleton, skip
                    }
                }
                if (target_bone < 0 || target_bone >= static_cast<int>(bone_count)) continue;
                
                // Bounds check all offsets before reading
                if (ch.time_offset + ch.position_key_count * sizeof(float) > data_size) continue;
                if (ch.position_offset + ch.position_key_count * sizeof(glm::vec3) > data_size) continue;
                if (ch.rotation_offset + ch.rotation_key_count * sizeof(glm::quat) > data_size) continue;
                if (ch.scale_offset + ch.scale_key_count * sizeof(glm::vec3) > data_size) continue;

                std::vector<float> position_times = BuildKeyTimes(data, ch, ch.position_key_count);
                std::vector<float> rotation_times = BuildKeyTimes(data, ch, ch.rotation_key_count);
                std::vector<float> scale_times = BuildKeyTimes(data, ch, ch.scale_key_count);
                std::vector<glm::vec3> positions(ch.position_key_count);
                std::memcpy(positions.data(), data + ch.position_offset, ch.position_key_count * sizeof(glm::vec3));
                std::vector<glm::quat> rotations(ch.rotation_key_count);
                std::memcpy(rotations.data(), data + ch.rotation_offset, ch.rotation_key_count * sizeof(glm::quat));
                std::vector<glm::vec3> scales(ch.scale_key_count);
                std::memcpy(scales.data(), data + ch.scale_offset, ch.scale_key_count * sizeof(glm::vec3));
                
                if (!position_times.empty() && !positions.empty()) {
                    sample.positions[target_bone] = Interpolate<glm::vec3>(position_times, positions, current_time);
                    sample.touched[target_bone] = true;
                }
                if (!rotation_times.empty() && !rotations.empty()) {
                    sample.rotations[target_bone] = Interpolate<glm::quat>(rotation_times, rotations, current_time);
                    sample.touched[target_bone] = true;
                }
                if (!scale_times.empty() && !scales.empty()) {
                    sample.scales[target_bone] = Interpolate<glm::vec3>(scale_times, scales, current_time);
                    sample.touched[target_bone] = true;
                }
            }
            return true;
        };

        auto EvaluateLegacyBlendTree = [&](std::vector<AnimBlendNode>& blend_nodes, float blend_parameter_value, SampleBuffer& out_sample) -> bool {
            if (blend_nodes.empty()) {
                return false;
            }

            std::vector<float> evaluated_weights(blend_nodes.size(), 0.0f);
            const bool use_threshold_blend = blend_nodes.size() >= 2;
            if (use_threshold_blend) {
                size_t lower_index = 0;
                size_t upper_index = blend_nodes.size() - 1;
                float lower_threshold = blend_nodes.front().threshold;
                float upper_threshold = blend_nodes.back().threshold;

                for (size_t i = 0; i < blend_nodes.size(); ++i) {
                    const float threshold = blend_nodes[i].threshold;
                    if (threshold <= blend_parameter_value) {
                        lower_index = i;
                        lower_threshold = threshold;
                    }
                    if (threshold >= blend_parameter_value) {
                        upper_index = i;
                        upper_threshold = threshold;
                        break;
                    }
                }

                if (blend_parameter_value <= blend_nodes.front().threshold) {
                    lower_index = 0;
                    upper_index = 0;
                    lower_threshold = blend_nodes.front().threshold;
                    upper_threshold = blend_nodes.front().threshold;
                } else if (blend_parameter_value >= blend_nodes.back().threshold) {
                    lower_index = blend_nodes.size() - 1;
                    upper_index = blend_nodes.size() - 1;
                    lower_threshold = blend_nodes.back().threshold;
                    upper_threshold = blend_nodes.back().threshold;
                }

                if (lower_index == upper_index || std::abs(upper_threshold - lower_threshold) <= 0.0001f) {
                    evaluated_weights[lower_index] = 1.0f;
                } else {
                    const float range = upper_threshold - lower_threshold;
                    const float t = std::clamp((blend_parameter_value - lower_threshold) / range, 0.0f, 1.0f);
                    evaluated_weights[lower_index] = 1.0f - t;
                    evaluated_weights[upper_index] = t;
                }
            } else {
                evaluated_weights[0] = 1.0f;
            }

            SampleBuffer accumulated(bone_count);
            std::fill(accumulated.positions.begin(), accumulated.positions.end(), glm::vec3(0.0f));
            std::fill(accumulated.rotations.begin(), accumulated.rotations.end(), glm::quat(0.0f, 0.0f, 0.0f, 0.0f));
            std::fill(accumulated.scales.begin(), accumulated.scales.end(), glm::vec3(0.0f));
            std::vector<bool> has_anim(bone_count, false);

            float total_weight = 0.0f;
            for (size_t node_index = 0; node_index < blend_nodes.size(); ++node_index) {
                auto& node = blend_nodes[node_index];
                const float evaluated_weight = use_threshold_blend ? evaluated_weights[node_index] : node.weight;
                if (evaluated_weight <= 0.0f || node.danim_path.empty()) continue;

                auto danim = asset_manager.LoadDanim(node.danim_path);
                if (!danim || danim->GetData().empty()) {
                    continue;
                }

                const uint8_t* data = danim->GetData().data();
                const asset::compiler::AnimHeader* header = reinterpret_cast<const asset::compiler::AnimHeader*>(data);
                if (!IsValidAnimHeader(header)) {
                    continue;
                }

                node.current_time = AdvanceClipTime(node.current_time, delta_time, node.speed, header->duration, node.loop);

                float clip_duration = 0.0f;
                SampleBuffer node_sample(bone_count);
                if (!EvaluateClip(node.danim_path, node.current_time, node_sample, clip_duration)) {
                    continue;
                }
                node.current_time = AdvanceClipTime(node.current_time, 0.0f, node.speed, clip_duration, node.loop);
                total_weight += evaluated_weight;

                for (uint32_t i = 0; i < bone_count; ++i) {
                    if (!has_anim[i]) {
                        accumulated.positions[i] = node_sample.positions[i] * evaluated_weight;
                        accumulated.rotations[i] = node_sample.rotations[i] * evaluated_weight;
                        accumulated.scales[i] = node_sample.scales[i] * evaluated_weight;
                        has_anim[i] = true;
                    } else {
                        accumulated.positions[i] += node_sample.positions[i] * evaluated_weight;
                        accumulated.rotations[i] += node_sample.rotations[i] * evaluated_weight;
                        accumulated.scales[i] += node_sample.scales[i] * evaluated_weight;
                    }
                }
            }

            if (total_weight <= 0.0f) {
                return false;
            }

            for (uint32_t i = 0; i < bone_count; ++i) {
                if (!has_anim[i]) {
                    continue;
                }
                out_sample.positions[i] = accumulated.positions[i] / total_weight;
                out_sample.rotations[i] = glm::normalize(accumulated.rotations[i]);
                out_sample.scales[i] = accumulated.scales[i] / total_weight;
            }
            return true;
        };

        if (animator.state_machine) {
            // --- STATE MACHINE PATH ---
            auto& fsm = *animator.state_machine;
            if (animator.current_state_name.empty()) {
                animator.current_state_name = fsm.GetDefaultState();
                animator.state_time = 0.0f;
            }
            
            const auto& states = fsm.GetStates();
            auto it = states.find(animator.current_state_name);
            if (it != states.end()) {
                const AnimState& current_state = it->second;
                
                // 1. Evaluate transitions
                if (!animator.is_transitioning) {
                    for (const auto& trans : current_state.transitions) {
                        if (fsm.EvaluateTransition(trans, animator.normalized_time)) {
                            // Start transition
                            animator.is_transitioning = true;
                            animator.next_state_name = trans.target_state;
                            animator.transition_duration = trans.transition_duration;
                            animator.transition_progress = 0.0f;
                            animator.next_state_time = 0.0f;
                            
                            // Reset triggers that fired this transition
                            for (const auto& cond : trans.conditions) {
                                auto param_it = fsm.GetParameters().find(cond.parameter_name);
                                if (param_it != fsm.GetParameters().end() && param_it->second.type == AnimParamType::Trigger) {
                                    fsm.ResetTrigger(cond.parameter_name);
                                }
                            }
                            break;
                        }
                    }
                }
                
                // 2. Advance time
                animator.state_time += delta_time * current_state.speed;
                if (animator.is_transitioning) {
                    if (animator.transition_duration <= 0.0f) {
                        animator.transition_progress = 1.0f;
                    } else {
                        animator.transition_progress += delta_time / animator.transition_duration;
                    }
                    
                    auto next_it = states.find(animator.next_state_name);
                    if (next_it != states.end()) {
                        animator.next_state_time += delta_time * next_it->second.speed;
                    }
                    
                    if (animator.transition_progress >= 1.0f) {
                        // Finish transition
                        animator.current_state_name = animator.next_state_name;
                        animator.state_time = animator.next_state_time;
                        animator.is_transitioning = false;
                        
                        it = states.find(animator.current_state_name);
                        if (it == states.end()) continue; // Safety check
                    }
                }
                
                // 3. Evaluate animation logic (current state)
                float current_duration = 1.0f;
                SampleBuffer current_sample(bone_count);
                
                if (!current_state.is_blend_tree) {
                    if (EvaluateClip(current_state.danim_path, animator.state_time, current_sample, current_duration)) {
                        animator.state_time = AdvanceClipTime(animator.state_time, 0.0f, current_state.speed, current_duration, current_state.loop);
                        animator.normalized_time = current_duration > 0.0f ? animator.state_time / current_duration : 0.0f;
                    }
                } else {
                    if (EvaluateLegacyBlendTree(animator.blend_nodes, animator.blend_parameter_value, current_sample)) {
                        animator.normalized_time = 0.0f;
                    }
                }
                
                // 4. Evaluate and Blend Next State (Crossfade)
                if (animator.is_transitioning) {
                    auto next_it = states.find(animator.next_state_name);
                    if (next_it != states.end()) {
                        const AnimState& next_state = next_it->second;
                        
                        float next_duration = 1.0f;
                        SampleBuffer next_sample(bone_count);
                        
                        if (!next_state.is_blend_tree) {
                            if (EvaluateClip(next_state.danim_path, animator.next_state_time, next_sample, next_duration)) {
                                animator.next_state_time = AdvanceClipTime(animator.next_state_time, 0.0f, next_state.speed, next_duration, next_state.loop);
                            }
                        } else {
                            EvaluateLegacyBlendTree(animator.blend_nodes, animator.blend_parameter_value, next_sample);
                        }
                        
                        // Crossfade blend
                        float t = std::clamp(animator.transition_progress, 0.0f, 1.0f);
                        for (uint32_t i = 0; i < bone_count; ++i) {
                            if (!current_sample.touched[i] && !next_sample.touched[i]) continue;  // keep bind pose
                            glm::vec3 final_p = glm::mix(current_sample.positions[i], next_sample.positions[i], t);
                            glm::quat final_r = glm::slerp(current_sample.rotations[i], next_sample.rotations[i], t);
                            glm::vec3 final_s = glm::mix(current_sample.scales[i], next_sample.scales[i], t);
                            local_transforms[i] = glm::translate(glm::mat4(1.0f), final_p) * glm::mat4_cast(final_r) * glm::scale(glm::mat4(1.0f), final_s);
                        }
                    }
                } else {
                    // Apply single state
                    ApplySamplesToLocalTransforms(current_sample, local_transforms, bone_count);
                }
            }
        } else if (!animator.use_anim_tree) {
            // --- LEGACY SINGLE ANIMATION PATH ---
            if (animator.danim_path.empty()) continue;

            SampleBuffer sample(bone_count);
            float duration = 0.0f;
            if (!EvaluateClip(animator.danim_path, animator.current_time, sample, duration)) {
                continue;
            }

            animator.current_time = AdvanceClipTime(animator.current_time, delta_time, animator.speed, duration, animator.loop);
            ApplySamplesToLocalTransforms(sample, local_transforms, bone_count);
        } else {
            // --- LEGACY ANIM TREE (1D Blend) PATH ---
            SampleBuffer sample(bone_count);
            if (EvaluateLegacyBlendTree(animator.blend_nodes, animator.blend_parameter_value, sample)) {
                ApplySamplesToLocalTransforms(sample, local_transforms, bone_count);
            }
        }

        // 3. Calculate global transforms (handles arbitrary bone ordering)
        std::vector<glm::mat4> global_transforms(bone_count);
        std::fill(computed.begin(), computed.end(), false);
        for (uint32_t i = 0; i < bone_count; ++i) {
            int parent_index = bones[i].parent_index;
            if (parent_index < 0 || parent_index >= static_cast<int>(bone_count)) {
                global_transforms[i] = local_transforms[i];
                computed[i] = true;
            }
        }
        for (uint32_t pass = 0; pass < bone_count; ++pass) {
            bool all_done = true;
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (computed[i]) continue;
                int parent_index = bones[i].parent_index;
                if (computed[parent_index]) {
                    global_transforms[i] = global_transforms[parent_index] * local_transforms[i];
                    computed[i] = true;
                } else {
                    all_done = false;
                }
            }
            if (all_done) break;
        }

        // 4. Calculate final bone matrices using relative deformation:
        //    final = anim_global * inv(bind_global)
        //    At bind pose this produces identity (no deformation).
        for (uint32_t i = 0; i < bone_count; ++i) {
            animator.final_bone_matrices[i] = global_transforms[i] * glm::inverse(bind_globals[i]);
        }
    }
}

} // namespace gameplay3d
} // namespace dse