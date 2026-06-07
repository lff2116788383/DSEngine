/**
 * @file lua_binding_ecs_spot_light.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * SpotLightComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_spot_light_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_spot_light_get_color(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_spot_light_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_set_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_spot_light_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spot_light_get_intensity(e));
    return 1;
}
int L_Set_spot_light_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_set_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_spot_light_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spot_light_get_radius(e));
    return 1;
}
int L_Set_spot_light_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_set_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_spot_light_inner_cone_angle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spot_light_get_inner_cone_angle(e));
    return 1;
}
int L_Set_spot_light_inner_cone_angle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_set_inner_cone_angle(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_spot_light_outer_cone_angle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spot_light_get_outer_cone_angle(e));
    return 1;
}
int L_Set_spot_light_outer_cone_angle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_set_outer_cone_angle(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_spot_light_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_spot_light_get_direction(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_spot_light_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_set_direction(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_spot_light_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_spot_light_get_enabled(e));
    return 1;
}
int L_Set_spot_light_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_spot_light_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_spot_light_get_cast_shadow(e));
    return 1;
}
int L_Set_spot_light_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_set_cast_shadow(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterSpotLightComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_spot_light_color", L_Get_spot_light_color},
        {"set_spot_light_color", L_Set_spot_light_color},
        {"get_spot_light_intensity", L_Get_spot_light_intensity},
        {"set_spot_light_intensity", L_Set_spot_light_intensity},
        {"get_spot_light_radius", L_Get_spot_light_radius},
        {"set_spot_light_radius", L_Set_spot_light_radius},
        {"get_spot_light_inner_cone_angle", L_Get_spot_light_inner_cone_angle},
        {"set_spot_light_inner_cone_angle", L_Set_spot_light_inner_cone_angle},
        {"get_spot_light_outer_cone_angle", L_Get_spot_light_outer_cone_angle},
        {"set_spot_light_outer_cone_angle", L_Set_spot_light_outer_cone_angle},
        {"get_spot_light_direction", L_Get_spot_light_direction},
        {"set_spot_light_direction", L_Set_spot_light_direction},
        {"get_spot_light_enabled", L_Get_spot_light_enabled},
        {"set_spot_light_enabled", L_Set_spot_light_enabled},
        {"get_spot_light_cast_shadow", L_Get_spot_light_cast_shadow},
        {"set_spot_light_cast_shadow", L_Set_spot_light_cast_shadow},
    });
}

} // namespace dse::runtime::lua_binding
