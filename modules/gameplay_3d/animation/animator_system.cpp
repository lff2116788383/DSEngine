#include "modules/gameplay_3d/animation/animator_system.h"
#include "modules/gameplay_3d/animation/animation_state_machine.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"
#include "modules/gameplay_3d/animation/anim_blend_weights.h"
#include "engine/base/debug.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <algorithm>
#include <cstring>
#include <thread>
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#   include <immintrin.h>
#   define DSE_ANIM_USE_SSE 1
#endif

// mat4×mat4: x86 走 SSE 手写矩阵乘法（骨骼计算热路径）；其余架构(ARM/Android)回退到 glm（列主序，语义一致）。
namespace {
#ifdef DSE_ANIM_USE_SSE
inline void Mat4MulSSE(const float* __restrict a, const float* __restrict b, float* __restrict out) {
    __m128 a0 = _mm_loadu_ps(a);
    __m128 a1 = _mm_loadu_ps(a + 4);
    __m128 a2 = _mm_loadu_ps(a + 8);
    __m128 a3 = _mm_loadu_ps(a + 12);
    for (int col = 0; col < 4; ++col) {
        __m128 bc = _mm_loadu_ps(b + col * 4);
        __m128 r = _mm_mul_ps(_mm_shuffle_ps(bc, bc, 0x00), a0);
        r = _mm_add_ps(r, _mm_mul_ps(_mm_shuffle_ps(bc, bc, 0x55), a1));
        r = _mm_add_ps(r, _mm_mul_ps(_mm_shuffle_ps(bc, bc, 0xAA), a2));
        r = _mm_add_ps(r, _mm_mul_ps(_mm_shuffle_ps(bc, bc, 0xFF), a3));
        _mm_storeu_ps(out + col * 4, r);
    }
}
#endif
inline glm::mat4 Mat4Mul(const glm::mat4& a, const glm::mat4& b) {
#ifdef DSE_ANIM_USE_SSE
    glm::mat4 result;
    Mat4MulSSE(&a[0][0], &b[0][0], &result[0][0]);
    return result;
#else
    return a * b;
#endif
}
} // anonymous namespace

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

    // Build topological order (BFS from roots) for single-pass global propagation
    cache.topo_order.clear();
    cache.topo_order.reserve(bone_count);
    {
        std::vector<bool> visited(bone_count, false);
        // First pass: add all roots
        for (uint32_t i = 0; i < bone_count; ++i) {
            int pi = cache.parent_indices[i];
            if (pi < 0 || pi >= static_cast<int>(bone_count)) {
                cache.topo_order.push_back(i);
                visited[i] = true;
            }
        }
        // BFS: process queue, adding children when parent is visited
        for (size_t head = 0; head < cache.topo_order.size(); ++head) {
            uint32_t parent = cache.topo_order[head];
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (!visited[i] && cache.parent_indices[i] == static_cast<int>(parent)) {
                    cache.topo_order.push_back(i);
                    visited[i] = true;
                }
            }
        }
        // Safety: add any remaining (should not happen with valid skeleton)
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (!visited[i]) cache.topo_order.push_back(i);
        }
    }

    // Compute bind_globals using topological order (single pass)
    for (uint32_t idx : cache.topo_order) {
        int pi = cache.parent_indices[idx];
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            cache.bind_globals[idx] = cache.local_bind_poses[idx];
        } else {
            cache.bind_globals[idx] = cache.bind_globals[pi] * cache.local_bind_poses[idx];
        }
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

    // Reusable sample buffers — avoid per-entity heap allocation
    SampleBuffer reuse_sample_a;
    SampleBuffer reuse_sample_b;
    SampleBuffer reuse_blend_accum;
    SampleBuffer reuse_blend_node;

    // Pose palette cache: skip EvaluateClip for entities sharing (skeleton, clip, quantized_time)
    std::unordered_map<uint64_t, Animator3DComponent*> pose_palette_cache;

    AssetManager* asset_manager_ptr = nullptr;
    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        if (!animator.enabled || animator.dskel_path.empty()) continue;

        // Animation LOD: 跳帧更新（保留上一帧的 pose_buffer 和 final_bone_matrices）
        if (animator.anim_lod_skip_ > 0) {
            animator.anim_lod_counter_++;
            if (animator.anim_lod_counter_ <= animator.anim_lod_skip_) {
                continue; // 跳过本帧
            }
            animator.anim_lod_counter_ = 0;
        }

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

            std::vector<float> evaluated_weights;
            const bool use_threshold_blend = blend_nodes.size() >= 2;
            if (use_threshold_blend) {
                std::vector<float> thresholds(blend_nodes.size());
                for (size_t i = 0; i < blend_nodes.size(); ++i) thresholds[i] = blend_nodes[i].threshold;
                evaluated_weights = anim_blend::ComputeBlend1DWeights(thresholds, blend_parameter_value);
            }

            reuse_blend_accum.Reset(bone_count);
            std::fill(reuse_blend_accum.positions.begin(), reuse_blend_accum.positions.end(), glm::vec3(0.0f));
            std::fill(reuse_blend_accum.rotations.begin(), reuse_blend_accum.rotations.end(), glm::quat(0.0f, 0.0f, 0.0f, 0.0f));
            std::fill(reuse_blend_accum.scales.begin(), reuse_blend_accum.scales.end(), glm::vec3(0.0f));
            auto& accumulated = reuse_blend_accum;
            auto& has_anim = reuse_blend_accum.touched;

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
                reuse_blend_node.Reset(bone_count);
                auto& node_sample = reuse_blend_node;
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

        // 2D blend space：反距离加权混合多个采样点（见 anim_blend::ComputeBlend2DWeights）。
        auto EvaluateLegacyBlendTree2D = [&](std::vector<AnimBlendNode2D>& nodes,
                                             glm::vec2 param, SampleBuffer& out_sample) -> bool {
            if (nodes.empty()) return false;

            std::vector<glm::vec2> points(nodes.size());
            for (size_t i = 0; i < nodes.size(); ++i) points[i] = glm::vec2(nodes[i].x, nodes[i].y);
            const std::vector<float> weights = anim_blend::ComputeBlend2DWeights(points, param);

            reuse_blend_accum.Reset(bone_count);
            std::fill(reuse_blend_accum.positions.begin(), reuse_blend_accum.positions.end(), glm::vec3(0.0f));
            std::fill(reuse_blend_accum.rotations.begin(), reuse_blend_accum.rotations.end(), glm::quat(0.0f, 0.0f, 0.0f, 0.0f));
            std::fill(reuse_blend_accum.scales.begin(), reuse_blend_accum.scales.end(), glm::vec3(0.0f));
            auto& accumulated = reuse_blend_accum;
            auto& has_anim = reuse_blend_accum.touched;

            float total_weight = 0.0f;
            for (size_t node_index = 0; node_index < nodes.size(); ++node_index) {
                auto& node = nodes[node_index];
                const float evaluated_weight = weights[node_index];
                if (evaluated_weight <= 0.001f || node.danim_path.empty()) continue;

                auto danim = asset_manager.LoadDanim(node.danim_path);
                if (!danim || danim->GetData().empty()) continue;

                const uint8_t* data = danim->GetData().data();
                const asset::compiler::AnimHeader* header = reinterpret_cast<const asset::compiler::AnimHeader*>(data);
                if (!anim_util::IsValidAnimHeader(header)) continue;

                node.current_time = AdvanceClipTime(node.current_time, delta_time, node.speed, header->duration, node.loop);

                float clip_duration = 0.0f;
                reuse_blend_node.Reset(bone_count);
                auto& node_sample = reuse_blend_node;
                if (!EvaluateClip(node.danim_path, node.current_time, node_sample, clip_duration)) continue;
                node.current_time = AdvanceClipTime(node.current_time, 0.0f, node.speed, clip_duration, node.loop);
                total_weight += evaluated_weight;

                for (uint32_t i = 0; i < bone_count; ++i) {
                    if (!node_sample.touched[i]) continue;
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

            if (total_weight <= 0.0f) return false;
            for (uint32_t i = 0; i < bone_count; ++i) {
                if (!has_anim[i]) continue;
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
                reuse_sample_a.Reset(bone_count);
                auto& current_sample = reuse_sample_a;

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
                        reuse_sample_b.Reset(bone_count);
                        auto& next_sample = reuse_sample_b;

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

            // Early palette key: skip EvaluateClip if same (skel, clip, quantized_time)
            std::hash<std::string> str_hasher;
            uint64_t early_key = str_hasher(animator.dskel_path);
            early_key ^= str_hasher(animator.danim_path) * 2654435761ULL;
            early_key ^= static_cast<uint64_t>(static_cast<int>(animator.current_time * 15.0f)) * 40503ULL;
            if (animator.lock_root_motion) early_key ^= 0xDEADBEEFULL;

            auto ppc_it = pose_palette_cache.find(early_key);
            if (ppc_it != pose_palette_cache.end()) {
                // Reuse pose from already-evaluated entity
                const auto& src = ppc_it->second->pose_buffer;
                auto& pb = animator.pose_buffer;
                for (uint32_t i = 0; i < bone_count; ++i) {
                    pb.positions[i] = src.positions[i];
                    pb.rotations[i] = src.rotations[i];
                    pb.scales[i] = src.scales[i];
                    pb.touched[i] = src.touched[i];
                }
                animator.current_time = AdvanceClipTime(animator.current_time, delta_time, animator.speed,
                    ppc_it->second->cached_duration_, animator.loop);
                animator.bone_palette_key = early_key;
                continue; // skip EvaluateClip, events, root motion
            }

            reuse_sample_a.Reset(bone_count);
            float duration = 0.0f;
            if (!EvaluateClip(animator.danim_path, animator.current_time, reuse_sample_a, duration)) {
                continue;
            }

            animator.current_time = AdvanceClipTime(animator.current_time, delta_time, animator.speed, duration, animator.loop);
            animator.cached_duration_ = duration;
            WriteSampleToPoseBuffer(reuse_sample_a);
            pose_palette_cache[early_key] = &animator;
        } else {
            // --- LEGACY ANIM TREE (1D / 2D Blend) PATH ---
            reuse_sample_a.Reset(bone_count);
            const bool blended = animator.blend_tree_is_2d
                ? EvaluateLegacyBlendTree2D(animator.blend_nodes_2d, animator.blend_parameter_2d, reuse_sample_a)
                : EvaluateLegacyBlendTree(animator.blend_nodes, animator.blend_parameter_value, reuse_sample_a);
            if (blended) {
                WriteSampleToPoseBuffer(reuse_sample_a);
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
                // Check common root-motion bone names (case-insensitive substring match)
                static const char* kRootMotionBoneNames[] = {
                    "Hips", "hips", "Pelvis", "pelvis", "Root", "root",
                    "mixamorig:Hips", "Bip01", "Bip001"
                };
                for (const char* candidate : kRootMotionBoneNames) {
                    auto it = bone_name_to_index.find(candidate);
                    if (it != bone_name_to_index.end() && it->second >= 0 && it->second < static_cast<int>(bone_count)) {
                        hips_idx = it->second;
                        break;
                    }
                }
                // Fallback: substring search for common patterns
                if (hips_idx < 0) {
                    for (const auto& [name, idx] : bone_name_to_index) {
                        if (idx < 0 || idx >= static_cast<int>(bone_count)) continue;
                        if (name.find("Hips") != std::string::npos ||
                            name.find("hips") != std::string::npos ||
                            name.find("Pelvis") != std::string::npos ||
                            name.find("pelvis") != std::string::npos) {
                            hips_idx = idx;
                            break;
                        }
                    }
                }
                // Last resort: first child of skeleton root
                if (hips_idx < 0) {
                    for (uint32_t i = 0; i < bone_count; ++i) {
                        int pi = cache.parent_indices[i];
                        if (pi >= 0 && pi < static_cast<int>(bone_count) &&
                            cache.parent_indices[pi] < 0) {
                            hips_idx = static_cast<int>(i);
                            break;
                        }
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
                key ^= static_cast<uint64_t>(static_cast<int>(animator.state_time * 15.0f)) * 40503ULL;
                if (animator.is_transitioning) {
                    key ^= str_hasher(animator.next_state_name) * 11400714819323198485ULL;
                    key ^= static_cast<uint64_t>(static_cast<int>(animator.transition_progress * 15.0f)) * 2246822519ULL;
                }
            } else if (!animator.use_anim_tree) {
                key ^= str_hasher(animator.danim_path) * 2654435761ULL;
                key ^= static_cast<uint64_t>(static_cast<int>(animator.current_time * 15.0f)) * 40503ULL;
            } else {
                key ^= static_cast<uint64_t>(static_cast<int>(animator.blend_parameter_value * 100.0f)) * 40503ULL;
            }
            if (animator.lock_root_motion) key ^= 0xDEADBEEFULL;
            animator.bone_palette_key = key;
        }
    }
}

// Per-entity bone matrix computation (standalone, thread-safe, no shared state)
static void ComputeEntityBones(Animator3DComponent& animator) {
    const auto& cache = animator.skel_cache;
    const uint32_t bone_count = cache.bone_count;
    const auto& pb = animator.pose_buffer;

    glm::mat4 local_transforms[MAX_BONES];
    glm::mat4 global_transforms[MAX_BONES];

    for (uint32_t i = 0; i < bone_count; ++i) {
        if (pb.touched[i]) {
            local_transforms[i] = glm::translate(glm::mat4(1.0f), pb.positions[i])
                * glm::mat4_cast(pb.rotations[i])
                * glm::scale(glm::mat4(1.0f), pb.scales[i]);
        } else {
            local_transforms[i] = cache.local_bind_poses[i];
        }
    }

    for (uint32_t idx : cache.topo_order) {
        int pi = cache.parent_indices[idx];
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            global_transforms[idx] = local_transforms[idx];
        } else {
            global_transforms[idx] = Mat4Mul(global_transforms[pi], local_transforms[idx]);
        }
    }

    for (uint32_t i = 0; i < bone_count; ++i) {
        animator.final_bone_matrices[i] = Mat4Mul(global_transforms[i], cache.inv_bind_globals[i]);
    }
}

void AnimatorSystem::ComputeFinalMatrices(World& world) {
    auto view = world.registry().view<Animator3DComponent>();
    if (view.empty()) return;

    // Phase 1 (serial): palette dedup — identify unique vs duplicate entities
    std::unordered_map<uint64_t, Animator3DComponent*> palette_owners;
    std::vector<Animator3DComponent*> unique_work; // 需要独立计算的实体
    std::vector<std::pair<Animator3DComponent*, Animator3DComponent*>> copy_work; // (dst, src)

    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        if (!animator.enabled || !animator.skel_cache.valid) continue;

        if (animator.bone_palette_key != 0) {
            auto [it, inserted] = palette_owners.try_emplace(animator.bone_palette_key, &animator);
            if (!inserted) {
                copy_work.emplace_back(&animator, it->second);
                continue;
            }
        }
        unique_work.push_back(&animator);
    }

    // Phase 2 (parallel): compute bone matrices for unique entities
    const size_t work_count = unique_work.size();
    constexpr size_t PARALLEL_THRESHOLD = 32;

    if (work_count > PARALLEL_THRESHOLD) {
        const unsigned int thread_count = std::min(
            static_cast<unsigned int>(std::thread::hardware_concurrency()),
            static_cast<unsigned int>(work_count));
        const size_t chunk_size = (work_count + thread_count - 1) / thread_count;

        std::vector<std::thread> threads;
        threads.reserve(thread_count - 1);

        for (unsigned int t = 1; t < thread_count; ++t) {
            size_t begin = t * chunk_size;
            size_t end = std::min(begin + chunk_size, work_count);
            if (begin >= end) break;
            threads.emplace_back([&unique_work, begin, end]() {
                for (size_t i = begin; i < end; ++i) {
                    ComputeEntityBones(*unique_work[i]);
                }
            });
        }
        // Main thread handles first chunk
        size_t main_end = std::min(chunk_size, work_count);
        for (size_t i = 0; i < main_end; ++i) {
            ComputeEntityBones(*unique_work[i]);
        }
        for (auto& t : threads) t.join();
    } else {
        for (auto* anim : unique_work) {
            ComputeEntityBones(*anim);
        }
    }

    // Phase 3 (serial): copy results to palette duplicates
    for (auto& [dst, src] : copy_work) {
        dst->final_bone_matrices = src->final_bone_matrices;
    }
}

void AnimatorSystem::Update(World& world, float delta_time) {
    EvaluateBaseAnim(world, delta_time);
    ComputeFinalMatrices(world);
}

} // namespace gameplay3d
} // namespace dse