/**
 * @file lua_binding_ecs_lightmap.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * LightmapComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_lightmap_lightmap_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_lightmap_get_lightmap_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_lightmap_lightmap_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_lightmap_set_lightmap_path(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_lightmap_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_lightmap_get_intensity(e));
    return 1;
}
int L_Set_lightmap_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_lightmap_set_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_lightmap_st_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0, w = 0;
    dse_lightmap_get_st_offset(e, &x, &y, &z, &w);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); lua_pushnumber(L, w);
    return 4;
}
int L_Set_lightmap_st_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_lightmap_set_st_offset(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}
int L_Get_lightmap_use_ao(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_lightmap_get_use_ao(e));
    return 1;
}
int L_Set_lightmap_use_ao(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_lightmap_set_use_ao(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterLightmapComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_lightmap_lightmap_path", L_Get_lightmap_lightmap_path},
        {"set_lightmap_lightmap_path", L_Set_lightmap_lightmap_path},
        {"get_lightmap_intensity", L_Get_lightmap_intensity},
        {"set_lightmap_intensity", L_Set_lightmap_intensity},
        {"get_lightmap_st_offset", L_Get_lightmap_st_offset},
        {"set_lightmap_st_offset", L_Set_lightmap_st_offset},
        {"get_lightmap_use_ao", L_Get_lightmap_use_ao},
        {"set_lightmap_use_ao", L_Set_lightmap_use_ao},
    });
}

} // namespace dse::runtime::lua_binding
