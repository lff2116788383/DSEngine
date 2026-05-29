#include "modules/gameplay_3d/animation/ik_solver_system.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"
#include "engine/ecs/components_3d.h"
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

// LookAt IK: rotate a single bone to face target.
// bind_local_rotation: the bone's bind-pose local rotation, used to derive its rest forward direction.
void SolveLookAt(
    const glm::vec3& bone_world_pos,
    const glm::vec3& target,
    const glm::mat4& parent_global,
    const glm::quat& bind_local_rotation,
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

    // Derive the bone's rest forward from its bind-pose rotation (apply bind rot to -Z)
    glm::vec3 bind_forward = glm::normalize(bind_local_rotation * glm::vec3(0.0f, 0.0f, -1.0f));

    // Compute rotation from bind forward to target direction
    float dot_val = glm::dot(bind_forward, dir_local);
    glm::quat target_rot;
    if (dot_val < -0.9999f) {
        // Nearly opposite: use an arbitrary perpendicular axis
        glm::vec3 perp = (std::abs(bind_forward.x) < 0.9f)
            ? glm::cross(glm::vec3(1, 0, 0), bind_forward)
            : glm::cross(glm::vec3(0, 1, 0), bind_forward);
        target_rot = glm::angleAxis(glm::pi<float>(), glm::normalize(perp));
    } else {
        target_rot = glm::rotation(bind_forward, dir_local);
    }

    // Blend between original rotation and look-at rotation
    out_local_rotation = glm::slerp(out_local_rotation, target_rot, weight);
    out_local_rotation = glm::normalize(out_local_rotation);
}

// CCD IK: Cyclic Coordinate Descent - iterative from closest-to-tip to root.
// Performance: computes full skeleton globals once per outer iteration;
// incrementally updates only chain bones within the inner loop.
void SolveCCD(
    const std::vector<int>& chain_indices,
    const glm::vec3& target,
    Animator3DComponent::PoseBuffer& pb,
    const Animator3DComponent::SkeletalCache& cache,
    int max_iterations,
    float tolerance,
    float weight)
{
    const size_t n = chain_indices.size();
    if (n < 2) return;

    for (int iter = 0; iter < max_iterations; ++iter) {
        // One full-skeleton pass per outer iteration
        std::vector<glm::mat4> globals;
        anim_util::ComputeBoneGlobals(pb, cache, globals);

        glm::vec3 tip_pos = glm::vec3(globals[chain_indices.back()][3]);
        if (glm::distance(tip_pos, target) < tolerance) break;

        // Rotate each bone from closest-to-tip down to root
        for (size_t ci = n - 1; ci > 0; --ci) {
            int bone_idx   = chain_indices[ci];
            int parent_idx = cache.parent_indices[bone_idx];
            if (parent_idx < 0) continue;

            glm::vec3 bone_pos = glm::vec3(globals[bone_idx][3]);

            glm::vec3 to_tip = tip_pos - bone_pos;
            float tip_len = glm::length(to_tip);
            if (tip_len < 1e-6f) continue;

            glm::vec3 to_target = target - bone_pos;
            float tgt_len = glm::length(to_target);
            if (tgt_len < 1e-6f) continue;

            glm::vec3 from_dir = to_tip / tip_len;
            glm::vec3 to_dir   = to_target / tgt_len;
            float dot_val = glm::dot(from_dir, to_dir);
            glm::quat delta_rot;
            if (dot_val > 0.9999f) {
                continue; // Already aligned, skip this bone
            } else if (dot_val < -0.9999f) {
                // Nearly opposite: use perpendicular axis
                glm::vec3 perp = (std::abs(from_dir.x) < 0.9f)
                    ? glm::normalize(glm::cross(glm::vec3(1, 0, 0), from_dir))
                    : glm::normalize(glm::cross(glm::vec3(0, 1, 0), from_dir));
                delta_rot = glm::angleAxis(glm::pi<float>(), perp);
            } else {
                delta_rot = glm::rotation(from_dir, to_dir);
            }
            glm::quat bone_world    = glm::normalize(glm::quat_cast(glm::mat3(globals[bone_idx])));
            glm::quat parent_world  = glm::normalize(glm::quat_cast(glm::mat3(globals[parent_idx])));
            glm::quat new_local     = glm::inverse(parent_world) * (delta_rot * bone_world);

            pb.rotations[bone_idx] = glm::normalize(glm::slerp(pb.rotations[bone_idx], new_local, weight));
            pb.touched[bone_idx] = true;

            // Incrementally propagate globals only for chain bones from ci onward
            for (size_t k = ci; k < n; ++k) {
                int idx  = chain_indices[k];
                int pidx = cache.parent_indices[idx];
                glm::mat4 local = pb.touched[idx]
                    ? glm::translate(glm::mat4(1.0f), pb.positions[idx])
                      * glm::mat4_cast(pb.rotations[idx])
                      * glm::scale(glm::mat4(1.0f), pb.scales[idx])
                    : cache.local_bind_poses[idx];
                globals[idx] = (pidx >= 0 && pidx < static_cast<int>(cache.bone_count))
                    ? globals[pidx] * local : local;
            }
            tip_pos = glm::vec3(globals[chain_indices.back()][3]);
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

                // Compute globals using shared function
                std::vector<glm::mat4> globals;
                std::vector<glm::vec3> positions;
                anim_util::ComputeChainPositions(chain.chain_indices, animator.pose_buffer, cache, positions, globals);

                glm::vec3 bone_pos = positions.back();
                int parent_idx = cache.parent_indices[bone_idx];
                glm::mat4 parent_global = (parent_idx >= 0 && parent_idx < static_cast<int>(cache.bone_count))
                    ? globals[parent_idx] : glm::mat4(1.0f);

                glm::quat bind_local_rot = glm::quat_cast(cache.local_bind_poses[bone_idx]);
                SolveLookAt(bone_pos, target, parent_global, bind_local_rot, animator.pose_buffer.rotations[bone_idx], chain.weight);
                animator.pose_buffer.touched[bone_idx] = true;

            } else if (chain.type == IKChainType::CCD) {
                // CCD IK: iterative from tip to root
                if (chain.chain_indices.size() < 2) continue;
                SolveCCD(chain.chain_indices, target, animator.pose_buffer, cache, 
                         chain.iterations, chain.tolerance, chain.weight);

            } else {
                // FABRIK
                if (chain.chain_indices.size() < 2) continue;

                // Compute current global positions using shared function
                std::vector<glm::vec3> positions;
                std::vector<glm::mat4> globals;
                anim_util::ComputeChainPositions(chain.chain_indices, animator.pose_buffer, cache, positions, globals);

                // Store pre-IK globals for writeback
                std::vector<glm::mat4> globals_before = globals;

                // Solve FABRIK (shared utility, same coordinate space as positions[])
                anim_util::SolveFABRIK(positions, target, chain.pole_vector, chain.iterations, chain.tolerance);

                // Write results back to pose_buffer for all chain bones
                anim_util::WriteBackFABRIKResult(chain.chain_indices, positions, globals_before,
                    animator.pose_buffer, cache, chain.weight);
            }
        }
    }
}

} // namespace gameplay3d
} // namespace dse
