/**
 * @file lua_binding_ecs_terrain_tile.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * TerrainTileManagerComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_terrain_tile_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_terrain_tile_get_enabled(e));
    return 1;
}
int L_Set_terrain_tile_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_terrain_tile_tile_world_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_terrain_tile_get_tile_world_size(e));
    return 1;
}
int L_Set_terrain_tile_tile_world_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_tile_world_size(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_terrain_tile_tile_resolution(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_terrain_tile_get_tile_resolution(e));
    return 1;
}
int L_Set_terrain_tile_tile_resolution(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_tile_resolution(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_terrain_tile_max_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_terrain_tile_get_max_height(e));
    return 1;
}
int L_Set_terrain_tile_max_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_max_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_terrain_tile_load_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_terrain_tile_get_load_radius(e));
    return 1;
}
int L_Set_terrain_tile_load_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_load_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_terrain_tile_unload_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_terrain_tile_get_unload_radius(e));
    return 1;
}
int L_Set_terrain_tile_unload_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_unload_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_terrain_tile_use_procedural(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_terrain_tile_get_use_procedural(e));
    return 1;
}
int L_Set_terrain_tile_use_procedural(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_use_procedural(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_terrain_tile_procedural_base_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_terrain_tile_get_procedural_base_height(e));
    return 1;
}
int L_Set_terrain_tile_procedural_base_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_procedural_base_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_terrain_tile_max_lod_levels(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_terrain_tile_get_max_lod_levels(e));
    return 1;
}
int L_Set_terrain_tile_max_lod_levels(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_max_lod_levels(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_terrain_tile_lod_distance_factor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_terrain_tile_get_lod_distance_factor(e));
    return 1;
}
int L_Set_terrain_tile_lod_distance_factor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_set_lod_distance_factor(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterTerrainTileManagerComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_terrain_tile_enabled", L_Get_terrain_tile_enabled},
        {"set_terrain_tile_enabled", L_Set_terrain_tile_enabled},
        {"get_terrain_tile_world_size", L_Get_terrain_tile_tile_world_size},
        {"set_terrain_tile_world_size", L_Set_terrain_tile_tile_world_size},
        {"get_terrain_tile_resolution", L_Get_terrain_tile_tile_resolution},
        {"set_terrain_tile_resolution", L_Set_terrain_tile_tile_resolution},
        {"get_terrain_tile_max_height", L_Get_terrain_tile_max_height},
        {"set_terrain_tile_max_height", L_Set_terrain_tile_max_height},
        {"get_terrain_tile_load_radius", L_Get_terrain_tile_load_radius},
        {"set_terrain_tile_load_radius", L_Set_terrain_tile_load_radius},
        {"get_terrain_tile_unload_radius", L_Get_terrain_tile_unload_radius},
        {"set_terrain_tile_unload_radius", L_Set_terrain_tile_unload_radius},
        {"get_terrain_tile_use_procedural", L_Get_terrain_tile_use_procedural},
        {"set_terrain_tile_use_procedural", L_Set_terrain_tile_use_procedural},
        {"get_terrain_tile_procedural_base_height", L_Get_terrain_tile_procedural_base_height},
        {"set_terrain_tile_procedural_base_height", L_Set_terrain_tile_procedural_base_height},
        {"get_terrain_tile_max_lod_levels", L_Get_terrain_tile_max_lod_levels},
        {"set_terrain_tile_max_lod_levels", L_Set_terrain_tile_max_lod_levels},
        {"get_terrain_tile_lod_distance_factor", L_Get_terrain_tile_lod_distance_factor},
        {"set_terrain_tile_lod_distance_factor", L_Set_terrain_tile_lod_distance_factor},
    });
}

} // namespace dse::runtime::lua_binding
