/**
 * @file lua_binding_ecs_volumetric_cloud.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * VolumetricCloudComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_volumetric_cloud_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_volumetric_cloud_get_enabled(e));
    return 1;
}
int L_Set_volumetric_cloud_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_volumetric_cloud_cloud_bottom(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_cloud_bottom(e));
    return 1;
}
int L_Set_volumetric_cloud_cloud_bottom(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_cloud_bottom(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_cloud_top(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_cloud_top(e));
    return 1;
}
int L_Set_volumetric_cloud_cloud_top(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_cloud_top(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_coverage(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_coverage(e));
    return 1;
}
int L_Set_volumetric_cloud_coverage(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_coverage(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_density(e));
    return 1;
}
int L_Set_volumetric_cloud_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_density(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_shape_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_shape_scale(e));
    return 1;
}
int L_Set_volumetric_cloud_shape_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_shape_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_detail_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_detail_scale(e));
    return 1;
}
int L_Set_volumetric_cloud_detail_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_detail_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_detail_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_detail_strength(e));
    return 1;
}
int L_Set_volumetric_cloud_detail_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_detail_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_erosion(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_erosion(e));
    return 1;
}
int L_Set_volumetric_cloud_erosion(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_erosion(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_wind_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_volumetric_cloud_get_wind_direction(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_volumetric_cloud_wind_direction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_wind_direction(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_volumetric_cloud_wind_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_wind_speed(e));
    return 1;
}
int L_Set_volumetric_cloud_wind_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_wind_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_silver_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_silver_intensity(e));
    return 1;
}
int L_Set_volumetric_cloud_silver_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_silver_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_silver_spread(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_silver_spread(e));
    return 1;
}
int L_Set_volumetric_cloud_silver_spread(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_silver_spread(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_powder_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_powder_strength(e));
    return 1;
}
int L_Set_volumetric_cloud_powder_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_powder_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_ambient_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_volumetric_cloud_get_ambient_strength(e));
    return 1;
}
int L_Set_volumetric_cloud_ambient_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_ambient_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_volumetric_cloud_half_resolution(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_volumetric_cloud_get_half_resolution(e));
    return 1;
}
int L_Set_volumetric_cloud_half_resolution(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_half_resolution(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_volumetric_cloud_temporal_reprojection(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_volumetric_cloud_get_temporal_reprojection(e));
    return 1;
}
int L_Set_volumetric_cloud_temporal_reprojection(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_temporal_reprojection(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_volumetric_cloud_cloud_shadow_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_volumetric_cloud_get_cloud_shadow_enabled(e));
    return 1;
}
int L_Set_volumetric_cloud_cloud_shadow_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_volumetric_cloud_set_cloud_shadow_enabled(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterVolumetricCloudComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_volumetric_cloud_enabled", L_Get_volumetric_cloud_enabled},
        {"set_volumetric_cloud_enabled", L_Set_volumetric_cloud_enabled},
        {"get_volumetric_cloud_cloud_bottom", L_Get_volumetric_cloud_cloud_bottom},
        {"set_volumetric_cloud_cloud_bottom", L_Set_volumetric_cloud_cloud_bottom},
        {"get_volumetric_cloud_cloud_top", L_Get_volumetric_cloud_cloud_top},
        {"set_volumetric_cloud_cloud_top", L_Set_volumetric_cloud_cloud_top},
        {"get_volumetric_cloud_coverage", L_Get_volumetric_cloud_coverage},
        {"set_volumetric_cloud_coverage", L_Set_volumetric_cloud_coverage},
        {"get_volumetric_cloud_density", L_Get_volumetric_cloud_density},
        {"set_volumetric_cloud_density", L_Set_volumetric_cloud_density},
        {"get_volumetric_cloud_shape_scale", L_Get_volumetric_cloud_shape_scale},
        {"set_volumetric_cloud_shape_scale", L_Set_volumetric_cloud_shape_scale},
        {"get_volumetric_cloud_detail_scale", L_Get_volumetric_cloud_detail_scale},
        {"set_volumetric_cloud_detail_scale", L_Set_volumetric_cloud_detail_scale},
        {"get_volumetric_cloud_detail_strength", L_Get_volumetric_cloud_detail_strength},
        {"set_volumetric_cloud_detail_strength", L_Set_volumetric_cloud_detail_strength},
        {"get_volumetric_cloud_erosion", L_Get_volumetric_cloud_erosion},
        {"set_volumetric_cloud_erosion", L_Set_volumetric_cloud_erosion},
        {"get_volumetric_cloud_wind_direction", L_Get_volumetric_cloud_wind_direction},
        {"set_volumetric_cloud_wind_direction", L_Set_volumetric_cloud_wind_direction},
        {"get_volumetric_cloud_wind_speed", L_Get_volumetric_cloud_wind_speed},
        {"set_volumetric_cloud_wind_speed", L_Set_volumetric_cloud_wind_speed},
        {"get_volumetric_cloud_silver_intensity", L_Get_volumetric_cloud_silver_intensity},
        {"set_volumetric_cloud_silver_intensity", L_Set_volumetric_cloud_silver_intensity},
        {"get_volumetric_cloud_silver_spread", L_Get_volumetric_cloud_silver_spread},
        {"set_volumetric_cloud_silver_spread", L_Set_volumetric_cloud_silver_spread},
        {"get_volumetric_cloud_powder_strength", L_Get_volumetric_cloud_powder_strength},
        {"set_volumetric_cloud_powder_strength", L_Set_volumetric_cloud_powder_strength},
        {"get_volumetric_cloud_ambient_strength", L_Get_volumetric_cloud_ambient_strength},
        {"set_volumetric_cloud_ambient_strength", L_Set_volumetric_cloud_ambient_strength},
        {"get_volumetric_cloud_half_resolution", L_Get_volumetric_cloud_half_resolution},
        {"set_volumetric_cloud_half_resolution", L_Set_volumetric_cloud_half_resolution},
        {"get_volumetric_cloud_temporal_reprojection", L_Get_volumetric_cloud_temporal_reprojection},
        {"set_volumetric_cloud_temporal_reprojection", L_Set_volumetric_cloud_temporal_reprojection},
        {"get_volumetric_cloud_cloud_shadow_enabled", L_Get_volumetric_cloud_cloud_shadow_enabled},
        {"set_volumetric_cloud_cloud_shadow_enabled", L_Set_volumetric_cloud_cloud_shadow_enabled},
    });
}

} // namespace dse::runtime::lua_binding
