/**
 * @file lua_binding_ecs_decal.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * DecalComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_decal_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_decal_get_enabled(e));
    return 1;
}
int L_Set_decal_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_decal_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_decal_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0, w = 0;
    dse_decal_get_color(e, &x, &y, &z, &w);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); lua_pushnumber(L, w);
    return 4;
}
int L_Set_decal_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_decal_set_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}
int L_Get_decal_angle_fade(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_decal_get_angle_fade(e));
    return 1;
}
int L_Set_decal_angle_fade(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_decal_set_angle_fade(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterDecalComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_decal_enabled", L_Get_decal_enabled},
        {"set_decal_enabled", L_Set_decal_enabled},
        {"get_decal_color", L_Get_decal_color},
        {"set_decal_color", L_Set_decal_color},
        {"get_decal_angle_fade", L_Get_decal_angle_fade},
        {"set_decal_angle_fade", L_Set_decal_angle_fade},
    });
}

} // namespace dse::runtime::lua_binding
