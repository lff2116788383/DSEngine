/**
 * @file lua_binding_free_ecs_light_gap.gen.cpp
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

int L_dse_dir_light_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_add(e);
    return 0;
}

int L_dse_dir_light_get_shadow_params(lua_State* L) {
    int out_cast_shadow = 0;
    float out_strength = 0;
    float out_c0 = 0;
    float out_c1 = 0;
    float out_c2 = 0;
    float out_lambda = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_dir_light_get_shadow_params(e, &out_cast_shadow, &out_strength, &out_c0, &out_c1, &out_c2, &out_lambda);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_cast_shadow);
    lua_pushnumber(L, out_strength);
    lua_pushnumber(L, out_c0);
    lua_pushnumber(L, out_c1);
    lua_pushnumber(L, out_c2);
    lua_pushnumber(L, out_lambda);
    return 7;
}

int L_dse_dir_light_has(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_dir_light_has(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_dir_light_set_shadow_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int cast_shadow = static_cast<int>(luaL_checkinteger(L, 2));
    float shadow_strength = static_cast<float>(luaL_checknumber(L, 3));
    float c0 = static_cast<float>(luaL_checknumber(L, 4));
    float c1 = static_cast<float>(luaL_checknumber(L, 5));
    float c2 = static_cast<float>(luaL_checknumber(L, 6));
    float lambda = static_cast<float>(luaL_checknumber(L, 7));
    dse_dir_light_set_shadow_params(e, cast_shadow, shadow_strength, c0, c1, c2, lambda);
    return 0;
}

int L_dse_point_light_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_point_light_add(e);
    return 0;
}

int L_dse_point_light_has(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_point_light_has(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_rendering_add_gi_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rendering_add_gi_probe(e);
    return 0;
}

int L_dse_rendering_add_light_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rendering_add_light_probe(e);
    return 0;
}

int L_dse_rendering_add_reflection_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rendering_add_reflection_probe(e);
    return 0;
}

int L_dse_rendering_get_gi_probe(lua_State* L) {
    float out_gi_intensity = 0;
    float out_origin[3] = {0, 0, 0};
    float out_extent[3] = {0, 0, 0};
    int out_resolution = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_rendering_get_gi_probe(e, &out_gi_intensity, out_origin, out_extent, &out_resolution);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_gi_intensity);
    lua_pushnumber(L, out_origin[0]);
    lua_pushnumber(L, out_origin[1]);
    lua_pushnumber(L, out_origin[2]);
    lua_pushnumber(L, out_extent[0]);
    lua_pushnumber(L, out_extent[1]);
    lua_pushnumber(L, out_extent[2]);
    lua_pushinteger(L, out_resolution);
    return 9;
}

int L_dse_rendering_get_gi_probe_ex(lua_State* L) {
    int out_enabled = 0;
    float out_normal_bias[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_rendering_get_gi_probe_ex(e, &out_enabled, out_normal_bias);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_enabled);
    lua_pushnumber(L, out_normal_bias[0]);
    lua_pushnumber(L, out_normal_bias[1]);
    lua_pushnumber(L, out_normal_bias[2]);
    return 5;
}

int L_dse_rendering_set_gi_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float gi_intensity = static_cast<float>(luaL_checknumber(L, 2));
    float ox = static_cast<float>(luaL_checknumber(L, 3));
    float oy = static_cast<float>(luaL_checknumber(L, 4));
    float oz = static_cast<float>(luaL_checknumber(L, 5));
    float ex = static_cast<float>(luaL_checknumber(L, 6));
    float ey = static_cast<float>(luaL_checknumber(L, 7));
    float ez = static_cast<float>(luaL_checknumber(L, 8));
    int res_x = static_cast<int>(luaL_checkinteger(L, 9));
    int res_y = static_cast<int>(luaL_checkinteger(L, 10));
    int res_z = static_cast<int>(luaL_checkinteger(L, 11));
    dse_rendering_set_gi_probe(e, gi_intensity, ox, oy, oz, ex, ey, ez, res_x, res_y, res_z);
    return 0;
}

int L_dse_rendering_set_gi_probe_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float normal_bias = static_cast<float>(luaL_checknumber(L, 2));
    float hysteresis = static_cast<float>(luaL_checknumber(L, 3));
    dse_rendering_set_gi_probe_bias(e, normal_bias, hysteresis);
    return 0;
}

int L_dse_sky_light_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_sky_light_add(e);
    return 0;
}

int L_dse_sky_light_has(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_sky_light_has(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_spot_light_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_add(e);
    return 0;
}

int L_dse_spot_light_has(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_spot_light_has(e);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeLightGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"dir_light_add", L_dse_dir_light_add},
        {"dir_light_get_shadow_params", L_dse_dir_light_get_shadow_params},
        {"dir_light_has", L_dse_dir_light_has},
        {"dir_light_set_shadow_params", L_dse_dir_light_set_shadow_params},
        {"point_light_add", L_dse_point_light_add},
        {"point_light_has", L_dse_point_light_has},
        {"rendering_add_gi_probe", L_dse_rendering_add_gi_probe},
        {"rendering_add_light_probe", L_dse_rendering_add_light_probe},
        {"rendering_add_reflection_probe", L_dse_rendering_add_reflection_probe},
        {"rendering_get_gi_probe", L_dse_rendering_get_gi_probe},
        {"rendering_get_gi_probe_ex", L_dse_rendering_get_gi_probe_ex},
        {"rendering_set_gi_probe", L_dse_rendering_set_gi_probe},
        {"rendering_set_gi_probe_bias", L_dse_rendering_set_gi_probe_bias},
        {"sky_light_add", L_dse_sky_light_add},
        {"sky_light_has", L_dse_sky_light_has},
        {"spot_light_add", L_dse_spot_light_add},
        {"spot_light_has", L_dse_spot_light_has},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
