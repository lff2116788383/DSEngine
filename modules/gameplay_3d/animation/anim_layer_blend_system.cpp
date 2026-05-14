#include "modules/gameplay_3d/animation/anim_layer_blend_system.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"
#include "engine/ecs/components_3d.h"
#include <algorithm>

namespace dse {
namespace gameplay3d {

using anim_util::AnimSampleBuffer;
using anim_util::AdvanceClipTime;

namespace {

bool EvaluateBlendTree1D(
    AssetManager& am,
    std::vector<AnimBlendNode>& nodes,
    float param_value,
    float delta_time,
    const std::unordered_map<std::string, int>& bone_name_to_index,
    uint32_t bone_count,
    AnimSampleBuffer& out)
{
    if (nodes.empty()) return false;

    // Compute threshold weights
    std::vector<float> weights(nodes.size(), 0.0f);
    if (nodes.size() >= 2) {
        size_t lo = 0, hi = nodes.size() - 1;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].threshold <= param_value) lo = i;
            if (nodes[i].threshold >= param_value) { hi = i; break; }
        }
        if (param_value <= nodes.front().threshold) { lo = hi = 0; }
        else if (param_value >= nodes.back().threshold) { lo = hi = nodes.size() - 1; }

        if (lo == hi || std::abs(nodes[hi].threshold - nodes[lo].threshold) <= 0.0001f) {
            weights[lo] = 1.0f;
        } else {
            float t = std::clamp((param_value - nodes[lo].threshold) /
                (nodes[hi].threshold - nodes[lo].threshold), 0.0f, 1.0f);
            weights[lo] = 1.0f - t;
            weights[hi] = t;
        }
    } else {
        weights[0] = 1.0f;
    }

    AnimSampleBuffer acc(bone_count);
    std::fill(acc.rotations.begin(), acc.rotations.end(), glm::quat(0.0f, 0.0f, 0.0f, 0.0f));
    std::fill(acc.positions.begin(), acc.positions.end(), glm::vec3(0.0f));
    std::fill(acc.scales.begin(), acc.scales.end(), glm::vec3(0.0f));
    std::vector<bool> has_anim(bone_count, false);

    float total_w = 0.0f;
    for (size_t ni = 0; ni < nodes.size(); ++ni) {
        auto& node = nodes[ni];
        float w = weights[ni];
        if (w <= 0.0f || node.danim_path.empty()) continue;

        float dur = 0.0f;
        AnimSampleBuffer ns(bone_count);
        node.current_time = AdvanceClipTime(node.current_time, delta_time, node.speed, 999.0f, node.loop);
        if (!anim_util::EvaluateClip(am, node.danim_path, node.current_time, bone_name_to_index, bone_count, ns, dur))
            continue;
        node.current_time = AdvanceClipTime(node.current_time, 0.0f, node.speed, dur, node.loop);
        total_w += w;

        for (uint32_t i = 0; i < bone_count; ++i) {
            if (!ns.touched[i]) continue;
            if (!has_anim[i]) {
                acc.positions[i] = ns.positions[i] * w;
                acc.rotations[i] = ns.rotations[i] * w;
                acc.scales[i] = ns.scales[i] * w;
                has_anim[i] = true;
            } else {
                acc.positions[i] += ns.positions[i] * w;
                acc.rotations[i] += ns.rotations[i] * w;
                acc.scales[i] += ns.scales[i] * w;
            }
        }
    }

    if (total_w <= 0.0f) return false;
    for (uint32_t i = 0; i < bone_count; ++i) {
        if (!has_anim[i]) continue;
        out.positions[i] = acc.positions[i] / total_w;
        out.rotations[i] = glm::normalize(acc.rotations[i]);
        out.scales[i] = acc.scales[i] / total_w;
        out.touched[i] = true;
    }
    return true;
}

bool EvaluateBlendTree2D(
    AssetManager& am,
    std::vector<AnimBlendNode2D>& nodes,
    glm::vec2 param,
    float delta_time,
    const std::unordered_map<std::string, int>& bone_name_to_index,
    uint32_t bone_count,
    AnimSampleBuffer& out)
{
    if (nodes.empty()) return false;

    // Inverse-distance weighting for arbitrary point layout (Grid Mode fallback)
    std::vector<float> weights(nodes.size(), 0.0f);
    float total_w = 0.0f;

    // Check for exact match first
    bool exact = false;
    for (size_t i = 0; i < nodes.size(); ++i) {
        float dx = param.x - nodes[i].x;
        float dy = param.y - nodes[i].y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < 1e-6f) {
            weights[i] = 1.0f;
            total_w = 1.0f;
            exact = true;
            break;
        }
    }

    if (!exact) {
        // Shepard's method (IDW p=2)
        for (size_t i = 0; i < nodes.size(); ++i) {
            float dx = param.x - nodes[i].x;
            float dy = param.y - nodes[i].y;
            float dist2 = dx * dx + dy * dy;
            float w = 1.0f / std::max(dist2, 1e-8f);
            weights[i] = w;
            total_w += w;
        }
    }

    if (total_w <= 0.0f) return false;

    AnimSampleBuffer acc(bone_count);
    std::fill(acc.rotations.begin(), acc.rotations.end(), glm::quat(0.0f, 0.0f, 0.0f, 0.0f));
    std::fill(acc.positions.begin(), acc.positions.end(), glm::vec3(0.0f));
    std::fill(acc.scales.begin(), acc.scales.end(), glm::vec3(0.0f));
    std::vector<bool> has_anim(bone_count, false);

    float effective_total = 0.0f;
    for (size_t ni = 0; ni < nodes.size(); ++ni) {
        float w = weights[ni] / total_w;
        if (w <= 0.001f) continue;
        auto& node = nodes[ni];

        float dur = 0.0f;
        AnimSampleBuffer ns(bone_count);
        node.current_time = AdvanceClipTime(node.current_time, delta_time, node.speed, 999.0f, node.loop);
        if (!anim_util::EvaluateClip(am, node.danim_path, node.current_time, bone_name_to_index, bone_count, ns, dur))
            continue;
        node.current_time = AdvanceClipTime(node.current_time, 0.0f, node.speed, dur, node.loop);
        effective_total += w;

        for (uint32_t i = 0; i < bone_count; ++i) {
            if (!ns.touched[i]) continue;
            if (!has_anim[i]) {
                acc.positions[i] = ns.positions[i] * w;
                acc.rotations[i] = ns.rotations[i] * w;
                acc.scales[i] = ns.scales[i] * w;
                has_anim[i] = true;
            } else {
                acc.positions[i] += ns.positions[i] * w;
                acc.rotations[i] += ns.rotations[i] * w;
                acc.scales[i] += ns.scales[i] * w;
            }
        }
    }

    if (effective_total <= 0.0f) return false;
    for (uint32_t i = 0; i < bone_count; ++i) {
        if (!has_anim[i]) continue;
        out.positions[i] = acc.positions[i] / effective_total;
        out.rotations[i] = glm::normalize(acc.rotations[i]);
        out.scales[i] = acc.scales[i] / effective_total;
        out.touched[i] = true;
    }
    return true;
}

void ResolveBoneMask(AnimLayerConfig& layer, const std::unordered_map<std::string, int>& name_to_idx,
    const std::vector<int>& parent_indices, uint32_t bone_count)
{
    if (!layer.bone_mask_dirty) return;
    layer.bone_mask_indices.clear();

    // Collect explicitly named bones
    std::vector<bool> in_mask(bone_count, false);
    for (const auto& name : layer.bone_mask_include) {
        auto it = name_to_idx.find(name);
        if (it != name_to_idx.end() && it->second >= 0 && it->second < static_cast<int>(bone_count)) {
            in_mask[it->second] = true;
        }
    }

    // Propagate to children: if parent is in mask, child is too
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = parent_indices[i];
        if (pi >= 0 && pi < static_cast<int>(bone_count) && in_mask[pi]) {
            in_mask[i] = true;
        }
    }

    for (uint32_t i = 0; i < bone_count; ++i) {
        if (in_mask[i]) layer.bone_mask_indices.push_back(static_cast<int>(i));
    }
    std::sort(layer.bone_mask_indices.begin(), layer.bone_mask_indices.end());
    layer.bone_mask_dirty = false;
}

bool IsBoneInMask(int bone_idx, const std::vector<int>& mask) {
    if (mask.empty()) return true; // empty = all bones
    return std::binary_search(mask.begin(), mask.end(), bone_idx);
}

} // anonymous namespace

AssetManager* AnimLayerBlendSystem::asset_manager_ = nullptr;

void AnimLayerBlendSystem::SetAssetManager(AssetManager* am) {
    asset_manager_ = am;
}

void AnimLayerBlendSystem::Update(World& world, float delta_time) {
    auto view = world.registry().view<Animator3DComponent, AnimLayerComponent>();

    AssetManager* am_ptr = nullptr;
    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        auto& layer_comp = view.get<AnimLayerComponent>(entity);
        if (!animator.enabled || !animator.skel_cache.valid || !layer_comp.enabled) continue;

        if (!am_ptr) am_ptr = &anim_util::RequireAssetManager(asset_manager_);

        const auto& cache = animator.skel_cache;
        const uint32_t bone_count = cache.bone_count;

        for (auto& layer : layer_comp.layers) {
            if (layer.weight <= 0.0f) continue;

            // Resolve bone mask
            ResolveBoneMask(layer, cache.bone_name_to_index, cache.parent_indices, bone_count);

            // Sample this layer's animation
            AnimSampleBuffer layer_sample(bone_count);
            bool sampled = false;

            switch (layer.source_type) {
            case AnimSourceType::SingleClip: {
                float dur = 0.0f;
                if (anim_util::EvaluateClip(*am_ptr, layer.danim_path, layer.current_time,
                        cache.bone_name_to_index, bone_count, layer_sample, dur)) {
                    layer.current_time = AdvanceClipTime(layer.current_time, delta_time, layer.speed, dur, layer.loop);
                    sampled = true;
                }
                break;
            }
            case AnimSourceType::BlendTree1D:
                sampled = EvaluateBlendTree1D(*am_ptr, layer.blend_nodes, layer.blend_parameter_value,
                    delta_time, cache.bone_name_to_index, bone_count, layer_sample);
                break;
            case AnimSourceType::BlendTree2D:
                sampled = EvaluateBlendTree2D(*am_ptr, layer.blend_nodes_2d, layer.blend_parameter_2d,
                    delta_time, cache.bone_name_to_index, bone_count, layer_sample);
                break;
            }

            if (!sampled) continue;

            // Blend layer_sample onto base pose_buffer
            auto& pb = animator.pose_buffer;
            float w = layer.weight;

            for (uint32_t i = 0; i < bone_count; ++i) {
                if (!layer_sample.touched[i]) continue;
                if (!IsBoneInMask(static_cast<int>(i), layer.bone_mask_indices)) continue;

                switch (layer.blend_mode) {
                case AnimLayerBlendMode::Override:
                    pb.positions[i] = glm::mix(pb.positions[i], layer_sample.positions[i], w);
                    pb.rotations[i] = glm::slerp(pb.rotations[i], layer_sample.rotations[i], w);
                    pb.scales[i] = glm::mix(pb.scales[i], layer_sample.scales[i], w);
                    pb.touched[i] = true;
                    break;

                case AnimLayerBlendMode::Additive: {
                    // Additive: delta = layer - bind, base += delta * weight
                    const auto& bp = cache.local_bind_poses[i];
                    glm::vec3 bind_scl(
                        glm::length(glm::vec3(bp[0])),
                        glm::length(glm::vec3(bp[1])),
                        glm::length(glm::vec3(bp[2])));

                    glm::vec3 bind_pos = glm::vec3(bp[3]);
                    pb.positions[i] += (layer_sample.positions[i] - bind_pos) * w;

                    // Rotation: strip scale before extracting bind rotation
                    glm::mat3 bind_rot_mat(
                        glm::vec3(bp[0]) / std::max(bind_scl.x, 1e-6f),
                        glm::vec3(bp[1]) / std::max(bind_scl.y, 1e-6f),
                        glm::vec3(bp[2]) / std::max(bind_scl.z, 1e-6f));
                    glm::quat bind_rot = glm::normalize(glm::quat_cast(bind_rot_mat));
                    glm::quat delta_rot = glm::inverse(bind_rot) * layer_sample.rotations[i];
                    pb.rotations[i] = glm::slerp(glm::quat(1, 0, 0, 0), delta_rot, w) * pb.rotations[i];
                    pb.rotations[i] = glm::normalize(pb.rotations[i]);

                    // Scale: multiplicative delta
                    glm::vec3 delta_scl = layer_sample.scales[i] / glm::max(bind_scl, glm::vec3(1e-6f));
                    pb.scales[i] *= glm::mix(glm::vec3(1.0f), delta_scl, w);
                    pb.touched[i] = true;
                    break;
                }
                }
            }
        }
    }
}

} // namespace gameplay3d
} // namespace dse
