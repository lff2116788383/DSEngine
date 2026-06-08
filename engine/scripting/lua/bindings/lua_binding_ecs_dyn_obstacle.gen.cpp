/**
 * @file lua_binding_ecs_dyn_obstacle.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * DynamicObstacleComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_dyn_obstacle_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_dyn_obstacle_get_enabled(e));
    return 1;
}
int L_Set_dyn_obstacle_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dyn_obstacle_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_dyn_obstacle_shape(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_dyn_obstacle_get_shape(e));
    return 1;
}
int L_Set_dyn_obstacle_shape(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dyn_obstacle_set_shape(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_dyn_obstacle_box_extents(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_dyn_obstacle_get_box_extents(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_dyn_obstacle_box_extents(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dyn_obstacle_set_box_extents(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_dyn_obstacle_cylinder_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_dyn_obstacle_get_cylinder_radius(e));
    return 1;
}
int L_Set_dyn_obstacle_cylinder_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dyn_obstacle_set_cylinder_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_dyn_obstacle_cylinder_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_dyn_obstacle_get_cylinder_height(e));
    return 1;
}
int L_Set_dyn_obstacle_cylinder_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dyn_obstacle_set_cylinder_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterDynamicObstacleComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_dyn_obstacle_enabled", L_Get_dyn_obstacle_enabled},
        {"set_dyn_obstacle_enabled", L_Set_dyn_obstacle_enabled},
        {"get_dyn_obstacle_shape", L_Get_dyn_obstacle_shape},
        {"set_dyn_obstacle_shape", L_Set_dyn_obstacle_shape},
        {"get_dyn_obstacle_box_extents", L_Get_dyn_obstacle_box_extents},
        {"set_dyn_obstacle_box_extents", L_Set_dyn_obstacle_box_extents},
        {"get_dyn_obstacle_cylinder_radius", L_Get_dyn_obstacle_cylinder_radius},
        {"set_dyn_obstacle_cylinder_radius", L_Set_dyn_obstacle_cylinder_radius},
        {"get_dyn_obstacle_cylinder_height", L_Get_dyn_obstacle_cylinder_height},
        {"set_dyn_obstacle_cylinder_height", L_Set_dyn_obstacle_cylinder_height},
    });
}

} // namespace dse::runtime::lua_binding
