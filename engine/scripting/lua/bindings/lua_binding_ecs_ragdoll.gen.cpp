/**
 * @file lua_binding_ecs_ragdoll.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * RagdollComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_ragdoll_active(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_ragdoll_get_active(e));
    return 1;
}
int L_Set_ragdoll_active(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_set_active(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_ragdoll_auto_setup(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_ragdoll_get_auto_setup(e));
    return 1;
}
int L_Set_ragdoll_auto_setup(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_set_auto_setup(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_ragdoll_total_mass(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_ragdoll_get_total_mass(e));
    return 1;
}
int L_Set_ragdoll_total_mass(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_set_total_mass(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_ragdoll_joint_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_ragdoll_get_joint_stiffness(e));
    return 1;
}
int L_Set_ragdoll_joint_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_set_joint_stiffness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_ragdoll_joint_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_ragdoll_get_joint_damping(e));
    return 1;
}
int L_Set_ragdoll_joint_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_set_joint_damping(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_ragdoll_collision_layer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_ragdoll_get_collision_layer(e));
    return 1;
}
int L_Set_ragdoll_collision_layer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_set_collision_layer(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_ragdoll_collision_mask(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_ragdoll_get_collision_mask(e));
    return 1;
}
int L_Set_ragdoll_collision_mask(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ragdoll_set_collision_mask(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

} // namespace

void RegisterRagdollComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_ragdoll_active", L_Get_ragdoll_active},
        {"set_ragdoll_active", L_Set_ragdoll_active},
        {"get_ragdoll_auto_setup", L_Get_ragdoll_auto_setup},
        {"set_ragdoll_auto_setup", L_Set_ragdoll_auto_setup},
        {"get_ragdoll_total_mass", L_Get_ragdoll_total_mass},
        {"set_ragdoll_total_mass", L_Set_ragdoll_total_mass},
        {"get_ragdoll_joint_stiffness", L_Get_ragdoll_joint_stiffness},
        {"set_ragdoll_joint_stiffness", L_Set_ragdoll_joint_stiffness},
        {"get_ragdoll_joint_damping", L_Get_ragdoll_joint_damping},
        {"set_ragdoll_joint_damping", L_Set_ragdoll_joint_damping},
        {"get_ragdoll_collision_layer", L_Get_ragdoll_collision_layer},
        {"set_ragdoll_collision_layer", L_Set_ragdoll_collision_layer},
        {"get_ragdoll_collision_mask", L_Get_ragdoll_collision_mask},
        {"set_ragdoll_collision_mask", L_Set_ragdoll_collision_mask},
    });
}

} // namespace dse::runtime::lua_binding
