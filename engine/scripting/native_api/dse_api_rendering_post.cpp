/**
 * @file dse_api_rendering_post.cpp
 * @brief DSEngine C ABI - Rendering Post 扩展
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/components_3d_render.h"

using namespace dse;
using namespace dse_api_internal;


extern "C" void dse_decal_add(uint32_t e, uint32_t albedo_texture) {
    World* world = GW();
    if (!world) return;
    auto& decal = world->registry().emplace_or_replace<DecalComponent>(TE(e));
    decal.enabled = true;
    decal.albedo_texture = albedo_texture;
}

extern "C" void dse_decal_set(uint32_t e, float r, float g, float b, float a, float angle_fade) {
    World* world = GW();
    if (!world) return;
    auto* decal = world->registry().try_get<DecalComponent>(TE(e));
    if (!decal) return;
    if (!Keep(r) && !Keep(g) && !Keep(b) && !Keep(a)) decal->color = glm::vec4(r, g, b, a);
    if (!Keep(angle_fade)) decal->angle_fade = angle_fade;
}

extern "C" int dse_post_process_get_state(uint32_t e, int* out_enabled, int* out_bloom, int* out_ssao,
                                           int* out_ssr, int* out_fxaa, int* out_dof) {
    World* world = GW();
    if (!world) return 0;
    const auto* pp = world->registry().try_get<PostProcessComponent>(TE(e));
    if (!pp) return 0;
    if (out_enabled) *out_enabled = pp->enabled ? 1 : 0;
    if (out_bloom) *out_bloom = pp->bloom_enabled ? 1 : 0;
    if (out_ssao) *out_ssao = pp->ssao_enabled ? 1 : 0;
    if (out_ssr) *out_ssr = pp->ssr_enabled ? 1 : 0;
    if (out_fxaa) *out_fxaa = pp->fxaa_enabled ? 1 : 0;
    if (out_dof) *out_dof = pp->dof_enabled ? 1 : 0;
    return 1;
}

