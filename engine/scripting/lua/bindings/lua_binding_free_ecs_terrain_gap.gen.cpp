/**
 * @file lua_binding_free_ecs_terrain_gap.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_terrain_get_height(lua_State* L) {
    float out_y = 0;
    float world_x = static_cast<float>(luaL_checknumber(L, 1));
    float world_z = static_cast<float>(luaL_checknumber(L, 2));
    int _ret = dse_terrain_get_height(world_x, world_z, &out_y);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_y);
    return 2;
}

int L_dse_terrain_get_lod(lua_State* L) {
    int out_lod = 0;
    int out_rx = 0;
    int out_rz = 0;
    int out_max_lod = 0;
    float out_lod_factor = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_get_lod(e, &out_lod, &out_rx, &out_rz, &out_max_lod, &out_lod_factor);
    lua_pushinteger(L, out_lod);
    lua_pushinteger(L, out_rx);
    lua_pushinteger(L, out_rz);
    lua_pushinteger(L, out_max_lod);
    lua_pushnumber(L, out_lod_factor);
    return 5;
}

int L_dse_terrain_load_heightmap(lua_State* L) {
    int out_w = 0;
    int out_h = 0;
    int out_ch = 0;
    int out_rx = 0;
    int out_rz = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int _ret = dse_terrain_load_heightmap(e, path, &out_w, &out_h, &out_ch, &out_rx, &out_rz);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_w);
    lua_pushinteger(L, out_h);
    lua_pushinteger(L, out_ch);
    lua_pushinteger(L, out_rx);
    lua_pushinteger(L, out_rz);
    return 6;
}

int L_dse_terrain_set_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int res_x = static_cast<int>(luaL_checkinteger(L, 2));
    int res_z = static_cast<int>(luaL_checkinteger(L, 3));
    int max_lod = static_cast<int>(luaL_checkinteger(L, 4));
    float lod_factor = static_cast<float>(luaL_checknumber(L, 5));
    int use_dynamic_lod = static_cast<int>(luaL_checkinteger(L, 6));
    dse_terrain_set_params(e, res_x, res_z, max_lod, lod_factor, use_dynamic_lod);
    return 0;
}

int L_dse_terrain_set_texture(lua_State* L) {
    uint32_t out_handle = 0;
    int out_w = 0;
    int out_h = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int _ret = dse_terrain_set_texture(e, path, &out_handle, &out_w, &out_h);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_handle));
    lua_pushinteger(L, out_w);
    lua_pushinteger(L, out_h);
    return 4;
}

int L_dse_water_get(lua_State* L) {
    int out_enabled = 0;
    float out_water_level[3] = {0, 0, 0};
    float out_deep_rgb[4] = {0, 0, 0, 0};
    float out_shallow_rgb[4] = {0, 0, 0, 0};
    float out_max_depth = 0;
    float out_transparency = 0;
    float out_wave = 0;
    float out_wdir[3] = {0, 0, 0};
    float out_refraction = 0;
    float out_reflection = 0;
    float out_spec_power = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_water_get(e, &out_enabled, out_water_level, out_deep_rgb, out_shallow_rgb, &out_max_depth, &out_transparency, &out_wave, out_wdir, &out_refraction, &out_reflection, &out_spec_power);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_enabled);
    lua_pushnumber(L, out_water_level[0]);
    lua_pushnumber(L, out_water_level[1]);
    lua_pushnumber(L, out_water_level[2]);
    lua_pushnumber(L, out_deep_rgb[0]);
    lua_pushnumber(L, out_deep_rgb[1]);
    lua_pushnumber(L, out_deep_rgb[2]);
    lua_pushnumber(L, out_deep_rgb[3]);
    lua_pushnumber(L, out_shallow_rgb[0]);
    lua_pushnumber(L, out_shallow_rgb[1]);
    lua_pushnumber(L, out_shallow_rgb[2]);
    lua_pushnumber(L, out_shallow_rgb[3]);
    lua_pushnumber(L, out_max_depth);
    lua_pushnumber(L, out_transparency);
    lua_pushnumber(L, out_wave);
    lua_pushnumber(L, out_wdir[0]);
    lua_pushnumber(L, out_wdir[1]);
    lua_pushnumber(L, out_wdir[2]);
    lua_pushnumber(L, out_refraction);
    lua_pushnumber(L, out_reflection);
    lua_pushnumber(L, out_spec_power);
    return 22;
}

int L_dse_water_set(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    float water_level = static_cast<float>(luaL_checknumber(L, 3));
    float dr = static_cast<float>(luaL_checknumber(L, 4));
    float dg = static_cast<float>(luaL_checknumber(L, 5));
    float db = static_cast<float>(luaL_checknumber(L, 6));
    float sr = static_cast<float>(luaL_checknumber(L, 7));
    float sg = static_cast<float>(luaL_checknumber(L, 8));
    float sb = static_cast<float>(luaL_checknumber(L, 9));
    float max_depth = static_cast<float>(luaL_checknumber(L, 10));
    float transparency = static_cast<float>(luaL_checknumber(L, 11));
    float wave_amp = static_cast<float>(luaL_checknumber(L, 12));
    float wave_freq = static_cast<float>(luaL_checknumber(L, 13));
    float wave_speed = static_cast<float>(luaL_checknumber(L, 14));
    float wdir_x = static_cast<float>(luaL_checknumber(L, 15));
    float wdir_y = static_cast<float>(luaL_checknumber(L, 16));
    float refraction = static_cast<float>(luaL_checknumber(L, 17));
    float reflection = static_cast<float>(luaL_checknumber(L, 18));
    float spec_power = static_cast<float>(luaL_checknumber(L, 19));
    float caustic_int = static_cast<float>(luaL_checknumber(L, 20));
    float caustic_scale = static_cast<float>(luaL_checknumber(L, 21));
    float foam_int = static_cast<float>(luaL_checknumber(L, 22));
    float foam_threshold = static_cast<float>(luaL_checknumber(L, 23));
    float ufog_density = static_cast<float>(luaL_checknumber(L, 24));
    float ufog_r = static_cast<float>(luaL_checknumber(L, 25));
    float ufog_g = static_cast<float>(luaL_checknumber(L, 26));
    float ufog_b = static_cast<float>(luaL_checknumber(L, 27));
    dse_water_set(e, enabled, water_level, dr, dg, db, sr, sg, sb, max_depth, transparency, wave_amp, wave_freq, wave_speed, wdir_x, wdir_y, refraction, reflection, spec_power, caustic_int, caustic_scale, foam_int, foam_threshold, ufog_density, ufog_r, ufog_g, ufog_b);
    return 0;
}

} // namespace

void RegisterFreeTerrainGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"terrain_get_height", L_dse_terrain_get_height},
        {"terrain_get_lod", L_dse_terrain_get_lod},
        {"terrain_load_heightmap", L_dse_terrain_load_heightmap},
        {"terrain_set_params", L_dse_terrain_set_params},
        {"terrain_set_texture", L_dse_terrain_set_texture},
        {"water_get", L_dse_water_get},
        {"water_set", L_dse_water_set},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
