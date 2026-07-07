/**
 * @file lua_binding_ecs_rendering_post.cpp
 * @brief PostProcess / Decal Lua 绑定 — C ABI 薄包装
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <cmath>

namespace dse::runtime::lua_binding {
namespace {

// Helper: check entity as uint32_t
inline uint32_t Ent(lua_State* L, int idx) {
    return static_cast<uint32_t>(luaL_checkinteger(L, idx));
}

// Helper: opt bool with current value fallback
inline int OptBool(lua_State* L, int idx, int current) {
    if (lua_isnoneornil(L, idx)) return current;
    return lua_toboolean(L, idx) ? 1 : 0;
}

// Helper: opt float with current value fallback
inline float OptFloat(lua_State* L, int idx, float current) {
    if (lua_isnoneornil(L, idx)) return current;
    return static_cast<float>(luaL_checknumber(L, idx));
}

// ============================================================
// PostProcess
// ============================================================

int L_EcsAddPostProcess(lua_State* L) {
    uint32_t e = Ent(L, 1);
    dse_post_process_add(e);
    dse_post_process_set_enabled(e, 1);
    dse_post_process_set_bloom_enabled(e, helper::CheckBool(L, 2));
    dse_post_process_set_bloom_threshold(e, helper::OptFloat(L, 3, 1.0f));
    dse_post_process_set_bloom_intensity(e, helper::OptFloat(L, 4, 1.0f));
    dse_post_process_set_exposure(e, helper::OptFloat(L, 5, 1.0f));
    return 0;
}

int L_EcsSetPostProcessBloom(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int enabled = dse_post_process_get_enabled(e);
    int bloom_en = dse_post_process_get_bloom_enabled(e);
    float thresh = dse_post_process_get_bloom_threshold(e);
    float intens = dse_post_process_get_bloom_intensity(e);
    float exposure = dse_post_process_get_exposure(e);
    float knee = dse_post_process_get_bloom_knee(e);
    float mip = dse_post_process_get_bloom_mip_weight(e);

    dse_post_process_set_enabled(e, OptBool(L, 2, enabled));
    dse_post_process_set_bloom_enabled(e, OptBool(L, 3, bloom_en));
    dse_post_process_set_bloom_threshold(e, OptFloat(L, 4, thresh));
    dse_post_process_set_bloom_intensity(e, OptFloat(L, 5, intens));
    dse_post_process_set_exposure(e, OptFloat(L, 6, exposure));
    dse_post_process_set_bloom_knee(e, OptFloat(L, 7, knee));
    dse_post_process_set_bloom_mip_weight(e, OptFloat(L, 8, mip));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessColor(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int cg = dse_post_process_get_color_grading_enabled(e);
    float exposure = dse_post_process_get_exposure(e);
    float gamma = dse_post_process_get_gamma(e);

    dse_post_process_set_color_grading_enabled(e, OptBool(L, 2, cg));
    dse_post_process_set_exposure(e, OptFloat(L, 3, exposure));
    dse_post_process_set_gamma(e, OptFloat(L, 4, gamma));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessSSAO(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int en = dse_post_process_get_ssao_enabled(e);
    float radius = dse_post_process_get_ssao_radius(e);
    float bias = dse_post_process_get_ssao_bias(e);
    int samples = dse_post_process_get_ssao_sample_count(e);
    float power = dse_post_process_get_ssao_power(e);
    float intens = dse_post_process_get_ssao_intensity(e);

    dse_post_process_set_ssao_enabled(e, OptBool(L, 2, en));
    dse_post_process_set_ssao_radius(e, OptFloat(L, 3, radius));
    dse_post_process_set_ssao_bias(e, OptFloat(L, 4, bias));
    int sc = static_cast<int>(OptFloat(L, 5, static_cast<float>(samples)));
    dse_post_process_set_ssao_sample_count(e, sc);
    dse_post_process_set_ssao_power(e, OptFloat(L, 6, power));
    dse_post_process_set_ssao_intensity(e, OptFloat(L, 7, intens));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessSSR(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int en = dse_post_process_get_ssr_enabled(e);
    float max_dist = dse_post_process_get_ssr_max_distance(e);
    float fade = dse_post_process_get_ssr_fade_distance(e);
    float rough = dse_post_process_get_ssr_max_roughness(e);
    float thick = dse_post_process_get_ssr_thickness(e);
    float step = dse_post_process_get_ssr_step_size(e);
    int max_steps = dse_post_process_get_ssr_max_steps(e);

    dse_post_process_set_ssr_enabled(e, OptBool(L, 2, en));
    dse_post_process_set_ssr_max_distance(e, OptFloat(L, 3, max_dist));
    dse_post_process_set_ssr_fade_distance(e, OptFloat(L, 4, fade));
    dse_post_process_set_ssr_max_roughness(e, OptFloat(L, 5, rough));
    dse_post_process_set_ssr_thickness(e, OptFloat(L, 6, thick));
    dse_post_process_set_ssr_step_size(e, OptFloat(L, 7, step));
    int ms = static_cast<int>(OptFloat(L, 8, static_cast<float>(max_steps)));
    dse_post_process_set_ssr_max_steps(e, ms);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessFXAA(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int fxaa = dse_post_process_get_fxaa_enabled(e);
    dse_post_process_set_fxaa_enabled(e, OptBool(L, 2, fxaa));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessAutoExposure(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int en = dse_post_process_get_auto_exposure_enabled(e);
    float min_v = dse_post_process_get_exposure_min(e);
    float max_v = dse_post_process_get_exposure_max(e);
    float up = dse_post_process_get_adaptation_speed_up(e);
    float down = dse_post_process_get_adaptation_speed_down(e);
    float comp = dse_post_process_get_exposure_compensation(e);

    dse_post_process_set_auto_exposure_enabled(e, OptBool(L, 2, en));
    dse_post_process_set_exposure_min(e, OptFloat(L, 3, min_v));
    dse_post_process_set_exposure_max(e, OptFloat(L, 4, max_v));
    dse_post_process_set_adaptation_speed_up(e, OptFloat(L, 5, up));
    dse_post_process_set_adaptation_speed_down(e, OptFloat(L, 6, down));
    dse_post_process_set_exposure_compensation(e, OptFloat(L, 7, comp));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessVignette(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int en = dse_post_process_get_vignette_enabled(e);
    float intens = dse_post_process_get_vignette_intensity(e);
    float radius = dse_post_process_get_vignette_radius(e);
    float soft = dse_post_process_get_vignette_softness(e);

    dse_post_process_set_vignette_enabled(e, OptBool(L, 2, en));
    dse_post_process_set_vignette_intensity(e, OptFloat(L, 3, intens));
    dse_post_process_set_vignette_radius(e, OptFloat(L, 4, radius));
    dse_post_process_set_vignette_softness(e, OptFloat(L, 5, soft));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessFilmGrain(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int en = dse_post_process_get_film_grain_enabled(e);
    float intens = dse_post_process_get_film_grain_intensity(e);
    float ts = dse_post_process_get_film_grain_time_scale(e);

    dse_post_process_set_film_grain_enabled(e, OptBool(L, 2, en));
    dse_post_process_set_film_grain_intensity(e, OptFloat(L, 3, intens));
    dse_post_process_set_film_grain_time_scale(e, OptFloat(L, 4, ts));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessColorLut(lua_State* L) {
    uint32_t e = Ent(L, 1);
    const char* path = luaL_optstring(L, 2, nullptr);
    float intensity = helper::OptFloat(L, 3, 1.0f);
    dse_post_process_set_color_lut(e, path, intensity);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessOutline(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int en = dse_post_process_get_outline_enabled(e);
    float cr = 0, cg = 0, cb = 0;
    dse_post_process_get_outline_color(e, &cr, &cg, &cb);
    float thick = dse_post_process_get_outline_thickness(e);
    float depth = dse_post_process_get_outline_depth_threshold(e);
    float norm = dse_post_process_get_outline_normal_threshold(e);

    dse_post_process_set_outline_enabled(e, OptBool(L, 2, en));
    dse_post_process_set_outline_color(e,
        OptFloat(L, 3, cr), OptFloat(L, 4, cg), OptFloat(L, 5, cb));
    dse_post_process_set_outline_thickness(e, OptFloat(L, 6, thick));
    dse_post_process_set_outline_depth_threshold(e, OptFloat(L, 7, depth));
    dse_post_process_set_outline_normal_threshold(e, OptFloat(L, 8, norm));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessFog(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int en = dse_post_process_get_fog_enabled(e);
    float dens = dse_post_process_get_fog_density(e);
    float falloff = dse_post_process_get_fog_height_falloff(e);
    float offset = dse_post_process_get_fog_height_offset(e);
    float start = dse_post_process_get_fog_start(e);
    float end = dse_post_process_get_fog_end(e);
    int steps = dse_post_process_get_fog_steps(e);
    float scatter = dse_post_process_get_fog_sun_scatter(e);
    float fr = 0, fg = 0, fb = 0;
    dse_post_process_get_fog_color(e, &fr, &fg, &fb);

    dse_post_process_set_fog_enabled(e, OptBool(L, 2, en));
    dse_post_process_set_fog_density(e, OptFloat(L, 3, dens));
    dse_post_process_set_fog_height_falloff(e, OptFloat(L, 4, falloff));
    dse_post_process_set_fog_height_offset(e, OptFloat(L, 5, offset));
    dse_post_process_set_fog_start(e, OptFloat(L, 6, start));
    dse_post_process_set_fog_end(e, OptFloat(L, 7, end));
    if (!lua_isnoneornil(L, 8)) dse_post_process_set_fog_steps(e, static_cast<int>(luaL_checknumber(L, 8)));
    else dse_post_process_set_fog_steps(e, steps);
    dse_post_process_set_fog_sun_scatter(e, OptFloat(L, 9, scatter));
    dse_post_process_set_fog_color(e,
        OptFloat(L, 10, fr), OptFloat(L, 11, fg), OptFloat(L, 12, fb));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessLightShaft(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int en = dse_post_process_get_light_shaft_enabled(e);
    float dens = dse_post_process_get_light_shaft_density(e);
    float weight = dse_post_process_get_light_shaft_weight(e);
    float decay = dse_post_process_get_light_shaft_decay(e);
    float exposure = dse_post_process_get_light_shaft_exposure(e);
    float intens = dse_post_process_get_light_shaft_intensity(e);
    int samples = dse_post_process_get_light_shaft_samples(e);
    float cr = 0, cg = 0, cb = 0;
    dse_post_process_get_light_shaft_color(e, &cr, &cg, &cb);

    dse_post_process_set_light_shaft_enabled(e, OptBool(L, 2, en));
    dse_post_process_set_light_shaft_density(e, OptFloat(L, 3, dens));
    dse_post_process_set_light_shaft_weight(e, OptFloat(L, 4, weight));
    dse_post_process_set_light_shaft_decay(e, OptFloat(L, 5, decay));
    dse_post_process_set_light_shaft_exposure(e, OptFloat(L, 6, exposure));
    dse_post_process_set_light_shaft_intensity(e, OptFloat(L, 7, intens));
    if (!lua_isnoneornil(L, 8)) dse_post_process_set_light_shaft_samples(e, static_cast<int>(luaL_checknumber(L, 8)));
    else dse_post_process_set_light_shaft_samples(e, samples);
    dse_post_process_set_light_shaft_color(e,
        OptFloat(L, 9, cr), OptFloat(L, 10, cg), OptFloat(L, 11, cb));
    lua_pushboolean(L, 1);
    return 1;
}

// ============================================================
// Decal
// ============================================================

int L_EcsAddDecal(lua_State* L) {
    uint32_t e = Ent(L, 1);
    dse_decal_add_simple(e);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetDecal(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int has_tex = lua_isnoneornil(L, 3) ? 0 : 1;
    uint32_t tex = has_tex ? static_cast<uint32_t>(luaL_checknumber(L, 3)) : 0;
    // Read current values for "keep current" semantics
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f, angle_fade = 0.0f;
    // C ABI decal_set_full takes all values directly; use OptFloat with defaults
    int enabled = lua_isnoneornil(L, 2) ? 1 : (lua_toboolean(L, 2) ? 1 : 0);
    r = OptFloat(L, 4, r);
    g = OptFloat(L, 5, g);
    b = OptFloat(L, 6, b);
    a = OptFloat(L, 7, a);
    angle_fade = OptFloat(L, 8, angle_fade);
    dse_decal_set_full(e, enabled, has_tex, tex, r, g, b, a, angle_fade);
    lua_pushboolean(L, 1);
    return 1;
}

// ============================================================
// PostProcess state query
// ============================================================

int L_EcsGetPostProcessState(lua_State* L) {
    uint32_t e = Ent(L, 1);
    lua_pushboolean(L, 1);
    helper::PushBool(L, dse_post_process_get_enabled(e));
    helper::PushBool(L, dse_post_process_get_bloom_enabled(e));
    helper::PushFloat(L, dse_post_process_get_bloom_threshold(e));
    helper::PushFloat(L, dse_post_process_get_bloom_intensity(e));
    helper::PushBool(L, dse_post_process_get_color_grading_enabled(e));
    helper::PushFloat(L, dse_post_process_get_exposure(e));
    helper::PushFloat(L, dse_post_process_get_gamma(e));
    helper::PushBool(L, dse_post_process_get_ssao_enabled(e));
    helper::PushFloat(L, dse_post_process_get_ssao_radius(e));
    helper::PushFloat(L, dse_post_process_get_ssao_bias(e));
    helper::PushBool(L, dse_post_process_get_fxaa_enabled(e));
    helper::PushBool(L, dse_post_process_get_vignette_enabled(e));
    helper::PushFloat(L, dse_post_process_get_vignette_intensity(e));
    helper::PushFloat(L, dse_post_process_get_vignette_radius(e));
    helper::PushFloat(L, dse_post_process_get_vignette_softness(e));
    helper::PushBool(L, dse_post_process_get_film_grain_enabled(e));
    helper::PushFloat(L, dse_post_process_get_film_grain_intensity(e));
    helper::PushFloat(L, dse_post_process_get_film_grain_time_scale(e));
    return 19;
}

} // namespace

void RegisterEcsRenderingPostBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_post_process",          L_EcsAddPostProcess},
        {"set_post_process_bloom",    L_EcsSetPostProcessBloom},
        {"set_post_process_color",    L_EcsSetPostProcessColor},
        {"set_post_process_ssao",     L_EcsSetPostProcessSSAO},
        {"set_post_process_ssr",      L_EcsSetPostProcessSSR},
        {"set_post_process_fxaa",     L_EcsSetPostProcessFXAA},
        {"set_post_process_auto_exposure", L_EcsSetPostProcessAutoExposure},
        {"set_post_process_vignette", L_EcsSetPostProcessVignette},
        {"set_post_process_film_grain", L_EcsSetPostProcessFilmGrain},
        {"set_post_process_color_lut", L_EcsSetPostProcessColorLut},
        {"set_post_process_outline",  L_EcsSetPostProcessOutline},
        {"set_post_process_fog",      L_EcsSetPostProcessFog},
        {"set_post_process_light_shaft", L_EcsSetPostProcessLightShaft},
        {"add_decal",                 L_EcsAddDecal},
        {"set_decal",                 L_EcsSetDecal},
        {"get_post_process_state",    L_EcsGetPostProcessState},
    });
}

} // namespace dse::runtime::lua_binding
