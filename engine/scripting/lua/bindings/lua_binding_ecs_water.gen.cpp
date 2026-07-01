/**
 * @file lua_binding_ecs_water.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * WaterComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_water_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_water_get_enabled(e));
    return 1;
}
int L_Set_water_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_water_water_level(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_water_level(e));
    return 1;
}
int L_Set_water_water_level(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_water_level(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_deep_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0, w = 0;
    dse_water_get_deep_color(e, &x, &y, &z, &w);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); lua_pushnumber(L, w);
    return 4;
}
int L_Set_water_deep_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_deep_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}
int L_Get_water_shallow_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0, w = 0;
    dse_water_get_shallow_color(e, &x, &y, &z, &w);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); lua_pushnumber(L, w);
    return 4;
}
int L_Set_water_shallow_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_shallow_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}
int L_Get_water_max_depth(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_max_depth(e));
    return 1;
}
int L_Set_water_max_depth(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_max_depth(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_transparency(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_transparency(e));
    return 1;
}
int L_Set_water_transparency(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_transparency(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_wave_amplitude(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_wave_amplitude(e));
    return 1;
}
int L_Set_water_wave_amplitude(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_wave_amplitude(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_wave_frequency(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_wave_frequency(e));
    return 1;
}
int L_Set_water_wave_frequency(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_wave_frequency(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_wave_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_wave_speed(e));
    return 1;
}
int L_Set_water_wave_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_wave_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_wave_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_water_get_wave_direction(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_water_wave_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_wave_direction(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_water_refraction_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_refraction_strength(e));
    return 1;
}
int L_Set_water_refraction_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_refraction_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_reflection_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_reflection_strength(e));
    return 1;
}
int L_Set_water_reflection_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_reflection_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_specular_power(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_specular_power(e));
    return 1;
}
int L_Set_water_specular_power(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_specular_power(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_caustic_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_caustic_intensity(e));
    return 1;
}
int L_Set_water_caustic_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_caustic_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_caustic_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_caustic_scale(e));
    return 1;
}
int L_Set_water_caustic_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_caustic_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_foam_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_foam_intensity(e));
    return 1;
}
int L_Set_water_foam_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_foam_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_foam_depth_threshold(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_foam_depth_threshold(e));
    return 1;
}
int L_Set_water_foam_depth_threshold(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_foam_depth_threshold(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_underwater_fog_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_water_get_underwater_fog_density(e));
    return 1;
}
int L_Set_water_underwater_fog_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_underwater_fog_density(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_water_underwater_fog_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0, w = 0;
    dse_water_get_underwater_fog_color(e, &x, &y, &z, &w);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); lua_pushnumber(L, w);
    return 4;
}
int L_Set_water_underwater_fog_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_set_underwater_fog_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}

} // namespace

void RegisterWaterComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_water_enabled", L_Get_water_enabled},
        {"set_water_enabled", L_Set_water_enabled},
        {"get_water_water_level", L_Get_water_water_level},
        {"set_water_water_level", L_Set_water_water_level},
        {"get_water_deep_color", L_Get_water_deep_color},
        {"set_water_deep_color", L_Set_water_deep_color},
        {"get_water_shallow_color", L_Get_water_shallow_color},
        {"set_water_shallow_color", L_Set_water_shallow_color},
        {"get_water_max_depth", L_Get_water_max_depth},
        {"set_water_max_depth", L_Set_water_max_depth},
        {"get_water_transparency", L_Get_water_transparency},
        {"set_water_transparency", L_Set_water_transparency},
        {"get_water_wave_amplitude", L_Get_water_wave_amplitude},
        {"set_water_wave_amplitude", L_Set_water_wave_amplitude},
        {"get_water_wave_frequency", L_Get_water_wave_frequency},
        {"set_water_wave_frequency", L_Set_water_wave_frequency},
        {"get_water_wave_speed", L_Get_water_wave_speed},
        {"set_water_wave_speed", L_Set_water_wave_speed},
        {"get_water_wave_direction", L_Get_water_wave_direction},
        {"set_water_wave_direction", L_Set_water_wave_direction},
        {"get_water_refraction_strength", L_Get_water_refraction_strength},
        {"set_water_refraction_strength", L_Set_water_refraction_strength},
        {"get_water_reflection_strength", L_Get_water_reflection_strength},
        {"set_water_reflection_strength", L_Set_water_reflection_strength},
        {"get_water_specular_power", L_Get_water_specular_power},
        {"set_water_specular_power", L_Set_water_specular_power},
        {"get_water_caustic_intensity", L_Get_water_caustic_intensity},
        {"set_water_caustic_intensity", L_Set_water_caustic_intensity},
        {"get_water_caustic_scale", L_Get_water_caustic_scale},
        {"set_water_caustic_scale", L_Set_water_caustic_scale},
        {"get_water_foam_intensity", L_Get_water_foam_intensity},
        {"set_water_foam_intensity", L_Set_water_foam_intensity},
        {"get_water_foam_depth_threshold", L_Get_water_foam_depth_threshold},
        {"set_water_foam_depth_threshold", L_Set_water_foam_depth_threshold},
        {"get_water_underwater_fog_density", L_Get_water_underwater_fog_density},
        {"set_water_underwater_fog_density", L_Set_water_underwater_fog_density},
        {"get_water_underwater_fog_color", L_Get_water_underwater_fog_color},
        {"set_water_underwater_fog_color", L_Set_water_underwater_fog_color},
    });
}

} // namespace dse::runtime::lua_binding
