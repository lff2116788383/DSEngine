/**
 * @file lua_binding_ecs_hair.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * HairComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_hair_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_hair_get_enabled(e));
    return 1;
}
int L_Set_hair_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_hair_hair_asset_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_hair_get_hair_asset_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_hair_hair_asset_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_hair_asset_path(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_hair_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hair_get_damping(e));
    return 1;
}
int L_Set_hair_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_damping(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_hair_stiffness_local(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hair_get_stiffness_local(e));
    return 1;
}
int L_Set_hair_stiffness_local(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_stiffness_local(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_hair_stiffness_global(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hair_get_stiffness_global(e));
    return 1;
}
int L_Set_hair_stiffness_global(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_stiffness_global(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_hair_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hair_get_gravity(e));
    return 1;
}
int L_Set_hair_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_gravity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_hair_wind(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_hair_get_wind(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_hair_wind(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_wind(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_hair_wind_turbulence(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hair_get_wind_turbulence(e));
    return 1;
}
int L_Set_hair_wind_turbulence(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_wind_turbulence(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_hair_root_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0, w = 0;
    dse_hair_get_root_color(e, &x, &y, &z, &w);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); lua_pushnumber(L, w);
    return 4;
}
int L_Set_hair_root_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_root_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}
int L_Get_hair_tip_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0, w = 0;
    dse_hair_get_tip_color(e, &x, &y, &z, &w);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); lua_pushnumber(L, w);
    return 4;
}
int L_Set_hair_tip_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_tip_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}
int L_Get_hair_fiber_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hair_get_fiber_radius(e));
    return 1;
}
int L_Set_hair_fiber_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_fiber_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_hair_opacity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_hair_get_opacity(e));
    return 1;
}
int L_Set_hair_opacity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_opacity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_hair_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_hair_get_cast_shadow(e));
    return 1;
}
int L_Set_hair_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_cast_shadow(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_hair_receive_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_hair_get_receive_shadow(e));
    return 1;
}
int L_Set_hair_receive_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_hair_set_receive_shadow(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterHairComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_hair_enabled", L_Get_hair_enabled},
        {"set_hair_enabled", L_Set_hair_enabled},
        {"get_hair_hair_asset_path", L_Get_hair_hair_asset_path},
        {"set_hair_hair_asset_path", L_Set_hair_hair_asset_path},
        {"get_hair_damping", L_Get_hair_damping},
        {"set_hair_damping", L_Set_hair_damping},
        {"get_hair_stiffness_local", L_Get_hair_stiffness_local},
        {"set_hair_stiffness_local", L_Set_hair_stiffness_local},
        {"get_hair_stiffness_global", L_Get_hair_stiffness_global},
        {"set_hair_stiffness_global", L_Set_hair_stiffness_global},
        {"get_hair_gravity", L_Get_hair_gravity},
        {"set_hair_gravity", L_Set_hair_gravity},
        {"get_hair_wind", L_Get_hair_wind},
        {"set_hair_wind", L_Set_hair_wind},
        {"get_hair_wind_turbulence", L_Get_hair_wind_turbulence},
        {"set_hair_wind_turbulence", L_Set_hair_wind_turbulence},
        {"get_hair_root_color", L_Get_hair_root_color},
        {"set_hair_root_color", L_Set_hair_root_color},
        {"get_hair_tip_color", L_Get_hair_tip_color},
        {"set_hair_tip_color", L_Set_hair_tip_color},
        {"get_hair_fiber_radius", L_Get_hair_fiber_radius},
        {"set_hair_fiber_radius", L_Set_hair_fiber_radius},
        {"get_hair_opacity", L_Get_hair_opacity},
        {"set_hair_opacity", L_Set_hair_opacity},
        {"get_hair_cast_shadow", L_Get_hair_cast_shadow},
        {"set_hair_cast_shadow", L_Set_hair_cast_shadow},
        {"get_hair_receive_shadow", L_Get_hair_receive_shadow},
        {"set_hair_receive_shadow", L_Set_hair_receive_shadow},
    });
}

} // namespace dse::runtime::lua_binding
