/**
 * @file lua_binding_free_ecs_post.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}
#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace dse::runtime::lua_binding {
namespace {

int L_dse_post_process_set_color_lut(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_optstring(L, 2, nullptr);
    float intensity = static_cast<float>(luaL_checknumber(L, 3));
    dse_post_process_set_color_lut(e, path, intensity);
    return 0;
}

int L_dse_decal_add_simple(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_decal_add_simple(e);
    return 0;
}

int L_dse_decal_set_full(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    int has_texture = static_cast<int>(luaL_checkinteger(L, 3));
    uint32_t texture = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    float r = static_cast<float>(luaL_checknumber(L, 5));
    float g = static_cast<float>(luaL_checknumber(L, 6));
    float b = static_cast<float>(luaL_checknumber(L, 7));
    float a = static_cast<float>(luaL_checknumber(L, 8));
    float angle_fade = static_cast<float>(luaL_checknumber(L, 9));
    dse_decal_set_full(e, enabled, has_texture, texture, r, g, b, a, angle_fade);
    return 0;
}

int L_set_post_process_fog(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float density = static_cast<float>(luaL_checknumber(L, 3));
    float height_falloff = static_cast<float>(luaL_checknumber(L, 4));
    float height_offset = static_cast<float>(luaL_checknumber(L, 5));
    float fog_start = static_cast<float>(luaL_checknumber(L, 6));
    float fog_end = static_cast<float>(luaL_checknumber(L, 7));
    int steps = static_cast<int>(luaL_checkinteger(L, 8));
    float sun_scatter = static_cast<float>(luaL_checknumber(L, 9));
    float fog_r = static_cast<float>(luaL_checknumber(L, 10));
    float fog_g = static_cast<float>(luaL_checknumber(L, 11));
    float fog_b = static_cast<float>(luaL_checknumber(L, 12));
    dse_post_process_set_fog_enabled(e, enabled);
    dse_post_process_set_fog_density(e, density);
    dse_post_process_set_fog_height_falloff(e, height_falloff);
    dse_post_process_set_fog_height_offset(e, height_offset);
    dse_post_process_set_fog_start(e, fog_start);
    dse_post_process_set_fog_end(e, fog_end);
    dse_post_process_set_fog_steps(e, steps);
    dse_post_process_set_fog_sun_scatter(e, sun_scatter);
    dse_post_process_set_fog_color(e, fog_r, fog_g, fog_b);
    return 0;
}

int L_set_post_process_light_shaft(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float density = static_cast<float>(luaL_checknumber(L, 3));
    float weight = static_cast<float>(luaL_checknumber(L, 4));
    float decay = static_cast<float>(luaL_checknumber(L, 5));
    float exposure = static_cast<float>(luaL_checknumber(L, 6));
    float intensity = static_cast<float>(luaL_checknumber(L, 7));
    int samples = static_cast<int>(luaL_checkinteger(L, 8));
    float color_r = static_cast<float>(luaL_checknumber(L, 9));
    float color_g = static_cast<float>(luaL_checknumber(L, 10));
    float color_b = static_cast<float>(luaL_checknumber(L, 11));
    dse_post_process_set_light_shaft_enabled(e, enabled);
    dse_post_process_set_light_shaft_density(e, density);
    dse_post_process_set_light_shaft_weight(e, weight);
    dse_post_process_set_light_shaft_decay(e, decay);
    dse_post_process_set_light_shaft_exposure(e, exposure);
    dse_post_process_set_light_shaft_intensity(e, intensity);
    dse_post_process_set_light_shaft_samples(e, samples);
    dse_post_process_set_light_shaft_color(e, color_r, color_g, color_b);
    return 0;
}

int L_set_post_process_ssao(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float radius = static_cast<float>(luaL_checknumber(L, 3));
    float bias = static_cast<float>(luaL_checknumber(L, 4));
    dse_post_process_set_ssao_enabled(e, enabled);
    dse_post_process_set_ssao_radius(e, radius);
    dse_post_process_set_ssao_bias(e, bias);
    return 0;
}

int L_set_post_process_auto_exposure(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float exp_min = static_cast<float>(luaL_checknumber(L, 3));
    float exp_max = static_cast<float>(luaL_checknumber(L, 4));
    float speed_up = static_cast<float>(luaL_checknumber(L, 5));
    float speed_down = static_cast<float>(luaL_checknumber(L, 6));
    float compensation = static_cast<float>(luaL_checknumber(L, 7));
    dse_post_process_set_auto_exposure_enabled(e, enabled);
    dse_post_process_set_exposure_min(e, exp_min);
    dse_post_process_set_exposure_max(e, exp_max);
    dse_post_process_set_adaptation_speed_up(e, speed_up);
    dse_post_process_set_adaptation_speed_down(e, speed_down);
    dse_post_process_set_exposure_compensation(e, compensation);
    return 0;
}

int L_set_post_process_film_grain(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float intensity = static_cast<float>(luaL_checknumber(L, 3));
    float time_scale = static_cast<float>(luaL_checknumber(L, 4));
    dse_post_process_set_film_grain_enabled(e, enabled);
    dse_post_process_set_film_grain_intensity(e, intensity);
    dse_post_process_set_film_grain_time_scale(e, time_scale);
    return 0;
}

int L_set_post_process_outline(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float r = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    float b = static_cast<float>(luaL_checknumber(L, 5));
    float thickness = static_cast<float>(luaL_checknumber(L, 6));
    float depth_threshold = static_cast<float>(luaL_checknumber(L, 7));
    float normal_threshold = static_cast<float>(luaL_checknumber(L, 8));
    dse_post_process_set_outline_enabled(e, enabled);
    dse_post_process_set_outline_color(e, r, g, b);
    dse_post_process_set_outline_thickness(e, thickness);
    dse_post_process_set_outline_depth_threshold(e, depth_threshold);
    dse_post_process_set_outline_normal_threshold(e, normal_threshold);
    return 0;
}

int L_set_post_process_vignette(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float intensity = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    float softness = static_cast<float>(luaL_checknumber(L, 5));
    dse_post_process_set_vignette_enabled(e, enabled);
    dse_post_process_set_vignette_intensity(e, intensity);
    dse_post_process_set_vignette_radius(e, radius);
    dse_post_process_set_vignette_softness(e, softness);
    return 0;
}

int L_set_post_process_bloom(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    int color_grading = helper::CheckBool(L, 3) ? 1 : 0;
    float threshold = static_cast<float>(luaL_checknumber(L, 4));
    float intensity = static_cast<float>(luaL_checknumber(L, 5));
    float exposure = static_cast<float>(luaL_checknumber(L, 6));
    dse_post_process_set_bloom_enabled(e, enabled);
    dse_post_process_set_color_grading_enabled(e, color_grading);
    dse_post_process_set_bloom_threshold(e, threshold);
    dse_post_process_set_bloom_intensity(e, intensity);
    dse_post_process_set_exposure(e, exposure);
    return 0;
}

int L_add_post_process(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float bloom_threshold = static_cast<float>(luaL_checknumber(L, 3));
    float bloom_intensity = static_cast<float>(luaL_checknumber(L, 4));
    float bloom_knee = static_cast<float>(luaL_checknumber(L, 5));
    dse_post_process_add(e);
    dse_post_process_set_enabled(e, enabled);
    dse_post_process_set_bloom_enabled(e, 1);
    dse_post_process_set_bloom_threshold(e, bloom_threshold);
    dse_post_process_set_bloom_intensity(e, bloom_intensity);
    dse_post_process_set_bloom_knee(e, bloom_knee);
    return 0;
}

} // namespace

void RegisterEcsRenderingPostBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"set_post_process_color_lut", L_dse_post_process_set_color_lut},
        {"add_decal", L_dse_decal_add_simple},
        {"set_decal", L_dse_decal_set_full},
        {"set_post_process_fog", L_set_post_process_fog},
        {"set_post_process_light_shaft", L_set_post_process_light_shaft},
        {"set_post_process_ssao", L_set_post_process_ssao},
        {"set_post_process_auto_exposure", L_set_post_process_auto_exposure},
        {"set_post_process_film_grain", L_set_post_process_film_grain},
        {"set_post_process_outline", L_set_post_process_outline},
        {"set_post_process_vignette", L_set_post_process_vignette},
        {"set_post_process_bloom", L_set_post_process_bloom},
        {"add_post_process", L_add_post_process},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
