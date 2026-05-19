/**
 * @file lua_binding_ecs_point_light.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * PointLightComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_point_light_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_point_light_get_color(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_point_light_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_point_light_set_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_point_light_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_point_light_get_intensity(e));
    return 1;
}
int L_Set_point_light_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_point_light_set_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_point_light_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_point_light_get_radius(e));
    return 1;
}
int L_Set_point_light_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_point_light_set_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_point_light_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_point_light_get_enabled(e));
    return 1;
}
int L_Set_point_light_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_point_light_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterPointLightComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_point_light_color", L_Get_point_light_color},
        {"set_point_light_color", L_Set_point_light_color},
        {"get_point_light_intensity", L_Get_point_light_intensity},
        {"set_point_light_intensity", L_Set_point_light_intensity},
        {"get_point_light_radius", L_Get_point_light_radius},
        {"set_point_light_radius", L_Set_point_light_radius},
        {"get_point_light_enabled", L_Get_point_light_enabled},
        {"set_point_light_enabled", L_Set_point_light_enabled},
    });
}

} // namespace dse::runtime::lua_binding
