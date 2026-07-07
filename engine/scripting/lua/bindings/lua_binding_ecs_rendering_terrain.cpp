/**
 * @file lua_binding_ecs_rendering_terrain.cpp
 * @brief Terrain / Water / Grass / Foliage / Tree 等环境 Lua 绑定 — C ABI 薄包装
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t Ent(lua_State* L, int idx) {
    return static_cast<uint32_t>(luaL_checkinteger(L, idx));
}

inline float OptF(lua_State* L, int idx, float def) {
    if (lua_isnoneornil(L, idx)) return def;
    return static_cast<float>(luaL_checknumber(L, idx));
}

// ============================================================
// Terrain
// ============================================================

int L_EcsAddTerrain(lua_State* L) {
    uint32_t e = Ent(L, 1);
    const char* heightmap_path = luaL_optstring(L, 2, "");
    float width = helper::OptFloat(L, 3, 100.0f);
    float depth = helper::OptFloat(L, 4, 100.0f);
    float max_height = helper::OptFloat(L, 5, 20.0f);
    dse_terrain_add(e, heightmap_path, width, depth, max_height);
    return 0;
}

int L_EcsSetTerrainParams(lua_State* L) {
    uint32_t e = Ent(L, 1);
    // Read current values for "keep current" semantics
    int cur_lod, cur_rx, cur_rz, cur_max_lod;
    float cur_factor;
    dse_terrain_get_lod(e, &cur_lod, &cur_rx, &cur_rz, &cur_max_lod, &cur_factor);
    int res_x = std::max(2, helper::OptInt(L, 2, cur_rx));
    int res_z = std::max(2, helper::OptInt(L, 3, cur_rz));
    int max_lod = std::max(1, helper::OptInt(L, 4, cur_max_lod));
    float lod_factor = std::max(0.1f, helper::OptFloat(L, 5, cur_factor));
    int use_dynamic = (lua_gettop(L) >= 6) ? (helper::CheckBool(L, 6) ? 1 : 0) : 0;
    dse_terrain_set_params(e, res_x, res_z, max_lod, lod_factor, use_dynamic);
    return 0;
}

int L_EcsSetTerrainHeight(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int x = helper::CheckInt(L, 2);
    int z = helper::CheckInt(L, 3);
    float height = helper::CheckFloat(L, 4);
    dse_terrain_set_height(e, x, z, height);
    return 0;
}

int L_EcsLoadTerrainHeightmap(lua_State* L) {
    uint32_t e = Ent(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int w, h, ch, rx, rz;
    int ok = dse_terrain_load_heightmap(e, path, &w, &h, &ch, &rx, &rz);
    if (ok) {
        lua_pushboolean(L, 1);
        helper::PushInt(L, w);
        helper::PushInt(L, h);
        helper::PushInt(L, ch);
        helper::PushInt(L, rx);
        helper::PushInt(L, rz);
        return 6;
    }
    lua_pushboolean(L, 0);
    return 1;
}

int L_EcsSetTerrainTexture(lua_State* L) {
    uint32_t e = Ent(L, 1);
    const char* path = luaL_checkstring(L, 2);
    uint32_t handle;
    int w, h;
    int ok = dse_terrain_set_texture(e, path, &handle, &w, &h);
    if (ok) {
        lua_pushboolean(L, 1);
        helper::PushInt(L, static_cast<int>(handle));
        helper::PushInt(L, w);
        helper::PushInt(L, h);
        return 4;
    }
    lua_pushboolean(L, 0);
    return 1;
}

int L_EcsGetTerrainLod(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int lod, rx, rz, max_lod;
    float factor;
    dse_terrain_get_lod(e, &lod, &rx, &rz, &max_lod, &factor);
    helper::PushInt(L, lod);
    helper::PushInt(L, rx);
    helper::PushInt(L, rz);
    helper::PushInt(L, max_lod);
    helper::PushFloat(L, factor);
    return 5;
}

int L_EcsSampleTerrainHeight(lua_State* L) {
    uint32_t e = Ent(L, 1);
    float wx = helper::CheckFloat(L, 2);
    float wz = helper::CheckFloat(L, 3);
    helper::PushFloat(L, dse_terrain_sample_height(e, wx, wz));
    return 1;
}

int L_EcsSetTerrainSplatTexture(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int layer = helper::CheckInt(L, 2);
    const char* path = luaL_checkstring(L, 3);
    lua_pushboolean(L, dse_terrain_set_splat_texture(e, layer, path));
    return 1;
}

// ============================================================
// Water
// ============================================================

int L_EcsAddWater(lua_State* L) {
    dse_water_add(Ent(L, 1));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetWater(lua_State* L) {
    uint32_t e = Ent(L, 1);
    // Read current values for "keep current" semantics
    int cur_en;
    float cur_level, cur_deep[3], cur_shallow[3], cur_max_d, cur_trans;
    float cur_wave[3], cur_wdir[2], cur_refract, cur_reflect, cur_spec;
    dse_water_get(e, &cur_en, &cur_level, cur_deep, cur_shallow,
                  &cur_max_d, &cur_trans, cur_wave, cur_wdir,
                  &cur_refract, &cur_reflect, &cur_spec);

    int enabled = lua_isnoneornil(L, 2) ? cur_en : (lua_toboolean(L, 2) ? 1 : 0);
    float water_level = OptF(L, 3, cur_level);
    float dr = OptF(L, 4, cur_deep[0]);
    float dg = OptF(L, 5, cur_deep[1]);
    float db = OptF(L, 6, cur_deep[2]);
    float sr = OptF(L, 7, cur_shallow[0]);
    float sg = OptF(L, 8, cur_shallow[1]);
    float sb = OptF(L, 9, cur_shallow[2]);
    float max_depth = OptF(L, 10, cur_max_d);
    float transparency = OptF(L, 11, cur_trans);
    float wave_amp = OptF(L, 12, cur_wave[0]);
    float wave_freq = OptF(L, 13, cur_wave[1]);
    float wave_speed = OptF(L, 14, cur_wave[2]);
    float wdir_x = OptF(L, 15, cur_wdir[0]);
    float wdir_y = OptF(L, 16, cur_wdir[1]);
    float refraction = OptF(L, 17, cur_refract);
    float reflection = OptF(L, 18, cur_reflect);
    float spec_power = OptF(L, 19, cur_spec);
    float caustic_int = OptF(L, 20, 0.5f);
    float caustic_scale = OptF(L, 21, 1.0f);
    float foam_int = OptF(L, 22, 0.5f);
    float foam_threshold = OptF(L, 23, 0.1f);
    float ufog_density = OptF(L, 24, 0.05f);
    float ufog_r = OptF(L, 25, 0.0f);
    float ufog_g = OptF(L, 26, 0.2f);
    float ufog_b = OptF(L, 27, 0.4f);

    dse_water_set(e, enabled, water_level, dr, dg, db, sr, sg, sb,
                  max_depth, transparency, wave_amp, wave_freq, wave_speed,
                  wdir_x, wdir_y, refraction, reflection, spec_power,
                  caustic_int, caustic_scale, foam_int, foam_threshold,
                  ufog_density, ufog_r, ufog_g, ufog_b);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsGetWater(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int enabled;
    float level, deep[3], shallow[3], max_d, trans, wave[3], wdir[2];
    float refract, reflect, spec;
    int ok = dse_water_get(e, &enabled, &level, deep, shallow,
                           &max_d, &trans, wave, wdir, &refract, &reflect, &spec);
    if (!ok) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, 1);
    helper::PushBool(L, enabled);
    helper::PushFloat(L, level);
    helper::PushFloat(L, deep[0]);
    helper::PushFloat(L, deep[1]);
    helper::PushFloat(L, deep[2]);
    helper::PushFloat(L, shallow[0]);
    helper::PushFloat(L, shallow[1]);
    helper::PushFloat(L, shallow[2]);
    helper::PushFloat(L, max_d);
    helper::PushFloat(L, trans);
    helper::PushFloat(L, wave[0]);
    helper::PushFloat(L, wave[1]);
    helper::PushFloat(L, wave[2]);
    helper::PushFloat(L, wdir[0]);
    helper::PushFloat(L, wdir[1]);
    helper::PushFloat(L, refract);
    helper::PushFloat(L, reflect);
    helper::PushFloat(L, spec);
    return 19;
}

// ============================================================
// Grass
// ============================================================

int L_EcsAddGrass(lua_State* L) {
    uint32_t e = Ent(L, 1);
    dse_grass_add(e, helper::OptFloat(L, 2, 1.0f), helper::OptFloat(L, 3, 50.0f),
                  helper::OptFloat(L, 4, 1.0f), helper::OptFloat(L, 5, 0.1f));
    return 0;
}

int L_EcsSetGrassParams(lua_State* L) {
    uint32_t e = Ent(L, 1);
    // Original uses lua_gettop to check which params are provided
    float density = (lua_gettop(L) >= 2) ? helper::CheckFloat(L, 2) : 1.0f;
    float spawn_radius = (lua_gettop(L) >= 3) ? helper::CheckFloat(L, 3) : 50.0f;
    float blade_height = (lua_gettop(L) >= 4) ? helper::CheckFloat(L, 4) : 1.0f;
    float blade_width = (lua_gettop(L) >= 5) ? helper::CheckFloat(L, 5) : 0.1f;
    float blade_var = (lua_gettop(L) >= 6) ? helper::CheckFloat(L, 6) : 0.3f;
    float chunk_size = (lua_gettop(L) >= 7) ? helper::CheckFloat(L, 7) : 16.0f;
    int seed = (lua_gettop(L) >= 8) ? helper::CheckInt(L, 8) : 0;
    dse_grass_set_params(e, density, spawn_radius, blade_height, blade_width,
                         blade_var, chunk_size, seed);
    return 0;
}

int L_EcsSetGrassColor(lua_State* L) {
    uint32_t e = Ent(L, 1);
    float br = helper::CheckFloat(L, 2), bg = helper::CheckFloat(L, 3), bb = helper::CheckFloat(L, 4);
    float tr = br, tg = bg, tb = bb;  // default tip = base
    if (lua_gettop(L) >= 7) {
        tr = helper::CheckFloat(L, 5);
        tg = helper::CheckFloat(L, 6);
        tb = helper::CheckFloat(L, 7);
    }
    dse_grass_set_color(e, br, bg, bb, tr, tg, tb);
    return 0;
}

int L_EcsSetGrassWind(lua_State* L) {
    uint32_t e = Ent(L, 1);
    float dx = helper::CheckFloat(L, 2), dy = helper::CheckFloat(L, 3);
    float speed = (lua_gettop(L) >= 4) ? helper::CheckFloat(L, 4) : 1.0f;
    float strength = (lua_gettop(L) >= 5) ? helper::CheckFloat(L, 5) : 0.5f;
    float turbulence = (lua_gettop(L) >= 6) ? helper::CheckFloat(L, 6) : 0.2f;
    dse_grass_set_wind(e, dx, dy, speed, strength, turbulence);
    return 0;
}

int L_EcsSetGrassLod(lua_State* L) {
    uint32_t e = Ent(L, 1);
    float near_d = helper::CheckFloat(L, 2), far_d = helper::CheckFloat(L, 3);
    int cast_shadow = (lua_gettop(L) >= 4) ? (lua_toboolean(L, 4) ? 1 : 0) : 0;
    float shadow_dist = (lua_gettop(L) >= 5) ? helper::CheckFloat(L, 5) : 50.0f;
    dse_grass_set_lod(e, near_d, far_d, cast_shadow, shadow_dist);
    return 0;
}

int L_EcsSetGrassEnabled(lua_State* L) {
    dse_grass_set_enabled(Ent(L, 1), lua_toboolean(L, 2) ? 1 : 0);
    return 0;
}

int L_EcsGetGrassStats(lua_State* L) {
    helper::PushInt(L, dse_grass_get_stats(Ent(L, 1)));
    return 1;
}

// ============================================================
// Tree
// ============================================================

int L_EcsAddTree(lua_State* L) {
    uint32_t e = Ent(L, 1);
    const char* mesh_path = luaL_optstring(L, 2, "");
    dse_tree_add(e, mesh_path);
    return 0;
}

// ============================================================
// TerrainTileManager
// ============================================================

int L_EcsAddTerrainTileManager(lua_State* L) {
    dse_terrain_tile_manager_add(Ent(L, 1));
    return 0;
}

// ============================================================
// DynamicObstacle
// ============================================================

int L_EcsAddDynamicObstacle(lua_State* L) {
    uint32_t e = Ent(L, 1);
    int shape = 0;  // default Box
    if (!lua_isnoneornil(L, 2)) {
        shape = helper::CheckInt(L, 2);
    }
    dse_dynamic_obstacle_add(e, shape);
    return 0;
}

// Foliage 逐字段 get/set 由 lua_binding_ecs_foliage.gen.cpp 提供（codegen）

int L_EcsAddFoliage(lua_State* L) {
    dse_foliage_add(Ent(L, 1));
    return 0;
}

// ============================================================
// NavMeshAutoRebake
// ============================================================

int L_EcsAddNavMeshAutoRebake(lua_State* L) {
    dse_navmesh_rebake_add(Ent(L, 1));
    return 0;
}

} // namespace

void RegisterEcsRenderingTerrainBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_terrain",               L_EcsAddTerrain},
        {"set_terrain_params",        L_EcsSetTerrainParams},
        {"set_terrain_height",        L_EcsSetTerrainHeight},
        {"load_terrain_heightmap",    L_EcsLoadTerrainHeightmap},
        {"set_terrain_texture",       L_EcsSetTerrainTexture},
        {"get_terrain_lod",           L_EcsGetTerrainLod},
        {"sample_terrain_height",     L_EcsSampleTerrainHeight},
        {"set_terrain_splat_texture", L_EcsSetTerrainSplatTexture},
        {"add_water",                 L_EcsAddWater},
        {"set_water",                 L_EcsSetWater},
        {"get_water",                 L_EcsGetWater},
        {"add_grass",                 L_EcsAddGrass},
        {"set_grass_params",          L_EcsSetGrassParams},
        {"set_grass_color",           L_EcsSetGrassColor},
        {"set_grass_wind",            L_EcsSetGrassWind},
        {"set_grass_lod",             L_EcsSetGrassLod},
        {"set_grass_enabled",         L_EcsSetGrassEnabled},
        {"get_grass_stats",           L_EcsGetGrassStats},
        {"add_tree",                  L_EcsAddTree},
        {"add_terrain_tile_manager",  L_EcsAddTerrainTileManager},
        {"add_dynamic_obstacle",      L_EcsAddDynamicObstacle},
        {"add_foliage",               L_EcsAddFoliage},
        // foliage 逐字段 get/set 由 lua_binding_ecs_foliage.gen.cpp 提供
        {"add_navmesh_auto_rebake",   L_EcsAddNavMeshAutoRebake},
    });
}

} // namespace dse::runtime::lua_binding
