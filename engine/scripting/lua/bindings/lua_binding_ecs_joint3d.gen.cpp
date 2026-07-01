/**
 * @file lua_binding_ecs_joint3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * Joint3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_joint3d_type(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_joint3d_get_type(e));
    return 1;
}
int L_Set_joint3d_type(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_type(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_joint3d_connected_entity_id(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_joint3d_get_connected_entity_id(e));
    return 1;
}
int L_Set_joint3d_connected_entity_id(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_connected_entity_id(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_joint3d_anchor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_joint3d_get_anchor(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_joint3d_anchor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_anchor(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_joint3d_connected_anchor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_joint3d_get_connected_anchor(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_joint3d_connected_anchor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_connected_anchor(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_joint3d_axis(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_joint3d_get_axis(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_joint3d_axis(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_axis(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_joint3d_use_limits(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_joint3d_get_use_limits(e));
    return 1;
}
int L_Set_joint3d_use_limits(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_use_limits(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_joint3d_lower_limit(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_joint3d_get_lower_limit(e));
    return 1;
}
int L_Set_joint3d_lower_limit(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_lower_limit(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_joint3d_upper_limit(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_joint3d_get_upper_limit(e));
    return 1;
}
int L_Set_joint3d_upper_limit(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_upper_limit(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_joint3d_min_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_joint3d_get_min_distance(e));
    return 1;
}
int L_Set_joint3d_min_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_min_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_joint3d_max_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_joint3d_get_max_distance(e));
    return 1;
}
int L_Set_joint3d_max_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_max_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_joint3d_spring_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_joint3d_get_spring_stiffness(e));
    return 1;
}
int L_Set_joint3d_spring_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_spring_stiffness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_joint3d_spring_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_joint3d_get_spring_damping(e));
    return 1;
}
int L_Set_joint3d_spring_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_spring_damping(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_joint3d_break_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_joint3d_get_break_force(e));
    return 1;
}
int L_Set_joint3d_break_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_break_force(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_joint3d_break_torque(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_joint3d_get_break_torque(e));
    return 1;
}
int L_Set_joint3d_break_torque(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_joint3d_set_break_torque(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterJoint3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_joint3d_type", L_Get_joint3d_type},
        {"set_joint3d_type", L_Set_joint3d_type},
        {"get_joint3d_connected_entity_id", L_Get_joint3d_connected_entity_id},
        {"set_joint3d_connected_entity_id", L_Set_joint3d_connected_entity_id},
        {"get_joint3d_anchor", L_Get_joint3d_anchor},
        {"set_joint3d_anchor", L_Set_joint3d_anchor},
        {"get_joint3d_connected_anchor", L_Get_joint3d_connected_anchor},
        {"set_joint3d_connected_anchor", L_Set_joint3d_connected_anchor},
        {"get_joint3d_axis", L_Get_joint3d_axis},
        {"set_joint3d_axis", L_Set_joint3d_axis},
        {"get_joint3d_use_limits", L_Get_joint3d_use_limits},
        {"set_joint3d_use_limits", L_Set_joint3d_use_limits},
        {"get_joint3d_lower_limit", L_Get_joint3d_lower_limit},
        {"set_joint3d_lower_limit", L_Set_joint3d_lower_limit},
        {"get_joint3d_upper_limit", L_Get_joint3d_upper_limit},
        {"set_joint3d_upper_limit", L_Set_joint3d_upper_limit},
        {"get_joint3d_min_distance", L_Get_joint3d_min_distance},
        {"set_joint3d_min_distance", L_Set_joint3d_min_distance},
        {"get_joint3d_max_distance", L_Get_joint3d_max_distance},
        {"set_joint3d_max_distance", L_Set_joint3d_max_distance},
        {"get_joint3d_spring_stiffness", L_Get_joint3d_spring_stiffness},
        {"set_joint3d_spring_stiffness", L_Set_joint3d_spring_stiffness},
        {"get_joint3d_spring_damping", L_Get_joint3d_spring_damping},
        {"set_joint3d_spring_damping", L_Set_joint3d_spring_damping},
        {"get_joint3d_break_force", L_Get_joint3d_break_force},
        {"set_joint3d_break_force", L_Set_joint3d_break_force},
        {"get_joint3d_break_torque", L_Get_joint3d_break_torque},
        {"set_joint3d_break_torque", L_Set_joint3d_break_torque},
    });
}

} // namespace dse::runtime::lua_binding
