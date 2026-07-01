/**
 * @file lua_binding_ecs_buoyancy.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * BuoyancyComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_buoyancy_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_buoyancy_get_enabled(e));
    return 1;
}
int L_Set_buoyancy_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_buoyancy_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_buoyancy_water_level(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_buoyancy_get_water_level(e));
    return 1;
}
int L_Set_buoyancy_water_level(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_buoyancy_set_water_level(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_buoyancy_use_fluid_system(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_buoyancy_get_use_fluid_system(e));
    return 1;
}
int L_Set_buoyancy_use_fluid_system(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_buoyancy_set_use_fluid_system(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_buoyancy_buoyancy_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_buoyancy_get_buoyancy_force(e));
    return 1;
}
int L_Set_buoyancy_buoyancy_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_buoyancy_set_buoyancy_force(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_buoyancy_water_drag(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_buoyancy_get_water_drag(e));
    return 1;
}
int L_Set_buoyancy_water_drag(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_buoyancy_set_water_drag(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_buoyancy_water_angular_drag(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_buoyancy_get_water_angular_drag(e));
    return 1;
}
int L_Set_buoyancy_water_angular_drag(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_buoyancy_set_water_angular_drag(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_buoyancy_submerge_depth(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_buoyancy_get_submerge_depth(e));
    return 1;
}
int L_Set_buoyancy_submerge_depth(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_buoyancy_set_submerge_depth(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterBuoyancyComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_buoyancy_enabled", L_Get_buoyancy_enabled},
        {"set_buoyancy_enabled", L_Set_buoyancy_enabled},
        {"get_buoyancy_water_level", L_Get_buoyancy_water_level},
        {"set_buoyancy_water_level", L_Set_buoyancy_water_level},
        {"get_buoyancy_use_fluid_system", L_Get_buoyancy_use_fluid_system},
        {"set_buoyancy_use_fluid_system", L_Set_buoyancy_use_fluid_system},
        {"get_buoyancy_buoyancy_force", L_Get_buoyancy_buoyancy_force},
        {"set_buoyancy_buoyancy_force", L_Set_buoyancy_buoyancy_force},
        {"get_buoyancy_water_drag", L_Get_buoyancy_water_drag},
        {"set_buoyancy_water_drag", L_Set_buoyancy_water_drag},
        {"get_buoyancy_water_angular_drag", L_Get_buoyancy_water_angular_drag},
        {"set_buoyancy_water_angular_drag", L_Set_buoyancy_water_angular_drag},
        {"get_buoyancy_submerge_depth", L_Get_buoyancy_submerge_depth},
        {"set_buoyancy_submerge_depth", L_Set_buoyancy_submerge_depth},
    });
}

} // namespace dse::runtime::lua_binding
