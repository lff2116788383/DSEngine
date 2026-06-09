/**
 * @file lua_binding_ecs_dir_light.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * DirectionalLight3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_dir_light_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_dir_light_get_direction(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_dir_light_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_set_direction(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_dir_light_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_dir_light_get_color(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_dir_light_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_set_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_dir_light_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_dir_light_get_intensity(e));
    return 1;
}
int L_Set_dir_light_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_set_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_dir_light_ambient_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_dir_light_get_ambient_intensity(e));
    return 1;
}
int L_Set_dir_light_ambient_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_set_ambient_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_dir_light_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_dir_light_get_cast_shadow(e));
    return 1;
}
int L_Set_dir_light_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_set_cast_shadow(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_dir_light_shadow_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_dir_light_get_shadow_strength(e));
    return 1;
}
int L_Set_dir_light_shadow_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_set_shadow_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_dir_light_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_dir_light_get_enabled(e));
    return 1;
}
int L_Set_dir_light_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterDirectionalLight3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_dir_light_direction", L_Get_dir_light_direction},
        {"set_dir_light_direction", L_Set_dir_light_direction},
        {"get_dir_light_color", L_Get_dir_light_color},
        {"set_dir_light_color", L_Set_dir_light_color},
        {"get_dir_light_intensity", L_Get_dir_light_intensity},
        {"set_dir_light_intensity", L_Set_dir_light_intensity},
        {"get_dir_light_ambient", L_Get_dir_light_ambient_intensity},
        {"set_dir_light_ambient", L_Set_dir_light_ambient_intensity},
        {"get_dir_light_cast_shadow", L_Get_dir_light_cast_shadow},
        {"set_dir_light_cast_shadow", L_Set_dir_light_cast_shadow},
        {"get_dir_light_shadow_strength", L_Get_dir_light_shadow_strength},
        {"set_dir_light_shadow_strength", L_Set_dir_light_shadow_strength},
        {"get_dir_light_enabled", L_Get_dir_light_enabled},
        {"set_dir_light_enabled", L_Set_dir_light_enabled},
    });
}

} // namespace dse::runtime::lua_binding
