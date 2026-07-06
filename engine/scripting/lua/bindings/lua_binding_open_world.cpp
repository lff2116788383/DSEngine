/**
 * @file lua_binding_open_world.cpp
 * @brief Lua 绑定：开放世界系统
 *
 * 薄包装：仅做 Lua 参数读取与结果入栈，所有逻辑委托 C ABI（dse_api_extended.cpp）。
 *
 * 暴露以下系统给 Lua 脚本：
 * - dse.world_partition / dse.hlod / dse.virtual_texture / dse.geometry_clipmap
 * - dse.global_sdf / dse.ai_lod / dse.gpu_particles / dse.world_state / dse.procedural
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/native_api/dse_api.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {

namespace {

// ─── World Partition ────────────────────────────────────────────────────────

int L_WPGetLoadedCount(lua_State* L) {
    lua_pushinteger(L, dse_wp_get_loaded_count());
    return 1;
}

int L_WPForceLoad(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    dse_wp_force_load(cx, cz);
    return 0;
}

int L_WPForceUnload(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    dse_wp_force_unload(cx, cz);
    return 0;
}

int L_WPWorldToCell(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float cell_size = static_cast<float>(luaL_optnumber(L, 4, 128.0));
    int cx = 0, cz = 0;
    dse_wp_world_to_cell(x, y, z, cell_size, &cx, &cz);
    lua_pushinteger(L, cx);
    lua_pushinteger(L, cz);
    return 2;
}

int L_WPCellToWorld(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    float cell_size = static_cast<float>(luaL_optnumber(L, 3, 128.0));
    float x = 0, y = 0, z = 0;
    dse_wp_cell_to_world(cx, cz, cell_size, &x, &y, &z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

// ─── HLOD ───────────────────────────────────────────────────────────────────

int L_HLODGetClusterCount(lua_State* L) {
    lua_pushinteger(L, dse_hlod_get_cluster_count());
    return 1;
}

int L_HLODGetActiveProxyCount(lua_State* L) {
    lua_pushinteger(L, dse_hlod_get_active_proxy_count());
    return 1;
}

// ─── Virtual Texture ────────────────────────────────────────────────────────

int L_VTGetCacheHitRate(lua_State* L) {
    lua_pushnumber(L, dse_vt_get_cache_hit_rate());
    return 1;
}

int L_VTGetPageTableSize(lua_State* L) {
    lua_pushinteger(L, dse_vt_get_page_table_size());
    return 1;
}

int L_VTGetPhysicalAtlasSize(lua_State* L) {
    lua_pushinteger(L, dse_vt_get_physical_atlas_size());
    return 1;
}

int L_VTGetOccupiedPages(lua_State* L) {
    lua_pushinteger(L, dse_vt_get_occupied_pages());
    return 1;
}

// ─── Geometry Clipmap ───────────────────────────────────────────────────────

int L_ClipmapGetLevelCount(lua_State* L) {
    lua_pushinteger(L, dse_clipmap_get_level_count());
    return 1;
}

int L_ClipmapSampleHeight(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    float y = 0.0f;
    dse_clipmap_sample_height(x, z, &y);
    lua_pushnumber(L, y);
    return 1;
}

int L_ClipmapGetConfig(lua_State* L) {
    float cell_size = 0.0f;
    int levels = 0;
    dse_clipmap_get_config(&cell_size, &levels);
    lua_newtable(L);
    lua_pushinteger(L, levels);      lua_setfield(L, -2, "num_levels");
    lua_pushnumber(L, cell_size);    lua_setfield(L, -2, "base_cell_size");
    return 1;
}

// ─── Global SDF ─────────────────────────────────────────────────────────────

int L_SDFQueryDistance(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, dse_sdf_query_distance(x, y, z));
    return 1;
}

int L_SDFGetCascadeCount(lua_State* L) {
    lua_pushinteger(L, dse_sdf_get_cascade_count());
    return 1;
}

int L_SDFRebuild(lua_State* L) {
    dse_sdf_rebuild();
    return 0;
}

// ─── AI LOD Scheduler ───────────────────────────────────────────────────────

int L_AILodRegister(lua_State* L) {
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float importance = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    dse_ai_lod_register(entity_id, importance);
    return 0;
}

int L_AILodUnregister(lua_State* L) {
    dse_ai_lod_unregister(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

int L_AILodShouldTick(lua_State* L) {
    lua_pushboolean(L, dse_ai_lod_should_tick(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

int L_AILodGetLevel(lua_State* L) {
    lua_pushinteger(L, dse_ai_lod_get_level(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

int L_AILodSetForceActive(lua_State* L) {
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int force = lua_toboolean(L, 2) ? 1 : 0;
    dse_ai_lod_set_force_active(entity_id, force);
    return 0;
}

int L_AILodGetRegisteredCount(lua_State* L) {
    lua_pushinteger(L, dse_ai_lod_get_registered_count());
    return 1;
}

int L_AILodGetConfig(lua_State* L) {
    float near_dist = 0, far_dist = 0;
    int max_level = 0;
    dse_ai_lod_get_config(&near_dist, &far_dist, &max_level);
    lua_newtable(L);
    lua_pushnumber(L, near_dist);  lua_setfield(L, -2, "near_distance");
    lua_pushnumber(L, far_dist);   lua_setfield(L, -2, "far_distance");
    lua_pushinteger(L, max_level); lua_setfield(L, -2, "max_level");
    return 1;
}

// ─── GPU Particles ──────────────────────────────────────────────────────────

int L_GpuParticleSetEnabled(lua_State* L) {
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = lua_toboolean(L, 2) ? 1 : 0;
    dse_gpu_particle_set_enabled(eid, enabled);
    return 0;
}

int L_GpuParticleSetRate(lua_State* L) {
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float rate = static_cast<float>(luaL_checknumber(L, 2));
    dse_gpu_particle_set_emission_rate(eid, rate);
    return 0;
}

int L_GpuParticleSetGravity(lua_State* L) {
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float gx = static_cast<float>(luaL_checknumber(L, 2));
    float gy = static_cast<float>(luaL_checknumber(L, 3));
    float gz = static_cast<float>(luaL_checknumber(L, 4));
    dse_gpu_particle_set_gravity(eid, gx, gy, gz);
    return 0;
}

int L_GpuParticleSetWind(lua_State* L) {
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    float wz = static_cast<float>(luaL_checknumber(L, 4));
    dse_gpu_particle_set_wind(eid, wx, wy, wz);
    return 0;
}

int L_GpuParticleSetColor(lua_State* L) {
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r1 = static_cast<float>(luaL_checknumber(L, 2));
    float g1 = static_cast<float>(luaL_checknumber(L, 3));
    float b1 = static_cast<float>(luaL_checknumber(L, 4));
    float a1 = static_cast<float>(luaL_checknumber(L, 5));
    float r2 = static_cast<float>(luaL_checknumber(L, 6));
    float g2 = static_cast<float>(luaL_checknumber(L, 7));
    float b2 = static_cast<float>(luaL_checknumber(L, 8));
    float a2 = static_cast<float>(luaL_checknumber(L, 9));
    dse_gpu_particle_set_color(eid, r1, g1, b1, a1, r2, g2, b2, a2);
    return 0;
}

// ─── World State Persistence ────────────────────────────────────────────────

int L_WSPSaveAll(lua_State* L) {
    dse_wsp_save_all();
    return 0;
}

int L_WSPSaveCell(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, dse_wsp_save_cell(cx, cz));
    return 1;
}

int L_WSPLoadCell(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, dse_wsp_load_cell(cx, cz));
    return 1;
}

int L_WSPResetCell(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    dse_wsp_reset_cell(cx, cz);
    return 0;
}

int L_WSPGetDirtyCount(lua_State* L) {
    lua_pushinteger(L, dse_wsp_get_dirty_count());
    return 1;
}

int L_WSPGetTotalMods(lua_State* L) {
    lua_pushinteger(L, dse_wsp_get_total_modifications());
    return 1;
}

int L_WSPRecordDestruction(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    uint64_t entity_id = static_cast<uint64_t>(luaL_checkinteger(L, 3));
    dse_wsp_record_destruction(cx, cz, entity_id);
    return 0;
}

// ─── Procedural Generator ───────────────────────────────────────────────────

int L_ProceduralPerlin2D(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_optinteger(L, 3, 0));
    lua_pushnumber(L, dse_procedural_perlin2d(x, z, seed));
    return 1;
}

int L_ProceduralSimplex2D(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_optinteger(L, 3, 0));
    lua_pushnumber(L, dse_procedural_simplex2d(x, z, seed));
    return 1;
}

int L_ProceduralWorley2D(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_optinteger(L, 3, 0));
    lua_pushnumber(L, dse_procedural_worley2d(x, z, seed));
    return 1;
}

int L_ProceduralFBM2D(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    int octaves = static_cast<int>(luaL_optinteger(L, 3, 6));
    float frequency = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float lacunarity = static_cast<float>(luaL_optnumber(L, 5, 2.0));
    float persistence = static_cast<float>(luaL_optnumber(L, 6, 0.5));
    uint32_t seed = static_cast<uint32_t>(luaL_optinteger(L, 7, 0));
    lua_pushnumber(L, dse_procedural_fbm2d(x, z, octaves, frequency, lacunarity, persistence, seed));
    return 1;
}

int L_ProceduralRandomSeed(lua_State* L) {
    uint64_t seed = static_cast<uint64_t>(luaL_checkinteger(L, 1));
    dse_procedural_random_seed(seed);
    return 0;
}

int L_ProceduralRandomFloat(lua_State* L) {
    float min_val = static_cast<float>(luaL_checknumber(L, 1));
    float max_val = static_cast<float>(luaL_checknumber(L, 2));
    lua_pushnumber(L, dse_procedural_random_float(min_val, max_val));
    return 1;
}

} // anonymous namespace

// ─── Registration ───────────────────────────────────────────────────────────

void RegisterOpenWorldBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    // dse.world_partition
    lua_newtable(L);
    set_fn("get_loaded_count", L_WPGetLoadedCount);
    set_fn("force_load", L_WPForceLoad);
    set_fn("force_unload", L_WPForceUnload);
    set_fn("world_to_cell", L_WPWorldToCell);
    set_fn("cell_to_world", L_WPCellToWorld);
    lua_setfield(L, -2, "world_partition");

    // dse.hlod
    lua_newtable(L);
    set_fn("get_cluster_count", L_HLODGetClusterCount);
    set_fn("get_active_proxy_count", L_HLODGetActiveProxyCount);
    lua_setfield(L, -2, "hlod");

    // dse.virtual_texture
    lua_newtable(L);
    set_fn("get_cache_hit_rate", L_VTGetCacheHitRate);
    set_fn("get_page_table_size", L_VTGetPageTableSize);
    set_fn("get_physical_atlas_size", L_VTGetPhysicalAtlasSize);
    set_fn("get_occupied_pages", L_VTGetOccupiedPages);
    lua_setfield(L, -2, "virtual_texture");

    // dse.geometry_clipmap
    lua_newtable(L);
    set_fn("get_level_count", L_ClipmapGetLevelCount);
    set_fn("sample_height", L_ClipmapSampleHeight);
    set_fn("get_config", L_ClipmapGetConfig);
    lua_setfield(L, -2, "geometry_clipmap");

    // dse.global_sdf
    lua_newtable(L);
    set_fn("query_distance", L_SDFQueryDistance);
    set_fn("get_cascade_count", L_SDFGetCascadeCount);
    set_fn("rebuild", L_SDFRebuild);
    lua_setfield(L, -2, "global_sdf");

    // dse.ai_lod
    lua_newtable(L);
    set_fn("register_entity", L_AILodRegister);
    set_fn("unregister_entity", L_AILodUnregister);
    set_fn("should_tick", L_AILodShouldTick);
    set_fn("get_level", L_AILodGetLevel);
    set_fn("set_force_active", L_AILodSetForceActive);
    set_fn("get_registered_count", L_AILodGetRegisteredCount);
    set_fn("get_config", L_AILodGetConfig);
    lua_setfield(L, -2, "ai_lod");

    // dse.gpu_particles
    lua_newtable(L);
    set_fn("set_enabled", L_GpuParticleSetEnabled);
    set_fn("set_emission_rate", L_GpuParticleSetRate);
    set_fn("set_gravity", L_GpuParticleSetGravity);
    set_fn("set_wind", L_GpuParticleSetWind);
    set_fn("set_color", L_GpuParticleSetColor);
    lua_setfield(L, -2, "gpu_particles");

    // dse.world_state
    lua_newtable(L);
    set_fn("save_all", L_WSPSaveAll);
    set_fn("save_cell", L_WSPSaveCell);
    set_fn("load_cell", L_WSPLoadCell);
    set_fn("reset_cell", L_WSPResetCell);
    set_fn("get_dirty_count", L_WSPGetDirtyCount);
    set_fn("get_total_modifications", L_WSPGetTotalMods);
    set_fn("record_destruction", L_WSPRecordDestruction);
    lua_setfield(L, -2, "world_state");

    // dse.procedural
    lua_newtable(L);
    set_fn("perlin2d", L_ProceduralPerlin2D);
    set_fn("simplex2d", L_ProceduralSimplex2D);
    set_fn("worley2d", L_ProceduralWorley2D);
    set_fn("fbm2d", L_ProceduralFBM2D);
    set_fn("random_seed", L_ProceduralRandomSeed);
    set_fn("random_float", L_ProceduralRandomFloat);
    lua_setfield(L, -2, "procedural");
}

} // namespace dse::runtime::lua_binding
