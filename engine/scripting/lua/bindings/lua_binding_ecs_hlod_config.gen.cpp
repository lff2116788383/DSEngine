/**
 * @file lua_binding_ecs_hlod_config.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * HLODConfigComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_hlod_config_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_hlod_config_get_enabled(e));
    return 1;
}
int L_Set_hlod_config_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hlod_config_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_hlod_config_hlod_data_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_hlod_config_get_hlod_data_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_hlod_config_hlod_data_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hlod_config_set_hlod_data_path(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_hlod_config_distance_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hlod_config_get_distance_scale(e));
    return 1;
}
int L_Set_hlod_config_distance_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hlod_config_set_distance_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_hlod_config_hysteresis(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hlod_config_get_hysteresis(e));
    return 1;
}
int L_Set_hlod_config_hysteresis(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hlod_config_set_hysteresis(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterHLODConfigComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_hlod_config_enabled", L_Get_hlod_config_enabled},
        {"set_hlod_config_enabled", L_Set_hlod_config_enabled},
        {"get_hlod_config_hlod_data_path", L_Get_hlod_config_hlod_data_path},
        {"set_hlod_config_hlod_data_path", L_Set_hlod_config_hlod_data_path},
        {"get_hlod_config_distance_scale", L_Get_hlod_config_distance_scale},
        {"set_hlod_config_distance_scale", L_Set_hlod_config_distance_scale},
        {"get_hlod_config_hysteresis", L_Get_hlod_config_hysteresis},
        {"set_hlod_config_hysteresis", L_Set_hlod_config_hysteresis},
    });
}

} // namespace dse::runtime::lua_binding
