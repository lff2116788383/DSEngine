/**
 * @file lua_binding_free_ecs_terrain.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_terrain_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* heightmap_path = luaL_checkstring(L, 2);
    float width = static_cast<float>(luaL_checknumber(L, 3));
    float depth = static_cast<float>(luaL_checknumber(L, 4));
    float max_height = static_cast<float>(luaL_checknumber(L, 5));
    dse_terrain_add(e, heightmap_path, width, depth, max_height);
    return 0;
}

int L_dse_terrain_set_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int x = static_cast<int>(luaL_checkinteger(L, 2));
    int z = static_cast<int>(luaL_checkinteger(L, 3));
    float height = static_cast<float>(luaL_checknumber(L, 4));
    dse_terrain_set_height(e, x, z, height);
    return 0;
}

int L_dse_terrain_sample_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wz = static_cast<float>(luaL_checknumber(L, 3));
    float _ret = dse_terrain_sample_height(e, wx, wz);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_terrain_set_splat_texture(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    const char* path = luaL_checkstring(L, 3);
    int _ret = dse_terrain_set_splat_texture(e, layer, path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_water_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_water_add(e);
    return 0;
}

int L_dse_grass_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float density = static_cast<float>(luaL_checknumber(L, 2));
    float spawn_radius = static_cast<float>(luaL_checknumber(L, 3));
    float blade_height = static_cast<float>(luaL_checknumber(L, 4));
    float blade_width = static_cast<float>(luaL_checknumber(L, 5));
    dse_grass_add(e, density, spawn_radius, blade_height, blade_width);
    return 0;
}

int L_dse_grass_set_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float density = static_cast<float>(luaL_checknumber(L, 2));
    float spawn_radius = static_cast<float>(luaL_checknumber(L, 3));
    float blade_height = static_cast<float>(luaL_checknumber(L, 4));
    float blade_width = static_cast<float>(luaL_checknumber(L, 5));
    float blade_height_var = static_cast<float>(luaL_checknumber(L, 6));
    float chunk_size = static_cast<float>(luaL_checknumber(L, 7));
    int seed = static_cast<int>(luaL_checkinteger(L, 8));
    dse_grass_set_params(e, density, spawn_radius, blade_height, blade_width, blade_height_var, chunk_size, seed);
    return 0;
}

int L_dse_grass_set_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float br = static_cast<float>(luaL_checknumber(L, 2));
    float bg = static_cast<float>(luaL_checknumber(L, 3));
    float bb = static_cast<float>(luaL_checknumber(L, 4));
    float tr = static_cast<float>(luaL_checknumber(L, 5));
    float tg = static_cast<float>(luaL_checknumber(L, 6));
    float tb = static_cast<float>(luaL_checknumber(L, 7));
    dse_grass_set_color(e, br, bg, bb, tr, tg, tb);
    return 0;
}

int L_dse_grass_set_wind(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dx = static_cast<float>(luaL_checknumber(L, 2));
    float dy = static_cast<float>(luaL_checknumber(L, 3));
    float speed = static_cast<float>(luaL_checknumber(L, 4));
    float strength = static_cast<float>(luaL_checknumber(L, 5));
    float turbulence = static_cast<float>(luaL_checknumber(L, 6));
    dse_grass_set_wind(e, dx, dy, speed, strength, turbulence);
    return 0;
}

int L_dse_grass_set_lod(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float near_dist = static_cast<float>(luaL_checknumber(L, 2));
    float far_dist = static_cast<float>(luaL_checknumber(L, 3));
    int cast_shadow = static_cast<int>(luaL_checkinteger(L, 4));
    float shadow_dist = static_cast<float>(luaL_checknumber(L, 5));
    dse_grass_set_lod(e, near_dist, far_dist, cast_shadow, shadow_dist);
    return 0;
}

int L_dse_grass_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_grass_set_enabled(e, enabled);
    return 0;
}

int L_dse_grass_get_stats(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_grass_get_stats(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_tree_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* mesh_path = luaL_checkstring(L, 2);
    dse_tree_add(e, mesh_path);
    return 0;
}

int L_dse_terrain_tile_manager_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_tile_manager_add(e);
    return 0;
}

int L_dse_dynamic_obstacle_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int shape = static_cast<int>(luaL_checkinteger(L, 2));
    dse_dynamic_obstacle_add(e, shape);
    return 0;
}

int L_dse_foliage_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_foliage_add(e);
    return 0;
}

int L_dse_navmesh_rebake_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_add(e);
    return 0;
}

} // namespace

void RegisterEcsRenderingTerrainBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_terrain", L_dse_terrain_add},
        {"set_terrain_height", L_dse_terrain_set_height},
        {"sample_terrain_height", L_dse_terrain_sample_height},
        {"set_terrain_splat_texture", L_dse_terrain_set_splat_texture},
        {"add_water", L_dse_water_add},
        {"add_grass", L_dse_grass_add},
        {"set_grass_params", L_dse_grass_set_params},
        {"set_grass_color", L_dse_grass_set_color},
        {"set_grass_wind", L_dse_grass_set_wind},
        {"set_grass_lod", L_dse_grass_set_lod},
        {"set_grass_enabled", L_dse_grass_set_enabled},
        {"get_grass_stats", L_dse_grass_get_stats},
        {"add_tree", L_dse_tree_add},
        {"add_terrain_tile_manager", L_dse_terrain_tile_manager_add},
        {"add_dynamic_obstacle", L_dse_dynamic_obstacle_add},
        {"add_foliage", L_dse_foliage_add},
        {"add_navmesh_auto_rebake", L_dse_navmesh_rebake_add},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
