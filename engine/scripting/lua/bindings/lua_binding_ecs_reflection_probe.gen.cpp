/**
 * @file lua_binding_ecs_reflection_probe.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * ReflectionProbeComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_reflection_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_reflection_probe_get_enabled(e));
    return 1;
}
int L_Set_reflection_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_reflection_probe_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_reflection_probe_influence_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_reflection_probe_get_influence_radius(e));
    return 1;
}
int L_Set_reflection_probe_influence_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_reflection_probe_set_influence_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_reflection_probe_box_size_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_reflection_probe_get_box_size_x(e));
    return 1;
}
int L_Set_reflection_probe_box_size_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_reflection_probe_set_box_size_x(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_reflection_probe_box_size_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_reflection_probe_get_box_size_y(e));
    return 1;
}
int L_Set_reflection_probe_box_size_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_reflection_probe_set_box_size_y(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_reflection_probe_box_size_z(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_reflection_probe_get_box_size_z(e));
    return 1;
}
int L_Set_reflection_probe_box_size_z(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_reflection_probe_set_box_size_z(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_reflection_probe_use_box_projection(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_reflection_probe_get_use_box_projection(e));
    return 1;
}
int L_Set_reflection_probe_use_box_projection(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_reflection_probe_set_use_box_projection(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_reflection_probe_resolution(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_reflection_probe_get_resolution(e));
    return 1;
}
int L_Set_reflection_probe_resolution(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_reflection_probe_set_resolution(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_reflection_probe_show_debug(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_reflection_probe_get_show_debug(e));
    return 1;
}
int L_Set_reflection_probe_show_debug(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_reflection_probe_set_show_debug(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterReflectionProbeComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_reflection_probe_enabled", L_Get_reflection_probe_enabled},
        {"set_reflection_probe_enabled", L_Set_reflection_probe_enabled},
        {"get_reflection_probe_influence_radius", L_Get_reflection_probe_influence_radius},
        {"set_reflection_probe_influence_radius", L_Set_reflection_probe_influence_radius},
        {"get_reflection_probe_box_size_x", L_Get_reflection_probe_box_size_x},
        {"set_reflection_probe_box_size_x", L_Set_reflection_probe_box_size_x},
        {"get_reflection_probe_box_size_y", L_Get_reflection_probe_box_size_y},
        {"set_reflection_probe_box_size_y", L_Set_reflection_probe_box_size_y},
        {"get_reflection_probe_box_size_z", L_Get_reflection_probe_box_size_z},
        {"set_reflection_probe_box_size_z", L_Set_reflection_probe_box_size_z},
        {"get_reflection_probe_use_box_projection", L_Get_reflection_probe_use_box_projection},
        {"set_reflection_probe_use_box_projection", L_Set_reflection_probe_use_box_projection},
        {"get_reflection_probe_resolution", L_Get_reflection_probe_resolution},
        {"set_reflection_probe_resolution", L_Set_reflection_probe_resolution},
        {"get_reflection_probe_show_debug", L_Get_reflection_probe_show_debug},
        {"set_reflection_probe_show_debug", L_Set_reflection_probe_show_debug},
    });
}

} // namespace dse::runtime::lua_binding
