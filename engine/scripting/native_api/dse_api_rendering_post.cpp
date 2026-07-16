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
    decal.albedo_texture =
        dse::render::TextureHandle::from_raw(albedo_texture);
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


extern "C" int dse_post_process_set_color(uint32_t e, int enabled, float exposure, float gamma) {
    World* world = GW();
    if (!world) return 0;
    auto* pp = world->registry().try_get<PostProcessComponent>(TE(e));
    if (!pp) return 0;
    pp->color_grading_enabled = (enabled != 0);
    if (!Keep(exposure)) pp->exposure = exposure;
    if (!Keep(gamma)) pp->gamma = gamma;
    return 1;
}

extern "C" int dse_post_process_get_color_state(uint32_t e, int* out_enabled, int* out_bloom_enabled,
                                                float* out_bloom_threshold, float* out_bloom_intensity,
                                                int* out_color_enabled, float* out_exposure, float* out_gamma,
                                                int* out_ssao_enabled, float* out_ssao_radius, float* out_ssao_bias,
                                                int* out_fxaa_enabled, int* out_vignette_enabled,
                                                float* out_vignette_intensity, float* out_vignette_radius,
                                                float* out_vignette_softness, int* out_film_grain_enabled,
                                                float* out_film_grain_intensity, float* out_film_grain_time_scale) {
    World* world = GW();
    if (!world) return 0;
    const auto* pp = world->registry().try_get<PostProcessComponent>(TE(e));
    if (!pp) return 0;
    if (out_enabled) *out_enabled = pp->enabled ? 1 : 0;
    if (out_bloom_enabled) *out_bloom_enabled = pp->bloom_enabled ? 1 : 0;
    if (out_bloom_threshold) *out_bloom_threshold = pp->bloom_threshold;
    if (out_bloom_intensity) *out_bloom_intensity = pp->bloom_intensity;
    if (out_color_enabled) *out_color_enabled = pp->color_grading_enabled ? 1 : 0;
    if (out_exposure) *out_exposure = pp->exposure;
    if (out_gamma) *out_gamma = pp->gamma;
    if (out_ssao_enabled) *out_ssao_enabled = pp->ssao_enabled ? 1 : 0;
    if (out_ssao_radius) *out_ssao_radius = pp->ssao_radius;
    if (out_ssao_bias) *out_ssao_bias = pp->ssao_bias;
    if (out_fxaa_enabled) *out_fxaa_enabled = pp->fxaa_enabled ? 1 : 0;
    if (out_vignette_enabled) *out_vignette_enabled = pp->vignette_enabled ? 1 : 0;
    if (out_vignette_intensity) *out_vignette_intensity = pp->vignette_intensity;
    if (out_vignette_radius) *out_vignette_radius = pp->vignette_radius;
    if (out_vignette_softness) *out_vignette_softness = pp->vignette_softness;
    if (out_film_grain_enabled) *out_film_grain_enabled = pp->film_grain_enabled ? 1 : 0;
    if (out_film_grain_intensity) *out_film_grain_intensity = pp->film_grain_intensity;
    if (out_film_grain_time_scale) *out_film_grain_time_scale = pp->film_grain_time_scale;
    return 1;
}
