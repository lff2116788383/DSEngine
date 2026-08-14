/**
 * @file dse_api_open_world_p2p5.cpp
 * @brief C ABI 实现：P2-P5 大世界系统（Mesh Streaming / Physics LOD / Terrain Deform / Audio LOD）。
 *
 * 系统实例由本层管理（单例），Lua 绑定仅做参数转换。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/render/mesh_streaming.h"
#include "engine/physics/physics3d/physics_lod.h"
#include "engine/physics/physics3d/i_physics3d_system.h"
#include "engine/core/service_locator.h"
#include "engine/terrain/terrain_deformation.h"
#include "engine/audio/audio_lod.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <entt/entt.hpp>

namespace {

std::unique_ptr<dse::render::MeshStreamingSystem> s_mesh_streaming;
std::unique_ptr<dse::physics3d::PhysicsLODSystem> s_physics_lod;
std::unique_ptr<dse::terrain::TerrainDeformationSystem> s_terrain_deform;
std::unique_ptr<dse::audio::AudioLODSystem> s_audio_lod;

bool Keep(float v) { return std::isnan(v); }

/// 获取当前 3D 物理系统（可能为 nullptr：物理后端未启用/未注册）
dse::physics3d::IPhysics3DSystem* GetPhysics3D() {
    return dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>();
}

}  // namespace

// ============================================================
// Mesh Streaming
// ============================================================

extern "C" void dse_mesh_streaming_init(float hysteresis, int load_budget_per_frame) {
    if (!s_mesh_streaming) s_mesh_streaming = std::make_unique<dse::render::MeshStreamingSystem>();
    dse::render::MeshStreamingConfig cfg;
    if (!Keep(hysteresis)) cfg.hysteresis_factor = hysteresis;
    if (load_budget_per_frame >= 0) cfg.load_budget_per_frame = load_budget_per_frame;
    s_mesh_streaming->Init(cfg);
}

extern "C" uint32_t dse_mesh_streaming_register_mesh(const char* name,
                                                     float x, float y, float z, float radius) {
    if (!s_mesh_streaming || !name) return 0;
    return s_mesh_streaming->RegisterMesh(name, glm::vec3(x, y, z), radius);
}

extern "C" void dse_mesh_streaming_add_lod(uint32_t mesh_id, uint32_t level, const char* path,
                                           float distance, uint32_t triangle_count) {
    if (!s_mesh_streaming || !path) return;
    s_mesh_streaming->AddLODLevel(mesh_id, level, path, distance, triangle_count);
}

extern "C" void dse_mesh_streaming_tick(float cam_x, float cam_y, float cam_z, float dt) {
    if (!s_mesh_streaming) return;
    s_mesh_streaming->Tick(glm::vec3(cam_x, cam_y, cam_z), dt);
}

extern "C" int dse_mesh_streaming_get_current_lod(uint32_t mesh_id) {
    if (!s_mesh_streaming) return 0;
    return static_cast<int>(s_mesh_streaming->GetCurrentLOD(mesh_id));
}

extern "C" int dse_mesh_streaming_get_mesh_count(void) {
    if (!s_mesh_streaming) return 0;
    return static_cast<int>(s_mesh_streaming->GetMeshCount());
}

extern "C" void dse_mesh_streaming_shutdown(void) {
    if (s_mesh_streaming) s_mesh_streaming->Shutdown();
}

// ============================================================
// Physics LOD
// ============================================================

extern "C" void dse_physics_lod_init(float full_distance, float reduced_distance,
                                     float simplified_distance) {
    if (!s_physics_lod) s_physics_lod = std::make_unique<dse::physics3d::PhysicsLODSystem>();
    dse::physics3d::PhysicsLODConfig cfg;
    if (!Keep(full_distance)) cfg.full_distance = full_distance;
    if (!Keep(reduced_distance)) cfg.reduced_distance = reduced_distance;
    if (!Keep(simplified_distance)) cfg.simplified_distance = simplified_distance;
    s_physics_lod->Init(cfg);
}

extern "C" void dse_physics_lod_register_body(uint32_t entity_id,
                                              float x, float y, float z, float radius) {
    if (!s_physics_lod) return;
    s_physics_lod->RegisterBody(entity_id, glm::vec3(x, y, z), radius);
}

extern "C" int dse_physics_lod_evaluate(float cam_x, float cam_y, float cam_z,
                                        uint32_t frame, uint32_t* out_ids, int cap) {
    if (!s_physics_lod) return 0;
    auto active = s_physics_lod->Evaluate(glm::vec3(cam_x, cam_y, cam_z), frame);

    // 接线物理后端：把 LOD 状态真正应用到刚体。
    // - Sleep 级 / 降频跳过帧（Reduced/Simplified 的 divider 帧）→ 休眠（不参与模拟）
    // - 本帧需要模拟 → 唤醒
    // 之前 LOD 判定只改内部状态机，对物理世界零影响（技术债 #2）。
    if (auto* phy = GetPhysics3D()) {
        s_physics_lod->ForEachBody([&](uint32_t eid, const dse::physics3d::PhysicsLODEntry&) {
            phy->SetBodySleepState(static_cast<entt::entity>(eid),
                                   !s_physics_lod->ShouldSimulateThisFrame(eid, frame));
        });
    }

    int total = static_cast<int>(active.size());
    if (out_ids && cap > 0) {
        int n = std::min(total, cap);
        for (int i = 0; i < n; ++i) out_ids[i] = active[static_cast<size_t>(i)];
    }
    return total;
}

extern "C" int dse_physics_lod_get_stats(int* out_stats) {
    if (!s_physics_lod) return 0;
    auto stats = s_physics_lod->GetLevelStats();
    if (out_stats) {
        out_stats[0] = static_cast<int>(stats.full);
        out_stats[1] = static_cast<int>(stats.reduced);
        out_stats[2] = static_cast<int>(stats.simplified);
        out_stats[3] = static_cast<int>(stats.sleeping);
    }
    return 1;
}

extern "C" void dse_physics_lod_wake(uint32_t entity_id) {
    if (s_physics_lod) s_physics_lod->WakeBody(entity_id);
    if (auto* phy = GetPhysics3D()) {
        phy->SetBodySleepState(static_cast<entt::entity>(entity_id), false);
    }
}

extern "C" void dse_physics_lod_sleep(uint32_t entity_id) {
    if (s_physics_lod) s_physics_lod->SleepBody(entity_id);
    if (auto* phy = GetPhysics3D()) {
        phy->SetBodySleepState(static_cast<entt::entity>(entity_id), true);
    }
}

extern "C" void dse_physics_lod_shutdown(void) {
    if (!s_physics_lod) return;
    // 先恢复所有 body 的模拟状态，避免 LOD 关闭后刚体仍被冻结在 offline
    if (auto* phy = GetPhysics3D()) {
        s_physics_lod->ForEachBody([&](uint32_t eid, const dse::physics3d::PhysicsLODEntry&) {
            phy->SetBodySleepState(static_cast<entt::entity>(eid), false);
        });
    }
    s_physics_lod->Shutdown();
}

// ============================================================
// Terrain Deformation
// ============================================================

extern "C" void dse_terrain_deform_init(float max_depth, float max_height) {
    if (!s_terrain_deform) s_terrain_deform = std::make_unique<dse::terrain::TerrainDeformationSystem>();
    dse::terrain::TerrainDeformConfig cfg;
    if (!Keep(max_depth)) cfg.max_deformation_depth = max_depth;
    if (!Keep(max_height)) cfg.max_deformation_height = max_height;
    s_terrain_deform->Init(cfg);
}

extern "C" int dse_terrain_deform_apply(int type, float x, float y, float z,
                                        float radius, float strength) {
    if (!s_terrain_deform) return 0;
    dse::terrain::DeformationOp op;
    op.type = static_cast<dse::terrain::DeformationType>(type);
    op.center = glm::vec3(x, y, z);
    op.radius = radius;
    op.strength = strength;
    return static_cast<int>(s_terrain_deform->ApplyDeformation(op));
}

extern "C" int dse_terrain_deform_undo(void) {
    return (s_terrain_deform && s_terrain_deform->Undo()) ? 1 : 0;
}

extern "C" int dse_terrain_deform_redo(void) {
    return (s_terrain_deform && s_terrain_deform->Redo()) ? 1 : 0;
}

extern "C" float dse_terrain_deform_sample_height(float x, float z) {
    if (!s_terrain_deform) return 0.0f;
    return s_terrain_deform->SampleHeight(x, z);
}

extern "C" void dse_terrain_deform_shutdown(void) {
    if (s_terrain_deform) s_terrain_deform->Shutdown();
}

// ============================================================
// Audio LOD
// ============================================================

extern "C" void dse_audio_lod_init(float full_distance, int max_active_sources) {
    if (!s_audio_lod) s_audio_lod = std::make_unique<dse::audio::AudioLODSystem>();
    dse::audio::AudioLODConfig cfg;
    if (!Keep(full_distance)) cfg.full_distance = full_distance;
    if (max_active_sources >= 0) cfg.max_active_sources = static_cast<uint32_t>(max_active_sources);
    s_audio_lod->Init(cfg);
}

extern "C" uint32_t dse_audio_lod_register_source(const char* path,
                                                  float x, float y, float z,
                                                  float max_distance, float priority) {
    if (!s_audio_lod || !path) return 0;
    return s_audio_lod->RegisterSource(path, glm::vec3(x, y, z), max_distance, priority);
}

extern "C" void dse_audio_lod_tick(float lx, float ly, float lz, float dt) {
    if (!s_audio_lod) return;
    s_audio_lod->Tick(glm::vec3(lx, ly, lz), glm::vec3(0.0f, 0.0f, -1.0f), dt);
}

extern "C" int dse_audio_lod_get_stats(int* out_stats) {
    if (!s_audio_lod) return 0;
    auto stats = s_audio_lod->GetStats();
    if (out_stats) {
        out_stats[0] = static_cast<int>(stats.full);
        out_stats[1] = static_cast<int>(stats.reduced);
        out_stats[2] = static_cast<int>(stats.virtual_count);
        out_stats[3] = static_cast<int>(stats.culled);
    }
    return 1;
}

extern "C" int dse_audio_lod_is_audible(uint32_t source_id) {
    return (s_audio_lod && s_audio_lod->IsSourceAudible(source_id)) ? 1 : 0;
}

extern "C" void dse_audio_lod_shutdown(void) {
    if (s_audio_lod) s_audio_lod->Shutdown();
}

// ============================================================
// Global cleanup
// ============================================================

extern "C" void dse_open_world_p2p5_shutdown(void) {
    s_mesh_streaming.reset();
    s_physics_lod.reset();
    s_terrain_deform.reset();
    s_audio_lod.reset();
}
