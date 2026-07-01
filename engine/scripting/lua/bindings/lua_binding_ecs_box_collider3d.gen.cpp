/**
 * @file lua_binding_ecs_box_collider3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * BoxCollider3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_box_collider3d_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_box_collider3d_get_size(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_box_collider3d_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_box_collider3d_set_size(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_box_collider3d_center(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_box_collider3d_get_center(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_box_collider3d_center(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_box_collider3d_set_center(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_box_collider3d_is_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_box_collider3d_get_is_trigger(e));
    return 1;
}
int L_Set_box_collider3d_is_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_box_collider3d_set_is_trigger(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_box_collider3d_bounciness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_box_collider3d_get_bounciness(e));
    return 1;
}
int L_Set_box_collider3d_bounciness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_box_collider3d_set_bounciness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_box_collider3d_friction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_box_collider3d_get_friction(e));
    return 1;
}
int L_Set_box_collider3d_friction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_box_collider3d_set_friction(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterBoxCollider3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_box_collider3d_size", L_Get_box_collider3d_size},
        {"set_box_collider3d_size", L_Set_box_collider3d_size},
        {"get_box_collider3d_center", L_Get_box_collider3d_center},
        {"set_box_collider3d_center", L_Set_box_collider3d_center},
        {"get_box_collider3d_is_trigger", L_Get_box_collider3d_is_trigger},
        {"set_box_collider3d_is_trigger", L_Set_box_collider3d_is_trigger},
        {"get_box_collider3d_bounciness", L_Get_box_collider3d_bounciness},
        {"set_box_collider3d_bounciness", L_Set_box_collider3d_bounciness},
        {"get_box_collider3d_friction", L_Get_box_collider3d_friction},
        {"set_box_collider3d_friction", L_Set_box_collider3d_friction},
    });
}

} // namespace dse::runtime::lua_binding
