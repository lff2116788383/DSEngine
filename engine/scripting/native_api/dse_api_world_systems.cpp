/**
 * @file dse_api_world_systems.cpp
 * @brief DSEngine C ABI - World Systems — Spline / Ocean / Editor / VSM / EQS / Distribution
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/terrain/spline_system.h"
#include "engine/render/ocean_system.h"
#include "engine/terrain/world_editor_tools.h"
#include "engine/render/virtual_shadow_map.h"
#include "engine/ai/eqs_system.h"
#include "engine/assets/asset_distribution.h"
#include "engine/ecs/components_3d_sky.h"

using namespace dse;
using namespace dse_api_internal;

// 脚本按需创建、生命周期归脚本的世界系统：文件级全局（非单例），仅本 TU 的 C ABI 消费，
// 由 Lua dse_*_init 创建、dse_*_update 每帧驱动，故不发布到 ServiceLocator。

static std::unique_ptr<dse::terrain::SplineSystem> g_spline_sys;
static std::unique_ptr<dse::render::OceanSystem> g_ocean_sys;
static std::unique_ptr<dse::terrain::WorldEditorTools> g_editor_sys;
static std::unique_ptr<dse::render::VirtualShadowMapSystem> g_vsm_sys;
static std::unique_ptr<dse::ai::EQSSystem> g_eqs_sys;
static std::unique_ptr<dse::assets::AssetDistribution> g_dist_sys;

// --- Spline ---

extern "C" int dse_spline_init(void) {
    g_spline_sys = std::make_unique<dse::terrain::SplineSystem>();
    return 1;
}

extern "C" void dse_spline_shutdown(void) {
    if (g_spline_sys) { g_spline_sys->Shutdown(); g_spline_sys.reset(); }
}

extern "C" uint32_t dse_spline_create(const char* name) {
    if (!g_spline_sys) return 0;
    return g_spline_sys->CreateSpline(name ? name : "");
}

extern "C" void dse_spline_destroy(uint32_t spline) {
    if (g_spline_sys) g_spline_sys->DestroySpline(spline);
}

extern "C" void dse_spline_add_point(uint32_t spline, float x, float y, float z, float width) {
    if (!g_spline_sys) return;
    dse::terrain::SplinePoint pt;
    pt.position = glm::vec3(x, y, z);
    pt.width = width;
    g_spline_sys->AddPoint(spline, pt);
}

extern "C" void dse_spline_set_point(uint32_t spline, int index, float x, float y, float z, float width) {
    if (!g_spline_sys) return;
    dse::terrain::SplinePoint pt;
    pt.position = glm::vec3(x, y, z);
    pt.width = width;
    g_spline_sys->SetPoint(spline, index, pt);
}

extern "C" void dse_spline_remove_point(uint32_t spline, int index) {
    if (g_spline_sys) g_spline_sys->RemovePoint(spline, index);
}

extern "C" int dse_spline_get_point_count(uint32_t spline) {
    return g_spline_sys ? g_spline_sys->GetPointCount(spline) : 0;
}

extern "C" float dse_spline_get_length(uint32_t spline) {
    return g_spline_sys ? g_spline_sys->GetSplineLength(spline) : 0.0f;
}

extern "C" void dse_spline_evaluate(uint32_t spline, float t, float* out_xyz) {
    if (!out_xyz) return;
    if (!g_spline_sys) { out_xyz[0] = out_xyz[1] = out_xyz[2] = 0; return; }
    auto s = g_spline_sys->EvaluateAtParam(spline, t);
    out_xyz[0] = s.position.x; out_xyz[1] = s.position.y; out_xyz[2] = s.position.z;
}

extern "C" void dse_spline_evaluate_distance(uint32_t spline, float dist, float* out_xyz) {
    if (!out_xyz) return;
    if (!g_spline_sys) { out_xyz[0] = out_xyz[1] = out_xyz[2] = 0; return; }
    auto s = g_spline_sys->EvaluateAtDistance(spline, dist);
    out_xyz[0] = s.position.x; out_xyz[1] = s.position.y; out_xyz[2] = s.position.z;
}

extern "C" float dse_spline_find_nearest(uint32_t spline, float x, float y, float z) {
    if (!g_spline_sys) return 0.0f;
    return g_spline_sys->FindNearestPoint(spline, glm::vec3(x, y, z));
}

extern "C" int dse_spline_gen_road(uint32_t spline, float segment_length, int width_segments) {
    if (!g_spline_sys) return 0;
    dse::terrain::RoadConfig cfg;
    cfg.segment_length = segment_length > 0 ? segment_length : 1.0f;
    cfg.width_segments = width_segments > 0 ? width_segments : 4;
    auto mesh = g_spline_sys->GenerateRoadMesh(spline, cfg);
    return static_cast<int>(mesh.vertices.size());
}

extern "C" int dse_spline_gen_river(uint32_t spline, float segment_length, float depth) {
    if (!g_spline_sys) return 0;
    dse::terrain::RiverConfig cfg;
    cfg.segment_length = segment_length > 0 ? segment_length : 2.0f;
    cfg.depth = depth;
    auto mesh = g_spline_sys->GenerateRiverMesh(spline, cfg);
    return static_cast<int>(mesh.vertices.size());
}

// --- Ocean ---

extern "C" int dse_ocean_init(int fft_resolution, float tile_size, float wind_speed, float choppiness) {
    dse::render::OceanConfig cfg;
    cfg.fft_resolution = fft_resolution > 0 ? fft_resolution : 256;
    cfg.tile_size = tile_size > 0 ? tile_size : 100.0f;
    cfg.wind_speed = wind_speed > 0 ? wind_speed : 10.0f;
    cfg.choppiness = choppiness;
    g_ocean_sys = std::make_unique<dse::render::OceanSystem>();
    g_ocean_sys->Init(cfg);
    return 1;
}

extern "C" void dse_ocean_shutdown(void) {
    if (g_ocean_sys) { g_ocean_sys->Shutdown(); g_ocean_sys.reset(); }
}

extern "C" void dse_ocean_update(float time, float cam_x, float cam_y, float cam_z) {
    if (g_ocean_sys) g_ocean_sys->Update(time, glm::vec3(cam_x, cam_y, cam_z));
}

extern "C" float dse_ocean_get_height(float x, float z) {
    return g_ocean_sys ? g_ocean_sys->GetHeightAt(x, z) : 0.0f;
}

extern "C" void dse_ocean_get_normal(float x, float z, float* out_xyz) {
    if (!out_xyz) return;
    if (!g_ocean_sys) { out_xyz[0] = 0; out_xyz[1] = 1; out_xyz[2] = 0; return; }
    auto n = g_ocean_sys->GetNormalAt(x, z);
    out_xyz[0] = n.x; out_xyz[1] = n.y; out_xyz[2] = n.z;
}

extern "C" float dse_ocean_get_foam(float x, float z) {
    return g_ocean_sys ? g_ocean_sys->GetFoamAt(x, z) : 0.0f;
}

extern "C" void dse_ocean_set_wind(float speed, float dx, float dz) {
    if (g_ocean_sys) g_ocean_sys->SetWind(speed, dx, dz);
}

extern "C" void dse_ocean_set_choppiness(float choppiness) {
    if (g_ocean_sys) g_ocean_sys->SetChoppiness(choppiness);
}

extern "C" void dse_ocean_get_stats(int* out_total_tiles, int* out_visible_tiles,
                                   int* out_fft_res, float* out_max_height) {
    if (!g_ocean_sys) {
        if (out_total_tiles) *out_total_tiles = 0;
        if (out_visible_tiles) *out_visible_tiles = 0;
        if (out_fft_res) *out_fft_res = 0;
        if (out_max_height) *out_max_height = 0;
        return;
    }
    auto stats = g_ocean_sys->GetStats();
    if (out_total_tiles) *out_total_tiles = stats.total_tiles;
    if (out_visible_tiles) *out_visible_tiles = stats.visible_tiles;
    if (out_fft_res) *out_fft_res = stats.fft_resolution;
    if (out_max_height) *out_max_height = stats.current_max_height;
}

extern "C" int dse_ocean_get_lod_count(void) {
    return g_ocean_sys ? g_ocean_sys->GetLODCount() : 0;
}

// --- Editor ---

extern "C" int dse_editor_init(void) {
    g_editor_sys = std::make_unique<dse::terrain::WorldEditorTools>();
    g_editor_sys->Init();
    return 1;
}

extern "C" void dse_editor_shutdown(void) {
    if (g_editor_sys) { g_editor_sys->Shutdown(); g_editor_sys.reset(); }
}

extern "C" int dse_editor_terrain_brush(int op, float x, float y, float z, float radius,
                                        float strength, float falloff) {
    if (!g_editor_sys) return 0;
    dse::terrain::BrushParams params;
    params.center = glm::vec3(x, y, z);
    params.radius = radius;
    params.strength = strength;
    params.falloff = falloff;
    return g_editor_sys->ApplyTerrainBrush(static_cast<dse::terrain::TerrainBrushOp>(op), params);
}

extern "C" void dse_editor_brush_preview(float x, float y, float z, float radius,
                                        float* out_min_x, float* out_min_y,
                                        float* out_max_x, float* out_max_y) {
    if (!g_editor_sys) return;
    dse::terrain::BrushParams params;
    params.center = glm::vec3(x, y, z);
    params.radius = radius;
    auto aabb = g_editor_sys->GetBrushPreview(params);
    if (out_min_x) *out_min_x = aabb.x;
    if (out_min_y) *out_min_y = aabb.y;
    if (out_max_x) *out_max_x = aabb.z;
    if (out_max_y) *out_max_y = aabb.w;
}

extern "C" int dse_editor_place_foliage(float x, float y, float z, float radius,
                                        float density, const char* mesh_path) {
    if (!g_editor_sys) return 0;
    dse::terrain::FoliageBrushParams params;
    params.center = glm::vec3(x, y, z);
    params.radius = radius;
    params.density = density;
    params.mesh_path = mesh_path ? mesh_path : "default_tree";
    return g_editor_sys->PlaceFoliage(params);
}

extern "C" int dse_editor_erase_foliage(float x, float y, float z, float radius) {
    if (!g_editor_sys) return 0;
    return g_editor_sys->EraseFoliage(glm::vec3(x, y, z), radius);
}

extern "C" int dse_editor_get_foliage_count(void) {
    return g_editor_sys ? g_editor_sys->GetFoliageCount() : 0;
}

extern "C" int dse_editor_begin_road(float width) {
    if (!g_editor_sys) return 0;
    return g_editor_sys->BeginRoadDraw(width);
}

extern "C" void dse_editor_add_road_point(uint32_t session, float x, float y, float z) {
    if (g_editor_sys) g_editor_sys->AddRoadPoint(session, glm::vec3(x, y, z));
}

extern "C" void dse_editor_end_road(uint32_t session) {
    if (g_editor_sys) g_editor_sys->EndRoadDraw(session);
}

extern "C" void dse_editor_update_partition_vis(float cam_x, float cam_y, float cam_z, float cell_size) {
    if (g_editor_sys) g_editor_sys->UpdatePartitionVisualization(glm::vec3(cam_x, cam_y, cam_z), cell_size);
}

extern "C" int dse_editor_get_cell_count(void) {
    return g_editor_sys ? g_editor_sys->GetVisibleCellCount() : 0;
}

extern "C" int dse_editor_undo(void) {
    return g_editor_sys ? (g_editor_sys->Undo() ? 1 : 0) : 0;
}

extern "C" int dse_editor_redo(void) {
    return g_editor_sys ? (g_editor_sys->Redo() ? 1 : 0) : 0;
}

// --- VSM ---

extern "C" int dse_vsm_init(uint32_t virtual_resolution, uint32_t page_size,
                            uint32_t pool_pages, uint32_t clipmap_levels) {
    dse::render::VSMConfig cfg;
    cfg.virtual_resolution = virtual_resolution > 0 ? virtual_resolution : 16384;
    cfg.page_size = page_size > 0 ? page_size : 128;
    cfg.physical_pool_pages = pool_pages > 0 ? pool_pages : 4096;
    cfg.clipmap_levels = clipmap_levels > 0 ? clipmap_levels : 5;
    g_vsm_sys = std::make_unique<dse::render::VirtualShadowMapSystem>();
    g_vsm_sys->Init(cfg);
    return 1;
}

extern "C" void dse_vsm_shutdown(void) {
    if (g_vsm_sys) { g_vsm_sys->Shutdown(); g_vsm_sys.reset(); }
}

extern "C" uint32_t dse_vsm_register_light(uint32_t light_id, int is_directional,
                                         float dx, float dy, float dz) {
    if (!g_vsm_sys) return 0;
    dse::render::ShadowLightInfo info;
    info.light_id = light_id;
    info.is_directional = (is_directional != 0);
    info.direction = glm::vec3(dx, dy, dz);
    return g_vsm_sys->RegisterLight(info);
}

extern "C" void dse_vsm_unregister_light(uint32_t light_id) {
    if (g_vsm_sys) g_vsm_sys->UnregisterLight(light_id);
}

extern "C" void dse_vsm_begin_frame(uint32_t frame, float cam_x, float cam_y, float cam_z) {
    if (g_vsm_sys) g_vsm_sys->BeginFrame(frame, glm::vec3(cam_x, cam_y, cam_z));
}

extern "C" void dse_vsm_end_frame(void) {
    if (g_vsm_sys) g_vsm_sys->EndFrame();
}

extern "C" void dse_vsm_invalidate(uint32_t light_id,
                                  float min_x, float min_y, float min_z,
                                  float max_x, float max_y, float max_z) {
    if (g_vsm_sys) g_vsm_sys->InvalidateRegion(light_id, glm::vec3(min_x, min_y, min_z),
                                               glm::vec3(max_x, max_y, max_z));
}

extern "C" void dse_vsm_mark_page_rendered(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id) {
    if (g_vsm_sys) g_vsm_sys->MarkPageRendered(vx, vy, mip, light_id);
}

extern "C" int dse_vsm_get_pages_to_render(void) {
    if (!g_vsm_sys) return 0;
    return static_cast<int>(g_vsm_sys->GetPagesToRender().size());
}

extern "C" int dse_vsm_lookup_page(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id,
                                   uint32_t* out_px, uint32_t* out_py) {
    if (!g_vsm_sys) return 0;
    uint32_t px, py;
    bool found = g_vsm_sys->LookupPage(vx, vy, mip, light_id, px, py);
    if (found) {
        if (out_px) *out_px = px;
        if (out_py) *out_py = py;
        return 1;
    }
    return 0;
}

extern "C" void dse_vsm_get_stats(int* out_total, int* out_mapped, int* out_dirty,
                                 int* out_rendered, int* out_cache_hit, int* out_pool_usage) {
    if (!g_vsm_sys) {
        if (out_total) *out_total = 0;
        if (out_mapped) *out_mapped = 0;
        if (out_dirty) *out_dirty = 0;
        if (out_rendered) *out_rendered = 0;
        if (out_cache_hit) *out_cache_hit = 0;
        if (out_pool_usage) *out_pool_usage = 0;
        return;
    }
    auto stats = g_vsm_sys->GetStats();
    if (out_total) *out_total = stats.total_pages;
    if (out_mapped) *out_mapped = stats.mapped_pages;
    if (out_dirty) *out_dirty = stats.dirty_pages;
    if (out_rendered) *out_rendered = stats.rendered_this_frame;
    if (out_cache_hit) *out_cache_hit = stats.cache_hit_rate_percent;
    if (out_pool_usage) *out_pool_usage = stats.physical_pool_usage_percent;
}

extern "C" int dse_vsm_get_clipmap_levels(void) {
    return g_vsm_sys ? g_vsm_sys->GetConfig().clipmap_levels : 0;
}

// --- EQS ---

extern "C" int dse_eqs_init(void) {
    g_eqs_sys = std::make_unique<dse::ai::EQSSystem>();
    g_eqs_sys->Init();
    return 1;
}

extern "C" void dse_eqs_shutdown(void) {
    if (g_eqs_sys) { g_eqs_sys->Shutdown(); g_eqs_sys.reset(); }
}

extern "C" uint32_t dse_eqs_create_template(const char* name) {
    if (!g_eqs_sys) return 0;
    return g_eqs_sys->CreateTemplate(name ? name : "");
}

extern "C" void dse_eqs_destroy_template(uint32_t tmpl) {
    if (g_eqs_sys) g_eqs_sys->DestroyTemplate(tmpl);
}

extern "C" void dse_eqs_set_generator(uint32_t tmpl, int type, float radius, float spacing, int max_points) {
    if (!g_eqs_sys) return;
    dse::ai::GeneratorConfig cfg;
    cfg.type = static_cast<dse::ai::GeneratorType>(type);
    cfg.radius = radius;
    cfg.spacing = spacing;
    cfg.max_points = max_points;
    g_eqs_sys->SetGenerator(tmpl, cfg);
}

extern "C" void dse_eqs_add_scorer(uint32_t tmpl, int type, float weight, int invert, float max_value) {
    if (!g_eqs_sys) return;
    dse::ai::ScorerConfig cfg;
    cfg.type = static_cast<dse::ai::ScorerType>(type);
    cfg.weight = weight;
    cfg.invert = (invert != 0);
    cfg.max_value = max_value;
    g_eqs_sys->AddScorer(tmpl, cfg);
}

extern "C" void dse_eqs_clear_scorers(uint32_t tmpl) {
    if (g_eqs_sys) g_eqs_sys->ClearScorers(tmpl);
}

extern "C" void dse_eqs_set_combine_mode(uint32_t tmpl, int mode) {
    if (g_eqs_sys) g_eqs_sys->SetCombineMode(tmpl, static_cast<dse::ai::CombineMode>(mode));
}

extern "C" void dse_eqs_set_max_results(uint32_t tmpl, uint32_t max_results) {
    if (g_eqs_sys) g_eqs_sys->SetMaxResults(tmpl, max_results);
}

extern "C" void dse_eqs_execute(uint32_t tmpl, float x, float y, float z, float* out_result) {
    if (!g_eqs_sys || !out_result) return;
    auto result = g_eqs_sys->Execute(tmpl, glm::vec3(x, y, z));
    out_result[0] = result.best_position.x;
    out_result[1] = result.best_position.y;
    out_result[2] = result.best_position.z;
    out_result[3] = result.best_score;
    out_result[4] = static_cast<float>(result.total_generated);
    out_result[5] = static_cast<float>(result.valid_count);
    out_result[6] = result.query_time_ms;
}

extern "C" int dse_eqs_get_template_count(void) {
    return g_eqs_sys ? g_eqs_sys->GetTemplateCount() : 0;
}

extern "C" void dse_eqs_execute_at(uint32_t tmpl, float px, float py, float pz,
                                  float cx, float cy, float cz, float* out_result) {
    if (!g_eqs_sys || !out_result) return;
    auto result = g_eqs_sys->ExecuteAt(tmpl, glm::vec3(px, py, pz), glm::vec3(cx, cy, cz));
    out_result[0] = result.best_position.x;
    out_result[1] = result.best_position.y;
    out_result[2] = result.best_position.z;
    out_result[3] = result.best_score;
    out_result[4] = static_cast<float>(result.valid_count);
}

// --- Distribution ---

extern "C" int dse_dist_init(float cell_size, int max_downloads, const char* cdn_url) {
    dse::assets::DistributionConfig cfg;
    cfg.cell_size = cell_size > 0 ? cell_size : 512.0f;
    cfg.max_concurrent_downloads = max_downloads > 0 ? max_downloads : 4;
    if (cdn_url) cfg.cdn_base_url = cdn_url;
    g_dist_sys = std::make_unique<dse::assets::AssetDistribution>();
    g_dist_sys->Init(cfg);
    return 1;
}

extern "C" void dse_dist_shutdown(void) {
    if (g_dist_sys) { g_dist_sys->Shutdown(); g_dist_sys.reset(); }
}

extern "C" int dse_dist_load_manifest(const char* path) {
    if (!g_dist_sys || !path) return 0;
    return g_dist_sys->LoadManifest(path) ? 1 : 0;
}

extern "C" int dse_dist_save_manifest(const char* path) {
    if (!g_dist_sys || !path) return 0;
    return g_dist_sys->SaveManifest(path) ? 1 : 0;
}

extern "C" int dse_dist_package_cell(int cx, int cz, int lod, const char* const* assets, int count) {
    if (!g_dist_sys) return 0;
    std::vector<std::string> v;
    for (int i = 0; i < count; ++i) if (assets[i]) v.emplace_back(assets[i]);
    return g_dist_sys->PackageCell(cx, cz, lod, v);
}

extern "C" void dse_dist_request_download(const char* package) {
    if (g_dist_sys && package) g_dist_sys->RequestDownload(package);
}

extern "C" void dse_dist_cancel_download(const char* package) {
    if (g_dist_sys && package) g_dist_sys->CancelDownload(package);
}

extern "C" void dse_dist_update_priorities(float x, float y, float z) {
    if (g_dist_sys) g_dist_sys->UpdatePriorities(glm::vec3(x, y, z));
}

extern "C" void dse_dist_tick(float dt) {
    if (g_dist_sys) g_dist_sys->Tick(dt);
}

extern "C" int dse_dist_is_installed(const char* package) {
    if (!g_dist_sys || !package) return 0;
    return g_dist_sys->IsPackageInstalled(package) ? 1 : 0;
}

extern "C" void dse_dist_get_stats(int* out_total, int* out_installed, int* out_downloading,
                                   int* out_pending, double* out_downloaded_bytes, double* out_speed_bps) {
    if (!g_dist_sys) {
        if (out_total) *out_total = 0;
        if (out_installed) *out_installed = 0;
        if (out_downloading) *out_downloading = 0;
        if (out_pending) *out_pending = 0;
        if (out_downloaded_bytes) *out_downloaded_bytes = 0;
        if (out_speed_bps) *out_speed_bps = 0;
        return;
    }
    auto stats = g_dist_sys->GetStats();
    if (out_total) *out_total = stats.total_packages;
    if (out_installed) *out_installed = stats.installed_packages;
    if (out_downloading) *out_downloading = stats.downloading_packages;
    if (out_pending) *out_pending = stats.pending_packages;
    if (out_downloaded_bytes) *out_downloaded_bytes = static_cast<double>(stats.total_downloaded_bytes);
    if (out_speed_bps) *out_speed_bps = stats.download_speed_bps;
}

extern "C" int dse_dist_get_missing(float x, float y, float z, float radius,
                                    char* out_buf, int buf_cap) {
    if (!g_dist_sys || !out_buf || buf_cap <= 0) return 0;
    auto missing = g_dist_sys->GetMissingPackages(glm::vec3(x, y, z), radius);
    int offset = 0;
    for (const auto& name : missing) {
        int len = static_cast<int>(name.size());
        if (offset + len + 1 >= buf_cap) break;
        memcpy(out_buf + offset, name.c_str(), len);
        offset += len;
        out_buf[offset++] = '\0';
    }
    if (offset < buf_cap) out_buf[offset] = '\0';
    return static_cast<int>(missing.size());
}

extern "C" int dse_dist_verify(const char* package) {
    if (!g_dist_sys || !package) return 0;
    return g_dist_sys->VerifyPackage(package) ? 1 : 0;
}

extern "C" uint64_t dse_dist_get_disk_usage(void) {
    return g_dist_sys ? g_dist_sys->GetDiskUsage() : 0;
}
