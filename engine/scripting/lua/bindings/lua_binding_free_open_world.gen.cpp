/**
 * @file lua_binding_free_open_world.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}
#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace dse::runtime::lua_binding {
namespace {

int L_dse_wp_get_loaded_count(lua_State* L) {
    int _ret = dse_wp_get_loaded_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_wp_force_load(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_wp_force_load(cx, cz);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_wp_force_unload(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_wp_force_unload(cx, cz);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_hlod_get_cluster_count(lua_State* L) {
    int _ret = dse_hlod_get_cluster_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_hlod_get_active_proxy_count(lua_State* L) {
    int _ret = dse_hlod_get_active_proxy_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_vt_get_cache_hit_rate(lua_State* L) {
    float _ret = dse_vt_get_cache_hit_rate();
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_vt_get_page_table_size(lua_State* L) {
    int _ret = dse_vt_get_page_table_size();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_vt_get_physical_atlas_size(lua_State* L) {
    int _ret = dse_vt_get_physical_atlas_size();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_vt_get_occupied_pages(lua_State* L) {
    int _ret = dse_vt_get_occupied_pages();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_clipmap_get_level_count(lua_State* L) {
    int _ret = dse_clipmap_get_level_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_sdf_query_distance(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float _ret = dse_sdf_query_distance(x, y, z);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_sdf_get_cascade_count(lua_State* L) {
    int _ret = dse_sdf_get_cascade_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_sdf_rebuild(lua_State* L) {
    int _ret = dse_sdf_rebuild();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ai_lod_register(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float importance = static_cast<float>(luaL_checknumber(L, 2));
    dse_ai_lod_register(e, importance);
    return 0;
}

int L_dse_ai_lod_unregister(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ai_lod_unregister(e);
    return 0;
}

int L_dse_ai_lod_should_tick(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ai_lod_should_tick(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ai_lod_get_level(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ai_lod_get_level(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ai_lod_set_force_active(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int force = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ai_lod_set_force_active(e, force);
    return 0;
}

int L_dse_ai_lod_get_registered_count(lua_State* L) {
    int _ret = dse_ai_lod_get_registered_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_gpu_particle_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_gpu_particle_set_enabled(e, enabled);
    return 0;
}

int L_dse_gpu_particle_set_emission_rate(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float rate = static_cast<float>(luaL_checknumber(L, 2));
    dse_gpu_particle_set_emission_rate(e, rate);
    return 0;
}

int L_dse_gpu_particle_set_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float gx = static_cast<float>(luaL_checknumber(L, 2));
    float gy = static_cast<float>(luaL_checknumber(L, 3));
    float gz = static_cast<float>(luaL_checknumber(L, 4));
    dse_gpu_particle_set_gravity(e, gx, gy, gz);
    return 0;
}

int L_dse_gpu_particle_set_wind(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    float wz = static_cast<float>(luaL_checknumber(L, 4));
    dse_gpu_particle_set_wind(e, wx, wy, wz);
    return 0;
}

int L_dse_gpu_particle_set_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r1 = static_cast<float>(luaL_checknumber(L, 2));
    float g1 = static_cast<float>(luaL_checknumber(L, 3));
    float b1 = static_cast<float>(luaL_checknumber(L, 4));
    float a1 = static_cast<float>(luaL_checknumber(L, 5));
    float r2 = static_cast<float>(luaL_checknumber(L, 6));
    float g2 = static_cast<float>(luaL_checknumber(L, 7));
    float b2 = static_cast<float>(luaL_checknumber(L, 8));
    float a2 = static_cast<float>(luaL_checknumber(L, 9));
    dse_gpu_particle_set_color(e, r1, g1, b1, a1, r2, g2, b2, a2);
    return 0;
}

int L_dse_wsp_save_all(lua_State* L) {
    int _ret = dse_wsp_save_all();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_wsp_save_cell(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_wsp_save_cell(cx, cz);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_wsp_load_cell(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_wsp_load_cell(cx, cz);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_wsp_reset_cell(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_wsp_reset_cell(cx, cz);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_wsp_get_dirty_count(lua_State* L) {
    int _ret = dse_wsp_get_dirty_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_wsp_get_total_modifications(lua_State* L) {
    int _ret = dse_wsp_get_total_modifications();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_wsp_record_destruction(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    dse_wsp_record_destruction(cx, cz, entity_id);
    return 0;
}

int L_dse_procedural_perlin2d(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    float _ret = dse_procedural_perlin2d(x, y, seed);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_procedural_simplex2d(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    float _ret = dse_procedural_simplex2d(x, y, seed);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_procedural_worley2d(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    float _ret = dse_procedural_worley2d(x, y, seed);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_procedural_fbm2d(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    int octaves = static_cast<int>(luaL_checkinteger(L, 3));
    float frequency = static_cast<float>(luaL_checknumber(L, 4));
    float lacunarity = static_cast<float>(luaL_checknumber(L, 5));
    float persistence = static_cast<float>(luaL_checknumber(L, 6));
    uint32_t seed = static_cast<uint32_t>(luaL_checkinteger(L, 7));
    float _ret = dse_procedural_fbm2d(x, y, octaves, frequency, lacunarity, persistence, seed);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_procedural_random_seed(lua_State* L) {
    uint32_t seed = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_procedural_random_seed(seed);
    return 0;
}

int L_dse_procedural_random_float(lua_State* L) {
    float min_val = static_cast<float>(luaL_checknumber(L, 1));
    float max_val = static_cast<float>(luaL_checknumber(L, 2));
    float _ret = dse_procedural_random_float(min_val, max_val);
    lua_pushnumber(L, _ret);
    return 1;
}

} // namespace

void RegisterOpenWorldBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"wpgetloadedcount", L_dse_wp_get_loaded_count},
        {"wpforceload", L_dse_wp_force_load},
        {"wpforceunload", L_dse_wp_force_unload},
        {"hlodgetclustercount", L_dse_hlod_get_cluster_count},
        {"hlodgetactiveproxycount", L_dse_hlod_get_active_proxy_count},
        {"vtgetcachehitrate", L_dse_vt_get_cache_hit_rate},
        {"vtgetpagetablesize", L_dse_vt_get_page_table_size},
        {"vtgetphysicalatlassize", L_dse_vt_get_physical_atlas_size},
        {"vtgetoccupiedpages", L_dse_vt_get_occupied_pages},
        {"clipmapgetlevelcount", L_dse_clipmap_get_level_count},
        {"sdfquerydistance", L_dse_sdf_query_distance},
        {"sdfgetcascadecount", L_dse_sdf_get_cascade_count},
        {"sdfrebuild", L_dse_sdf_rebuild},
        {"ailodregister", L_dse_ai_lod_register},
        {"ailodunregister", L_dse_ai_lod_unregister},
        {"ailodshouldtick", L_dse_ai_lod_should_tick},
        {"ailodgetlevel", L_dse_ai_lod_get_level},
        {"ailodsetforceactive", L_dse_ai_lod_set_force_active},
        {"ailodgetregisteredcount", L_dse_ai_lod_get_registered_count},
        {"gpuparticlesetenabled", L_dse_gpu_particle_set_enabled},
        {"gpuparticlesetrate", L_dse_gpu_particle_set_emission_rate},
        {"gpuparticlesetgravity", L_dse_gpu_particle_set_gravity},
        {"gpuparticlesetwind", L_dse_gpu_particle_set_wind},
        {"gpuparticlesetcolor", L_dse_gpu_particle_set_color},
        {"wspsaveall", L_dse_wsp_save_all},
        {"wspsavecell", L_dse_wsp_save_cell},
        {"wsploadcell", L_dse_wsp_load_cell},
        {"wspresetcell", L_dse_wsp_reset_cell},
        {"wspgetdirtycount", L_dse_wsp_get_dirty_count},
        {"wspgettotalmods", L_dse_wsp_get_total_modifications},
        {"wsprecorddestruction", L_dse_wsp_record_destruction},
        {"proceduralperlin2d", L_dse_procedural_perlin2d},
        {"proceduralsimplex2d", L_dse_procedural_simplex2d},
        {"proceduralworley2d", L_dse_procedural_worley2d},
        {"proceduralfbm2d", L_dse_procedural_fbm2d},
        {"proceduralrandomseed", L_dse_procedural_random_seed},
        {"proceduralrandomfloat", L_dse_procedural_random_float},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
