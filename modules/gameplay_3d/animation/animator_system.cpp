#include "modules/gameplay_3d/animation/animator_system.h"
#include "modules/gameplay_3d/animation/animation_state_machine.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"
#include "engine/base/debug.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <algorithm>
#include <cstring>

namespace dse {
namespace gameplay3d {

using anim_util::AdvanceClipTime;
using anim_util::AnimSampleBuffer;
using SampleBuffer = AnimSampleBuffer;

AssetManager* AnimatorSystem::asset_manager_ = nullptr;

void AnimatorSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

// Build or refresh SkeletalCache for an entity. Returns false if dskel invalid.
bool BuildSkeletalCache(Animator3DComponent& animator, AssetManager& asset_manager, [[maybe_unused]] unsigned int entity_id) {
    auto& cache = animator.skel_cache;
    if (cache.valid && cache.cached_dskel_path == animator.dskel_path) {
        return true;
    }
    cache.valid = false;
    auto dskel = asset_manager.LoadDskel(animator.dskel_path);
    if (!dskel || dskel->GetData().empty()) return false;

    const uint8_t* skel_data = dskel->GetData().data();
    const auto* skel_header = reinterpret_cast<const asset::compiler::SkelHeader*>(skel_data);
    if (skel_header->magic[0] != 'D' || skel_header->magic[1] != 'S' ||
        skel_header->magic[2] != 'E' || skel_header->magic[3] != 'S') return false;

    const auto* bones = reinterpret_cast<const asset::compiler::BoneDesc*>(
        skel_data + sizeof(asset::compiler::SkelHeader));
    uint32_t bone_count = std::min(skel_header->bone_count, static_cast<uint32_t>(MAX_BONES));

    cache.bone_count = bone_count;
    cache.parent_indices.resize(bone_count);
    cache.local_bind_poses.resize(bone_count);
    cache.bind_globals.resize(bone_count);
    cache.bone_name_to_index.clear();

    for (uint32_t i = 0; i < bone_count; ++i) {
        cache.parent_indices[i] = bones[i].parent_index;
        cache.local_bind_poses[i] = bones[i].local_transform;
    }

    // Parse v2 bone name table
    if (skel_header->version >= 2) {
        const uint8_t* name_ptr = skel_data + sizeof(asset::compiler::SkelHeader) + bone_count * sizeof(asset::compiler::BoneDesc);
        const uint8_t* skel_end = skel_data + dskel->GetData().size();
        for (uint32_t bi = 0; bi < bone_count && name_ptr + 2 <= skel_end; ++bi) {
            uint16_t name_len = static_cast<uint16_t>(name_ptr[0] | (name_ptr[1] << 8));
            name_ptr += 2;
            if (name_ptr + name_len > skel_end) break;
            std::string bname(reinterpret_cast<const char*>(name_ptr), name_len);
            name_ptr += name_len;
            cache.bone_name_to_index[bname] = static_cast<int>(bi);
        }
    }

    // Compute bind_globals (iterative, handles arbitrary ordering)
    std::vector<bool> computed(bone_count, false);
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = cache.parent_indices[i];
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            cache.bind_globals[i] = cache.local_bind_poses[i];
            computed[i] = true;
        }
    }
    for (uint32_t pass = 0; pass < bone_count; ++pass) {
        bool all_done = true;
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (computed[i]) continue;
            int pi = cache.parent_indices[i];
            if (computed[pi]) {
                cache.bind_globals[i] = cache.bind_globals[pi] * cache.local_bind_poses[i];
                computed[i] = true;
            } else {
                all_done = false;
            }
        }
        if (all_done) break;
    }

    // Precompute inv_bind_globals (static, avoids per-frame glm::inverse)
    cache.inv_bind_globals.resize(bone_count);
    for (uint32_t i = 0; i < bone_count; ++i) {
        cache.inv_bind_globals[i] = glm::inverse(cache.bind_globals[i]);
    }

    // Ensure buffers sized
    if (animator.final_bone_matrices.size() < bone_count) {
        animator.final_bone_matrices.resize(bone_count, glm::mat4(1.0f));
        static int log_count = 0;
        if (log_count < 2) {
            DEBUG_LOG_INFO("[3D][VSE15.22] animator_system_first_update entity={} dskel_path={} bone_count={} final_bones={} note=animator_system_confirms_bone_count",
                entity_id, animator.dskel_path, bone_count, animator.final_bone_matrices.size());
            ++log_count;
        }
    }
    animator.pose_buffer.Resize(bone_count);

    cache.cached_dskel_path = animator.dskel_path;
    cache.valid = true;
    return true;
}

void AnimatorSystem::EvaluateBaseAnim(World& world, float delta_time) {
    auto view = world.registry().view<Animator3DComponent>();
    if (view.empty()) return;

    AssetManager* asset_manager_ptr = nullptr;
    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        if (!animator.enabled || animator.dskel_path.empty()) continue;

        if (!asset_manager_ptr) {
            asset_manager_ptr = &anim_util::RequireAssetManager(asset_manager_);
        }
        auto& asset_manager = *asset_manager_ptr;

        if (!BuildSkeletalCache(animator, asset_manager, static_cast<unsigned int>(entity))) continue;

        auto& cache = animator.skel_cache;
        const uint32_t bone_count = cache.bone_count;
        const auto& bone_name_to_index = cache.bone_name_to_index;
        
        auto EvaluateClip = [&](const std::string& anim_path, float current_time, SampleBuffer& sample, float& out_duration) -> bool {
            return anim_util::EvaluateClip(asset_manager, anim_path, current_time, bone_name_to_index, bone_count, sample, out_duration);
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
                if (!anim_util::IsValidAnimHeader(header)) {
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

        // Helper: copy SampleBuffer into pose_buffer
        auto WriteSampleToPoseBuffer = [&](const SampleBuffer& sample) {
            auto& pb = animator.pose_buffer;
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (!sample.touched[i]) continue;
                pb.positions[i] = sample.positions[i];
                pb.rotations[i] = sample.rotations[i];
                pb.scales[i] = sample.scales[i];
                pb.touched[i] = true;
            }
        };

        // Initialize pose_buffer from bind pose decomposition
        animator.pose_buffer.Resize(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i) {
            const auto& bp = cache.local_bind_poses[i];
            glm::vec3 scl(
                glm::length(glm::vec3(bp[0])),
                glm::length(glm::vec3(bp[1])),
                glm::length(glm::vec3(bp[2])));
            animator.pose_buffer.positions[i] = glm::vec3(bp[3]);
            animator.pose_buffer.scales[i] = scl;
            glm::mat3 rot_mat(
                glm::vec3(bp[0]) / std::max(scl.x, 1e-6f),
                glm::vec3(bp[1]) / std::max(scl.y, 1e-6f),
                glm::vec3(bp[2]) / std::max(scl.z, 1e-6f));
            animator.pose_buffer.rotations[i] = glm::quat_cast(rot_mat);
            animator.pose_buffer.touched[i] = false;
        }

        // Clear per-frame event queue
        animator.fired_events.clear();

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
                            animator.is_transitioning = true;
                            animator.next_state_name = trans.target_state;
                            animator.transition_duration = trans.transition_duration;
                            animator.transition_progress = 0.0f;
                            animator.next_state_time = 0.0f;

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
                        animator.current_state_name = animator.next_state_name;
                        animator.state_time = animator.next_state_time;
                        animator.is_transitioning = false;

                        it = states.find(animator.current_state_name);
                        if (it == states.end()) continue;
                    }
                }

                // 3. Evaluate animation (current state)
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

                // 4. Crossfade blend
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

                        float t = std::clamp(animator.transition_progress, 0.0f, 1.0f);
                        auto& pb = animator.pose_buffer;
                        for (uint32_t i = 0; i < bone_count; ++i) {
                            if (!current_sample.touched[i] && !next_sample.touched[i]) continue;
                            pb.positions[i] = glm::mix(current_sample.positions[i], next_sample.positions[i], t);
                            pb.rotations[i] = glm::slerp(current_sample.rotations[i], next_sample.rotations[i], t);
                            pb.scales[i] = glm::mix(current_sample.scales[i], next_sample.scales[i], t);
                            pb.touched[i] = true;
                        }
                    }
                } else {
                    WriteSampleToPoseBuffer(current_sample);
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
            WriteSampleToPoseBuffer(sample);
        } else {
            // --- LEGACY ANIM TREE (1D Blend) PATH ---
            SampleBuffer sample(bone_count);
            if (EvaluateLegacyBlendTree(animator.blend_nodes, animator.blend_parameter_value, sample)) {
                WriteSampleToPoseBuffer(sample);
            }
        }

        // Animation event checking (use state_time for FSM path, current_time for legacy)
        float event_time = animator.state_machine ? animator.state_time : animator.current_time;
        // Detect loop wrap-around: if time went backwards, reset all fired flags
        if (event_time < animator.prev_event_time_) {
            for (auto& evt : animator.events) evt.fired = false;
        }
        animator.prev_event_time_ = event_time;
        for (auto& evt : animator.events) {
            if (!evt.fired && event_time >= evt.trigger_time) {
                evt.fired = true;
                animator.fired_events.push_back(evt.name);
            }
        }

        // Root motion lock / extraction
        if (animator.lock_root_motion || animator.extract_root_motion) {
            int hips_idx = -1;
            if (!bone_name_to_index.empty()) {
                for (const auto& [name, idx] : bone_name_to_index) {
                    if (name.find("Hips") != std::string::npos && idx >= 0 && idx < static_cast<int>(bone_count)) {
                        hips_idx = idx;
                        break;
                    }
                }
            }

            if (hips_idx >= 0) {
                auto& pb = animator.pose_buffer;
                if (animator.extract_root_motion) {
                    animator.root_motion_delta = pb.positions[hips_idx] - animator.prev_root_position_;
                    animator.root_motion_rotation_delta = glm::inverse(animator.prev_root_rotation_) * pb.rotations[hips_idx];
                    animator.prev_root_position_ = pb.positions[hips_idx];
                    animator.prev_root_rotation_ = pb.rotations[hips_idx];
                }
                if (animator.lock_root_motion) {
                    pb.positions[hips_idx] = glm::vec3(cache.local_bind_poses[hips_idx][3]);
                }
            } else if (animator.lock_root_motion) {
                // Fallback: lock root bones (depth 0)
                for (uint32_t i = 0; i < bone_count; ++i) {
                    int pi = cache.parent_indices[i];
                    if (pi < 0 || pi >= static_cast<int>(bone_count)) {
                        animator.pose_buffer.positions[i] = glm::vec3(cache.local_bind_poses[i][3]);
                    }
                }
            }
        }

        // Bone palette key: 根据动画状态生成哈希，用于 ComputeFinalMatrices 去重
        // 相同 (skeleton, clip, quantized_time) 的实体共享同一组骨骼矩阵
        {
            std::hash<std::string> str_hasher;
            uint64_t key = str_hasher(animator.dskel_path);
            if (animator.state_machine) {
                key ^= str_hasher(animator.current_state_name) * 2654435761ULL;
                key ^= static_cast<uint64_t>(static_cast<int>(animator.state_time * 30.0f)) * 40503ULL;
                if (animator.is_transitioning) {
                    key ^= str_hasher(animator.next_state_name) * 11400714819323198485ULL;
                    key ^= static_cast<uint64_t>(static_cast<int>(animator.transition_progress * 30.0f)) * 2246822519ULL;
                }
            } else if (!animator.use_anim_tree) {
                key ^= str_hasher(animator.danim_path) * 2654435761ULL;
                key ^= static_cast<uint64_t>(static_cast<int>(animator.current_time * 30.0f)) * 40503ULL;
            } else {
                key ^= static_cast<uint64_t>(static_cast<int>(animator.blend_parameter_value * 100.0f)) * 40503ULL;
            }
            if (animator.lock_root_motion) key ^= 0xDEADBEEFULL;
            animator.bone_palette_key = key;
        }
    }
}

void AnimatorSystem::ComputeFinalMatrices(World& world) {
    auto view = world.registry().view<Animator3DComponent>();
    if (view.empty()) return;

    // Bone palette deduplication: 相同动画状态（clip + quantized_time）只计算一次
    // key = bone_palette_key (由 EvaluateBaseAnim 填充)
    std::unordered_map<uint64_t, const std::vector<glm::mat4>*> palette_cache;

    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        if (!animator.enabled || !animator.skel_cache.valid) continue;

        // 尝试复用已计算的相同动画状态
        if (animator.bone_palette_key != 0) {
            auto pc_it = palette_cache.find(animator.bone_palette_key);
            if (pc_it != palette_cache.end()) {
                animator.final_bone_matrices = *(pc_it->second);
                continue;
            }
        }

        const auto& cache = animator.skel_cache;
        const uint32_t bone_count = cache.bone_count;
        const auto& pb = animator.pose_buffer;

        // Build local transforms from pose_buffer SRT
        std::vector<glm::mat4> local_transforms(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (pb.touched[i]) {
                local_transforms[i] = glm::translate(glm::mat4(1.0f), pb.positions[i])
                    * glm::mat4_cast(pb.rotations[i])
                    * glm::scale(glm::mat4(1.0f), pb.scales[i]);
            } else {
                local_transforms[i] = cache.local_bind_poses[i];
            }
        }

        // Calculate global transforms (handles arbitrary bone ordering)
        std::vector<glm::mat4> global_transforms(bone_count);
        std::vector<bool> computed(bone_count, false);
        for (uint32_t i = 0; i < bone_count; ++i) {
            int pi = cache.parent_indices[i];
            if (pi < 0 || pi >= static_cast<int>(bone_count)) {
                global_transforms[i] = local_transforms[i];
                computed[i] = true;
            }
        }
        for (uint32_t pass = 0; pass < bone_count; ++pass) {
            bool all_done = true;
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (computed[i]) continue;
                int pi = cache.parent_indices[i];
                if (computed[pi]) {
                    global_transforms[i] = global_transforms[pi] * local_transforms[i];
                    computed[i] = true;
                } else {
                    all_done = false;
                }
            }
            if (all_done) break;
        }

        // final = anim_global * inv(bind_global) [precomputed]
        for (uint32_t i = 0; i < bone_count; ++i) {
            animator.final_bone_matrices[i] = global_transforms[i] * cache.inv_bind_globals[i];
        }

        // 注册到 palette cache 供后续相同动画状态的实体复用
        if (animator.bone_palette_key != 0) {
            palette_cache[animator.bone_palette_key] = &animator.final_bone_matrices;
        }
    }
}

void AnimatorSystem::Update(World& world, float delta_time) {
    EvaluateBaseAnim(world, delta_time);
    ComputeFinalMatrices(world);
}

} // namespace gameplay3d
} // namespace dse