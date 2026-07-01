/**
 * @file lua_binding_ecs_gi_probe.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * GIProbeVolumeComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_gi_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_gi_probe_get_enabled(e));
    return 1;
}
int L_Set_gi_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_gi_probe_origin(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_gi_probe_get_origin(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_gi_probe_origin(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_origin(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_gi_probe_extent(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_gi_probe_get_extent(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_gi_probe_extent(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_extent(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_gi_probe_resolution_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_gi_probe_get_resolution_x(e));
    return 1;
}
int L_Set_gi_probe_resolution_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_resolution_x(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_gi_probe_resolution_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_gi_probe_get_resolution_y(e));
    return 1;
}
int L_Set_gi_probe_resolution_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_resolution_y(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_gi_probe_resolution_z(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_gi_probe_get_resolution_z(e));
    return 1;
}
int L_Set_gi_probe_resolution_z(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_resolution_z(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_gi_probe_irradiance_texels(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_gi_probe_get_irradiance_texels(e));
    return 1;
}
int L_Set_gi_probe_irradiance_texels(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_irradiance_texels(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_gi_probe_visibility_texels(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_gi_probe_get_visibility_texels(e));
    return 1;
}
int L_Set_gi_probe_visibility_texels(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_visibility_texels(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_gi_probe_rays_per_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_gi_probe_get_rays_per_probe(e));
    return 1;
}
int L_Set_gi_probe_rays_per_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_rays_per_probe(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_gi_probe_hysteresis(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_gi_probe_get_hysteresis(e));
    return 1;
}
int L_Set_gi_probe_hysteresis(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_hysteresis(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_gi_probe_gi_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_gi_probe_get_gi_intensity(e));
    return 1;
}
int L_Set_gi_probe_gi_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_gi_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_gi_probe_normal_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_gi_probe_get_normal_bias(e));
    return 1;
}
int L_Set_gi_probe_normal_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_normal_bias(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_gi_probe_view_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_gi_probe_get_view_bias(e));
    return 1;
}
int L_Set_gi_probe_view_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_view_bias(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_gi_probe_show_debug_probes(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_gi_probe_get_show_debug_probes(e));
    return 1;
}
int L_Set_gi_probe_show_debug_probes(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gi_probe_set_show_debug_probes(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterGIProbeVolumeComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_gi_probe_enabled", L_Get_gi_probe_enabled},
        {"set_gi_probe_enabled", L_Set_gi_probe_enabled},
        {"get_gi_probe_origin", L_Get_gi_probe_origin},
        {"set_gi_probe_origin", L_Set_gi_probe_origin},
        {"get_gi_probe_extent", L_Get_gi_probe_extent},
        {"set_gi_probe_extent", L_Set_gi_probe_extent},
        {"get_gi_probe_resolution_x", L_Get_gi_probe_resolution_x},
        {"set_gi_probe_resolution_x", L_Set_gi_probe_resolution_x},
        {"get_gi_probe_resolution_y", L_Get_gi_probe_resolution_y},
        {"set_gi_probe_resolution_y", L_Set_gi_probe_resolution_y},
        {"get_gi_probe_resolution_z", L_Get_gi_probe_resolution_z},
        {"set_gi_probe_resolution_z", L_Set_gi_probe_resolution_z},
        {"get_gi_probe_irradiance_texels", L_Get_gi_probe_irradiance_texels},
        {"set_gi_probe_irradiance_texels", L_Set_gi_probe_irradiance_texels},
        {"get_gi_probe_visibility_texels", L_Get_gi_probe_visibility_texels},
        {"set_gi_probe_visibility_texels", L_Set_gi_probe_visibility_texels},
        {"get_gi_probe_rays_per_probe", L_Get_gi_probe_rays_per_probe},
        {"set_gi_probe_rays_per_probe", L_Set_gi_probe_rays_per_probe},
        {"get_gi_probe_hysteresis", L_Get_gi_probe_hysteresis},
        {"set_gi_probe_hysteresis", L_Set_gi_probe_hysteresis},
        {"get_gi_probe_gi_intensity", L_Get_gi_probe_gi_intensity},
        {"set_gi_probe_gi_intensity", L_Set_gi_probe_gi_intensity},
        {"get_gi_probe_normal_bias", L_Get_gi_probe_normal_bias},
        {"set_gi_probe_normal_bias", L_Set_gi_probe_normal_bias},
        {"get_gi_probe_view_bias", L_Get_gi_probe_view_bias},
        {"set_gi_probe_view_bias", L_Set_gi_probe_view_bias},
        {"get_gi_probe_show_debug_probes", L_Get_gi_probe_show_debug_probes},
        {"set_gi_probe_show_debug_probes", L_Set_gi_probe_show_debug_probes},
    });
}

} // namespace dse::runtime::lua_binding
