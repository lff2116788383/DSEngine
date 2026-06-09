/**
 * @file dse_api_post_process.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：PostProcessComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_render.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>
#include <cstring>

using Entity = entt::entity;

namespace {
inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline bool V(uint32_t e) { World* w = GW(); return w && w->registry().valid(static_cast<Entity>(static_cast<entt::id_type>(e))); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
template<typename T> T* GC(uint32_t e) { World* w = GW(); if (!V(e)) return nullptr; return w->registry().try_get<T>(TE(e)); }
template<typename T> const T* GCC(uint32_t e) { return GC<T>(e); }
}

/* ---- PostProcessComponent ---- */
extern "C" int dse_post_process_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" int dse_post_process_get_bloom_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->bloom_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_bloom_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->bloom_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_bloom_threshold(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->bloom_threshold : 1.0f;
}
extern "C" void dse_post_process_set_bloom_threshold(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->bloom_threshold = v;
    }
}
extern "C" float dse_post_process_get_bloom_intensity(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->bloom_intensity : 0.5f;
}
extern "C" void dse_post_process_set_bloom_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->bloom_intensity = v;
    }
}
extern "C" float dse_post_process_get_bloom_knee(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->bloom_knee : 0.1f;
}
extern "C" void dse_post_process_set_bloom_knee(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->bloom_knee = v;
    }
}
extern "C" float dse_post_process_get_bloom_mip_weight(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->bloom_mip_weight : 0.005f;
}
extern "C" void dse_post_process_set_bloom_mip_weight(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->bloom_mip_weight = v;
    }
}
extern "C" int dse_post_process_get_color_grading_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->color_grading_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_color_grading_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->color_grading_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_exposure(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->exposure : 1.0f;
}
extern "C" void dse_post_process_set_exposure(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->exposure = v;
    }
}
extern "C" float dse_post_process_get_gamma(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->gamma : 2.2f;
}
extern "C" void dse_post_process_set_gamma(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->gamma = v;
    }
}
extern "C" int dse_post_process_get_ssao_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->ssao_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_ssao_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssao_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_ssao_radius(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssao_radius : 0.5f;
}
extern "C" void dse_post_process_set_ssao_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssao_radius = v;
    }
}
extern "C" float dse_post_process_get_ssao_bias(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssao_bias : 0.025f;
}
extern "C" void dse_post_process_set_ssao_bias(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssao_bias = v;
    }
}
extern "C" int dse_post_process_get_ssao_sample_count(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? static_cast<int>(c->ssao_sample_count) : 32;
}
extern "C" void dse_post_process_set_ssao_sample_count(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssao_sample_count = v;
    }
}
extern "C" float dse_post_process_get_ssao_power(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssao_power : 2.0f;
}
extern "C" void dse_post_process_set_ssao_power(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssao_power = v;
    }
}
extern "C" float dse_post_process_get_ssao_intensity(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssao_intensity : 1.0f;
}
extern "C" void dse_post_process_set_ssao_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssao_intensity = v;
    }
}
extern "C" int dse_post_process_get_auto_exposure_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->auto_exposure_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_auto_exposure_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->auto_exposure_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_exposure_min(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->exposure_min : 0.1f;
}
extern "C" void dse_post_process_set_exposure_min(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->exposure_min = v;
    }
}
extern "C" float dse_post_process_get_exposure_max(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->exposure_max : 10.0f;
}
extern "C" void dse_post_process_set_exposure_max(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->exposure_max = v;
    }
}
extern "C" float dse_post_process_get_adaptation_speed_up(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->adaptation_speed_up : 2.0f;
}
extern "C" void dse_post_process_set_adaptation_speed_up(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->adaptation_speed_up = v;
    }
}
extern "C" float dse_post_process_get_adaptation_speed_down(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->adaptation_speed_down : 1.0f;
}
extern "C" void dse_post_process_set_adaptation_speed_down(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->adaptation_speed_down = v;
    }
}
extern "C" float dse_post_process_get_exposure_compensation(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->exposure_compensation : 0.0f;
}
extern "C" void dse_post_process_set_exposure_compensation(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->exposure_compensation = v;
    }
}
extern "C" float dse_post_process_get_color_lut_intensity(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->color_lut_intensity : 1.0f;
}
extern "C" void dse_post_process_set_color_lut_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->color_lut_intensity = v;
    }
}
extern "C" int dse_post_process_get_vignette_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->vignette_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_vignette_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->vignette_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_vignette_intensity(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->vignette_intensity : 0.35f;
}
extern "C" void dse_post_process_set_vignette_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->vignette_intensity = v;
    }
}
extern "C" float dse_post_process_get_vignette_radius(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->vignette_radius : 0.75f;
}
extern "C" void dse_post_process_set_vignette_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->vignette_radius = v;
    }
}
extern "C" float dse_post_process_get_vignette_softness(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->vignette_softness : 0.35f;
}
extern "C" void dse_post_process_set_vignette_softness(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->vignette_softness = v;
    }
}
extern "C" int dse_post_process_get_film_grain_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->film_grain_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_film_grain_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->film_grain_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_film_grain_intensity(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->film_grain_intensity : 0.08f;
}
extern "C" void dse_post_process_set_film_grain_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->film_grain_intensity = v;
    }
}
extern "C" float dse_post_process_get_film_grain_time_scale(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->film_grain_time_scale : 1.0f;
}
extern "C" void dse_post_process_set_film_grain_time_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->film_grain_time_scale = v;
    }
}
extern "C" int dse_post_process_get_fxaa_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->fxaa_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_fxaa_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fxaa_enabled = (v != 0);
    }
}
extern "C" int dse_post_process_get_taa_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->taa_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_taa_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->taa_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_taa_blend_factor(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->taa_blend_factor : 0.1f;
}
extern "C" void dse_post_process_set_taa_blend_factor(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->taa_blend_factor = v;
    }
}
extern "C" int dse_post_process_get_contact_shadow_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->contact_shadow_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_contact_shadow_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->contact_shadow_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_contact_shadow_strength(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->contact_shadow_strength : 0.5f;
}
extern "C" void dse_post_process_set_contact_shadow_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->contact_shadow_strength = v;
    }
}
extern "C" int dse_post_process_get_contact_shadow_steps(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? static_cast<int>(c->contact_shadow_steps) : 16;
}
extern "C" void dse_post_process_set_contact_shadow_steps(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->contact_shadow_steps = v;
    }
}
extern "C" float dse_post_process_get_contact_shadow_step_size(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->contact_shadow_step_size : 0.5f;
}
extern "C" void dse_post_process_set_contact_shadow_step_size(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->contact_shadow_step_size = v;
    }
}
extern "C" int dse_post_process_get_dof_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->dof_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_dof_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->dof_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_dof_focus_distance(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->dof_focus_distance : 100.0f;
}
extern "C" void dse_post_process_set_dof_focus_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->dof_focus_distance = v;
    }
}
extern "C" float dse_post_process_get_dof_focus_range(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->dof_focus_range : 50.0f;
}
extern "C" void dse_post_process_set_dof_focus_range(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->dof_focus_range = v;
    }
}
extern "C" float dse_post_process_get_dof_bokeh_radius(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->dof_bokeh_radius : 4.0f;
}
extern "C" void dse_post_process_set_dof_bokeh_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->dof_bokeh_radius = v;
    }
}
extern "C" int dse_post_process_get_motion_blur_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->motion_blur_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_motion_blur_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->motion_blur_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_motion_blur_intensity(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->motion_blur_intensity : 1.0f;
}
extern "C" void dse_post_process_set_motion_blur_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->motion_blur_intensity = v;
    }
}
extern "C" int dse_post_process_get_motion_blur_samples(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? static_cast<int>(c->motion_blur_samples) : 8;
}
extern "C" void dse_post_process_set_motion_blur_samples(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->motion_blur_samples = v;
    }
}
extern "C" int dse_post_process_get_ssr_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->ssr_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_ssr_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssr_enabled = (v != 0);
    }
}
extern "C" float dse_post_process_get_ssr_max_distance(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssr_max_distance : 100.0f;
}
extern "C" void dse_post_process_set_ssr_max_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssr_max_distance = v;
    }
}
extern "C" float dse_post_process_get_ssr_thickness(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssr_thickness : 0.5f;
}
extern "C" void dse_post_process_set_ssr_thickness(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssr_thickness = v;
    }
}
extern "C" float dse_post_process_get_ssr_step_size(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssr_step_size : 1.0f;
}
extern "C" void dse_post_process_set_ssr_step_size(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssr_step_size = v;
    }
}
extern "C" int dse_post_process_get_ssr_max_steps(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? static_cast<int>(c->ssr_max_steps) : 64;
}
extern "C" void dse_post_process_set_ssr_max_steps(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssr_max_steps = v;
    }
}
extern "C" float dse_post_process_get_ssr_fade_distance(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssr_fade_distance : 0.2f;
}
extern "C" void dse_post_process_set_ssr_fade_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssr_fade_distance = v;
    }
}
extern "C" float dse_post_process_get_ssr_max_roughness(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->ssr_max_roughness : 0.5f;
}
extern "C" void dse_post_process_set_ssr_max_roughness(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->ssr_max_roughness = v;
    }
}
extern "C" int dse_post_process_get_outline_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->outline_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_outline_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->outline_enabled = (v != 0);
    }
}
extern "C" void dse_post_process_get_outline_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::PostProcessComponent>(e)) { *x = c->outline_color.x; *y = c->outline_color.y; *z = c->outline_color.z; }
}
extern "C" void dse_post_process_set_outline_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->outline_color = glm::vec3(x, y, z);
    }
}
extern "C" float dse_post_process_get_outline_thickness(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->outline_thickness : 1.0f;
}
extern "C" void dse_post_process_set_outline_thickness(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->outline_thickness = v;
    }
}
extern "C" float dse_post_process_get_outline_depth_threshold(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->outline_depth_threshold : 0.1f;
}
extern "C" void dse_post_process_set_outline_depth_threshold(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->outline_depth_threshold = v;
    }
}
extern "C" float dse_post_process_get_outline_normal_threshold(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->outline_normal_threshold : 0.4f;
}
extern "C" void dse_post_process_set_outline_normal_threshold(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->outline_normal_threshold = v;
    }
}
extern "C" int dse_post_process_get_light_shaft_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->light_shaft_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_light_shaft_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->light_shaft_enabled = (v != 0);
    }
}
extern "C" void dse_post_process_get_light_shaft_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::PostProcessComponent>(e)) { *x = c->light_shaft_color.x; *y = c->light_shaft_color.y; *z = c->light_shaft_color.z; }
}
extern "C" void dse_post_process_set_light_shaft_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->light_shaft_color = glm::vec3(x, y, z);
    }
}
extern "C" float dse_post_process_get_light_shaft_density(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->light_shaft_density : 0.84f;
}
extern "C" void dse_post_process_set_light_shaft_density(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->light_shaft_density = v;
    }
}
extern "C" float dse_post_process_get_light_shaft_weight(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->light_shaft_weight : 0.04f;
}
extern "C" void dse_post_process_set_light_shaft_weight(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->light_shaft_weight = v;
    }
}
extern "C" float dse_post_process_get_light_shaft_decay(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->light_shaft_decay : 0.97f;
}
extern "C" void dse_post_process_set_light_shaft_decay(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->light_shaft_decay = v;
    }
}
extern "C" float dse_post_process_get_light_shaft_exposure(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->light_shaft_exposure : 0.4f;
}
extern "C" void dse_post_process_set_light_shaft_exposure(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->light_shaft_exposure = v;
    }
}
extern "C" float dse_post_process_get_light_shaft_intensity(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->light_shaft_intensity : 1.0f;
}
extern "C" void dse_post_process_set_light_shaft_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->light_shaft_intensity = v;
    }
}
extern "C" int dse_post_process_get_light_shaft_samples(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? static_cast<int>(c->light_shaft_samples) : 64;
}
extern "C" void dse_post_process_set_light_shaft_samples(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->light_shaft_samples = v;
    }
}
extern "C" int dse_post_process_get_fog_enabled(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return (c && c->fog_enabled) ? 1 : 0;
}
extern "C" void dse_post_process_set_fog_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_enabled = (v != 0);
    }
}
extern "C" void dse_post_process_get_fog_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::PostProcessComponent>(e)) { *x = c->fog_color.x; *y = c->fog_color.y; *z = c->fog_color.z; }
}
extern "C" void dse_post_process_set_fog_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_color = glm::vec3(x, y, z);
    }
}
extern "C" float dse_post_process_get_fog_density(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->fog_density : 0.02f;
}
extern "C" void dse_post_process_set_fog_density(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_density = v;
    }
}
extern "C" float dse_post_process_get_fog_height_falloff(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->fog_height_falloff : 0.3f;
}
extern "C" void dse_post_process_set_fog_height_falloff(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_height_falloff = v;
    }
}
extern "C" float dse_post_process_get_fog_height_offset(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->fog_height_offset : 0.0f;
}
extern "C" void dse_post_process_set_fog_height_offset(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_height_offset = v;
    }
}
extern "C" float dse_post_process_get_fog_start(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->fog_start : 0.0f;
}
extern "C" void dse_post_process_set_fog_start(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_start = v;
    }
}
extern "C" float dse_post_process_get_fog_end(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->fog_end : 1000.0f;
}
extern "C" void dse_post_process_set_fog_end(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_end = v;
    }
}
extern "C" int dse_post_process_get_fog_steps(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? static_cast<int>(c->fog_steps) : 16;
}
extern "C" void dse_post_process_set_fog_steps(uint32_t e, int v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_steps = v;
    }
}
extern "C" float dse_post_process_get_fog_sun_scatter(uint32_t e) {
    const auto* c = GCC<dse::PostProcessComponent>(e);
    return c ? c->fog_sun_scatter : 0.6f;
}
extern "C" void dse_post_process_set_fog_sun_scatter(uint32_t e, float v) {
    if (auto* c = GC<dse::PostProcessComponent>(e)) {
        c->fog_sun_scatter = v;
    }
}
