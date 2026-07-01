/**
 * @file lua_binding_ecs_virtual_texture.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * VirtualTextureComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_virtual_texture_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_virtual_texture_get_enabled(e));
    return 1;
}
int L_Set_virtual_texture_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_virtual_texture_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_virtual_texture_vt_id(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_virtual_texture_get_vt_id(e));
    return 1;
}
int L_Set_virtual_texture_vt_id(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_virtual_texture_set_vt_id(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_virtual_texture_tile_data_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_virtual_texture_get_tile_data_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_virtual_texture_tile_data_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_virtual_texture_set_tile_data_path(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_virtual_texture_virtual_width(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_virtual_texture_get_virtual_width(e));
    return 1;
}
int L_Set_virtual_texture_virtual_width(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_virtual_texture_set_virtual_width(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_virtual_texture_virtual_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_virtual_texture_get_virtual_height(e));
    return 1;
}
int L_Set_virtual_texture_virtual_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_virtual_texture_set_virtual_height(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_virtual_texture_mip_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_virtual_texture_get_mip_bias(e));
    return 1;
}
int L_Set_virtual_texture_mip_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_virtual_texture_set_mip_bias(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterVirtualTextureComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_virtual_texture_enabled", L_Get_virtual_texture_enabled},
        {"set_virtual_texture_enabled", L_Set_virtual_texture_enabled},
        {"get_virtual_texture_vt_id", L_Get_virtual_texture_vt_id},
        {"set_virtual_texture_vt_id", L_Set_virtual_texture_vt_id},
        {"get_virtual_texture_tile_data_path", L_Get_virtual_texture_tile_data_path},
        {"set_virtual_texture_tile_data_path", L_Set_virtual_texture_tile_data_path},
        {"get_virtual_texture_virtual_width", L_Get_virtual_texture_virtual_width},
        {"set_virtual_texture_virtual_width", L_Set_virtual_texture_virtual_width},
        {"get_virtual_texture_virtual_height", L_Get_virtual_texture_virtual_height},
        {"set_virtual_texture_virtual_height", L_Set_virtual_texture_virtual_height},
        {"get_virtual_texture_mip_bias", L_Get_virtual_texture_mip_bias},
        {"set_virtual_texture_mip_bias", L_Set_virtual_texture_mip_bias},
    });
}

} // namespace dse::runtime::lua_binding
