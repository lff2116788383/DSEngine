/**
 * @file lua_binding_ecs_streaming_origin.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * StreamingOriginComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_streaming_origin_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_streaming_origin_get_enabled(e));
    return 1;
}
int L_Set_streaming_origin_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_streaming_origin_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_streaming_origin_load_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_streaming_origin_get_load_radius(e));
    return 1;
}
int L_Set_streaming_origin_load_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_streaming_origin_set_load_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_streaming_origin_unload_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_streaming_origin_get_unload_radius(e));
    return 1;
}
int L_Set_streaming_origin_unload_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_streaming_origin_set_unload_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterStreamingOriginComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_streaming_origin_enabled", L_Get_streaming_origin_enabled},
        {"set_streaming_origin_enabled", L_Set_streaming_origin_enabled},
        {"get_streaming_origin_load_radius", L_Get_streaming_origin_load_radius},
        {"set_streaming_origin_load_radius", L_Set_streaming_origin_load_radius},
        {"get_streaming_origin_unload_radius", L_Get_streaming_origin_unload_radius},
        {"set_streaming_origin_unload_radius", L_Set_streaming_origin_unload_radius},
    });
}

} // namespace dse::runtime::lua_binding
