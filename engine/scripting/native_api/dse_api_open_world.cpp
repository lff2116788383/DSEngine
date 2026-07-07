/**
 * @file dse_api_open_world.cpp
 * @brief DSEngine C ABI - Open World — 使用引擎子系统实现
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/scene/world_partition.h"
#include "engine/render/hlod/hlod_system.h"
#include "engine/render/virtual_texture/virtual_texture.h"
#include "engine/terrain/geometry_clipmap.h"
#include "engine/render/sdf/global_sdf.h"
#include "engine/ai/ai_lod_scheduler.h"
#include "engine/scene/world_state_persistence.h"
#include "engine/procedural/procedural_generator.h"

#include "engine/render/particles/gpu_particle_system.h"
using namespace dse_api_internal;


extern "C" int dse_wp_get_loaded_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    return sys ? static_cast<int>(sys->LoadedCellCount()) : 0;
}
extern "C" int dse_wp_force_load(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    if (sys) sys->ForceLoadCell({cx, cz});
    return 0;
}
extern "C" int dse_wp_force_unload(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    if (sys) sys->ForceUnloadCell({cx, cz});
    return 0;
}
extern "C" void dse_wp_world_to_cell(float x, float y, float z, float cell_size, int* out_cx, int* out_cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    if (!sys || !out_cx || !out_cz) { if (out_cx) *out_cx = 0; if (out_cz) *out_cz = 0; return; }
    auto coord = sys->WorldToCell(glm::vec3(x, y, z), cell_size);
    *out_cx = coord.x; *out_cz = coord.y;
}
extern "C" void dse_wp_cell_to_world(int cx, int cz, float cell_size, float* out_x, float* out_y, float* out_z) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
    if (!sys) { if (out_x) *out_x = 0; if (out_y) *out_y = 0; if (out_z) *out_z = 0; return; }
    auto w = sys->CellToWorld({cx, cz}, cell_size);
    if (out_x) *out_x = w.x; if (out_y) *out_y = w.y; if (out_z) *out_z = w.z;
}

extern "C" int dse_hlod_get_cluster_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::HLODSystem>();
    return sys ? static_cast<int>(sys->GetClusters().size()) : 0;
}
extern "C" int dse_hlod_get_active_proxy_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::HLODSystem>();
    return sys ? static_cast<int>(sys->ActiveProxyCount()) : 0;
}

extern "C" float dse_vt_get_cache_hit_rate(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
    return sys ? sys->CacheHitRate() : 0.0f;
}
extern "C" int dse_vt_get_page_table_size(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
    return sys ? static_cast<int>(sys->PageTableSize()) : 0;
}
extern "C" int dse_vt_get_physical_atlas_size(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
    return sys ? static_cast<int>(sys->PhysicalAtlasSize()) : 0;
}
extern "C" int dse_vt_get_occupied_pages(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
    return sys ? static_cast<int>(sys->GetCache().OccupiedCount()) : 0;
}

extern "C" int dse_clipmap_get_level_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::terrain::GeometryClipmapSystem>();
    return sys ? sys->LevelCount() : 0;
}
extern "C" int dse_clipmap_sample_height(float x, float z, float* out_y) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::terrain::GeometryClipmapSystem>();
    if (!sys || !out_y) { if (out_y) *out_y = 0.0f; return 0; }
    *out_y = sys->SampleHeight(x, z);
    return 1;
}
extern "C" void dse_clipmap_get_config(float* out_cell_size, int* out_levels) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::terrain::GeometryClipmapSystem>();
    if (!sys) { if (out_cell_size) *out_cell_size = 1.0f; if (out_levels) *out_levels = 0; return; }
    const auto& cfg = sys->GetConfig();
    if (out_cell_size) *out_cell_size = cfg.base_cell_size;
    if (out_levels) *out_levels = cfg.num_levels;
}

extern "C" float dse_sdf_query_distance(float x, float y, float z) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::GlobalSDFSystem>();
    return sys ? sys->QueryDistance(glm::vec3(x, y, z)) : 9999.0f;
}
extern "C" int dse_sdf_get_cascade_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::GlobalSDFSystem>();
    return sys ? sys->CascadeCount() : 0;
}
extern "C" int dse_sdf_rebuild(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::render::GlobalSDFSystem>();
    if (sys) sys->RebuildAll();
    return 1;
}

extern "C" void dse_ai_lod_register(uint32_t e, float importance) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    if (sys) sys->Register(e, importance);
}
extern "C" void dse_ai_lod_unregister(uint32_t e) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    if (sys) sys->Unregister(e);
}
extern "C" int dse_ai_lod_should_tick(uint32_t e) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    return sys ? (sys->ShouldTick(e) ? 1 : 0) : 1;
}
extern "C" int dse_ai_lod_get_level(uint32_t e) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    return sys ? static_cast<int>(sys->GetLevel(e)) : 0;
}
extern "C" void dse_ai_lod_set_force_active(uint32_t e, int force) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    if (sys) sys->SetForceActive(e, force != 0);
}
extern "C" int dse_ai_lod_get_registered_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    return sys ? static_cast<int>(sys->RegisteredCount()) : 0;
}
extern "C" void dse_ai_lod_get_config(float* out_near_dist, float* out_far_dist, int* out_max_level) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
    if (!sys) { if (out_near_dist) *out_near_dist = 100.0f; if (out_far_dist) *out_far_dist = 1000.0f; if (out_max_level) *out_max_level = 3; return; }
    const auto& cfg = sys->GetConfig();
    if (out_near_dist) *out_near_dist = cfg.near_distance;
    if (out_far_dist) *out_far_dist = cfg.far_distance;
    if (out_max_level) *out_max_level = 3;
}

// GPU Particles — per-entity
extern "C" void dse_gpu_particle_set_enabled(uint32_t e, int enabled) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.enabled = (enabled != 0);
    }
}
extern "C" void dse_gpu_particle_set_emission_rate(uint32_t e, float rate) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.emission_rate = rate;
    }
}
extern "C" void dse_gpu_particle_set_gravity(uint32_t e, float gx, float gy, float gz) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.gravity = glm::vec3(gx, gy, gz);
    }
}
extern "C" void dse_gpu_particle_set_wind(uint32_t e, float wx, float wy, float wz) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.wind = glm::vec3(wx, wy, wz);
    }
}
extern "C" void dse_gpu_particle_set_color(uint32_t e, float r1, float g1, float b1, float a1,
                                           float r2, float g2, float b2, float a2) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = static_cast<entt::entity>(e);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) {
            comp->config.color_start = glm::vec4(r1, g1, b1, a1);
            comp->config.color_end = glm::vec4(r2, g2, b2, a2);
        }
    }
}

extern "C" int dse_wsp_save_all(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    if (sys) sys->SaveAll();
    return 1;
}
extern "C" int dse_wsp_save_cell(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    return sys ? (sys->SaveCell(cx, cz) ? 1 : 0) : 0;
}
extern "C" int dse_wsp_load_cell(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    return sys ? (sys->LoadCell(cx, cz) ? 1 : 0) : 0;
}
extern "C" int dse_wsp_reset_cell(int cx, int cz) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    if (sys) sys->ResetCell(cx, cz);
    return 1;
}
extern "C" int dse_wsp_get_dirty_count(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    return sys ? static_cast<int>(sys->DirtyCellCount()) : 0;
}
extern "C" int dse_wsp_get_total_modifications(void) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    return sys ? static_cast<int>(sys->TotalModificationCount()) : 0;
}
extern "C" void dse_wsp_record_destruction(int cx, int cz, uint64_t entity_id) {
    auto* sys = dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
    if (sys) sys->RecordDestruction(cx, cz, entity_id);
}

// Procedural noise — 使用引擎 procedural 命名空间
extern "C" float dse_procedural_perlin2d(float x, float y, uint32_t seed) {
    return dse::procedural::PerlinNoise2D(x, y, seed);
}
extern "C" float dse_procedural_simplex2d(float x, float y, uint32_t seed) {
    return dse::procedural::SimplexNoise2D(x, y, seed);
}
extern "C" float dse_procedural_worley2d(float x, float y, uint32_t seed) {
    return dse::procedural::WorleyNoise2D(x, y, seed);
}
extern "C" float dse_procedural_fbm2d(float x, float y, int octaves, float frequency,
                                     float lacunarity, float persistence, uint32_t seed) {
    dse::procedural::FBMParams params;
    params.octaves = octaves;
    params.frequency = frequency;
    params.lacunarity = lacunarity;
    params.persistence = persistence;
    params.seed = seed;
    return dse::procedural::FBM2D(x, y, params);
}

static dse::procedural::PCGRandom g_pcg(42);
extern "C" void dse_procedural_random_seed(uint64_t seed) {
    g_pcg = dse::procedural::PCGRandom(seed);
}
extern "C" float dse_procedural_random_float(float min_val, float max_val) {
    return g_pcg.Range(min_val, max_val);
}

