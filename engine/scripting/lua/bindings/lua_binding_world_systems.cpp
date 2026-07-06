/**
 * @file lua_binding_world_systems.cpp
 * @brief Lua 薄包装：6 大开放世界系统
 *
 * 仅做 Lua 栈 ↔ C ABI 参数转换，所有逻辑委托 dse_* 函数。
 * 1. Spline (道路/河流)         — dse.spline
 * 2. Ocean (大规模海洋)          — dse.ocean
 * 3. World Editor Tools (编辑器) — dse.editor
 * 4. Virtual Shadow Map (虚拟阴影)— dse.vsm
 * 5. EQS (环境查询)              — dse.eqs
 * 6. Asset Distribution (打包分发)— dse.distribution
 *
 * Total: 86 Lua C functions
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include <vector>
#include <string>
#include <cstring>

namespace dse::runtime::lua_binding {
namespace {

// ============================================================
// §1  dse.spline — 样条系统 (14 functions)
// ============================================================

static int l_spline_init(lua_State* L) {
    lua_pushinteger(L, dse_spline_init());
    return 1;
}

static int l_spline_shutdown(lua_State* L) {
    dse_spline_shutdown();
    return 0;
}

static int l_spline_create(lua_State* L) {
    uint32_t id = dse_spline_create(luaL_checkstring(L, 1));
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

static int l_spline_destroy(lua_State* L) {
    dse_spline_destroy(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_spline_add_point(lua_State* L) {
    dse_spline_add_point(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                         static_cast<float>(luaL_checknumber(L, 2)),
                         static_cast<float>(luaL_checknumber(L, 3)),
                         static_cast<float>(luaL_checknumber(L, 4)),
                         static_cast<float>(luaL_optnumber(L, 5, 4.0)));
    return 0;
}

static int l_spline_set_point(lua_State* L) {
    dse_spline_set_point(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                         static_cast<int>(luaL_checkinteger(L, 2)),
                         static_cast<float>(luaL_checknumber(L, 3)),
                         static_cast<float>(luaL_checknumber(L, 4)),
                         static_cast<float>(luaL_checknumber(L, 5)),
                         static_cast<float>(luaL_optnumber(L, 6, 4.0)));
    return 0;
}

static int l_spline_remove_point(lua_State* L) {
    dse_spline_remove_point(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                            static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

static int l_spline_get_point_count(lua_State* L) {
    lua_pushinteger(L, dse_spline_get_point_count(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

static int l_spline_get_length(lua_State* L) {
    lua_pushnumber(L, dse_spline_get_length(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

static int l_spline_evaluate(lua_State* L) {
    float xyz[3] = {0, 0, 0};
    dse_spline_evaluate(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                        static_cast<float>(luaL_checknumber(L, 2)), xyz);
    lua_pushnumber(L, xyz[0]); lua_pushnumber(L, xyz[1]); lua_pushnumber(L, xyz[2]);
    return 3;
}

static int l_spline_evaluate_distance(lua_State* L) {
    float xyz[3] = {0, 0, 0};
    dse_spline_evaluate_distance(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                 static_cast<float>(luaL_checknumber(L, 2)), xyz);
    lua_pushnumber(L, xyz[0]); lua_pushnumber(L, xyz[1]); lua_pushnumber(L, xyz[2]);
    return 3;
}

static int l_spline_find_nearest(lua_State* L) {
    float t = dse_spline_find_nearest(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                      static_cast<float>(luaL_checknumber(L, 2)),
                                      static_cast<float>(luaL_checknumber(L, 3)),
                                      static_cast<float>(luaL_checknumber(L, 4)));
    lua_pushnumber(L, t);
    return 1;
}

static int l_spline_gen_road(lua_State* L) {
    int count = dse_spline_gen_road(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                    static_cast<float>(luaL_optnumber(L, 2, 1.0)),
                                    static_cast<int>(luaL_optinteger(L, 3, 4)));
    lua_pushinteger(L, count);
    return 1;
}

static int l_spline_gen_river(lua_State* L) {
    int count = dse_spline_gen_river(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                     static_cast<float>(luaL_optnumber(L, 2, 2.0)),
                                     static_cast<float>(luaL_optnumber(L, 3, 2.0)));
    lua_pushinteger(L, count);
    return 1;
}

// ============================================================
// §2  dse.ocean — 海洋系统 (10 functions)
// ============================================================

static int l_ocean_init(lua_State* L) {
    int fft_res = 256;
    float tile_size = 512.0f;
    float wind_speed = 8.0f;
    float choppiness = 1.0f;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "fft_resolution");
        if (!lua_isnil(L, -1)) fft_res = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "tile_size");
        if (!lua_isnil(L, -1)) tile_size = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "wind_speed");
        if (!lua_isnil(L, -1)) wind_speed = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "choppiness");
        if (!lua_isnil(L, -1)) choppiness = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pushinteger(L, dse_ocean_init(fft_res, tile_size, wind_speed, choppiness));
    return 1;
}

static int l_ocean_shutdown(lua_State* L) {
    dse_ocean_shutdown();
    return 0;
}

static int l_ocean_update(lua_State* L) {
    dse_ocean_update(static_cast<float>(luaL_checknumber(L, 1)),
                     static_cast<float>(luaL_checknumber(L, 2)),
                     static_cast<float>(luaL_checknumber(L, 3)),
                     static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}

static int l_ocean_get_height(lua_State* L) {
    lua_pushnumber(L, dse_ocean_get_height(static_cast<float>(luaL_checknumber(L, 1)),
                                           static_cast<float>(luaL_checknumber(L, 2))));
    return 1;
}

static int l_ocean_get_normal(lua_State* L) {
    float xyz[3] = {0, 1, 0};
    dse_ocean_get_normal(static_cast<float>(luaL_checknumber(L, 1)),
                         static_cast<float>(luaL_checknumber(L, 2)), xyz);
    lua_pushnumber(L, xyz[0]); lua_pushnumber(L, xyz[1]); lua_pushnumber(L, xyz[2]);
    return 3;
}

static int l_ocean_get_foam(lua_State* L) {
    lua_pushnumber(L, dse_ocean_get_foam(static_cast<float>(luaL_checknumber(L, 1)),
                                         static_cast<float>(luaL_checknumber(L, 2))));
    return 1;
}

static int l_ocean_set_wind(lua_State* L) {
    dse_ocean_set_wind(static_cast<float>(luaL_checknumber(L, 1)),
                       static_cast<float>(luaL_checknumber(L, 2)),
                       static_cast<float>(luaL_checknumber(L, 3)));
    return 0;
}

static int l_ocean_set_choppiness(lua_State* L) {
    dse_ocean_set_choppiness(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

static int l_ocean_get_stats(lua_State* L) {
    int total = 0, visible = 0, fft_res = 0;
    float max_height = 0;
    dse_ocean_get_stats(&total, &visible, &fft_res, &max_height);
    lua_newtable(L);
    lua_pushinteger(L, total);       lua_setfield(L, -2, "total_tiles");
    lua_pushinteger(L, visible);     lua_setfield(L, -2, "visible_tiles");
    lua_pushinteger(L, fft_res);     lua_setfield(L, -2, "fft_resolution");
    lua_pushnumber(L, max_height);   lua_setfield(L, -2, "max_height");
    return 1;
}

static int l_ocean_get_lod_count(lua_State* L) {
    lua_pushinteger(L, dse_ocean_get_lod_count());
    return 1;
}

// ============================================================
// §3  dse.editor — 编辑器世界工具 (14 functions)
// ============================================================

static int l_editor_init(lua_State* L) {
    lua_pushinteger(L, dse_editor_init());
    return 1;
}

static int l_editor_shutdown(lua_State* L) {
    dse_editor_shutdown();
    return 0;
}

static int l_editor_terrain_brush(lua_State* L) {
    int result = dse_editor_terrain_brush(static_cast<int>(luaL_checkinteger(L, 1)),
                                          static_cast<float>(luaL_checknumber(L, 2)),
                                          static_cast<float>(luaL_checknumber(L, 3)),
                                          static_cast<float>(luaL_checknumber(L, 4)),
                                          static_cast<float>(luaL_checknumber(L, 5)),
                                          static_cast<float>(luaL_optnumber(L, 6, 0.5)),
                                          static_cast<float>(luaL_optnumber(L, 7, 0.5)));
    lua_pushinteger(L, result);
    return 1;
}

static int l_editor_brush_preview(lua_State* L) {
    float min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    dse_editor_brush_preview(static_cast<float>(luaL_checknumber(L, 1)),
                             static_cast<float>(luaL_checknumber(L, 2)),
                             static_cast<float>(luaL_checknumber(L, 3)),
                             static_cast<float>(luaL_checknumber(L, 4)),
                             &min_x, &min_y, &max_x, &max_y);
    lua_pushnumber(L, min_x); lua_pushnumber(L, min_y);
    lua_pushnumber(L, max_x); lua_pushnumber(L, max_y);
    return 4;
}

static int l_editor_place_foliage(lua_State* L) {
    int result = dse_editor_place_foliage(static_cast<float>(luaL_checknumber(L, 1)),
                                          static_cast<float>(luaL_checknumber(L, 2)),
                                          static_cast<float>(luaL_checknumber(L, 3)),
                                          static_cast<float>(luaL_checknumber(L, 4)),
                                          static_cast<float>(luaL_optnumber(L, 5, 0.5)),
                                          luaL_optstring(L, 6, "default_tree"));
    lua_pushinteger(L, result);
    return 1;
}

static int l_editor_erase_foliage(lua_State* L) {
    lua_pushinteger(L, dse_editor_erase_foliage(static_cast<float>(luaL_checknumber(L, 1)),
                                                static_cast<float>(luaL_checknumber(L, 2)),
                                                static_cast<float>(luaL_checknumber(L, 3)),
                                                static_cast<float>(luaL_checknumber(L, 4))));
    return 1;
}

static int l_editor_get_foliage_count(lua_State* L) {
    lua_pushinteger(L, dse_editor_get_foliage_count());
    return 1;
}

static int l_editor_begin_road(lua_State* L) {
    lua_pushinteger(L, dse_editor_begin_road(static_cast<float>(luaL_optnumber(L, 1, 4.0))));
    return 1;
}

static int l_editor_add_road_point(lua_State* L) {
    dse_editor_add_road_point(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                              static_cast<float>(luaL_checknumber(L, 2)),
                              static_cast<float>(luaL_checknumber(L, 3)),
                              static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}

static int l_editor_end_road(lua_State* L) {
    dse_editor_end_road(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_editor_update_partition_vis(lua_State* L) {
    dse_editor_update_partition_vis(static_cast<float>(luaL_checknumber(L, 1)),
                                    static_cast<float>(luaL_checknumber(L, 2)),
                                    static_cast<float>(luaL_checknumber(L, 3)),
                                    static_cast<float>(luaL_optnumber(L, 4, 256.0)));
    return 0;
}

static int l_editor_get_cell_count(lua_State* L) {
    lua_pushinteger(L, dse_editor_get_cell_count());
    return 1;
}

static int l_editor_undo(lua_State* L) {
    lua_pushboolean(L, dse_editor_undo());
    return 1;
}

static int l_editor_redo(lua_State* L) {
    lua_pushboolean(L, dse_editor_redo());
    return 1;
}

// ============================================================
// §4  dse.vsm — 虚拟阴影贴图 (12 functions)
// ============================================================

static int l_vsm_init(lua_State* L) {
    uint32_t virtual_res = 16384;
    uint32_t page_size = 128;
    uint32_t pool_pages = 512;
    uint32_t clipmap_levels = 6;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "virtual_resolution");
        if (!lua_isnil(L, -1)) virtual_res = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "page_size");
        if (!lua_isnil(L, -1)) page_size = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "pool_pages");
        if (!lua_isnil(L, -1)) pool_pages = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "clipmap_levels");
        if (!lua_isnil(L, -1)) clipmap_levels = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }
    lua_pushinteger(L, dse_vsm_init(virtual_res, page_size, pool_pages, clipmap_levels));
    return 1;
}

static int l_vsm_shutdown(lua_State* L) {
    dse_vsm_shutdown();
    return 0;
}

static int l_vsm_register_light(lua_State* L) {
    uint32_t reg = dse_vsm_register_light(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                          lua_toboolean(L, 2) ? 1 : 0,
                                          static_cast<float>(luaL_optnumber(L, 3, 0)),
                                          static_cast<float>(luaL_optnumber(L, 4, -1)),
                                          static_cast<float>(luaL_optnumber(L, 5, 0)));
    lua_pushinteger(L, static_cast<lua_Integer>(reg));
    return 1;
}

static int l_vsm_unregister_light(lua_State* L) {
    dse_vsm_unregister_light(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_vsm_begin_frame(lua_State* L) {
    dse_vsm_begin_frame(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                        static_cast<float>(luaL_checknumber(L, 2)),
                        static_cast<float>(luaL_checknumber(L, 3)),
                        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}

static int l_vsm_end_frame(lua_State* L) {
    dse_vsm_end_frame();
    return 0;
}

static int l_vsm_invalidate(lua_State* L) {
    dse_vsm_invalidate(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                       static_cast<float>(luaL_checknumber(L, 2)),
                       static_cast<float>(luaL_checknumber(L, 3)),
                       static_cast<float>(luaL_checknumber(L, 4)),
                       static_cast<float>(luaL_checknumber(L, 5)),
                       static_cast<float>(luaL_checknumber(L, 6)),
                       static_cast<float>(luaL_checknumber(L, 7)));
    return 0;
}

static int l_vsm_mark_rendered(lua_State* L) {
    dse_vsm_mark_page_rendered(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                               static_cast<uint32_t>(luaL_checkinteger(L, 2)),
                               static_cast<uint32_t>(luaL_checkinteger(L, 3)),
                               static_cast<uint32_t>(luaL_checkinteger(L, 4)));
    return 0;
}

static int l_vsm_get_pages_to_render(lua_State* L) {
    lua_pushinteger(L, dse_vsm_get_pages_to_render());
    return 1;
}

static int l_vsm_lookup_page(lua_State* L) {
    uint32_t px = 0, py = 0;
    int found = dse_vsm_lookup_page(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                    static_cast<uint32_t>(luaL_checkinteger(L, 2)),
                                    static_cast<uint32_t>(luaL_checkinteger(L, 3)),
                                    static_cast<uint32_t>(luaL_checkinteger(L, 4)),
                                    &px, &py);
    if (found) {
        lua_pushboolean(L, 1);
        lua_pushinteger(L, static_cast<lua_Integer>(px));
        lua_pushinteger(L, static_cast<lua_Integer>(py));
        return 3;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int l_vsm_get_stats(lua_State* L) {
    int total = 0, mapped = 0, dirty = 0, rendered = 0, cache_hit = 0, pool_usage = 0;
    dse_vsm_get_stats(&total, &mapped, &dirty, &rendered, &cache_hit, &pool_usage);
    lua_newtable(L);
    lua_pushinteger(L, total);     lua_setfield(L, -2, "total_pages");
    lua_pushinteger(L, mapped);    lua_setfield(L, -2, "mapped_pages");
    lua_pushinteger(L, dirty);     lua_setfield(L, -2, "dirty_pages");
    lua_pushinteger(L, rendered);  lua_setfield(L, -2, "rendered_this_frame");
    lua_pushinteger(L, cache_hit); lua_setfield(L, -2, "cache_hit_percent");
    lua_pushinteger(L, pool_usage);lua_setfield(L, -2, "pool_usage_percent");
    return 1;
}

static int l_vsm_get_clipmap_levels(lua_State* L) {
    lua_pushinteger(L, dse_vsm_get_clipmap_levels());
    return 1;
}

// ============================================================
// §5  dse.eqs — 环境查询系统 (12 functions)
// ============================================================

static int l_eqs_init(lua_State* L) {
    lua_pushinteger(L, dse_eqs_init());
    return 1;
}

static int l_eqs_shutdown(lua_State* L) {
    dse_eqs_shutdown();
    return 0;
}

static int l_eqs_create_template(lua_State* L) {
    uint32_t id = dse_eqs_create_template(luaL_checkstring(L, 1));
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

static int l_eqs_destroy_template(lua_State* L) {
    dse_eqs_destroy_template(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_eqs_set_generator(lua_State* L) {
    dse_eqs_set_generator(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                          static_cast<int>(luaL_checkinteger(L, 2)),
                          static_cast<float>(luaL_optnumber(L, 3, 20.0)),
                          static_cast<float>(luaL_optnumber(L, 4, 2.0)),
                          static_cast<int>(luaL_optinteger(L, 5, 200)));
    return 0;
}

static int l_eqs_add_scorer(lua_State* L) {
    dse_eqs_add_scorer(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                       static_cast<int>(luaL_checkinteger(L, 2)),
                       static_cast<float>(luaL_optnumber(L, 3, 1.0)),
                       lua_toboolean(L, 4) ? 1 : 0,
                       static_cast<float>(luaL_optnumber(L, 5, 100.0)));
    return 0;
}

static int l_eqs_clear_scorers(lua_State* L) {
    dse_eqs_clear_scorers(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_eqs_set_combine_mode(lua_State* L) {
    dse_eqs_set_combine_mode(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                             static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

static int l_eqs_set_max_results(lua_State* L) {
    dse_eqs_set_max_results(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                            static_cast<uint32_t>(luaL_checkinteger(L, 2)));
    return 0;
}

static int l_eqs_execute(lua_State* L) {
    float out[7] = {0, 0, 0, 0, 0, 0, 0};
    dse_eqs_execute(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                    static_cast<float>(luaL_checknumber(L, 2)),
                    static_cast<float>(luaL_checknumber(L, 3)),
                    static_cast<float>(luaL_checknumber(L, 4)), out);
    lua_newtable(L);
    lua_pushnumber(L, out[0]); lua_setfield(L, -2, "best_x");
    lua_pushnumber(L, out[1]); lua_setfield(L, -2, "best_y");
    lua_pushnumber(L, out[2]); lua_setfield(L, -2, "best_z");
    lua_pushnumber(L, out[3]); lua_setfield(L, -2, "best_score");
    lua_pushinteger(L, static_cast<int>(out[4])); lua_setfield(L, -2, "total_generated");
    lua_pushinteger(L, static_cast<int>(out[5])); lua_setfield(L, -2, "valid_count");
    lua_pushnumber(L, out[6]); lua_setfield(L, -2, "query_time_ms");
    return 1;
}

static int l_eqs_get_template_count(lua_State* L) {
    lua_pushinteger(L, dse_eqs_get_template_count());
    return 1;
}

static int l_eqs_execute_at(lua_State* L) {
    float out[5] = {0, 0, 0, 0, 0};
    dse_eqs_execute_at(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                       static_cast<float>(luaL_checknumber(L, 2)),
                       static_cast<float>(luaL_checknumber(L, 3)),
                       static_cast<float>(luaL_checknumber(L, 4)),
                       static_cast<float>(luaL_checknumber(L, 5)),
                       static_cast<float>(luaL_checknumber(L, 6)),
                       static_cast<float>(luaL_checknumber(L, 7)), out);
    lua_newtable(L);
    lua_pushnumber(L, out[0]); lua_setfield(L, -2, "best_x");
    lua_pushnumber(L, out[1]); lua_setfield(L, -2, "best_y");
    lua_pushnumber(L, out[2]); lua_setfield(L, -2, "best_z");
    lua_pushnumber(L, out[3]); lua_setfield(L, -2, "best_score");
    lua_pushinteger(L, static_cast<int>(out[4])); lua_setfield(L, -2, "valid_count");
    return 1;
}

// ============================================================
// §6  dse.distribution — 打包分发管线 (14 functions)
// ============================================================

static int l_dist_init(lua_State* L) {
    float cell_size = 512.0f;
    int max_downloads = 4;
    const char* cdn_url = nullptr;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "cell_size");
        if (!lua_isnil(L, -1)) cell_size = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "max_downloads");
        if (!lua_isnil(L, -1)) max_downloads = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "cdn_url");
        if (!lua_isnil(L, -1)) cdn_url = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    lua_pushinteger(L, dse_dist_init(cell_size, max_downloads, cdn_url));
    return 1;
}

static int l_dist_shutdown(lua_State* L) {
    dse_dist_shutdown();
    return 0;
}

static int l_dist_load_manifest(lua_State* L) {
    lua_pushboolean(L, dse_dist_load_manifest(luaL_checkstring(L, 1)));
    return 1;
}

static int l_dist_save_manifest(lua_State* L) {
    lua_pushboolean(L, dse_dist_save_manifest(luaL_checkstring(L, 1)));
    return 1;
}

static int l_dist_package_cell(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    int lod = static_cast<int>(luaL_optinteger(L, 3, 0));

    // Build C string array from Lua table
    std::vector<std::string> assets_storage;
    std::vector<const char*> assets;
    if (lua_istable(L, 4)) {
        lua_Integer n = static_cast<lua_Integer>(lua_rawlen(L, 4));
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L, 4, i);
            if (lua_isstring(L, -1)) {
                assets_storage.emplace_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
        for (const auto& s : assets_storage) assets.push_back(s.c_str());
    }

    int result = dse_dist_package_cell(cx, cz, lod, assets.data(), static_cast<int>(assets.size()));
    lua_pushinteger(L, result);
    return 1;
}

static int l_dist_request_download(lua_State* L) {
    dse_dist_request_download(luaL_checkstring(L, 1));
    return 0;
}

static int l_dist_cancel_download(lua_State* L) {
    dse_dist_cancel_download(luaL_checkstring(L, 1));
    return 0;
}

static int l_dist_update_priorities(lua_State* L) {
    dse_dist_update_priorities(static_cast<float>(luaL_checknumber(L, 1)),
                               static_cast<float>(luaL_checknumber(L, 2)),
                               static_cast<float>(luaL_checknumber(L, 3)));
    return 0;
}

static int l_dist_tick(lua_State* L) {
    dse_dist_tick(static_cast<float>(luaL_optnumber(L, 1, 0.016)));
    return 0;
}

static int l_dist_is_installed(lua_State* L) {
    lua_pushboolean(L, dse_dist_is_installed(luaL_checkstring(L, 1)));
    return 1;
}

static int l_dist_get_stats(lua_State* L) {
    int total = 0, installed = 0, downloading = 0, pending = 0;
    double dl_bytes = 0, speed = 0;
    dse_dist_get_stats(&total, &installed, &downloading, &pending, &dl_bytes, &speed);
    lua_newtable(L);
    lua_pushinteger(L, total);       lua_setfield(L, -2, "total_packages");
    lua_pushinteger(L, installed);   lua_setfield(L, -2, "installed");
    lua_pushinteger(L, downloading); lua_setfield(L, -2, "downloading");
    lua_pushinteger(L, pending);     lua_setfield(L, -2, "pending");
    lua_pushnumber(L, dl_bytes);     lua_setfield(L, -2, "downloaded_bytes");
    lua_pushnumber(L, speed);        lua_setfield(L, -2, "speed_bps");
    return 1;
}

static int l_dist_get_missing(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 512.0));

    // C ABI 使用 null 分隔的缓冲区
    char buf[8192];
    int count = dse_dist_get_missing(x, y, z, radius, buf, sizeof(buf));

    lua_newtable(L);
    int offset = 0;
    int idx = 1;
    for (int i = 0; i < count && offset < static_cast<int>(sizeof(buf)); ++i) {
        const char* name = buf + offset;
        lua_pushstring(L, name);
        lua_rawseti(L, -2, idx++);
        offset += static_cast<int>(strlen(name)) + 1;
    }
    return 1;
}

static int l_dist_verify(lua_State* L) {
    lua_pushboolean(L, dse_dist_verify(luaL_checkstring(L, 1)));
    return 1;
}

static int l_dist_get_disk_usage(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(dse_dist_get_disk_usage()));
    return 1;
}

// ============================================================
// Registration
// ============================================================

static const luaL_Reg spline_funcs[] = {
    {"init", l_spline_init}, {"shutdown", l_spline_shutdown},
    {"create", l_spline_create}, {"destroy", l_spline_destroy},
    {"add_point", l_spline_add_point}, {"set_point", l_spline_set_point},
    {"remove_point", l_spline_remove_point}, {"get_point_count", l_spline_get_point_count},
    {"get_length", l_spline_get_length}, {"evaluate", l_spline_evaluate},
    {"evaluate_distance", l_spline_evaluate_distance}, {"find_nearest", l_spline_find_nearest},
    {"gen_road", l_spline_gen_road}, {"gen_river", l_spline_gen_river},
    {nullptr, nullptr}
};

static const luaL_Reg ocean_funcs[] = {
    {"init", l_ocean_init}, {"shutdown", l_ocean_shutdown},
    {"update", l_ocean_update}, {"get_height", l_ocean_get_height},
    {"get_normal", l_ocean_get_normal}, {"get_foam", l_ocean_get_foam},
    {"set_wind", l_ocean_set_wind}, {"set_choppiness", l_ocean_set_choppiness},
    {"get_stats", l_ocean_get_stats}, {"get_lod_count", l_ocean_get_lod_count},
    {nullptr, nullptr}
};

static const luaL_Reg editor_funcs[] = {
    {"init", l_editor_init}, {"shutdown", l_editor_shutdown},
    {"terrain_brush", l_editor_terrain_brush}, {"brush_preview", l_editor_brush_preview},
    {"place_foliage", l_editor_place_foliage}, {"erase_foliage", l_editor_erase_foliage},
    {"get_foliage_count", l_editor_get_foliage_count},
    {"begin_road", l_editor_begin_road}, {"add_road_point", l_editor_add_road_point},
    {"end_road", l_editor_end_road},
    {"update_partition_vis", l_editor_update_partition_vis}, {"get_cell_count", l_editor_get_cell_count},
    {"undo", l_editor_undo}, {"redo", l_editor_redo},
    {nullptr, nullptr}
};

static const luaL_Reg vsm_funcs[] = {
    {"init", l_vsm_init}, {"shutdown", l_vsm_shutdown},
    {"register_light", l_vsm_register_light}, {"unregister_light", l_vsm_unregister_light},
    {"begin_frame", l_vsm_begin_frame}, {"end_frame", l_vsm_end_frame},
    {"invalidate", l_vsm_invalidate}, {"mark_rendered", l_vsm_mark_rendered},
    {"get_pages_to_render", l_vsm_get_pages_to_render}, {"lookup_page", l_vsm_lookup_page},
    {"get_stats", l_vsm_get_stats}, {"get_clipmap_levels", l_vsm_get_clipmap_levels},
    {nullptr, nullptr}
};

static const luaL_Reg eqs_funcs[] = {
    {"init", l_eqs_init}, {"shutdown", l_eqs_shutdown},
    {"create_template", l_eqs_create_template}, {"destroy_template", l_eqs_destroy_template},
    {"set_generator", l_eqs_set_generator}, {"add_scorer", l_eqs_add_scorer},
    {"clear_scorers", l_eqs_clear_scorers}, {"set_combine_mode", l_eqs_set_combine_mode},
    {"set_max_results", l_eqs_set_max_results}, {"execute", l_eqs_execute},
    {"get_template_count", l_eqs_get_template_count}, {"execute_at", l_eqs_execute_at},
    {nullptr, nullptr}
};

static const luaL_Reg dist_funcs[] = {
    {"init", l_dist_init}, {"shutdown", l_dist_shutdown},
    {"load_manifest", l_dist_load_manifest}, {"save_manifest", l_dist_save_manifest},
    {"package_cell", l_dist_package_cell}, {"request_download", l_dist_request_download},
    {"cancel_download", l_dist_cancel_download}, {"update_priorities", l_dist_update_priorities},
    {"tick", l_dist_tick}, {"is_installed", l_dist_is_installed},
    {"get_stats", l_dist_get_stats}, {"get_missing", l_dist_get_missing},
    {"verify", l_dist_verify}, {"get_disk_usage", l_dist_get_disk_usage},
    {nullptr, nullptr}
};

} // anonymous namespace

extern "C" int luaopen_dse_world_systems(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_setglobal(L, "dse"); lua_getglobal(L, "dse"); }

    lua_newtable(L); luaL_setfuncs(L, spline_funcs, 0); lua_setfield(L, -2, "spline");
    lua_newtable(L); luaL_setfuncs(L, ocean_funcs, 0);  lua_setfield(L, -2, "ocean");
    lua_newtable(L); luaL_setfuncs(L, editor_funcs, 0); lua_setfield(L, -2, "editor");
    lua_newtable(L); luaL_setfuncs(L, vsm_funcs, 0);    lua_setfield(L, -2, "vsm");
    lua_newtable(L); luaL_setfuncs(L, eqs_funcs, 0);    lua_setfield(L, -2, "eqs");
    lua_newtable(L); luaL_setfuncs(L, dist_funcs, 0);   lua_setfield(L, -2, "distribution");

    lua_pop(L, 1);
    return 0;
}

void RegisterWorldSystemsBindings(lua_State* L) {
    static bool registered = false;
    if (!registered) {
        BindingCleanupRegistry::Instance().Register(ShutdownWorldSystemsBindings);
        registered = true;
    }
    luaopen_dse_world_systems(L);
}

// C ABI 管理所有单例状态，Lua 侧无需清理
void ShutdownWorldSystemsBindings() {}

} // namespace dse::runtime::lua_binding
