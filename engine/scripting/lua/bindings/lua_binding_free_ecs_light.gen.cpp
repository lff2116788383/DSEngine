/**
 * @file lua_binding_free_ecs_light.gen.cpp
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

int L_dse_rendering_add_skybox(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* cubemap_path = luaL_checkstring(L, 2);
    dse_rendering_add_skybox(e, cubemap_path);
    return 0;
}

int L_dse_rendering_set_gi_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rendering_set_gi_probe_enabled(e, enabled);
    return 0;
}

int L_dse_rendering_set_light_probe_ex(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float influence_radius = static_cast<float>(luaL_checknumber(L, 2));
    int needs_rebake = static_cast<int>(luaL_checkinteger(L, 3));
    dse_rendering_set_light_probe_ex(e, influence_radius, needs_rebake);
    return 0;
}

int L_dse_rendering_set_light_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rendering_set_light_probe_enabled(e, enabled);
    return 0;
}

int L_dse_rendering_set_reflection_probe_ex(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float influence_radius = static_cast<float>(luaL_checknumber(L, 2));
    float box_x = static_cast<float>(luaL_checknumber(L, 3));
    float box_y = static_cast<float>(luaL_checknumber(L, 4));
    float box_z = static_cast<float>(luaL_checknumber(L, 5));
    int resolution = static_cast<int>(luaL_checkinteger(L, 6));
    dse_rendering_set_reflection_probe_ex(e, influence_radius, box_x, box_y, box_z, resolution);
    return 0;
}

int L_dse_rendering_set_reflection_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rendering_set_reflection_probe_enabled(e, enabled);
    return 0;
}

int L_add_directional_light_3d(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dx = static_cast<float>(luaL_checknumber(L, 2));
    float dy = static_cast<float>(luaL_checknumber(L, 3));
    float dz = static_cast<float>(luaL_checknumber(L, 4));
    float r = static_cast<float>(luaL_checknumber(L, 5));
    float g = static_cast<float>(luaL_checknumber(L, 6));
    float b = static_cast<float>(luaL_checknumber(L, 7));
    float intensity = static_cast<float>(luaL_checknumber(L, 8));
    float ambient = static_cast<float>(luaL_checknumber(L, 9));
    float shadow_strength = static_cast<float>(luaL_checknumber(L, 10));
    dse_dir_light_add(e);
    dse_dir_light_set_direction(e, dx, dy, dz);
    dse_dir_light_set_color(e, r, g, b);
    dse_dir_light_set_intensity(e, intensity);
    dse_dir_light_set_ambient_intensity(e, ambient);
    dse_dir_light_set_shadow_strength(e, shadow_strength);
    return 0;
}

int L_dse_compat_set_directional_light_3d(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float dx = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    float dy = lua_isnoneornil(L, 4) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 4));
    float dz = lua_isnoneornil(L, 5) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 5));
    float r = lua_isnoneornil(L, 6) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 6));
    float g = lua_isnoneornil(L, 7) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 7));
    float b = lua_isnoneornil(L, 8) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 8));
    float intensity = lua_isnoneornil(L, 9) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 9));
    float ambient = lua_isnoneornil(L, 10) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 10));
    float shadow_strength = lua_isnoneornil(L, 11) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 11));
    dse_compat_set_directional_light_3d(e, enabled, dx, dy, dz, r, g, b, intensity, ambient, shadow_strength);
    return 0;
}

int L_dse_dir_light_set_shadow_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float strength = static_cast<float>(luaL_checknumber(L, 3));
    float c0 = static_cast<float>(luaL_checknumber(L, 4));
    float c1 = static_cast<float>(luaL_checknumber(L, 5));
    float c2 = static_cast<float>(luaL_checknumber(L, 6));
    float lambda = static_cast<float>(luaL_checknumber(L, 7));
    dse_dir_light_set_shadow_params(e, enabled, strength, c0, c1, c2, lambda);
    return 0;
}

int L_add_point_light_3d(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float intensity = static_cast<float>(luaL_checknumber(L, 5));
    float radius = static_cast<float>(luaL_checknumber(L, 6));
    dse_point_light_add(e);
    dse_point_light_set_color(e, r, g, b);
    dse_point_light_set_intensity(e, intensity);
    dse_point_light_set_radius(e, radius);
    return 0;
}

int L_dse_compat_set_point_light_3d(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = lua_isnoneornil(L, 2) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 2));
    float g = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    float b = lua_isnoneornil(L, 4) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 4));
    float intensity = lua_isnoneornil(L, 5) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 5));
    float radius = lua_isnoneornil(L, 6) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 6));
    dse_compat_set_point_light_3d(e, r, g, b, intensity, radius);
    return 0;
}

int L_dse_point_light_set_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    dse_point_light_set_cast_shadow(e, enabled);
    return 0;
}

int L_add_spot_light_3d(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dx = static_cast<float>(luaL_checknumber(L, 2));
    float dy = static_cast<float>(luaL_checknumber(L, 3));
    float dz = static_cast<float>(luaL_checknumber(L, 4));
    float r = static_cast<float>(luaL_checknumber(L, 5));
    float g = static_cast<float>(luaL_checknumber(L, 6));
    float b = static_cast<float>(luaL_checknumber(L, 7));
    float intensity = static_cast<float>(luaL_checknumber(L, 8));
    float radius = static_cast<float>(luaL_checknumber(L, 9));
    float inner = static_cast<float>(luaL_checknumber(L, 10));
    float outer = static_cast<float>(luaL_checknumber(L, 11));
    dse_spot_light_add(e);
    dse_spot_light_set_direction(e, dx, dy, dz);
    dse_spot_light_set_color(e, r, g, b);
    dse_spot_light_set_intensity(e, intensity);
    dse_spot_light_set_radius(e, radius);
    dse_spot_light_set_inner_cone_angle(e, inner);
    dse_spot_light_set_outer_cone_angle(e, outer);
    return 0;
}

int L_dse_compat_set_spot_light_3d(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dx = lua_isnoneornil(L, 2) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 2));
    float dy = lua_isnoneornil(L, 3) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 3));
    float dz = lua_isnoneornil(L, 4) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 4));
    float r = lua_isnoneornil(L, 5) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 5));
    float g = lua_isnoneornil(L, 6) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 6));
    float b = lua_isnoneornil(L, 7) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 7));
    float intensity = lua_isnoneornil(L, 8) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 8));
    float radius = lua_isnoneornil(L, 9) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 9));
    float inner = lua_isnoneornil(L, 10) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 10));
    float outer = lua_isnoneornil(L, 11) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, 11));
    dse_compat_set_spot_light_3d(e, dx, dy, dz, r, g, b, intensity, radius, inner, outer);
    return 0;
}

int L_dse_spot_light_set_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    dse_spot_light_set_cast_shadow(e, enabled);
    return 0;
}

} // namespace

void RegisterEcsRenderingLightBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_skybox", L_dse_rendering_add_skybox},
        {"set_gi_probe_enabled", L_dse_rendering_set_gi_probe_enabled},
        {"set_light_probe", L_dse_rendering_set_light_probe_ex},
        {"set_light_probe_enabled", L_dse_rendering_set_light_probe_enabled},
        {"set_reflection_probe", L_dse_rendering_set_reflection_probe_ex},
        {"set_reflection_probe_enabled", L_dse_rendering_set_reflection_probe_enabled},
        {"add_directional_light_3d", L_add_directional_light_3d},
        {"set_directional_light_3d", L_dse_compat_set_directional_light_3d},
        {"set_directional_light_shadow", L_dse_dir_light_set_shadow_params},
        {"add_point_light_3d", L_add_point_light_3d},
        {"set_point_light_3d", L_dse_compat_set_point_light_3d},
        {"set_point_light_shadow", L_dse_point_light_set_cast_shadow},
        {"add_spot_light_3d", L_add_spot_light_3d},
        {"set_spot_light_3d", L_dse_compat_set_spot_light_3d},
        {"set_spot_light_shadow", L_dse_spot_light_set_cast_shadow},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
