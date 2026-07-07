/**
 * @file dse_api_animation_ext.cpp
 * @brief DSEngine C ABI - Animation 扩展
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/components_3d_animation.h"

using namespace dse_api_internal;


extern "C" void dse_anim3d_set_blend_tree_1d(uint32_t e, const char* const* clips, const float* thresholds,
                                             const float* speeds, int count) {
    World* world = GW();
    if (!world || !clips || !thresholds) return;
    auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!anim) return;
    anim->use_anim_tree = true;
    anim->blend_tree_is_2d = false;
    anim->blend_nodes.clear();
    for (int i = 0; i < count; ++i) {
        AnimBlendNode node;
        if (clips[i]) node.danim_path = clips[i];
        node.threshold = thresholds[i];
        node.speed = speeds ? speeds[i] : 1.0f;
        anim->blend_nodes.push_back(std::move(node));
    }
}

extern "C" void dse_anim3d_set_blend_param(uint32_t e, float value) {
    World* world = GW();
    if (!world) return;
    auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    if (anim) anim->blend_parameter_value = value;
}

extern "C" float dse_anim3d_get_blend_param(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* anim = world->registry().try_get<Animator3DComponent>(TE(e));
    return anim ? anim->blend_parameter_value : 0.0f;
}

extern "C" void dse_anim3d_set_layer_weight(uint32_t e, int layer, float weight) {
    World* world = GW();
    if (!world) return;
    auto* layers = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!layers) return;
    if (layer >= 0 && layer < static_cast<int>(layers->layers.size())) {
        layers->layers[static_cast<size_t>(layer)].weight = weight;
    }
}

extern "C" float dse_anim3d_get_layer_weight(uint32_t e, int layer) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* layers = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!layers || layer < 0 || layer >= static_cast<int>(layers->layers.size())) return 0.0f;
    return layers->layers[static_cast<size_t>(layer)].weight;
}

extern "C" void dse_anim3d_set_layer_mask(uint32_t e, int layer, const char* const* bones, int count) {
    World* world = GW();
    if (!world || !bones) return;
    auto* layers = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!layers) return;
    if (layer >= 0 && layer < static_cast<int>(layers->layers.size())) {
        auto& cfg = layers->layers[static_cast<size_t>(layer)];
        cfg.bone_mask_include.clear();
        for (int i = 0; i < count; ++i) {
            if (bones[i]) cfg.bone_mask_include.push_back(bones[i]);
        }
        cfg.bone_mask_dirty = true;
    }
}

