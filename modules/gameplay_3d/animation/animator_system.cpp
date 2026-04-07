#include "modules/gameplay_3d/animation/animator_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <stdexcept>

namespace dse {
namespace gameplay3d {

namespace {

AssetManager& RequireAssetManager(AssetManager* asset_manager) {
    if (asset_manager != nullptr) {
        return *asset_manager;
    }
    throw std::runtime_error("AnimatorSystem requires an injected AssetManager");
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

    const float* all_times = reinterpret_cast<const float*>(data + ch.time_offset);
    return std::vector<float>(all_times, all_times + key_count);
}

}

AssetManager* AnimatorSystem::asset_manager_ = nullptr;

void AnimatorSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

void AnimatorSystem::Update(World& world, float delta_time) {
    auto& asset_manager = RequireAssetManager(asset_manager_);
    auto view = world.registry().view<Animator3DComponent>();
    
    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        if (!animator.enabled) {
            continue;
        }
        
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

        if (animator.final_bone_matrices.size() < bone_count) {
            animator.final_bone_matrices.resize(bone_count, glm::mat4(1.0f));
        }

        // 1. Initialize local transforms with bind pose
        std::vector<glm::mat4> local_transforms(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i) {
            local_transforms[i] = bones[i].local_transform;
        }
        
        auto EvaluateClip = [&](const std::string& anim_path, float current_time, std::vector<glm::vec3>& pos, std::vector<glm::quat>& rot, std::vector<glm::vec3>& scale, float& out_duration) -> bool {
            auto danim = asset_manager.LoadDanim(anim_path);
            if (!danim || danim->GetData().empty()) return false;
            
            const uint8_t* data = danim->GetData().data();
            const asset::compiler::AnimHeader* header = reinterpret_cast<const asset::compiler::AnimHeader*>(data);
            out_duration = header->duration;
            
            const asset::compiler::AnimChannelDesc* channels = reinterpret_cast<const asset::compiler::AnimChannelDesc*>(data + sizeof(asset::compiler::AnimHeader));
            for (uint32_t i = 0; i < header->channel_count; ++i) {
                const auto& ch = channels[i];
                if (ch.target_node_index < 0 || ch.target_node_index >= static_cast<int>(bone_count)) continue;
                
                std::vector<float> position_times = BuildKeyTimes(data, ch, ch.position_key_count);
                std::vector<float> rotation_times = BuildKeyTimes(data, ch, ch.rotation_key_count);
                std::vector<float> scale_times = BuildKeyTimes(data, ch, ch.scale_key_count);
                std::vector<glm::vec3> positions(reinterpret_cast<const glm::vec3*>(data + ch.position_offset), reinterpret_cast<const glm::vec3*>(data + ch.position_offset) + ch.position_key_count);
                std::vector<glm::quat> rotations(reinterpret_cast<const glm::quat*>(data + ch.rotation_offset), reinterpret_cast<const glm::quat*>(data + ch.rotation_offset) + ch.rotation_key_count);
                std::vector<glm::vec3> scales(reinterpret_cast<const glm::vec3*>(data + ch.scale_offset), reinterpret_cast<const glm::vec3*>(data + ch.scale_offset) + ch.scale_key_count);
                
                if (!position_times.empty() && !positions.empty()) {
                    pos[ch.target_node_index] = Interpolate<glm::vec3>(position_times, positions, current_time);
                }
                if (!rotation_times.empty() && !rotations.empty()) {
                    rot[ch.target_node_index] = Interpolate<glm::quat>(rotation_times, rotations, current_time);
                }
                if (!scale_times.empty() && !scales.empty()) {
                    scale[ch.target_node_index] = Interpolate<glm::vec3>(scale_times, scales, current_time);
                }
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
                std::vector<glm::vec3> p0(bone_count, glm::vec3(0.0f));
                std::vector<glm::quat> r0(bone_count, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                std::vector<glm::vec3> s0(bone_count, glm::vec3(1.0f));
                
                if (!current_state.is_blend_tree) {
                    if (EvaluateClip(current_state.danim_path, animator.state_time, p0, r0, s0, current_duration)) {
                        if (animator.state_time > current_duration) {
                            if (current_state.loop) animator.state_time = std::fmod(animator.state_time, current_duration);
                            else animator.state_time = current_duration;
                        }
                        animator.normalized_time = animator.state_time / current_duration;
                    }
                } else {
                    // TODO: Evaluate 1D Blend Tree for current state
                }
                
                // 4. Evaluate and Blend Next State (Crossfade)
                if (animator.is_transitioning) {
                    auto next_it = states.find(animator.next_state_name);
                    if (next_it != states.end()) {
                        const AnimState& next_state = next_it->second;
                        float next_duration = 1.0f;
                        std::vector<glm::vec3> p1(bone_count, glm::vec3(0.0f));
                        std::vector<glm::quat> r1(bone_count, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                        std::vector<glm::vec3> s1(bone_count, glm::vec3(1.0f));
                        
                        if (!next_state.is_blend_tree) {
                            if (EvaluateClip(next_state.danim_path, animator.next_state_time, p1, r1, s1, next_duration)) {
                                if (animator.next_state_time > next_duration) {
                                    if (next_state.loop) animator.next_state_time = std::fmod(animator.next_state_time, next_duration);
                                    else animator.next_state_time = next_duration;
                                }
                            }
                        }
                        
                        // Crossfade blend
                        float t = std::clamp(animator.transition_progress, 0.0f, 1.0f);
                        for (uint32_t i = 0; i < bone_count; ++i) {
                            glm::vec3 final_p = glm::mix(p0[i], p1[i], t);
                            glm::quat final_r = glm::slerp(r0[i], r1[i], t);
                            glm::vec3 final_s = glm::mix(s0[i], s1[i], t);
                            local_transforms[i] = glm::translate(glm::mat4(1.0f), final_p) * glm::mat4_cast(final_r) * glm::scale(glm::mat4(1.0f), final_s);
                        }
                    }
                } else {
                    // Apply single state
                    for (uint32_t i = 0; i < bone_count; ++i) {
                        local_transforms[i] = glm::translate(glm::mat4(1.0f), p0[i]) * glm::mat4_cast(r0[i]) * glm::scale(glm::mat4(1.0f), s0[i]);
                    }
                }
            }
        } else if (!animator.use_anim_tree) {
            // --- LEGACY SINGLE ANIMATION PATH ---
            if (animator.danim_path.empty()) continue;
            auto danim = asset_manager.LoadDanim(animator.danim_path);
            if (!danim || danim->GetData().empty()) continue;
            const uint8_t* data = danim->GetData().data();
            const asset::compiler::AnimHeader* header = reinterpret_cast<const asset::compiler::AnimHeader*>(data);
            
            // Update time
            auto& current_time = animator.current_time;
            current_time += delta_time * animator.speed;
            if (current_time > header->duration) {
                if (animator.loop) current_time = std::fmod(current_time, header->duration);
                else current_time = header->duration;
            }
            
            const asset::compiler::AnimChannelDesc* channels = reinterpret_cast<const asset::compiler::AnimChannelDesc*>(data + sizeof(asset::compiler::AnimHeader));
            for (uint32_t i = 0; i < header->channel_count; ++i) {
                const auto& ch = channels[i];
                if (ch.target_node_index < 0 || ch.target_node_index >= static_cast<int>(bone_count)) continue;
                
                std::vector<float> position_times = BuildKeyTimes(data, ch, ch.position_key_count);
                std::vector<float> rotation_times = BuildKeyTimes(data, ch, ch.rotation_key_count);
                std::vector<float> scale_times = BuildKeyTimes(data, ch, ch.scale_key_count);
                std::vector<glm::vec3> positions(reinterpret_cast<const glm::vec3*>(data + ch.position_offset), reinterpret_cast<const glm::vec3*>(data + ch.position_offset) + ch.position_key_count);
                std::vector<glm::quat> rotations(reinterpret_cast<const glm::quat*>(data + ch.rotation_offset), reinterpret_cast<const glm::quat*>(data + ch.rotation_offset) + ch.rotation_key_count);
                std::vector<glm::vec3> scales(reinterpret_cast<const glm::vec3*>(data + ch.scale_offset), reinterpret_cast<const glm::vec3*>(data + ch.scale_offset) + ch.scale_key_count);
                
                glm::vec3 p = !position_times.empty() && !positions.empty()
                    ? Interpolate<glm::vec3>(position_times, positions, current_time)
                    : glm::vec3(0.0f);
                glm::quat r = !rotation_times.empty() && !rotations.empty()
                    ? Interpolate<glm::quat>(rotation_times, rotations, current_time)
                    : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3 s = !scale_times.empty() && !scales.empty()
                    ? Interpolate<glm::vec3>(scale_times, scales, current_time)
                    : glm::vec3(1.0f);
                
                local_transforms[ch.target_node_index] = glm::translate(glm::mat4(1.0f), p) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
            }
        } else {
            // --- LEGACY ANIM TREE (1D Blend) PATH ---
            std::vector<glm::vec3> blended_positions(bone_count, glm::vec3(0.0f));
            std::vector<glm::quat> blended_rotations(bone_count, glm::quat(0.0f, 0.0f, 0.0f, 0.0f));
            std::vector<glm::vec3> blended_scales(bone_count, glm::vec3(0.0f));
            std::vector<bool> has_anim(bone_count, false);
            
            float total_weight = 0.0f;
            for (auto& node : animator.blend_nodes) {
                if (node.weight <= 0.0f) continue;
                auto danim = asset_manager.LoadDanim(node.danim_path);
                if (!danim || danim->GetData().empty()) continue;
                
                total_weight += node.weight;
                const uint8_t* data = danim->GetData().data();
                const asset::compiler::AnimHeader* header = reinterpret_cast<const asset::compiler::AnimHeader*>(data);
                
                node.current_time += delta_time * node.speed;
                if (node.current_time > header->duration) {
                    if (node.loop) node.current_time = std::fmod(node.current_time, header->duration);
                    else node.current_time = header->duration;
                }
                
                const asset::compiler::AnimChannelDesc* channels = reinterpret_cast<const asset::compiler::AnimChannelDesc*>(data + sizeof(asset::compiler::AnimHeader));
                for (uint32_t i = 0; i < header->channel_count; ++i) {
                    const auto& ch = channels[i];
                    if (ch.target_node_index < 0 || ch.target_node_index >= static_cast<int>(bone_count)) continue;
                    
                    std::vector<float> position_times = BuildKeyTimes(data, ch, ch.position_key_count);
                    std::vector<float> rotation_times = BuildKeyTimes(data, ch, ch.rotation_key_count);
                    std::vector<float> scale_times = BuildKeyTimes(data, ch, ch.scale_key_count);
                    std::vector<glm::vec3> positions(reinterpret_cast<const glm::vec3*>(data + ch.position_offset), reinterpret_cast<const glm::vec3*>(data + ch.position_offset) + ch.position_key_count);
                    std::vector<glm::quat> rotations(reinterpret_cast<const glm::quat*>(data + ch.rotation_offset), reinterpret_cast<const glm::quat*>(data + ch.rotation_offset) + ch.rotation_key_count);
                    std::vector<glm::vec3> scales(reinterpret_cast<const glm::vec3*>(data + ch.scale_offset), reinterpret_cast<const glm::vec3*>(data + ch.scale_offset) + ch.scale_key_count);
                    
                    glm::vec3 p = !position_times.empty() && !positions.empty()
                        ? Interpolate<glm::vec3>(position_times, positions, node.current_time)
                        : glm::vec3(0.0f);
                    glm::quat r = !rotation_times.empty() && !rotations.empty()
                        ? Interpolate<glm::quat>(rotation_times, rotations, node.current_time)
                        : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    glm::vec3 s = !scale_times.empty() && !scales.empty()
                        ? Interpolate<glm::vec3>(scale_times, scales, node.current_time)
                        : glm::vec3(1.0f);
                    
                    if (!has_anim[ch.target_node_index]) {
                        blended_positions[ch.target_node_index] = p * node.weight;
                        blended_rotations[ch.target_node_index] = r * node.weight;
                        blended_scales[ch.target_node_index] = s * node.weight;
                        has_anim[ch.target_node_index] = true;
                    } else {
                        blended_positions[ch.target_node_index] += p * node.weight;
                        blended_rotations[ch.target_node_index] += r * node.weight;
                        blended_scales[ch.target_node_index] += s * node.weight;
                    }
                }
            }
            
            if (total_weight > 0.0f) {
                for (uint32_t i = 0; i < bone_count; ++i) {
                    if (has_anim[i]) {
                        glm::vec3 p = blended_positions[i] / total_weight;
                        glm::quat r = glm::normalize(blended_rotations[i]); // Approximate quaternion blending
                        glm::vec3 s = blended_scales[i] / total_weight;
                        local_transforms[i] = glm::translate(glm::mat4(1.0f), p) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
                    }
                }
            }
        }

        // 3. Calculate global transforms
        std::vector<glm::mat4> global_transforms(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i) {
            int parent_index = bones[i].parent_index;
            if (parent_index >= 0 && parent_index < static_cast<int>(bone_count)) {
                // Assuming bones are topologically sorted (parent before child)
                global_transforms[i] = global_transforms[parent_index] * local_transforms[i];
            } else {
                global_transforms[i] = local_transforms[i];
            }
        }

        // 4. Calculate final bone matrices (Global * InverseBind)
        for (uint32_t i = 0; i < bone_count; ++i) {
            animator.final_bone_matrices[i] = global_transforms[i] * bones[i].inverse_bind_matrix;
        }
    }
}

} // namespace gameplay3d
} // namespace dse