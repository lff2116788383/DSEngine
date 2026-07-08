/**
 * @file lua_binding_free_ecs_fx.gen.cpp
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

int L_dse_steering_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float max_velocity = static_cast<float>(luaL_checknumber(L, 2));
    float max_force = static_cast<float>(luaL_checknumber(L, 3));
    float mass = static_cast<float>(luaL_checknumber(L, 4));
    dse_steering_add(e, max_velocity, max_force, mass);
    return 0;
}

int L_dse_lod_add_level(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* mesh_path = luaL_checkstring(L, 2);
    float screen_size_threshold = static_cast<float>(luaL_checknumber(L, 3));
    dse_lod_add_level(e, mesh_path, screen_size_threshold);
    return 0;
}

int L_dse_lod_set_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float scale = static_cast<float>(luaL_checknumber(L, 2));
    dse_lod_set_scale(e, scale);
    return 0;
}

int L_dse_lod_set_min_screen_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_size = static_cast<float>(luaL_checknumber(L, 2));
    dse_lod_set_min_screen_size(e, min_size);
    return 0;
}

int L_dse_lod_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_lod_set_enabled(e, enabled);
    return 0;
}

int L_dse_hair_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* asset_path = luaL_checkstring(L, 2);
    int num_follow_per_guide = static_cast<int>(luaL_checkinteger(L, 3));
    dse_hair_add(e, asset_path, num_follow_per_guide);
    return 0;
}

int L_dse_hair_set_physics(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float damping = static_cast<float>(luaL_checknumber(L, 2));
    float stiffness_local = static_cast<float>(luaL_checknumber(L, 3));
    float stiffness_global = static_cast<float>(luaL_checknumber(L, 4));
    float gravity = static_cast<float>(luaL_checknumber(L, 5));
    dse_hair_set_physics(e, damping, stiffness_local, stiffness_global, gravity);
    return 0;
}

int L_dse_hair_set_render(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float root_r = static_cast<float>(luaL_checknumber(L, 2));
    float root_g = static_cast<float>(luaL_checknumber(L, 3));
    float root_b = static_cast<float>(luaL_checknumber(L, 4));
    float root_a = static_cast<float>(luaL_checknumber(L, 5));
    float tip_r = static_cast<float>(luaL_checknumber(L, 6));
    float tip_g = static_cast<float>(luaL_checknumber(L, 7));
    float tip_b = static_cast<float>(luaL_checknumber(L, 8));
    float tip_a = static_cast<float>(luaL_checknumber(L, 9));
    float fiber_radius = static_cast<float>(luaL_checknumber(L, 10));
    float opacity = static_cast<float>(luaL_checknumber(L, 11));
    dse_hair_set_render(e, root_r, root_g, root_b, root_a, tip_r, tip_g, tip_b, tip_a, fiber_radius, opacity);
    return 0;
}

int L_dse_hair_set_wind_full(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    float wz = static_cast<float>(luaL_checknumber(L, 4));
    float turbulence = static_cast<float>(luaL_checknumber(L, 5));
    dse_hair_set_wind_full(e, wx, wy, wz, turbulence);
    return 0;
}

int L_dse_hair_set_lod(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float lod0_distance = static_cast<float>(luaL_checknumber(L, 2));
    float lod1_distance = static_cast<float>(luaL_checknumber(L, 3));
    float lod2_distance = static_cast<float>(luaL_checknumber(L, 4));
    float cull_distance = static_cast<float>(luaL_checknumber(L, 5));
    dse_hair_set_lod(e, lod0_distance, lod1_distance, lod2_distance, cull_distance);
    return 0;
}

} // namespace

void RegisterEcsRenderingFxBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_steering", L_dse_steering_add},
        {"lod_add_level", L_dse_lod_add_level},
        {"lod_set_scale", L_dse_lod_set_scale},
        {"lod_set_min_screen_size", L_dse_lod_set_min_screen_size},
        {"lod_set_enabled", L_dse_lod_set_enabled},
        {"add_hair", L_dse_hair_add},
        {"set_hair_physics", L_dse_hair_set_physics},
        {"set_hair_render", L_dse_hair_set_render},
        {"set_hair_wind", L_dse_hair_set_wind_full},
        {"set_hair_lod", L_dse_hair_set_lod},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
