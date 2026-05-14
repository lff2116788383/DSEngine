#include "modules/gameplay_3d/animation/ik_solver_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace dse {
namespace gameplay3d {

namespace {

// Build the chain of bone indices from root_bone to tip_bone (inclusive).
// Returns true if chain was built successfully.
bool BuildChainIndices(IKChainConfig& chain, const Animator3DComponent::SkeletalCache& cache) {
    if (!chain.indices_dirty) return !chain.chain_indices.empty();

    chain.indices_dirty = false;
    chain.chain_indices.clear();
    chain.root_bone_index = -1;
    chain.tip_bone_index = -1;

    if (chain.root_bone.empty() || chain.tip_bone.empty()) return false;

    auto root_it = cache.bone_name_to_index.find(chain.root_bone);
    auto tip_it = cache.bone_name_to_index.find(chain.tip_bone);
    if (root_it == cache.bone_name_to_index.end() || tip_it == cache.bone_name_to_index.end())
        return false;

    chain.root_bone_index = root_it->second;
    chain.tip_bone_index = tip_it->second;

    // Walk from tip to root via parent indices
    int current = chain.tip_bone_index;
    const int max_depth = static_cast<int>(cache.bone_count);
    for (int depth = 0; depth < max_depth; ++depth) {
        chain.chain_indices.push_back(current);
        if (current == chain.root_bone_index) break;
        int parent = cache.parent_indices[current];
        if (parent < 0 || parent >= static_cast<int>(cache.bone_count)) break;
        current = parent;
    }

    // Reverse so chain[0] = root, chain[last] = tip
    std::reverse(chain.chain_indices.begin(), chain.chain_indices.end());

    // Validate: chain must start at root and end at tip
    if (chain.chain_indices.empty() ||
        chain.chain_indices.front() != chain.root_bone_index ||
        chain.chain_indices.back() != chain.tip_bone_index) {
        chain.chain_indices.clear();
        return false;
    }
    return true;
}

// Compute world-space positions for chain bones from pose_buffer.
// global_pose[i] = TRS matrix in model space for bone i.
void ComputeChainGlobalPositions(
    const std::vector<int>& chain_indices,
    const Animator3DComponent::PoseBuffer& pb,
    const Animator3DComponent::SkeletalCache& cache,
    std::vector<glm::vec3>& out_positions,
    std::vector<glm::mat4>& out_globals)
{
    const uint32_t bone_count = cache.bone_count;
    // We need globals for the full skeleton up to our chain.
    // For efficiency, compute only ancestors + chain bones.
    // Simplified: compute all bone globals (cache-friendly for small skeletons).
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

    // Extract chain positions
    out_positions.resize(chain_indices.size());
    for (size_t i = 0; i < chain_indices.size(); ++i) {
        out_positions[i] = glm::vec3(out_globals[chain_indices[i]][3]);
    }
}

// FABRIK forward-backward reaching IK
void SolveFABRIK(
    std::vector<glm::vec3>& positions,
    const glm::vec3& target,
    const glm::vec3& pole_vector,
    int max_iterations,
    float tolerance)
{
    const size_t n = positions.size();
    if (n < 2) return;

    // Compute bone lengths
    std::vector<float> lengths(n - 1);
    float total_length = 0.0f;
    for (size_t i = 0; i < n - 1; ++i) {
        lengths[i] = glm::distance(positions[i], positions[i + 1]);
        total_length += lengths[i];
    }

    glm::vec3 root_pos = positions[0];
    float dist_to_target = glm::distance(root_pos, target);

    // Unreachable check
    if (dist_to_target > total_length) {
        // Stretch towards target
        glm::vec3 dir = glm::normalize(target - root_pos);
        for (size_t i = 1; i < n; ++i) {
            positions[i] = positions[i - 1] + dir * lengths[i - 1];
        }
        return;
    }

    for (int iter = 0; iter < max_iterations; ++iter) {
        // Check convergence
        float tip_error = glm::distance(positions[n - 1], target);
        if (tip_error < tolerance) break;

        // Forward pass: tip → root
        positions[n - 1] = target;
        for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
            glm::vec3 dir = positions[i] - positions[i + 1];
            float len = glm::length(dir);
            if (len > 1e-6f) dir /= len;
            else dir = glm::vec3(0.0f, 1.0f, 0.0f);
            positions[i] = positions[i + 1] + dir * lengths[i];
        }

        // Backward pass: root → tip
        positions[0] = root_pos;
        for (size_t i = 0; i < n - 1; ++i) {
            glm::vec3 dir = positions[i + 1] - positions[i];
            float len = glm::length(dir);
            if (len > 1e-6f) dir /= len;
            else dir = glm::vec3(0.0f, 1.0f, 0.0f);
            positions[i + 1] = positions[i] + dir * lengths[i];
        }

        // Pole vector constraint for middle joints (bend direction hint)
        if (n > 2 && glm::length(pole_vector) > 1e-6f) {
            for (size_t i = 1; i < n - 1; ++i) {
                // Project middle joint onto the line root→tip, then push toward pole
                glm::vec3 line = positions[n - 1] - positions[0];
                float line_len = glm::length(line);
                if (line_len < 1e-6f) continue;
                glm::vec3 line_dir = line / line_len;
                float t = glm::dot(positions[i] - positions[0], line_dir);
                glm::vec3 projected = positions[0] + line_dir * t;
                glm::vec3 to_joint = positions[i] - projected;
                float dist = glm::length(to_joint);

                // Only apply if joint is very close to the line (ambiguous bend)
                if (dist < lengths[i - 1] * 0.01f) {
                    glm::vec3 pole_dir = pole_vector - glm::dot(pole_vector, line_dir) * line_dir;
                    float pole_len = glm::length(pole_dir);
                    if (pole_len > 1e-6f) {
                        positions[i] = projected + (pole_dir / pole_len) * lengths[i - 1] * 0.1f;
                        // Re-enforce distance from neighbors
                        glm::vec3 d1 = positions[i] - positions[i - 1];
                        float l1 = glm::length(d1);
                        if (l1 > 1e-6f) positions[i] = positions[i - 1] + (d1 / l1) * lengths[i - 1];
                    }
                }
            }
        }
    }
}

// LookAt IK: rotate a single bone to face target
void SolveLookAt(
    const glm::vec3& bone_world_pos,
    const glm::vec3& target,
    const glm::mat4& parent_global,
    glm::quat& out_local_rotation,
    float weight)
{
    glm::vec3 dir_world = target - bone_world_pos;
    float len = glm::length(dir_world);
    if (len < 1e-6f) return;
    dir_world /= len;

    // Transform target direction to parent local space
    glm::mat3 inv_parent_rot = glm::transpose(glm::mat3(parent_global));
    glm::vec3 dir_local = glm::normalize(inv_parent_rot * dir_world);

    // Compute rotation from default forward (-Z) to target direction
    glm::vec3 forward(0.0f, 0.0f, -1.0f);
    glm::quat target_rot = glm::rotation(forward, dir_local);

    // Blend between original rotation and look-at rotation (NOT cumulative)
    out_local_rotation = glm::slerp(out_local_rotation, target_rot, weight);
    out_local_rotation = glm::normalize(out_local_rotation);
}

// Convert world-space IK positions back to local-space rotations in pose_buffer.
// For each chain bone, compute what local rotation makes the bone point toward
// the next chain bone (in the IK result), then blend with the original.
void WriteBackFABRIKResult(
    const std::vector<int>& chain_indices,
    const std::vector<glm::vec3>& ik_positions,
    const std::vector<glm::mat4>& globals_before,
    Animator3DComponent::PoseBuffer& pb,
    const Animator3DComponent::SkeletalCache& cache,
    float weight)
{
    if (chain_indices.size() < 2) return;

    // We incrementally update parent globals as we apply IK rotations.
    // Start with pre-IK globals, then update each bone in chain order.
    std::vector<glm::mat4> updated_globals = globals_before;

    for (size_t ci = 0; ci < chain_indices.size() - 1; ++ci) {
        int bone_idx = chain_indices[ci];
        int child_idx = chain_indices[ci + 1];

        // Direction from bone to child in current (updated) world space
        glm::vec3 cur_dir = glm::vec3(updated_globals[child_idx][3]) - glm::vec3(updated_globals[bone_idx][3]);
        float cur_len = glm::length(cur_dir);
        if (cur_len < 1e-6f) continue;
        cur_dir /= cur_len;

        // Desired direction from IK result
        glm::vec3 ik_dir = ik_positions[ci + 1] - ik_positions[ci];
        float ik_len = glm::length(ik_dir);
        if (ik_len < 1e-6f) continue;
        ik_dir /= ik_len;

        // Delta rotation in world space
        glm::quat delta_world = glm::rotation(cur_dir, ik_dir);

        // Get the bone's current world rotation
        glm::quat bone_world_rot = glm::normalize(glm::quat_cast(glm::mat3(updated_globals[bone_idx])));
        // New world rotation after IK
        glm::quat new_world_rot = delta_world * bone_world_rot;

        // Convert to local: local = inv(parent_world_rot) * world_rot
        int parent_idx = cache.parent_indices[bone_idx];
        glm::quat parent_world_rot = (parent_idx >= 0 && parent_idx < static_cast<int>(cache.bone_count))
            ? glm::normalize(glm::quat_cast(glm::mat3(updated_globals[parent_idx])))
            : glm::quat(1, 0, 0, 0);
        glm::quat new_local = glm::inverse(parent_world_rot) * new_world_rot;

        // Blend with original local rotation
        glm::quat final_local = glm::slerp(pb.rotations[bone_idx], new_local, weight);
        pb.rotations[bone_idx] = glm::normalize(final_local);
        pb.touched[bone_idx] = true;

        // Update the global for this bone (so next bone in chain sees updated parent)
        glm::mat4 parent_global = (parent_idx >= 0 && parent_idx < static_cast<int>(cache.bone_count))
            ? updated_globals[parent_idx] : glm::mat4(1.0f);
        glm::mat4 new_local_mat = glm::translate(glm::mat4(1.0f), pb.positions[bone_idx])
            * glm::mat4_cast(pb.rotations[bone_idx])
            * glm::scale(glm::mat4(1.0f), pb.scales[bone_idx]);
        updated_globals[bone_idx] = parent_global * new_local_mat;

        // Also propagate to child (it may be next in chain)
        if (pb.touched[child_idx]) {
            glm::mat4 child_local = glm::translate(glm::mat4(1.0f), pb.positions[child_idx])
                * glm::mat4_cast(pb.rotations[child_idx])
                * glm::scale(glm::mat4(1.0f), pb.scales[child_idx]);
            updated_globals[child_idx] = updated_globals[bone_idx] * child_local;
        } else {
            updated_globals[child_idx] = updated_globals[bone_idx] * cache.local_bind_poses[child_idx];
        }
    }
}

} // anonymous namespace

void IKSolverSystem::Update(World& world, float delta_time) {
    (void)delta_time;
    auto view = world.registry().view<Animator3DComponent, IKChain3DComponent>();

    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        auto& ik_comp = view.get<IKChain3DComponent>(entity);
        if (!animator.enabled || !animator.skel_cache.valid || !ik_comp.enabled) continue;

        const auto& cache = animator.skel_cache;

        for (auto& chain : ik_comp.chains) {
            if (chain.weight <= 0.0f) continue;

            // Resolve chain bone indices
            if (!BuildChainIndices(chain, cache)) continue;

            // Get target position
            glm::vec3 target = chain.target_position;
            if (chain.target_entity != UINT32_MAX) {
                auto te = static_cast<entt::entity>(chain.target_entity);
                if (world.registry().valid(te) &&
                    world.registry().all_of<TransformComponent>(te)) {
                    target = world.registry().get<TransformComponent>(te).position;
                }
            }

            if (chain.type == IKChainType::LookAt) {
                // LookAt IK: single bone rotation
                int bone_idx = chain.tip_bone_index;
                if (bone_idx < 0) continue;

                // Compute globals to get bone world position
                std::vector<glm::vec3> positions;
                std::vector<glm::mat4> globals;
                ComputeChainGlobalPositions(chain.chain_indices, animator.pose_buffer, cache, positions, globals);

                glm::vec3 bone_pos = glm::vec3(globals[bone_idx][3]);
                int parent_idx = cache.parent_indices[bone_idx];
                glm::mat4 parent_global = (parent_idx >= 0 && parent_idx < static_cast<int>(cache.bone_count))
                    ? globals[parent_idx] : glm::mat4(1.0f);

                SolveLookAt(bone_pos, target, parent_global, animator.pose_buffer.rotations[bone_idx], chain.weight);
                animator.pose_buffer.touched[bone_idx] = true;

            } else {
                // FABRIK
                if (chain.chain_indices.size() < 2) continue;

                // Compute current global positions
                std::vector<glm::vec3> positions;
                std::vector<glm::mat4> globals;
                ComputeChainGlobalPositions(chain.chain_indices, animator.pose_buffer, cache, positions, globals);

                // Store pre-IK globals for writeback
                std::vector<glm::mat4> globals_before = globals;

                // Solve FABRIK
                SolveFABRIK(positions, target, chain.pole_vector, chain.iterations, chain.tolerance);

                // Write results back to pose_buffer
                WriteBackFABRIKResult(chain.chain_indices, positions, globals_before,
                    animator.pose_buffer, cache, chain.weight);
            }
        }
    }
}

} // namespace gameplay3d
} // namespace dse
