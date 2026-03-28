#include "modules/gameplay_3d/animation/animator_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace dse {
namespace gameplay3d {

namespace {

template<typename T>
T Interpolate(const std::vector<float>& times, const std::vector<T>& values, float current_time) {
    if (times.empty() || values.empty()) return T();
    if (times.size() == 1) return values[0];
    
    // Find keyframes
    size_t p0 = 0;
    for (size_t i = 0; i < times.size() - 1; ++i) {
        if (current_time < times[i + 1]) {
            p0 = i;
            break;
        }
    }
    size_t p1 = p0 + 1;
    
    float t0 = times[p0];
    float t1 = times[p1];
    float factor = (current_time - t0) / (t1 - t0);
    factor = std::clamp(factor, 0.0f, 1.0f);
    
    if constexpr (std::is_same_v<T, glm::quat>) {
        return glm::slerp(values[p0], values[p1], factor);
    } else {
        return glm::mix(values[p0], values[p1], factor);
    }
}

}

void AnimatorSystem::Update(World& world, float delta_time) {
    auto view = world.registry().view<Animator3DComponent>();
    
    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        if (!animator.enabled || animator.danim_path.empty()) {
            continue;
        }
        
        auto dskel = AssetManager::Instance().LoadDskel(animator.dskel_path);
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

        if (!animator.use_anim_tree) {
            // Legacy single animation
            if (animator.danim_path.empty()) continue;
            auto danim = AssetManager::Instance().LoadDanim(animator.danim_path);
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
                
                std::vector<float> times(reinterpret_cast<const float*>(data + ch.time_offset), reinterpret_cast<const float*>(data + ch.time_offset) + ch.position_key_count);
                std::vector<glm::vec3> positions(reinterpret_cast<const glm::vec3*>(data + ch.position_offset), reinterpret_cast<const glm::vec3*>(data + ch.position_offset) + ch.position_key_count);
                std::vector<glm::quat> rotations(reinterpret_cast<const glm::quat*>(data + ch.rotation_offset), reinterpret_cast<const glm::quat*>(data + ch.rotation_offset) + ch.rotation_key_count);
                std::vector<glm::vec3> scales(reinterpret_cast<const glm::vec3*>(data + ch.scale_offset), reinterpret_cast<const glm::vec3*>(data + ch.scale_offset) + ch.scale_key_count);
                
                glm::vec3 p = Interpolate<glm::vec3>(times, positions, current_time);
                glm::quat r = Interpolate<glm::quat>(times, rotations, current_time);
                glm::vec3 s = Interpolate<glm::vec3>(times, scales, current_time);
                
                local_transforms[ch.target_node_index] = glm::translate(glm::mat4(1.0f), p) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
            }
        } else {
            // AnimTree blending (1D blend example)
            std::vector<glm::vec3> blended_positions(bone_count, glm::vec3(0.0f));
            std::vector<glm::quat> blended_rotations(bone_count, glm::quat(0.0f, 0.0f, 0.0f, 0.0f));
            std::vector<glm::vec3> blended_scales(bone_count, glm::vec3(0.0f));
            std::vector<bool> has_anim(bone_count, false);
            
            float total_weight = 0.0f;
            for (auto& node : animator.blend_nodes) {
                if (node.weight <= 0.0f) continue;
                auto danim = AssetManager::Instance().LoadDanim(node.danim_path);
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
                    
                    std::vector<float> times(reinterpret_cast<const float*>(data + ch.time_offset), reinterpret_cast<const float*>(data + ch.time_offset) + ch.position_key_count);
                    std::vector<glm::vec3> positions(reinterpret_cast<const glm::vec3*>(data + ch.position_offset), reinterpret_cast<const glm::vec3*>(data + ch.position_offset) + ch.position_key_count);
                    std::vector<glm::quat> rotations(reinterpret_cast<const glm::quat*>(data + ch.rotation_offset), reinterpret_cast<const glm::quat*>(data + ch.rotation_offset) + ch.rotation_key_count);
                    std::vector<glm::vec3> scales(reinterpret_cast<const glm::vec3*>(data + ch.scale_offset), reinterpret_cast<const glm::vec3*>(data + ch.scale_offset) + ch.scale_key_count);
                    
                    glm::vec3 p = Interpolate<glm::vec3>(times, positions, node.current_time);
                    glm::quat r = Interpolate<glm::quat>(times, rotations, node.current_time);
                    glm::vec3 s = Interpolate<glm::vec3>(times, scales, node.current_time);
                    
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