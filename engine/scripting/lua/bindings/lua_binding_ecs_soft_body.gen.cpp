/**
 * @file lua_binding_ecs_soft_body.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * SoftBodyComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_soft_body_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_soft_body_get_enabled(e));
    return 1;
}
int L_Set_soft_body_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_soft_body_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_soft_body_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_soft_body_get_stiffness(e));
    return 1;
}
int L_Set_soft_body_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_soft_body_set_stiffness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_soft_body_solver_iterations(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_soft_body_get_solver_iterations(e));
    return 1;
}
int L_Set_soft_body_solver_iterations(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_soft_body_set_solver_iterations(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_soft_body_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_soft_body_get_damping(e));
    return 1;
}
int L_Set_soft_body_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_soft_body_set_damping(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_soft_body_use_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_soft_body_get_use_gravity(e));
    return 1;
}
int L_Set_soft_body_use_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_soft_body_set_use_gravity(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_soft_body_gravity_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_soft_body_get_gravity_scale(e));
    return 1;
}
int L_Set_soft_body_gravity_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_soft_body_set_gravity_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_soft_body_volume_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_soft_body_get_volume_stiffness(e));
    return 1;
}
int L_Set_soft_body_volume_stiffness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_soft_body_set_volume_stiffness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterSoftBodyComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_soft_body_enabled", L_Get_soft_body_enabled},
        {"set_soft_body_enabled", L_Set_soft_body_enabled},
        {"get_soft_body_stiffness", L_Get_soft_body_stiffness},
        {"set_soft_body_stiffness", L_Set_soft_body_stiffness},
        {"get_soft_body_solver_iterations", L_Get_soft_body_solver_iterations},
        {"set_soft_body_solver_iterations", L_Set_soft_body_solver_iterations},
        {"get_soft_body_damping", L_Get_soft_body_damping},
        {"set_soft_body_damping", L_Set_soft_body_damping},
        {"get_soft_body_use_gravity", L_Get_soft_body_use_gravity},
        {"set_soft_body_use_gravity", L_Set_soft_body_use_gravity},
        {"get_soft_body_gravity_scale", L_Get_soft_body_gravity_scale},
        {"set_soft_body_gravity_scale", L_Set_soft_body_gravity_scale},
        {"get_soft_body_volume_stiffness", L_Get_soft_body_volume_stiffness},
        {"set_soft_body_volume_stiffness", L_Set_soft_body_volume_stiffness},
    });
}

} // namespace dse::runtime::lua_binding
