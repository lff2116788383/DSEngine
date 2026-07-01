/**
 * @file lua_binding_world_systems.cpp
 * @brief Lua bindings for 6 open-world systems:
 *        1. Spline (道路/河流)
 *        2. Ocean (大规模海洋)
 *        3. World Editor Tools (编辑器工具)
 *        4. Virtual Shadow Map (虚拟阴影)
 *        5. EQS (环境查询)
 *        6. Asset Distribution (打包分发)
 *
 * Registers under dse.spline / dse.ocean / dse.editor / dse.vsm / dse.eqs / dse.distribution
 * Total: 86 Lua C functions
 */

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}
#include <memory>
#include <string>

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/terrain/spline_system.h"
#include "engine/render/ocean_system.h"
#include "engine/terrain/world_editor_tools.h"
#include "engine/render/virtual_shadow_map.h"
#include "engine/ai/eqs_system.h"
#include "engine/assets/asset_distribution.h"

using namespace dse::terrain;
using namespace dse::render;
using namespace dse::ai;
using namespace dse::assets;

// ============================================================
// Singletons
// ============================================================
static std::unique_ptr<SplineSystem> s_spline;
static std::unique_ptr<OceanSystem> s_ocean;
static std::unique_ptr<WorldEditorTools> s_editor;
static std::unique_ptr<VirtualShadowMapSystem> s_vsm;
static std::unique_ptr<EQSSystem> s_eqs;
static std::unique_ptr<AssetDistribution> s_distribution;

// ============================================================
// §1  dse.spline — 样条系统 (14 functions)
// ============================================================

static int l_spline_init(lua_State* L) {
    s_spline = std::make_unique<SplineSystem>();
    return 0;
}

static int l_spline_shutdown(lua_State* L) {
    if (s_spline) { s_spline->Shutdown(); s_spline.reset(); }
    return 0;
}

static int l_spline_create(lua_State* L) {
    if (!s_spline) { lua_pushinteger(L, -1); return 1; }
    const char* name = luaL_checkstring(L, 1);
    lua_pushinteger(L, s_spline->CreateSpline(name));
    return 1;
}

static int l_spline_destroy(lua_State* L) {
    if (!s_spline) return 0;
    s_spline->DestroySpline(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_spline_add_point(lua_State* L) {
    if (!s_spline) return 0;
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    SplinePoint pt;
    pt.position.x = static_cast<float>(luaL_checknumber(L, 2));
    pt.position.y = static_cast<float>(luaL_checknumber(L, 3));
    pt.position.z = static_cast<float>(luaL_checknumber(L, 4));
    pt.width = static_cast<float>(luaL_optnumber(L, 5, 4.0));
    s_spline->AddPoint(id, pt);
    return 0;
}

static int l_spline_set_point(lua_State* L) {
    if (!s_spline) return 0;
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t idx = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    SplinePoint pt;
    pt.position.x = static_cast<float>(luaL_checknumber(L, 3));
    pt.position.y = static_cast<float>(luaL_checknumber(L, 4));
    pt.position.z = static_cast<float>(luaL_checknumber(L, 5));
    pt.width = static_cast<float>(luaL_optnumber(L, 6, 4.0));
    s_spline->SetPoint(id, idx, pt);
    return 0;
}

static int l_spline_remove_point(lua_State* L) {
    if (!s_spline) return 0;
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t idx = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    s_spline->RemovePoint(id, idx);
    return 0;
}

static int l_spline_get_point_count(lua_State* L) {
    if (!s_spline) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, s_spline->GetPointCount(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

static int l_spline_get_length(lua_State* L) {
    if (!s_spline) { lua_pushnumber(L, 0); return 1; }
    lua_pushnumber(L, s_spline->GetSplineLength(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

static int l_spline_evaluate(lua_State* L) {
    if (!s_spline) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 3; }
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float t = static_cast<float>(luaL_checknumber(L, 2));
    auto s = s_spline->EvaluateAtParam(id, t);
    lua_pushnumber(L, s.position.x);
    lua_pushnumber(L, s.position.y);
    lua_pushnumber(L, s.position.z);
    return 3;
}

static int l_spline_evaluate_distance(lua_State* L) {
    if (!s_spline) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 3; }
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dist = static_cast<float>(luaL_checknumber(L, 2));
    auto s = s_spline->EvaluateAtDistance(id, dist);
    lua_pushnumber(L, s.position.x);
    lua_pushnumber(L, s.position.y);
    lua_pushnumber(L, s.position.z);
    return 3;
}

static int l_spline_find_nearest(lua_State* L) {
    if (!s_spline) { lua_pushnumber(L, 0); return 1; }
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 pos(static_cast<float>(luaL_checknumber(L, 2)),
                  static_cast<float>(luaL_checknumber(L, 3)),
                  static_cast<float>(luaL_checknumber(L, 4)));
    lua_pushnumber(L, s_spline->FindNearestPoint(id, pos));
    return 1;
}

static int l_spline_gen_road(lua_State* L) {
    if (!s_spline) { lua_pushinteger(L, 0); return 1; }
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    RoadConfig cfg;
    cfg.segment_length = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    cfg.width_segments = static_cast<int>(luaL_optinteger(L, 3, 4));
    auto mesh = s_spline->GenerateRoadMesh(id, cfg);
    lua_pushinteger(L, static_cast<lua_Integer>(mesh.vertices.size()));
    return 1;
}

static int l_spline_gen_river(lua_State* L) {
    if (!s_spline) { lua_pushinteger(L, 0); return 1; }
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    RiverConfig cfg;
    cfg.segment_length = static_cast<float>(luaL_optnumber(L, 2, 2.0));
    cfg.depth = static_cast<float>(luaL_optnumber(L, 3, 2.0));
    auto mesh = s_spline->GenerateRiverMesh(id, cfg);
    lua_pushinteger(L, static_cast<lua_Integer>(mesh.vertices.size()));
    return 1;
}

// ============================================================
// §2  dse.ocean — 海洋系统 (10 functions)
// ============================================================

static int l_ocean_init(lua_State* L) {
    OceanConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "fft_resolution");
        if (!lua_isnil(L, -1)) cfg.fft_resolution = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "tile_size");
        if (!lua_isnil(L, -1)) cfg.tile_size = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "wind_speed");
        if (!lua_isnil(L, -1)) cfg.wind_speed = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "choppiness");
        if (!lua_isnil(L, -1)) cfg.choppiness = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    s_ocean = std::make_unique<OceanSystem>();
    s_ocean->Init(cfg);
    return 0;
}

static int l_ocean_shutdown(lua_State* L) {
    if (s_ocean) { s_ocean->Shutdown(); s_ocean.reset(); }
    return 0;
}

static int l_ocean_update(lua_State* L) {
    if (!s_ocean) return 0;
    float time = static_cast<float>(luaL_checknumber(L, 1));
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));
    s_ocean->Update(time, glm::vec3(cx, cy, cz));
    return 0;
}

static int l_ocean_get_height(lua_State* L) {
    if (!s_ocean) { lua_pushnumber(L, 0); return 1; }
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    lua_pushnumber(L, s_ocean->GetHeightAt(x, z));
    return 1;
}

static int l_ocean_get_normal(lua_State* L) {
    if (!s_ocean) { lua_pushnumber(L, 0); lua_pushnumber(L, 1); lua_pushnumber(L, 0); return 3; }
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    auto n = s_ocean->GetNormalAt(x, z);
    lua_pushnumber(L, n.x); lua_pushnumber(L, n.y); lua_pushnumber(L, n.z);
    return 3;
}

static int l_ocean_get_foam(lua_State* L) {
    if (!s_ocean) { lua_pushnumber(L, 0); return 1; }
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    lua_pushnumber(L, s_ocean->GetFoamAt(x, z));
    return 1;
}

static int l_ocean_set_wind(lua_State* L) {
    if (!s_ocean) return 0;
    float speed = static_cast<float>(luaL_checknumber(L, 1));
    float dx = static_cast<float>(luaL_checknumber(L, 2));
    float dz = static_cast<float>(luaL_checknumber(L, 3));
    s_ocean->SetWind(speed, dx, dz);
    return 0;
}

static int l_ocean_set_choppiness(lua_State* L) {
    if (!s_ocean) return 0;
    s_ocean->SetChoppiness(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

static int l_ocean_get_stats(lua_State* L) {
    if (!s_ocean) { lua_newtable(L); return 1; }
    auto stats = s_ocean->GetStats();
    lua_newtable(L);
    lua_pushinteger(L, stats.total_tiles); lua_setfield(L, -2, "total_tiles");
    lua_pushinteger(L, stats.visible_tiles); lua_setfield(L, -2, "visible_tiles");
    lua_pushinteger(L, stats.fft_resolution); lua_setfield(L, -2, "fft_resolution");
    lua_pushnumber(L, stats.current_max_height); lua_setfield(L, -2, "max_height");
    return 1;
}

static int l_ocean_get_lod_count(lua_State* L) {
    if (!s_ocean) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, s_ocean->GetLODCount());
    return 1;
}

// ============================================================
// §3  dse.editor — 编辑器世界工具 (14 functions)
// ============================================================

static int l_editor_init(lua_State* L) {
    s_editor = std::make_unique<WorldEditorTools>();
    s_editor->Init();
    return 0;
}

static int l_editor_shutdown(lua_State* L) {
    if (s_editor) { s_editor->Shutdown(); s_editor.reset(); }
    return 0;
}

static int l_editor_terrain_brush(lua_State* L) {
    if (!s_editor) { lua_pushinteger(L, 0); return 1; }
    int op = static_cast<int>(luaL_checkinteger(L, 1));
    BrushParams params;
    params.center.x = static_cast<float>(luaL_checknumber(L, 2));
    params.center.y = static_cast<float>(luaL_checknumber(L, 3));
    params.center.z = static_cast<float>(luaL_checknumber(L, 4));
    params.radius = static_cast<float>(luaL_checknumber(L, 5));
    params.strength = static_cast<float>(luaL_optnumber(L, 6, 0.5));
    params.falloff = static_cast<float>(luaL_optnumber(L, 7, 0.5));
    lua_pushinteger(L, s_editor->ApplyTerrainBrush(static_cast<TerrainBrushOp>(op), params));
    return 1;
}

static int l_editor_brush_preview(lua_State* L) {
    if (!s_editor) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 4; }
    BrushParams params;
    params.center.x = static_cast<float>(luaL_checknumber(L, 1));
    params.center.y = static_cast<float>(luaL_checknumber(L, 2));
    params.center.z = static_cast<float>(luaL_checknumber(L, 3));
    params.radius = static_cast<float>(luaL_checknumber(L, 4));
    auto aabb = s_editor->GetBrushPreview(params);
    lua_pushnumber(L, aabb.x); lua_pushnumber(L, aabb.y); lua_pushnumber(L, aabb.z); lua_pushnumber(L, aabb.w);
    return 4;
}

static int l_editor_place_foliage(lua_State* L) {
    if (!s_editor) { lua_pushinteger(L, 0); return 1; }
    FoliageBrushParams params;
    params.center.x = static_cast<float>(luaL_checknumber(L, 1));
    params.center.y = static_cast<float>(luaL_checknumber(L, 2));
    params.center.z = static_cast<float>(luaL_checknumber(L, 3));
    params.radius = static_cast<float>(luaL_checknumber(L, 4));
    params.density = static_cast<float>(luaL_optnumber(L, 5, 0.5));
    params.mesh_path = luaL_optstring(L, 6, "default_tree");
    lua_pushinteger(L, s_editor->PlaceFoliage(params));
    return 1;
}

static int l_editor_erase_foliage(lua_State* L) {
    if (!s_editor) { lua_pushinteger(L, 0); return 1; }
    glm::vec3 center(static_cast<float>(luaL_checknumber(L, 1)),
                     static_cast<float>(luaL_checknumber(L, 2)),
                     static_cast<float>(luaL_checknumber(L, 3)));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    lua_pushinteger(L, s_editor->EraseFoliage(center, radius));
    return 1;
}

static int l_editor_get_foliage_count(lua_State* L) {
    if (!s_editor) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, s_editor->GetFoliageCount());
    return 1;
}

static int l_editor_begin_road(lua_State* L) {
    if (!s_editor) { lua_pushinteger(L, 0); return 1; }
    float width = static_cast<float>(luaL_optnumber(L, 1, 4.0));
    lua_pushinteger(L, s_editor->BeginRoadDraw(width));
    return 1;
}

static int l_editor_add_road_point(lua_State* L) {
    if (!s_editor) return 0;
    uint32_t session = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 pt(static_cast<float>(luaL_checknumber(L, 2)),
                 static_cast<float>(luaL_checknumber(L, 3)),
                 static_cast<float>(luaL_checknumber(L, 4)));
    s_editor->AddRoadPoint(session, pt);
    return 0;
}

static int l_editor_end_road(lua_State* L) {
    if (!s_editor) return 0;
    s_editor->EndRoadDraw(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_editor_update_partition_vis(lua_State* L) {
    if (!s_editor) return 0;
    glm::vec3 cam(static_cast<float>(luaL_checknumber(L, 1)),
                  static_cast<float>(luaL_checknumber(L, 2)),
                  static_cast<float>(luaL_checknumber(L, 3)));
    float cell_size = static_cast<float>(luaL_optnumber(L, 4, 256.0));
    s_editor->UpdatePartitionVisualization(cam, cell_size);
    return 0;
}

static int l_editor_get_cell_count(lua_State* L) {
    if (!s_editor) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, s_editor->GetVisibleCellCount());
    return 1;
}

static int l_editor_undo(lua_State* L) {
    if (!s_editor) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, s_editor->Undo());
    return 1;
}

static int l_editor_redo(lua_State* L) {
    if (!s_editor) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, s_editor->Redo());
    return 1;
}

// ============================================================
// §4  dse.vsm — 虚拟阴影贴图 (12 functions)
// ============================================================

static int l_vsm_init(lua_State* L) {
    VSMConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "virtual_resolution");
        if (!lua_isnil(L, -1)) cfg.virtual_resolution = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "page_size");
        if (!lua_isnil(L, -1)) cfg.page_size = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "pool_pages");
        if (!lua_isnil(L, -1)) cfg.physical_pool_pages = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "clipmap_levels");
        if (!lua_isnil(L, -1)) cfg.clipmap_levels = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }
    s_vsm = std::make_unique<VirtualShadowMapSystem>();
    s_vsm->Init(cfg);
    return 0;
}

static int l_vsm_shutdown(lua_State* L) {
    if (s_vsm) { s_vsm->Shutdown(); s_vsm.reset(); }
    return 0;
}

static int l_vsm_register_light(lua_State* L) {
    if (!s_vsm) { lua_pushinteger(L, 0); return 1; }
    ShadowLightInfo info;
    info.light_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    info.is_directional = lua_toboolean(L, 2) != 0;
    info.direction.x = static_cast<float>(luaL_optnumber(L, 3, 0));
    info.direction.y = static_cast<float>(luaL_optnumber(L, 4, -1));
    info.direction.z = static_cast<float>(luaL_optnumber(L, 5, 0));
    lua_pushinteger(L, s_vsm->RegisterLight(info));
    return 1;
}

static int l_vsm_unregister_light(lua_State* L) {
    if (!s_vsm) return 0;
    s_vsm->UnregisterLight(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_vsm_begin_frame(lua_State* L) {
    if (!s_vsm) return 0;
    uint32_t frame = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));
    s_vsm->BeginFrame(frame, glm::vec3(cx, cy, cz));
    return 0;
}

static int l_vsm_end_frame(lua_State* L) {
    if (!s_vsm) return 0;
    s_vsm->EndFrame();
    return 0;
}

static int l_vsm_invalidate(lua_State* L) {
    if (!s_vsm) return 0;
    uint32_t light_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 mn(static_cast<float>(luaL_checknumber(L, 2)),
                 static_cast<float>(luaL_checknumber(L, 3)),
                 static_cast<float>(luaL_checknumber(L, 4)));
    glm::vec3 mx(static_cast<float>(luaL_checknumber(L, 5)),
                 static_cast<float>(luaL_checknumber(L, 6)),
                 static_cast<float>(luaL_checknumber(L, 7)));
    s_vsm->InvalidateRegion(light_id, mn, mx);
    return 0;
}

static int l_vsm_mark_rendered(lua_State* L) {
    if (!s_vsm) return 0;
    uint32_t vx = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t vy = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t mip = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    uint32_t lid = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    s_vsm->MarkPageRendered(vx, vy, mip, lid);
    return 0;
}

static int l_vsm_get_pages_to_render(lua_State* L) {
    if (!s_vsm) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(s_vsm->GetPagesToRender().size()));
    return 1;
}

static int l_vsm_lookup_page(lua_State* L) {
    if (!s_vsm) { lua_pushboolean(L, 0); return 1; }
    uint32_t vx = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t vy = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t mip = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    uint32_t lid = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    uint32_t px, py;
    bool found = s_vsm->LookupPage(vx, vy, mip, lid, px, py);
    lua_pushboolean(L, found);
    if (found) { lua_pushinteger(L, px); lua_pushinteger(L, py); return 3; }
    return 1;
}

static int l_vsm_get_stats(lua_State* L) {
    if (!s_vsm) { lua_newtable(L); return 1; }
    auto stats = s_vsm->GetStats();
    lua_newtable(L);
    lua_pushinteger(L, stats.total_pages); lua_setfield(L, -2, "total_pages");
    lua_pushinteger(L, stats.mapped_pages); lua_setfield(L, -2, "mapped_pages");
    lua_pushinteger(L, stats.dirty_pages); lua_setfield(L, -2, "dirty_pages");
    lua_pushinteger(L, stats.rendered_this_frame); lua_setfield(L, -2, "rendered_this_frame");
    lua_pushinteger(L, stats.cache_hit_rate_percent); lua_setfield(L, -2, "cache_hit_percent");
    lua_pushinteger(L, stats.physical_pool_usage_percent); lua_setfield(L, -2, "pool_usage_percent");
    return 1;
}

static int l_vsm_get_clipmap_levels(lua_State* L) {
    if (!s_vsm) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, s_vsm->GetConfig().clipmap_levels);
    return 1;
}

// ============================================================
// §5  dse.eqs — 环境查询系统 (12 functions)
// ============================================================

static int l_eqs_init(lua_State* L) {
    s_eqs = std::make_unique<EQSSystem>();
    s_eqs->Init();
    return 0;
}

static int l_eqs_shutdown(lua_State* L) {
    if (s_eqs) { s_eqs->Shutdown(); s_eqs.reset(); }
    return 0;
}

static int l_eqs_create_template(lua_State* L) {
    if (!s_eqs) { lua_pushinteger(L, -1); return 1; }
    const char* name = luaL_checkstring(L, 1);
    lua_pushinteger(L, s_eqs->CreateTemplate(name));
    return 1;
}

static int l_eqs_destroy_template(lua_State* L) {
    if (!s_eqs) return 0;
    s_eqs->DestroyTemplate(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_eqs_set_generator(lua_State* L) {
    if (!s_eqs) return 0;
    uint32_t tid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    GeneratorConfig cfg;
    cfg.type = static_cast<GeneratorType>(luaL_checkinteger(L, 2));
    cfg.radius = static_cast<float>(luaL_optnumber(L, 3, 20.0));
    cfg.spacing = static_cast<float>(luaL_optnumber(L, 4, 2.0));
    cfg.max_points = static_cast<int>(luaL_optinteger(L, 5, 200));
    s_eqs->SetGenerator(tid, cfg);
    return 0;
}

static int l_eqs_add_scorer(lua_State* L) {
    if (!s_eqs) return 0;
    uint32_t tid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    ScorerConfig cfg;
    cfg.type = static_cast<ScorerType>(luaL_checkinteger(L, 2));
    cfg.weight = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    cfg.invert = lua_toboolean(L, 4) != 0;
    cfg.max_value = static_cast<float>(luaL_optnumber(L, 5, 100.0));
    s_eqs->AddScorer(tid, cfg);
    return 0;
}

static int l_eqs_clear_scorers(lua_State* L) {
    if (!s_eqs) return 0;
    s_eqs->ClearScorers(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

static int l_eqs_set_combine_mode(lua_State* L) {
    if (!s_eqs) return 0;
    uint32_t tid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    CombineMode mode = static_cast<CombineMode>(luaL_checkinteger(L, 2));
    s_eqs->SetCombineMode(tid, mode);
    return 0;
}

static int l_eqs_set_max_results(lua_State* L) {
    if (!s_eqs) return 0;
    uint32_t tid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t max_r = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    s_eqs->SetMaxResults(tid, max_r);
    return 0;
}

static int l_eqs_execute(lua_State* L) {
    if (!s_eqs) { lua_newtable(L); return 1; }
    uint32_t tid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 pos(static_cast<float>(luaL_checknumber(L, 2)),
                  static_cast<float>(luaL_checknumber(L, 3)),
                  static_cast<float>(luaL_checknumber(L, 4)));
    auto result = s_eqs->Execute(tid, pos);

    lua_newtable(L);
    lua_pushnumber(L, result.best_position.x); lua_setfield(L, -2, "best_x");
    lua_pushnumber(L, result.best_position.y); lua_setfield(L, -2, "best_y");
    lua_pushnumber(L, result.best_position.z); lua_setfield(L, -2, "best_z");
    lua_pushnumber(L, result.best_score); lua_setfield(L, -2, "best_score");
    lua_pushinteger(L, result.total_generated); lua_setfield(L, -2, "total_generated");
    lua_pushinteger(L, result.valid_count); lua_setfield(L, -2, "valid_count");
    lua_pushnumber(L, result.query_time_ms); lua_setfield(L, -2, "query_time_ms");
    return 1;
}

static int l_eqs_get_template_count(lua_State* L) {
    if (!s_eqs) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, s_eqs->GetTemplateCount());
    return 1;
}

static int l_eqs_execute_at(lua_State* L) {
    if (!s_eqs) { lua_newtable(L); return 1; }
    uint32_t tid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 pos(static_cast<float>(luaL_checknumber(L, 2)),
                  static_cast<float>(luaL_checknumber(L, 3)),
                  static_cast<float>(luaL_checknumber(L, 4)));
    glm::vec3 center(static_cast<float>(luaL_checknumber(L, 5)),
                     static_cast<float>(luaL_checknumber(L, 6)),
                     static_cast<float>(luaL_checknumber(L, 7)));
    auto result = s_eqs->ExecuteAt(tid, pos, center);

    lua_newtable(L);
    lua_pushnumber(L, result.best_position.x); lua_setfield(L, -2, "best_x");
    lua_pushnumber(L, result.best_position.y); lua_setfield(L, -2, "best_y");
    lua_pushnumber(L, result.best_position.z); lua_setfield(L, -2, "best_z");
    lua_pushnumber(L, result.best_score); lua_setfield(L, -2, "best_score");
    lua_pushinteger(L, result.valid_count); lua_setfield(L, -2, "valid_count");
    return 1;
}

// ============================================================
// §6  dse.distribution — 打包分发管线 (14 functions)
// ============================================================

static int l_dist_init(lua_State* L) {
    DistributionConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "cell_size");
        if (!lua_isnil(L, -1)) cfg.cell_size = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "max_downloads");
        if (!lua_isnil(L, -1)) cfg.max_concurrent_downloads = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "cdn_url");
        if (!lua_isnil(L, -1)) cfg.cdn_base_url = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    s_distribution = std::make_unique<AssetDistribution>();
    s_distribution->Init(cfg);
    return 0;
}

static int l_dist_shutdown(lua_State* L) {
    if (s_distribution) { s_distribution->Shutdown(); s_distribution.reset(); }
    return 0;
}

static int l_dist_load_manifest(lua_State* L) {
    if (!s_distribution) { lua_pushboolean(L, 0); return 1; }
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, s_distribution->LoadManifest(path));
    return 1;
}

static int l_dist_save_manifest(lua_State* L) {
    if (!s_distribution) { lua_pushboolean(L, 0); return 1; }
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, s_distribution->SaveManifest(path));
    return 1;
}

static int l_dist_package_cell(lua_State* L) {
    if (!s_distribution) { lua_pushinteger(L, 0); return 1; }
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cy = static_cast<int>(luaL_checkinteger(L, 2));
    int lod = static_cast<int>(luaL_optinteger(L, 3, 0));
    // Assets from table arg 4
    std::vector<std::string> assets;
    if (lua_istable(L, 4)) {
        lua_Integer n = static_cast<lua_Integer>(lua_rawlen(L, 4));
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L, 4, i);
            if (lua_isstring(L, -1)) assets.emplace_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pushinteger(L, s_distribution->PackageCell(cx, cy, lod, assets));
    return 1;
}

static int l_dist_request_download(lua_State* L) {
    if (!s_distribution) return 0;
    s_distribution->RequestDownload(luaL_checkstring(L, 1));
    return 0;
}

static int l_dist_cancel_download(lua_State* L) {
    if (!s_distribution) return 0;
    s_distribution->CancelDownload(luaL_checkstring(L, 1));
    return 0;
}

static int l_dist_update_priorities(lua_State* L) {
    if (!s_distribution) return 0;
    glm::vec3 pos(static_cast<float>(luaL_checknumber(L, 1)),
                  static_cast<float>(luaL_checknumber(L, 2)),
                  static_cast<float>(luaL_checknumber(L, 3)));
    s_distribution->UpdatePriorities(pos);
    return 0;
}

static int l_dist_tick(lua_State* L) {
    if (!s_distribution) return 0;
    float dt = static_cast<float>(luaL_optnumber(L, 1, 0.016));
    s_distribution->Tick(dt);
    return 0;
}

static int l_dist_is_installed(lua_State* L) {
    if (!s_distribution) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, s_distribution->IsPackageInstalled(luaL_checkstring(L, 1)));
    return 1;
}

static int l_dist_get_stats(lua_State* L) {
    if (!s_distribution) { lua_newtable(L); return 1; }
    auto stats = s_distribution->GetStats();
    lua_newtable(L);
    lua_pushinteger(L, stats.total_packages); lua_setfield(L, -2, "total_packages");
    lua_pushinteger(L, stats.installed_packages); lua_setfield(L, -2, "installed");
    lua_pushinteger(L, stats.downloading_packages); lua_setfield(L, -2, "downloading");
    lua_pushinteger(L, stats.pending_packages); lua_setfield(L, -2, "pending");
    lua_pushnumber(L, static_cast<double>(stats.total_downloaded_bytes)); lua_setfield(L, -2, "downloaded_bytes");
    lua_pushnumber(L, stats.download_speed_bps); lua_setfield(L, -2, "speed_bps");
    return 1;
}

static int l_dist_get_missing(lua_State* L) {
    if (!s_distribution) { lua_newtable(L); return 1; }
    glm::vec3 pos(static_cast<float>(luaL_checknumber(L, 1)),
                  static_cast<float>(luaL_checknumber(L, 2)),
                  static_cast<float>(luaL_checknumber(L, 3)));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 512.0));
    auto missing = s_distribution->GetMissingPackages(pos, radius);
    lua_newtable(L);
    for (size_t i = 0; i < missing.size(); ++i) {
        lua_pushstring(L, missing[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

static int l_dist_verify(lua_State* L) {
    if (!s_distribution) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, s_distribution->VerifyPackage(luaL_checkstring(L, 1)));
    return 1;
}

static int l_dist_get_disk_usage(lua_State* L) {
    if (!s_distribution) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(s_distribution->GetDiskUsage()));
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

extern "C" int luaopen_dse_world_systems(lua_State* L) {
    // Get or create global dse table
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_setglobal(L, "dse"); lua_getglobal(L, "dse"); }

    // dse.spline
    lua_newtable(L);
    luaL_setfuncs(L, spline_funcs, 0);
    lua_setfield(L, -2, "spline");

    // dse.ocean
    lua_newtable(L);
    luaL_setfuncs(L, ocean_funcs, 0);
    lua_setfield(L, -2, "ocean");

    // dse.editor
    lua_newtable(L);
    luaL_setfuncs(L, editor_funcs, 0);
    lua_setfield(L, -2, "editor");

    // dse.vsm
    lua_newtable(L);
    luaL_setfuncs(L, vsm_funcs, 0);
    lua_setfield(L, -2, "vsm");

    // dse.eqs
    lua_newtable(L);
    luaL_setfuncs(L, eqs_funcs, 0);
    lua_setfield(L, -2, "eqs");

    // dse.distribution
    lua_newtable(L);
    luaL_setfuncs(L, dist_funcs, 0);
    lua_setfield(L, -2, "distribution");

    lua_pop(L, 1); // pop dse
    return 0;
}

// Namespace wrapper for DSE registry integration
namespace dse::runtime::lua_binding {
void RegisterWorldSystemsBindings(lua_State* L) {
    static bool registered = false;
    if (!registered) {
        BindingCleanupRegistry::Instance().Register(ShutdownWorldSystemsBindings);
        registered = true;
    }
    luaopen_dse_world_systems(L);
}

void ShutdownWorldSystemsBindings() {
    s_spline.reset();
    s_ocean.reset();
    s_editor.reset();
    s_vsm.reset();
    s_eqs.reset();
    s_distribution.reset();
}
}
