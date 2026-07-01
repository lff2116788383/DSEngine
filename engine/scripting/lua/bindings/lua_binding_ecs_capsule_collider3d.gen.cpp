/**
 * @file lua_binding_ecs_capsule_collider3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * CapsuleCollider3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_capsule_collider3d_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_capsule_collider3d_get_radius(e));
    return 1;
}
int L_Set_capsule_collider3d_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_capsule_collider3d_set_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_capsule_collider3d_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_capsule_collider3d_get_height(e));
    return 1;
}
int L_Set_capsule_collider3d_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_capsule_collider3d_set_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_capsule_collider3d_center(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_capsule_collider3d_get_center(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_capsule_collider3d_center(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_capsule_collider3d_set_center(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_capsule_collider3d_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_capsule_collider3d_get_direction(e));
    return 1;
}
int L_Set_capsule_collider3d_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_capsule_collider3d_set_direction(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_capsule_collider3d_is_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_capsule_collider3d_get_is_trigger(e));
    return 1;
}
int L_Set_capsule_collider3d_is_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_capsule_collider3d_set_is_trigger(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_capsule_collider3d_bounciness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_capsule_collider3d_get_bounciness(e));
    return 1;
}
int L_Set_capsule_collider3d_bounciness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_capsule_collider3d_set_bounciness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_capsule_collider3d_friction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_capsule_collider3d_get_friction(e));
    return 1;
}
int L_Set_capsule_collider3d_friction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_capsule_collider3d_set_friction(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterCapsuleCollider3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_capsule_collider3d_radius", L_Get_capsule_collider3d_radius},
        {"set_capsule_collider3d_radius", L_Set_capsule_collider3d_radius},
        {"get_capsule_collider3d_height", L_Get_capsule_collider3d_height},
        {"set_capsule_collider3d_height", L_Set_capsule_collider3d_height},
        {"get_capsule_collider3d_center", L_Get_capsule_collider3d_center},
        {"set_capsule_collider3d_center", L_Set_capsule_collider3d_center},
        {"get_capsule_collider3d_direction", L_Get_capsule_collider3d_direction},
        {"set_capsule_collider3d_direction", L_Set_capsule_collider3d_direction},
        {"get_capsule_collider3d_is_trigger", L_Get_capsule_collider3d_is_trigger},
        {"set_capsule_collider3d_is_trigger", L_Set_capsule_collider3d_is_trigger},
        {"get_capsule_collider3d_bounciness", L_Get_capsule_collider3d_bounciness},
        {"set_capsule_collider3d_bounciness", L_Set_capsule_collider3d_bounciness},
        {"get_capsule_collider3d_friction", L_Get_capsule_collider3d_friction},
        {"set_capsule_collider3d_friction", L_Set_capsule_collider3d_friction},
    });
}

} // namespace dse::runtime::lua_binding
