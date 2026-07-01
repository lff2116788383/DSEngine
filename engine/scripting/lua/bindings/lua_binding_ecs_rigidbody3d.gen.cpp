/**
 * @file lua_binding_ecs_rigidbody3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * RigidBody3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_rigidbody3d_type(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_rigidbody3d_get_type(e));
    return 1;
}
int L_Set_rigidbody3d_type(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_type(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_rigidbody3d_mass(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_rigidbody3d_get_mass(e));
    return 1;
}
int L_Set_rigidbody3d_mass(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_mass(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_rigidbody3d_drag(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_rigidbody3d_get_drag(e));
    return 1;
}
int L_Set_rigidbody3d_drag(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_drag(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_rigidbody3d_angular_drag(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_rigidbody3d_get_angular_drag(e));
    return 1;
}
int L_Set_rigidbody3d_angular_drag(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_angular_drag(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_rigidbody3d_use_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_rigidbody3d_get_use_gravity(e));
    return 1;
}
int L_Set_rigidbody3d_use_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_use_gravity(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_rigidbody3d_gravity_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_rigidbody3d_get_gravity_scale(e));
    return 1;
}
int L_Set_rigidbody3d_gravity_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_gravity_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_rigidbody3d_is_kinematic(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_rigidbody3d_get_is_kinematic(e));
    return 1;
}
int L_Set_rigidbody3d_is_kinematic(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_is_kinematic(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_rigidbody3d_collision_layer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_rigidbody3d_get_collision_layer(e));
    return 1;
}
int L_Set_rigidbody3d_collision_layer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_collision_layer(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_rigidbody3d_collision_mask(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_rigidbody3d_get_collision_mask(e));
    return 1;
}
int L_Set_rigidbody3d_collision_mask(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_set_collision_mask(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

} // namespace

void RegisterRigidBody3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_rigidbody3d_type", L_Get_rigidbody3d_type},
        {"set_rigidbody3d_type", L_Set_rigidbody3d_type},
        {"get_rigidbody3d_mass", L_Get_rigidbody3d_mass},
        {"set_rigidbody3d_mass", L_Set_rigidbody3d_mass},
        {"get_rigidbody3d_drag", L_Get_rigidbody3d_drag},
        {"set_rigidbody3d_drag", L_Set_rigidbody3d_drag},
        {"get_rigidbody3d_angular_drag", L_Get_rigidbody3d_angular_drag},
        {"set_rigidbody3d_angular_drag", L_Set_rigidbody3d_angular_drag},
        {"get_rigidbody3d_use_gravity", L_Get_rigidbody3d_use_gravity},
        {"set_rigidbody3d_use_gravity", L_Set_rigidbody3d_use_gravity},
        {"get_rigidbody3d_gravity_scale", L_Get_rigidbody3d_gravity_scale},
        {"set_rigidbody3d_gravity_scale", L_Set_rigidbody3d_gravity_scale},
        {"get_rigidbody3d_is_kinematic", L_Get_rigidbody3d_is_kinematic},
        {"set_rigidbody3d_is_kinematic", L_Set_rigidbody3d_is_kinematic},
        {"get_rigidbody3d_collision_layer", L_Get_rigidbody3d_collision_layer},
        {"set_rigidbody3d_collision_layer", L_Set_rigidbody3d_collision_layer},
        {"get_rigidbody3d_collision_mask", L_Get_rigidbody3d_collision_mask},
        {"set_rigidbody3d_collision_mask", L_Set_rigidbody3d_collision_mask},
    });
}

} // namespace dse::runtime::lua_binding
