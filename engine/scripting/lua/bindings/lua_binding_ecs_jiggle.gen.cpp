/**
 * @file lua_binding_ecs_jiggle.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * JiggleBoneComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_jiggle_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_jiggle_get_enabled(e));
    return 1;
}
int L_Set_jiggle_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_jiggle_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_jiggle_stiffness_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_jiggle_get_stiffness_scale(e));
    return 1;
}
int L_Set_jiggle_stiffness_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_jiggle_set_stiffness_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_jiggle_damping_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_jiggle_get_damping_scale(e));
    return 1;
}
int L_Set_jiggle_damping_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_jiggle_set_damping_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_jiggle_gravity_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_jiggle_get_gravity_scale(e));
    return 1;
}
int L_Set_jiggle_gravity_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_jiggle_set_gravity_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterJiggleBoneComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_jiggle_enabled", L_Get_jiggle_enabled},
        {"set_jiggle_enabled", L_Set_jiggle_enabled},
        {"get_jiggle_stiffness_scale", L_Get_jiggle_stiffness_scale},
        {"set_jiggle_stiffness_scale", L_Set_jiggle_stiffness_scale},
        {"get_jiggle_damping_scale", L_Get_jiggle_damping_scale},
        {"set_jiggle_damping_scale", L_Set_jiggle_damping_scale},
        {"get_jiggle_gravity_scale", L_Get_jiggle_gravity_scale},
        {"set_jiggle_gravity_scale", L_Set_jiggle_gravity_scale},
    });
}

} // namespace dse::runtime::lua_binding
