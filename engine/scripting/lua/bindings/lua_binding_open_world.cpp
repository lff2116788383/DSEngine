/**
 * @file lua_binding_open_world.cpp
 * @brief Lua 绑定：开放世界系统
 *
 * 暴露以下系统给 Lua 脚本：
 * - dse.world_partition: 世界分区流式加载
 * - dse.hlod: 层级 LOD 系统
 * - dse.virtual_texture: 虚拟纹理系统
 * - dse.geometry_clipmap: 连续 LOD 地形
 * - dse.global_sdf: 全局距离场
 * - dse.ai_lod: AI LOD 分层调度
 * - dse.gpu_particles: GPU 粒子系统
 * - dse.world_state: 世界状态持久化
 * - dse.procedural: 程序化生成
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scene/world_partition.h"
#include "engine/render/hlod/hlod_system.h"
#include "engine/render/virtual_texture/virtual_texture.h"
#include "engine/terrain/geometry_clipmap.h"
#include "engine/render/sdf/global_sdf.h"
#include "engine/ai/ai_lod_scheduler.h"
#include "engine/render/particles/gpu_particle_system.h"
#include "engine/scene/world_state_persistence.h"
#include "engine/procedural/procedural_generator.h"
#include "engine/core/service_locator.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {

namespace {

// ─── World Partition ────────────────────────────────────────────────────────

dse::WorldPartitionSystem* GetWorldPartitionSystem() {
    return dse::core::ServiceLocator::Instance().Get<dse::WorldPartitionSystem>();
}

// world_partition.get_loaded_count() → int
int L_WPGetLoadedCount(lua_State* L) {
    auto* sys = GetWorldPartitionSystem();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->LoadedCellCount()));
    return 1;
}

// world_partition.force_load(cx, cy)
int L_WPForceLoad(lua_State* L) {
    auto* sys = GetWorldPartitionSystem();
    if (!sys) return 0;
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cy = static_cast<int>(luaL_checkinteger(L, 2));
    sys->ForceLoadCell({cx, cy});
    return 0;
}

// world_partition.force_unload(cx, cy)
int L_WPForceUnload(lua_State* L) {
    auto* sys = GetWorldPartitionSystem();
    if (!sys) return 0;
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cy = static_cast<int>(luaL_checkinteger(L, 2));
    sys->ForceUnloadCell({cx, cy});
    return 0;
}

// world_partition.world_to_cell(x, y, z, cell_size) → cx, cy
int L_WPWorldToCell(lua_State* L) {
    auto* sys = GetWorldPartitionSystem();
    if (!sys) { lua_pushinteger(L, 0); lua_pushinteger(L, 0); return 2; }
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float cell_size = static_cast<float>(luaL_optnumber(L, 4, 128.0));
    dse::CellCoord coord = sys->WorldToCell(glm::vec3(x, y, z), cell_size);
    lua_pushinteger(L, coord.x);
    lua_pushinteger(L, coord.y);
    return 2;
}

// world_partition.cell_to_world(cx, cy, cell_size) → x, y, z
int L_WPCellToWorld(lua_State* L) {
    auto* sys = GetWorldPartitionSystem();
    if (!sys) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 3; }
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cy = static_cast<int>(luaL_checkinteger(L, 2));
    float cell_size = static_cast<float>(luaL_optnumber(L, 3, 128.0));
    glm::vec3 world_pos = sys->CellToWorld({cx, cy}, cell_size);
    lua_pushnumber(L, world_pos.x);
    lua_pushnumber(L, world_pos.y);
    lua_pushnumber(L, world_pos.z);
    return 3;
}

// ─── HLOD ───────────────────────────────────────────────────────────────────

dse::render::HLODSystem* GetHLODSystem() {
    return dse::core::ServiceLocator::Instance().Get<dse::render::HLODSystem>();
}

// hlod.get_cluster_count() → int
int L_HLODGetClusterCount(lua_State* L) {
    auto* sys = GetHLODSystem();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->GetClusters().size()));
    return 1;
}

// hlod.get_active_proxy_count() → int
int L_HLODGetActiveProxyCount(lua_State* L) {
    auto* sys = GetHLODSystem();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->ActiveProxyCount()));
    return 1;
}

// ─── Virtual Texture ────────────────────────────────────────────────────────

dse::vt::VirtualTextureSystem* GetVirtualTextureSystem() {
    return dse::core::ServiceLocator::Instance().Get<dse::vt::VirtualTextureSystem>();
}

// virtual_texture.get_cache_hit_rate() → float
int L_VTGetCacheHitRate(lua_State* L) {
    auto* sys = GetVirtualTextureSystem();
    if (!sys) { lua_pushnumber(L, 0.0); return 1; }
    lua_pushnumber(L, sys->CacheHitRate());
    return 1;
}

// virtual_texture.get_page_table_size() → int
int L_VTGetPageTableSize(lua_State* L) {
    auto* sys = GetVirtualTextureSystem();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->PageTableSize()));
    return 1;
}

// virtual_texture.get_physical_atlas_size() → int
int L_VTGetPhysicalAtlasSize(lua_State* L) {
    auto* sys = GetVirtualTextureSystem();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->PhysicalAtlasSize()));
    return 1;
}

// virtual_texture.get_occupied_pages() → int
int L_VTGetOccupiedPages(lua_State* L) {
    auto* sys = GetVirtualTextureSystem();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->GetCache().OccupiedCount()));
    return 1;
}

// ─── Geometry Clipmap ───────────────────────────────────────────────────────

dse::terrain::GeometryClipmapSystem* GetGeometryClipmapSystem() {
    return dse::core::ServiceLocator::Instance().Get<dse::terrain::GeometryClipmapSystem>();
}

// geometry_clipmap.get_level_count() → int
int L_ClipmapGetLevelCount(lua_State* L) {
    auto* sys = GetGeometryClipmapSystem();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, sys->LevelCount());
    return 1;
}

// geometry_clipmap.sample_height(x, z) → float
int L_ClipmapSampleHeight(lua_State* L) {
    auto* sys = GetGeometryClipmapSystem();
    if (!sys) { lua_pushnumber(L, 0.0); return 1; }
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    lua_pushnumber(L, sys->SampleHeight(x, z));
    return 1;
}

// geometry_clipmap.get_config() → table {num_levels, grid_size, base_cell_size, height_scale}
int L_ClipmapGetConfig(lua_State* L) {
    auto* sys = GetGeometryClipmapSystem();
    if (!sys) { lua_newtable(L); return 1; }
    const auto& cfg = sys->GetConfig();
    lua_newtable(L);
    lua_pushinteger(L, cfg.num_levels);   lua_setfield(L, -2, "num_levels");
    lua_pushinteger(L, cfg.grid_size);    lua_setfield(L, -2, "grid_size");
    lua_pushnumber(L, cfg.base_cell_size); lua_setfield(L, -2, "base_cell_size");
    lua_pushnumber(L, cfg.height_scale);  lua_setfield(L, -2, "height_scale");
    return 1;
}

// ─── Global SDF ─────────────────────────────────────────────────────────────

dse::render::GlobalSDFSystem* GetGlobalSDFSystem() {
    return dse::core::ServiceLocator::Instance().Get<dse::render::GlobalSDFSystem>();
}

// global_sdf.query_distance(x, y, z) → float
int L_SDFQueryDistance(lua_State* L) {
    auto* sys = GetGlobalSDFSystem();
    if (!sys) { lua_pushnumber(L, 9999.0); return 1; }
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, sys->QueryDistance(glm::vec3(x, y, z)));
    return 1;
}

// global_sdf.get_cascade_count() → int
int L_SDFGetCascadeCount(lua_State* L) {
    auto* sys = GetGlobalSDFSystem();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, sys->CascadeCount());
    return 1;
}

// global_sdf.rebuild()
int L_SDFRebuild(lua_State* L) {
    auto* sys = GetGlobalSDFSystem();
    if (sys) sys->RebuildAll();
    return 0;
}

// ─── AI LOD Scheduler ───────────────────────────────────────────────────────

dse::ai::AILodScheduler* GetAILodScheduler() {
    return dse::core::ServiceLocator::Instance().Get<dse::ai::AILodScheduler>();
}

// ai_lod.register(entity_id, importance)
int L_AILodRegister(lua_State* L) {
    auto* sys = GetAILodScheduler();
    if (!sys) return 0;
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float importance = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    sys->Register(entity_id, importance);
    return 0;
}

// ai_lod.unregister(entity_id)
int L_AILodUnregister(lua_State* L) {
    auto* sys = GetAILodScheduler();
    if (!sys) return 0;
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    sys->Unregister(entity_id);
    return 0;
}

// ai_lod.should_tick(entity_id) → bool
int L_AILodShouldTick(lua_State* L) {
    auto* sys = GetAILodScheduler();
    if (!sys) { lua_pushboolean(L, 1); return 1; }
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, sys->ShouldTick(entity_id) ? 1 : 0);
    return 1;
}

// ai_lod.get_level(entity_id) → int (0=Near,1=Medium,2=Far,3=Dormant)
int L_AILodGetLevel(lua_State* L) {
    auto* sys = GetAILodScheduler();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, static_cast<int>(sys->GetLevel(entity_id)));
    return 1;
}

// ai_lod.set_force_active(entity_id, active)
int L_AILodSetForceActive(lua_State* L) {
    auto* sys = GetAILodScheduler();
    if (!sys) return 0;
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool active = lua_toboolean(L, 2) != 0;
    sys->SetForceActive(entity_id, active);
    return 0;
}

// ai_lod.get_registered_count() → int
int L_AILodGetRegisteredCount(lua_State* L) {
    auto* sys = GetAILodScheduler();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->RegisteredCount()));
    return 1;
}

// ai_lod.get_config() → table
int L_AILodGetConfig(lua_State* L) {
    auto* sys = GetAILodScheduler();
    if (!sys) { lua_newtable(L); return 1; }
    const auto& cfg = sys->GetConfig();
    lua_newtable(L);
    lua_pushnumber(L, cfg.near_distance);    lua_setfield(L, -2, "near_distance");
    lua_pushnumber(L, cfg.medium_distance);  lua_setfield(L, -2, "medium_distance");
    lua_pushnumber(L, cfg.far_distance);     lua_setfield(L, -2, "far_distance");
    lua_pushinteger(L, cfg.medium_skip_frames); lua_setfield(L, -2, "medium_skip_frames");
    lua_pushinteger(L, cfg.far_skip_frames); lua_setfield(L, -2, "far_skip_frames");
    lua_pushnumber(L, cfg.hysteresis);       lua_setfield(L, -2, "hysteresis");
    return 1;
}

// ─── GPU Particles ──────────────────────────────────────────────────────────

// gpu_particles 通过 ECS 组件绑定（GpuParticleComponent），这里暴露工具函数

// gpu_particles.set_enabled(entity_id, enabled)
int L_GpuParticleSetEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool enabled = lua_toboolean(L, 2) != 0;
    auto entity = static_cast<entt::entity>(eid);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.enabled = enabled;
    }
    return 0;
}

// gpu_particles.set_emission_rate(entity_id, rate)
int L_GpuParticleSetRate(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float rate = static_cast<float>(luaL_checknumber(L, 2));
    auto entity = static_cast<entt::entity>(eid);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.emission_rate = rate;
    }
    return 0;
}

// gpu_particles.set_gravity(entity_id, gx, gy, gz)
int L_GpuParticleSetGravity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float gx = static_cast<float>(luaL_checknumber(L, 2));
    float gy = static_cast<float>(luaL_checknumber(L, 3));
    float gz = static_cast<float>(luaL_checknumber(L, 4));
    auto entity = static_cast<entt::entity>(eid);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.gravity = glm::vec3(gx, gy, gz);
    }
    return 0;
}

// gpu_particles.set_wind(entity_id, wx, wy, wz)
int L_GpuParticleSetWind(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    float wz = static_cast<float>(luaL_checknumber(L, 4));
    auto entity = static_cast<entt::entity>(eid);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) comp->config.wind = glm::vec3(wx, wy, wz);
    }
    return 0;
}

// gpu_particles.set_color(entity_id, r1,g1,b1,a1, r2,g2,b2,a2) — start/end color
int L_GpuParticleSetColor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r1 = static_cast<float>(luaL_checknumber(L, 2));
    float g1 = static_cast<float>(luaL_checknumber(L, 3));
    float b1 = static_cast<float>(luaL_checknumber(L, 4));
    float a1 = static_cast<float>(luaL_checknumber(L, 5));
    float r2 = static_cast<float>(luaL_checknumber(L, 6));
    float g2 = static_cast<float>(luaL_checknumber(L, 7));
    float b2 = static_cast<float>(luaL_checknumber(L, 8));
    float a2 = static_cast<float>(luaL_checknumber(L, 9));
    auto entity = static_cast<entt::entity>(eid);
    if (world->registry().valid(entity)) {
        auto* comp = world->registry().try_get<dse::render::GpuParticleComponent>(entity);
        if (comp) {
            comp->config.color_start = glm::vec4(r1, g1, b1, a1);
            comp->config.color_end = glm::vec4(r2, g2, b2, a2);
        }
    }
    return 0;
}

// ─── World State Persistence ────────────────────────────────────────────────

dse::WorldStatePersistence* GetWorldStatePersistence() {
    return dse::core::ServiceLocator::Instance().Get<dse::WorldStatePersistence>();
}

// world_state.save_all()
int L_WSPSaveAll(lua_State* L) {
    auto* sys = GetWorldStatePersistence();
    if (sys) sys->SaveAll();
    return 0;
}

// world_state.save_cell(cx, cy) → bool
int L_WSPSaveCell(lua_State* L) {
    auto* sys = GetWorldStatePersistence();
    if (!sys) { lua_pushboolean(L, 0); return 1; }
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cy = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, sys->SaveCell(cx, cy) ? 1 : 0);
    return 1;
}

// world_state.load_cell(cx, cy) → bool
int L_WSPLoadCell(lua_State* L) {
    auto* sys = GetWorldStatePersistence();
    if (!sys) { lua_pushboolean(L, 0); return 1; }
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cy = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, sys->LoadCell(cx, cy) ? 1 : 0);
    return 1;
}

// world_state.reset_cell(cx, cy)
int L_WSPResetCell(lua_State* L) {
    auto* sys = GetWorldStatePersistence();
    if (!sys) return 0;
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cy = static_cast<int>(luaL_checkinteger(L, 2));
    sys->ResetCell(cx, cy);
    return 0;
}

// world_state.get_dirty_count() → int
int L_WSPGetDirtyCount(lua_State* L) {
    auto* sys = GetWorldStatePersistence();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->DirtyCellCount()));
    return 1;
}

// world_state.get_total_modifications() → int
int L_WSPGetTotalMods(lua_State* L) {
    auto* sys = GetWorldStatePersistence();
    if (!sys) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(sys->TotalModificationCount()));
    return 1;
}

// world_state.record_destruction(cx, cy, entity_id)
int L_WSPRecordDestruction(lua_State* L) {
    auto* sys = GetWorldStatePersistence();
    if (!sys) return 0;
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cy = static_cast<int>(luaL_checkinteger(L, 2));
    uint64_t entity_id = static_cast<uint64_t>(luaL_checkinteger(L, 3));
    sys->RecordDestruction(cx, cy, entity_id);
    return 0;
}

// ─── Procedural Generator ───────────────────────────────────────────────────

// 模块级 PCG 随机器（由 random_seed 设置种子）
static dse::procedural::PCGRandom s_lua_pcg(42);

// procedural.perlin2d(x, z, seed?) → float
int L_ProceduralPerlin2D(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_optinteger(L, 3, 0));
    lua_pushnumber(L, dse::procedural::PerlinNoise2D(x, z, seed));
    return 1;
}

// procedural.simplex2d(x, z, seed?) → float
int L_ProceduralSimplex2D(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_optinteger(L, 3, 0));
    lua_pushnumber(L, dse::procedural::SimplexNoise2D(x, z, seed));
    return 1;
}

// procedural.worley2d(x, z, seed?) → float
int L_ProceduralWorley2D(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t seed = static_cast<uint32_t>(luaL_optinteger(L, 3, 0));
    lua_pushnumber(L, dse::procedural::WorleyNoise2D(x, z, seed));
    return 1;
}

// procedural.fbm2d(x, z, octaves?, frequency?, lacunarity?, persistence?, seed?) → float
int L_ProceduralFBM2D(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    dse::procedural::FBMParams params;
    params.octaves = static_cast<int>(luaL_optinteger(L, 3, 6));
    params.frequency = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    params.lacunarity = static_cast<float>(luaL_optnumber(L, 5, 2.0));
    params.persistence = static_cast<float>(luaL_optnumber(L, 6, 0.5));
    params.seed = static_cast<uint32_t>(luaL_optinteger(L, 7, 0));
    lua_pushnumber(L, dse::procedural::FBM2D(x, z, params));
    return 1;
}

// procedural.random_seed(seed)
int L_ProceduralRandomSeed(lua_State* L) {
    uint64_t seed = static_cast<uint64_t>(luaL_checkinteger(L, 1));
    s_lua_pcg = dse::procedural::PCGRandom(seed);
    return 0;
}

// procedural.random_float(min, max) → float
int L_ProceduralRandomFloat(lua_State* L) {
    float min_val = static_cast<float>(luaL_checknumber(L, 1));
    float max_val = static_cast<float>(luaL_checknumber(L, 2));
    lua_pushnumber(L, s_lua_pcg.Range(min_val, max_val));
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
