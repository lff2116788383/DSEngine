/**
 * @file lua_binding_ecs_tree.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * TreeComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_tree_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_tree_get_enabled(e));
    return 1;
}
int L_Set_tree_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_tree_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_density(e));
    return 1;
}
int L_Set_tree_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_density(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_spawn_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_spawn_radius(e));
    return 1;
}
int L_Set_tree_spawn_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_spawn_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_chunk_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_chunk_size(e));
    return 1;
}
int L_Set_tree_chunk_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_chunk_size(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_min_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_min_scale(e));
    return 1;
}
int L_Set_tree_min_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_min_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_max_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_max_scale(e));
    return 1;
}
int L_Set_tree_max_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_max_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_lod1_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_lod1_distance(e));
    return 1;
}
int L_Set_tree_lod1_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_lod1_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_cull_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_cull_distance(e));
    return 1;
}
int L_Set_tree_cull_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_cull_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_wind_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_wind_strength(e));
    return 1;
}
int L_Set_tree_wind_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_wind_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_wind_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_wind_speed(e));
    return 1;
}
int L_Set_tree_wind_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_wind_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_tree_get_cast_shadow(e));
    return 1;
}
int L_Set_tree_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_cast_shadow(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_tree_shadow_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_shadow_distance(e));
    return 1;
}
int L_Set_tree_shadow_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_shadow_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_seed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_tree_get_seed(e));
    return 1;
}
int L_Set_tree_seed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_seed(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_tree_height_variation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_height_variation(e));
    return 1;
}
int L_Set_tree_height_variation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_height_variation(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_tree_random_rotation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_tree_get_random_rotation(e));
    return 1;
}
int L_Set_tree_random_rotation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_random_rotation(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_tree_billboard_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_tree_get_billboard_distance(e));
    return 1;
}
int L_Set_tree_billboard_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_tree_set_billboard_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterTreeComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_tree_enabled", L_Get_tree_enabled},
        {"set_tree_enabled", L_Set_tree_enabled},
        {"get_tree_density", L_Get_tree_density},
        {"set_tree_density", L_Set_tree_density},
        {"get_tree_spawn_radius", L_Get_tree_spawn_radius},
        {"set_tree_spawn_radius", L_Set_tree_spawn_radius},
        {"get_tree_chunk_size", L_Get_tree_chunk_size},
        {"set_tree_chunk_size", L_Set_tree_chunk_size},
        {"get_tree_min_scale", L_Get_tree_min_scale},
        {"set_tree_min_scale", L_Set_tree_min_scale},
        {"get_tree_max_scale", L_Get_tree_max_scale},
        {"set_tree_max_scale", L_Set_tree_max_scale},
        {"get_tree_lod1_distance", L_Get_tree_lod1_distance},
        {"set_tree_lod1_distance", L_Set_tree_lod1_distance},
        {"get_tree_cull_distance", L_Get_tree_cull_distance},
        {"set_tree_cull_distance", L_Set_tree_cull_distance},
        {"get_tree_wind_strength", L_Get_tree_wind_strength},
        {"set_tree_wind_strength", L_Set_tree_wind_strength},
        {"get_tree_wind_speed", L_Get_tree_wind_speed},
        {"set_tree_wind_speed", L_Set_tree_wind_speed},
        {"get_tree_cast_shadow", L_Get_tree_cast_shadow},
        {"set_tree_cast_shadow", L_Set_tree_cast_shadow},
        {"get_tree_shadow_distance", L_Get_tree_shadow_distance},
        {"set_tree_shadow_distance", L_Set_tree_shadow_distance},
        {"get_tree_seed", L_Get_tree_seed},
        {"set_tree_seed", L_Set_tree_seed},
        {"get_tree_height_variation", L_Get_tree_height_variation},
        {"set_tree_height_variation", L_Set_tree_height_variation},
        {"get_tree_random_rotation", L_Get_tree_random_rotation},
        {"set_tree_random_rotation", L_Set_tree_random_rotation},
        {"get_tree_billboard_distance", L_Get_tree_billboard_distance},
        {"set_tree_billboard_distance", L_Set_tree_billboard_distance},
    });
}

} // namespace dse::runtime::lua_binding
