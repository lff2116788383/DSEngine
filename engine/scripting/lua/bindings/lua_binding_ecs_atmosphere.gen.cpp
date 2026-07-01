/**
 * @file lua_binding_ecs_atmosphere.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * AtmosphereComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_atmosphere_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_atmosphere_get_enabled(e));
    return 1;
}
int L_Set_atmosphere_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_atmosphere_planet_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_planet_radius(e));
    return 1;
}
int L_Set_atmosphere_planet_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_planet_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_atmosphere_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_atmosphere_height(e));
    return 1;
}
int L_Set_atmosphere_atmosphere_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_atmosphere_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_rayleigh_coeff(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_atmosphere_get_rayleigh_coeff(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_atmosphere_rayleigh_coeff(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_rayleigh_coeff(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_atmosphere_rayleigh_scale_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_rayleigh_scale_height(e));
    return 1;
}
int L_Set_atmosphere_rayleigh_scale_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_rayleigh_scale_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_mie_coeff(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_mie_coeff(e));
    return 1;
}
int L_Set_atmosphere_mie_coeff(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_mie_coeff(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_mie_scale_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_mie_scale_height(e));
    return 1;
}
int L_Set_atmosphere_mie_scale_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_mie_scale_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_mie_g(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_mie_g(e));
    return 1;
}
int L_Set_atmosphere_mie_g(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_mie_g(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_mie_albedo(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_atmosphere_get_mie_albedo(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_atmosphere_mie_albedo(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_mie_albedo(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_atmosphere_ozone_coeff(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_atmosphere_get_ozone_coeff(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_atmosphere_ozone_coeff(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_ozone_coeff(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_atmosphere_ozone_center_h(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_ozone_center_h(e));
    return 1;
}
int L_Set_atmosphere_ozone_center_h(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_ozone_center_h(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_ozone_width(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_ozone_width(e));
    return 1;
}
int L_Set_atmosphere_ozone_width(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_ozone_width(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_sun_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_sun_intensity(e));
    return 1;
}
int L_Set_atmosphere_sun_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_sun_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_sun_disk_angle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_atmosphere_get_sun_disk_angle(e));
    return 1;
}
int L_Set_atmosphere_sun_disk_angle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_sun_disk_angle(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_atmosphere_aerial_perspective_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_atmosphere_get_aerial_perspective_enabled(e));
    return 1;
}
int L_Set_atmosphere_aerial_perspective_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_atmosphere_set_aerial_perspective_enabled(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterAtmosphereComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_atmosphere_enabled", L_Get_atmosphere_enabled},
        {"set_atmosphere_enabled", L_Set_atmosphere_enabled},
        {"get_atmosphere_planet_radius", L_Get_atmosphere_planet_radius},
        {"set_atmosphere_planet_radius", L_Set_atmosphere_planet_radius},
        {"get_atmosphere_atmosphere_height", L_Get_atmosphere_atmosphere_height},
        {"set_atmosphere_atmosphere_height", L_Set_atmosphere_atmosphere_height},
        {"get_atmosphere_rayleigh_coeff", L_Get_atmosphere_rayleigh_coeff},
        {"set_atmosphere_rayleigh_coeff", L_Set_atmosphere_rayleigh_coeff},
        {"get_atmosphere_rayleigh_scale_height", L_Get_atmosphere_rayleigh_scale_height},
        {"set_atmosphere_rayleigh_scale_height", L_Set_atmosphere_rayleigh_scale_height},
        {"get_atmosphere_mie_coeff", L_Get_atmosphere_mie_coeff},
        {"set_atmosphere_mie_coeff", L_Set_atmosphere_mie_coeff},
        {"get_atmosphere_mie_scale_height", L_Get_atmosphere_mie_scale_height},
        {"set_atmosphere_mie_scale_height", L_Set_atmosphere_mie_scale_height},
        {"get_atmosphere_mie_g", L_Get_atmosphere_mie_g},
        {"set_atmosphere_mie_g", L_Set_atmosphere_mie_g},
        {"get_atmosphere_mie_albedo", L_Get_atmosphere_mie_albedo},
        {"set_atmosphere_mie_albedo", L_Set_atmosphere_mie_albedo},
        {"get_atmosphere_ozone_coeff", L_Get_atmosphere_ozone_coeff},
        {"set_atmosphere_ozone_coeff", L_Set_atmosphere_ozone_coeff},
        {"get_atmosphere_ozone_center_h", L_Get_atmosphere_ozone_center_h},
        {"set_atmosphere_ozone_center_h", L_Set_atmosphere_ozone_center_h},
        {"get_atmosphere_ozone_width", L_Get_atmosphere_ozone_width},
        {"set_atmosphere_ozone_width", L_Set_atmosphere_ozone_width},
        {"get_atmosphere_sun_intensity", L_Get_atmosphere_sun_intensity},
        {"set_atmosphere_sun_intensity", L_Set_atmosphere_sun_intensity},
        {"get_atmosphere_sun_disk_angle", L_Get_atmosphere_sun_disk_angle},
        {"set_atmosphere_sun_disk_angle", L_Set_atmosphere_sun_disk_angle},
        {"get_atmosphere_aerial_perspective_enabled", L_Get_atmosphere_aerial_perspective_enabled},
        {"set_atmosphere_aerial_perspective_enabled", L_Set_atmosphere_aerial_perspective_enabled},
    });
}

} // namespace dse::runtime::lua_binding
