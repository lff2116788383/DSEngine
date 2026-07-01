/**
 * @file lua_binding_ecs_light_probe.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * LightProbeComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_light_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_light_probe_get_enabled(e));
    return 1;
}
int L_Set_light_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_light_probe_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_light_probe_influence_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_light_probe_get_influence_radius(e));
    return 1;
}
int L_Set_light_probe_influence_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_light_probe_set_influence_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_light_probe_show_debug(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_light_probe_get_show_debug(e));
    return 1;
}
int L_Set_light_probe_show_debug(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_light_probe_set_show_debug(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterLightProbeComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_light_probe_enabled", L_Get_light_probe_enabled},
        {"set_light_probe_enabled", L_Set_light_probe_enabled},
        {"get_light_probe_influence_radius", L_Get_light_probe_influence_radius},
        {"set_light_probe_influence_radius", L_Set_light_probe_influence_radius},
        {"get_light_probe_show_debug", L_Get_light_probe_show_debug},
        {"set_light_probe_show_debug", L_Set_light_probe_show_debug},
    });
}

} // namespace dse::runtime::lua_binding
