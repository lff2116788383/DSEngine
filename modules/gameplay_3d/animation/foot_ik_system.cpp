#include "modules/gameplay_3d/animation/foot_ik_system.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/core/service_locator.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace dse {
namespace gameplay3d {

namespace {

// Build bone indices for FootIK configuration
bool BuildFootIKIndices(FootIKConfig& config, const Animator3DComponent::SkeletalCache& cache) {
    if (!config.indices_dirty) return config.foot_bone_index >= 0 && config.hip_bone_index >= 0;

    config.indices_dirty = false;
    config.foot_bone_index = -1;
    config.hip_bone_index = -1;

    if (config.foot_bone.empty() || config.hip_bone.empty()) return false;

    auto foot_it = cache.bone_name_to_index.find(config.foot_bone);
    auto hip_it = cache.bone_name_to_index.find(config.hip_bone);
    if (foot_it == cache.bone_name_to_index.end() || hip_it == cache.bone_name_to_index.end())
        return false;

    config.foot_bone_index = foot_it->second;
    config.hip_bone_index = hip_it->second;
    
    // Build chain indices from hip to foot
    config.chain_indices.clear();
    int current = config.foot_bone_index;
    const int max_depth = static_cast<int>(cache.bone_count);
    for (int depth = 0; depth < max_depth; ++depth) {
        config.chain_indices.push_back(current);
        if (current == config.hip_bone_index) break;
        int parent = cache.parent_indices[current];
        if (parent < 0 || parent >= static_cast<int>(cache.bone_count)) break;
        current = parent;
    }
    std::reverse(config.chain_indices.begin(), config.chain_indices.end());
    
    return !config.chain_indices.empty();
}

// Raycast to find ground height at world position
float GetGroundHeight(const glm::vec3& world_pos, dse::physics3d::IPhysics3DSystem* physics, float max_distance) {
    if (!physics) return world_pos.y; // No physics: treat current position as ground

    // Raycast downward from above the position
    glm::vec3 ray_origin = world_pos + glm::vec3(0.0f, max_distance, 0.0f);
    glm::vec3 ray_direction = glm::vec3(0.0f, -1.0f, 0.0f);

    auto result = physics->Raycast(ray_origin, ray_direction, max_distance * 2.0f);
    if (result.hit) {
        return result.hit_point.y;
    }
    return world_pos.y; // No hit: treat current position as ground
}

// FABRIK solver for a leg chain.
// hip_world_pos and target_foot_world_pos are in world space.
// FABRIK is performed in model space; entity_world_matrix converts between them.
void SolveLegFABRIK(
    const std::vector<int>& chain_indices,
    const glm::vec3& hip_world_pos,
    const glm::vec3& target_foot_world_pos,
    const Animator3DComponent::SkeletalCache& cache,
    Animator3DComponent::PoseBuffer& pb,
    const glm::mat4& entity_world_matrix,
    float weight)
{
    if (chain_indices.size() < 2) return;

    // Compute full skeleton globals in model space
    std::vector<glm::mat4> globals;
    anim_util::ComputeBoneGlobals(pb, cache, globals);

    // Convert world-space inputs to model space so FABRIK is consistent
    glm::mat4 inv_world = glm::inverse(entity_world_matrix);
    glm::vec3 root_model  = glm::vec3(inv_world * glm::vec4(hip_world_pos, 1.0f));
    glm::vec3 target_model = glm::vec3(inv_world * glm::vec4(target_foot_world_pos, 1.0f));

    // Extract model-space chain positions; pin root to hip
    const size_t n = chain_indices.size();
    std::vector<glm::vec3> positions(n);
    for (size_t i = 0; i < n; ++i)
        positions[i] = glm::vec3(globals[chain_indices[i]][3]);
    positions[0] = root_model;

    // Solve FABRIK entirely in model space
    anim_util::SolveFABRIK(positions, target_model, glm::vec3(0.0f), 5, 0.01f);

    // Write back rotations for ALL chain bones using incremental propagation
    anim_util::WriteBackFABRIKResult(chain_indices, positions, globals, pb, cache, weight);
}

} // anonymous namespace

void FootIKSystem::Update(World& world, float delta_time) {
    auto view = world.registry().view<Animator3DComponent, FootIK3DComponent, TransformComponent>();

    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        auto& foot_ik = view.get<FootIK3DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (!animator.enabled || !animator.skel_cache.valid || !foot_ik.enabled) continue;

        // Get physics system for ground detection
        auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>();
        if (!physics) continue;

        const auto& cache = animator.skel_cache;

        // Build entity world matrix
        glm::mat4 entity_world_matrix = glm::translate(glm::mat4(1.0f), transform.position)
            * glm::mat4_cast(transform.rotation)
            * glm::scale(glm::mat4(1.0f), transform.scale);

        // Compute globals for all bones (needed for hip positions)
        std::vector<glm::mat4> globals;
        anim_util::ComputeBoneGlobals(animator.pose_buffer, cache, globals);

        // Solve IK for each foot
        std::vector<glm::vec3> foot_positions;
        for (auto& foot_config : foot_ik.feet) {
            if (!BuildFootIKIndices(foot_config, cache)) continue;
            if (foot_config.chain_indices.size() < 2) continue;

            // Get hip world position
            int hip_idx = foot_config.hip_bone_index;
            glm::vec3 hip_world_pos = glm::vec3(entity_world_matrix * glm::vec4(glm::vec3(globals[hip_idx][3]), 1.0f));

            // Get current foot world position
            int foot_idx = foot_config.foot_bone_index;
            glm::vec3 foot_world_pos = glm::vec3(entity_world_matrix * glm::vec4(glm::vec3(globals[foot_idx][3]), 1.0f));

            // Get ground height at foot position
            float ground_y = GetGroundHeight(foot_world_pos, physics, foot_config.max_ground_distance);

            // Calculate target foot position (ground level + foot height offset)
            glm::vec3 target_foot_pos = foot_world_pos;
            target_foot_pos.y = ground_y + foot_config.foot_height;

            // Clamp vertical offset
            float vertical_offset = target_foot_pos.y - foot_world_pos.y;
            if (std::abs(vertical_offset) > foot_config.max_ground_distance) {
                target_foot_pos.y = foot_world_pos.y + 
                    (vertical_offset > 0.0f ? foot_config.max_ground_distance : -foot_config.max_ground_distance);
            }

            // Apply blend speed
            float blend_factor = foot_config.blend_speed * delta_time;
            if (blend_factor > 1.0f) blend_factor = 1.0f;
            float adjusted_weight = foot_config.weight * blend_factor;

            // Solve leg IK using FABRIK
            SolveLegFABRIK(foot_config.chain_indices, hip_world_pos, target_foot_pos,
                          cache, animator.pose_buffer, entity_world_matrix, adjusted_weight);

            foot_positions.push_back(target_foot_pos);
        }

        // Pelvis adjustment: lower the pelvis by the amount the lowest foot was raised,
        // so the character doesn't float above the ground on uneven terrain.
        if (!foot_positions.empty() && foot_ik.pelvis_weight > 0.0f) {
            // Find root bone (bone with no parent)
            int root_idx = -1;
            for (uint32_t i = 0; i < cache.bone_count; ++i) {
                if (cache.parent_indices[i] < 0) { root_idx = static_cast<int>(i); break; }
            }

            if (root_idx >= 0 && !foot_ik.feet.empty()) {
                // Measure how much the lowest foot was displaced downward relative to its original position
                float min_foot_delta = 0.0f;
                for (size_t fi = 0; fi < foot_positions.size(); ++fi) {
                    int foot_idx = foot_ik.feet[fi].foot_bone_index;
                    if (foot_idx < 0) continue;
                    float original_world_y = (entity_world_matrix * glm::vec4(glm::vec3(globals[foot_idx][3]), 1.0f)).y;
                    float delta = foot_positions[fi].y - original_world_y;
                    if (delta < min_foot_delta) min_foot_delta = delta;
                }

                // Only move pelvis downward (negative delta means foot needed to go down)
                if (min_foot_delta < 0.0f) {
                    float pelvis_drop = min_foot_delta;
                    if (pelvis_drop < -foot_ik.max_pelvis_offset) pelvis_drop = -foot_ik.max_pelvis_offset;

                    // Convert world-space Y offset to model-space
                    glm::mat4 inv_world = glm::inverse(entity_world_matrix);
                    glm::vec3 local_offset = glm::vec3(inv_world * glm::vec4(0.0f, pelvis_drop, 0.0f, 0.0f));

                    animator.pose_buffer.positions[root_idx] += local_offset * foot_ik.pelvis_weight;
                    animator.pose_buffer.touched[root_idx] = true;
                }
            }
        }
    }
}

} // namespace gameplay3d
} // namespace dse
