/**
 * @file dse_api_ecs_core.cpp
 * @brief C ABI 实现：ECS 核心 — 子场景/SceneManager、UUID、通用组件查询、
 *        层级、脚本、AABB、时间缩放。
 *
 * 供 Lua / C# 共享同一实现，语义与原 Lua 绑定逐一等价。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/script.h"
#include "engine/ecs/time_scale_component.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/physics_2d.h"
#include "engine/ecs/gameplay.h"
#include "engine/ecs/components_3d_foliage.h"
#include "engine/ecs/uuid_component.h"
#include "engine/scene/scene.h"
#include "engine/scene/sub_scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"

#include <glm/glm.hpp>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline AssetManager* GAM() { return static_cast<AssetManager*>(dse_get_asset_manager_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }

scene::SceneManager* GetSceneManager() {
    return core::ServiceLocator::Instance().Get<scene::SceneManager>();
}

// 将相对路径解析为基于 data root 的完整路径。
std::string ResolveScenePath(const char* path) {
    std::string resolved;
    if (AssetManager* am = GAM()) {
        resolved = am->GetDataRoot();
        if (!resolved.empty() && resolved.back() != '/' && resolved.back() != '\\') {
            resolved += '/';
        }
    }
    resolved += path ? path : "";
    return resolved;
}

inline int CopyStr(const std::string& s, char* out, int cap) {
    if (!out || cap <= 0) return 0;
    int n = static_cast<int>(s.size());
    if (n > cap - 1) n = cap - 1;
    std::memcpy(out, s.data(), static_cast<size_t>(n));
    out[n] = '\0';
    return n;
}

template <typename T>
int CollectComponentView(World* w, uint32_t* out, int cap) {
    auto view = w->registry().view<T>();
    int total = 0;
    for (auto e : view) {
        if (out && total < cap) out[total] = static_cast<uint32_t>(e);
        ++total;
    }
    return total;
}

template <typename T>
int CountComponentView(World* w) {
    return static_cast<int>(w->registry().view<T>().size());
}

template <typename T>
int HasComponentT(World* w, uint32_t e) {
    if (!w->registry().valid(TE(e))) return 0;
    return w->registry().all_of<T>(TE(e)) ? 1 : 0;
}

struct ComponentOps {
    int (*collect)(World*, uint32_t*, int);
    int (*count)(World*);
    int (*has)(World*, uint32_t);
};

template <typename T>
ComponentOps MakeOps() {
    return ComponentOps{&CollectComponentView<T>, &CountComponentView<T>, &HasComponentT<T>};
}

const std::unordered_map<std::string, ComponentOps>& ComponentOpsTable() {
    static const std::unordered_map<std::string, ComponentOps> table = {
        // 核心
        {"transform",               MakeOps<TransformComponent>()},
        {"parent",                  MakeOps<ParentComponent>()},
        {"script",                  MakeOps<ScriptComponent>()},
        {"uuid",                    MakeOps<UUIDComponent>()},
        {"gameplay_tuning",         MakeOps<GameplayTuningComponent>()},
        // 2D 渲染 / 物理
        {"sprite_renderer",         MakeOps<SpriteRendererComponent>()},
        {"spine_renderer",          MakeOps<SpineRendererComponent>()},
        {"material_instance",       MakeOps<MaterialInstanceComponent>()},
        {"camera",                  MakeOps<CameraComponent>()},
        {"camera_follow",           MakeOps<CameraFollowComponent>()},
        {"rigidbody_2d",            MakeOps<RigidBody2DComponent>()},
        {"box_collider_2d",         MakeOps<BoxCollider2DComponent>()},
        {"circle_collider_2d",      MakeOps<CircleCollider2DComponent>()},
        // 3D 渲染
        {"mesh_renderer",           MakeOps<dse::MeshRendererComponent>()},
        {"lod_group",               MakeOps<dse::LODGroupComponent>()},
        {"camera_3d",               MakeOps<dse::Camera3DComponent>()},
        {"free_camera_controller",  MakeOps<dse::FreeCameraControllerComponent>()},
        {"post_process",            MakeOps<dse::PostProcessComponent>()},
        {"decal",                   MakeOps<dse::DecalComponent>()},
        {"directional_light",       MakeOps<dse::DirectionalLight3DComponent>()},
        {"point_light",             MakeOps<dse::PointLightComponent>()},
        {"spot_light",              MakeOps<dse::SpotLightComponent>()},
        {"sky_light",               MakeOps<dse::SkyLightComponent>()},
        {"skybox",                  MakeOps<dse::SkyboxComponent>()},
        {"water",                   MakeOps<dse::WaterComponent>()},
        {"grass",                   MakeOps<dse::GrassComponent>()},
        {"hair",                    MakeOps<dse::HairComponent>()},
        {"light_probe",             MakeOps<dse::LightProbeComponent>()},
        {"reflection_probe",        MakeOps<dse::ReflectionProbeComponent>()},
        {"gi_probe_volume",         MakeOps<dse::GIProbeVolumeComponent>()},
        {"morph_target",            MakeOps<dse::MorphTargetComponent>()},
        {"sub_scene",               MakeOps<dse::SubSceneComponent>()},
        // 地形 / 植被
        {"terrain",                 MakeOps<dse::TerrainComponent>()},
        {"terrain_tile_manager",    MakeOps<dse::TerrainTileManagerComponent>()},
        {"tree",                    MakeOps<dse::TreeComponent>()},
        {"foliage",                 MakeOps<dse::FoliageComponent>()},
        // 3D 物理
        {"rigidbody_3d",            MakeOps<dse::RigidBody3DComponent>()},
        {"box_collider_3d",         MakeOps<dse::BoxCollider3DComponent>()},
        {"sphere_collider_3d",      MakeOps<dse::SphereCollider3DComponent>()},
        {"capsule_collider_3d",     MakeOps<dse::CapsuleCollider3DComponent>()},
        {"mesh_collider_3d",        MakeOps<dse::MeshCollider3DComponent>()},
        {"joint_3d",                MakeOps<dse::Joint3DComponent>()},
        {"character_controller_3d", MakeOps<dse::CharacterController3DComponent>()},
        {"terrain_heightmap",       MakeOps<dse::TerrainHeightmapComponent>()},
        // Ragdoll/Vehicle/Buoyancy 仅在 3D 物理后端（PhysX/Jolt）启用时定义（见
        // engine/ecs/components_3d_physics.h），无物理构建（如 Web MVP）下不绑定。
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
        {"ragdoll",                 MakeOps<dse::RagdollComponent>()},
#endif
        {"soft_body",               MakeOps<dse::SoftBodyComponent>()},
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
        {"vehicle",                 MakeOps<dse::VehicleComponent>()},
#endif
        {"rope",                    MakeOps<dse::RopeComponent>()},
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
        {"buoyancy",                MakeOps<dse::BuoyancyComponent>()},
#endif
        {"cloth",                   MakeOps<dse::ClothComponent>()},
        {"fluid_emitter",           MakeOps<dse::FluidEmitterComponent>()},
        // 动画
        {"animator_3d",             MakeOps<dse::Animator3DComponent>()},
        {"anim_layer",              MakeOps<dse::AnimLayerComponent>()},
        {"ik_chain_3d",             MakeOps<dse::IKChain3DComponent>()},
        {"foot_ik",                 MakeOps<dse::FootIK3DComponent>()},
        {"bone_attachment",         MakeOps<dse::BoneAttachmentComponent>()},
        // 粒子 / 天空 / 天气
        {"particle_system_3d",      MakeOps<dse::ParticleSystem3DComponent>()},
        {"atmosphere",              MakeOps<dse::AtmosphereComponent>()},
        {"volumetric_cloud",        MakeOps<dse::VolumetricCloudComponent>()},
        {"day_night_cycle",         MakeOps<dse::DayNightCycleComponent>()},
        {"weather",                 MakeOps<dse::WeatherComponent>()},
        {"snow_cover",              MakeOps<dse::SnowCoverComponent>()},
        {"fracture",                MakeOps<dse::FractureComponent>()},
        // 导航
        {"dynamic_obstacle",        MakeOps<dse::DynamicObstacleComponent>()},
        {"navmesh_auto_rebake",     MakeOps<dse::NavMeshAutoRebakeComponent>()},
    };
    return table;
}

}  // namespace

// ============================================================
// SubScene / SceneManager
// ============================================================

extern "C" int dse_scene_load_sub(const char* path, int* out_entity_count) {
    if (out_entity_count) *out_entity_count = 0;
    World* w = GW();
    AssetManager* am = GAM();
    if (!w || !am || !path) return 0;
    scene::SubScene sub;
    if (!sub.Load(*w, *am, ResolveScenePath(path))) return 0;
    if (out_entity_count) *out_entity_count = static_cast<int>(sub.EntityCount());
    return 1;
}

extern "C" int dse_scene_load_sub_async(const char* path) {
    auto* sm = GetSceneManager();
    if (!sm || !path) return 0;
    sm->LoadSubSceneAsync(ResolveScenePath(path));
    return 1;
}

extern "C" void dse_scene_unload_sub(const char* path) {
    auto* sm = GetSceneManager();
    if (sm && path) sm->UnloadSubScene(ResolveScenePath(path));
}

extern "C" void dse_scene_unload_all_subs(void) {
    if (auto* sm = GetSceneManager()) sm->UnloadAll();
}

extern "C" int dse_scene_is_sub_loaded(const char* path) {
    auto* sm = GetSceneManager();
    if (!sm || !path) return 0;
    return sm->IsSubSceneLoaded(ResolveScenePath(path)) ? 1 : 0;
}

extern "C" int dse_scene_get_loaded_subs(char* out, int cap) {
    auto* sm = GetSceneManager();
    if (!sm) {
        if (out && cap > 0) out[0] = '\0';
        return 0;
    }
    auto paths = sm->GetLoadedSubScenes();
    std::string joined;
    for (size_t i = 0; i < paths.size(); ++i) {
        if (i > 0) joined += '\n';
        joined += paths[i];
    }
    CopyStr(joined, out, cap);
    return static_cast<int>(paths.size());
}

extern "C" int dse_scene_get_sub_count(void) {
    auto* sm = GetSceneManager();
    return sm ? static_cast<int>(sm->LoadedCount()) : 0;
}

extern "C" int dse_scene_get_pending_count(void) {
    auto* sm = GetSceneManager();
    return sm ? static_cast<int>(sm->PendingCount()) : 0;
}

extern "C" void dse_scene_transition_to(const char* path, int mode, float fade_duration) {
    auto* sm = GetSceneManager();
    if (!sm || !path) return;
    scene::TransitionMode m = scene::TransitionMode::Fade;
    if (mode == 0) m = scene::TransitionMode::Instant;
    else if (mode == 1) m = scene::TransitionMode::Additive;
    sm->TransitionTo(ResolveScenePath(path), m, fade_duration);
}

extern "C" int dse_scene_get_transition_state(void) {
    auto* sm = GetSceneManager();
    if (!sm) return 0;
    switch (sm->GetTransitionState()) {
        case scene::TransitionState::FadingOut: return 1;
        case scene::TransitionState::Loading:   return 2;
        case scene::TransitionState::FadingIn:  return 3;
        case scene::TransitionState::Idle:
        default:                                return 0;
    }
}

extern "C" float dse_scene_get_fade_progress(void) {
    auto* sm = GetSceneManager();
    return sm ? sm->GetFadeProgress() : 0.0f;
}

extern "C" int dse_scene_get_active(char* out, int cap) {
    auto* sm = GetSceneManager();
    return CopyStr(sm ? sm->GetActiveScenePath() : std::string(), out, cap);
}

// ============================================================
// UUIDComponent
// ============================================================

extern "C" int dse_uuid_get(uint32_t e, char* out, int cap) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return 0;
    const auto* uc = w->registry().try_get<UUIDComponent>(TE(e));
    if (!uc || uc->uuid == 0) return 0;
    CopyStr(uc->ToString(), out, cap);
    return 1;
}

extern "C" int dse_uuid_set(uint32_t e, const char* uuid_str, char* out, int cap) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return 0;
    uint64_t uuid = uuid_str ? UUIDComponent::FromString(uuid_str) : UUIDComponent::Generate();
    auto& uc = w->registry().emplace_or_replace<UUIDComponent>(TE(e));
    uc.uuid = uuid;
    CopyStr(uc.ToString(), out, cap);
    return 1;
}

extern "C" uint32_t dse_uuid_resolve(const char* uuid_str) {
    auto* sm = GetSceneManager();
    if (!sm || !uuid_str) return 0xFFFFFFFFu;
    uint64_t uuid = UUIDComponent::FromString(uuid_str);
    Entity e = sm->ResolveReference(uuid);
    if (e == entt::null) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(e);
}

// ============================================================
// 通用组件查询
// ============================================================

extern "C" int dse_ecs_find_entities_by_mesh_path(const char* mesh_path,
                                                  uint32_t* out, int cap) {
    World* w = GW();
    if (!w || !mesh_path) return 0;
    int total = 0;
    auto view = w->registry().view<MeshRendererComponent>();
    for (auto entity : view) {
        const auto& mesh = view.get<MeshRendererComponent>(entity);
        if (mesh.mesh_path == mesh_path) {
            if (out && total < cap) out[total] = static_cast<uint32_t>(entity);
            ++total;
        }
    }
    return total;
}

extern "C" int dse_ecs_find_entities_with(const char* component, uint32_t* out, int cap) {
    if (!component) return -1;
    const auto& table = ComponentOpsTable();
    auto it = table.find(component);
    if (it == table.end()) return -1;
    World* w = GW();
    if (!w) return 0;
    return it->second.collect(w, out, cap);
}

extern "C" int dse_ecs_count_entities_with(const char* component) {
    if (!component) return -1;
    const auto& table = ComponentOpsTable();
    auto it = table.find(component);
    if (it == table.end()) return -1;
    World* w = GW();
    return w ? it->second.count(w) : 0;
}

extern "C" int dse_ecs_has_component(uint32_t e, const char* component) {
    if (!component) return -1;
    const auto& table = ComponentOpsTable();
    auto it = table.find(component);
    if (it == table.end()) return -1;
    World* w = GW();
    return w ? it->second.has(w, e) : 0;
}

extern "C" int dse_ecs_get_queryable_components(char* out, int cap) {
    const auto& table = ComponentOpsTable();
    std::string joined;
    int count = 0;
    for (const auto& kv : table) {
        if (count > 0) joined += '\n';
        joined += kv.first;
        ++count;
    }
    CopyStr(joined, out, cap);
    return count;
}

// ============================================================
// AABB
// ============================================================

extern "C" int dse_ecs_get_world_aabb(uint32_t e, float* out_min_max) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return 0;
    const auto* bbox = w->registry().try_get<dse::BoundingBoxComponent>(TE(e));
    if (!bbox) return 0;

    glm::vec3 wmin;
    glm::vec3 wmax;
    const auto* tc = w->registry().try_get<TransformComponent>(TE(e));
    if (tc) {
        // 中心经矩阵变换；半长用矩阵各列绝对值缩放（保守 OBB->AABB 包覆）。
        const glm::vec3 center = glm::vec3(tc->local_to_world * glm::vec4(bbox->center(), 1.0f));
        const glm::vec3 ext = bbox->extents();
        const glm::mat3 m(tc->local_to_world);
        const glm::vec3 world_ext(
            std::abs(m[0][0]) * ext.x + std::abs(m[1][0]) * ext.y + std::abs(m[2][0]) * ext.z,
            std::abs(m[0][1]) * ext.x + std::abs(m[1][1]) * ext.y + std::abs(m[2][1]) * ext.z,
            std::abs(m[0][2]) * ext.x + std::abs(m[1][2]) * ext.y + std::abs(m[2][2]) * ext.z);
        wmin = center - world_ext;
        wmax = center + world_ext;
    } else {
        wmin = bbox->min_extents;
        wmax = bbox->max_extents;
    }

    if (out_min_max) {
        out_min_max[0] = wmin.x; out_min_max[1] = wmin.y; out_min_max[2] = wmin.z;
        out_min_max[3] = wmax.x; out_min_max[4] = wmax.y; out_min_max[5] = wmax.z;
    }
    return 1;
}

extern "C" int dse_ecs_get_local_aabb(uint32_t e, float* out_min_max) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return 0;
    const auto* bbox = w->registry().try_get<dse::BoundingBoxComponent>(TE(e));
    if (!bbox) return 0;
    if (out_min_max) {
        out_min_max[0] = bbox->min_extents.x;
        out_min_max[1] = bbox->min_extents.y;
        out_min_max[2] = bbox->min_extents.z;
        out_min_max[3] = bbox->max_extents.x;
        out_min_max[4] = bbox->max_extents.y;
        out_min_max[5] = bbox->max_extents.z;
    }
    return 1;
}

// ============================================================
// TimeScaleComponent
// ============================================================

extern "C" void dse_ecs_set_time_scale(uint32_t e, float scale) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    if (scale < 0.0f) scale = 0.0f;
    w->registry().emplace_or_replace<dse::TimeScaleComponent>(TE(e), scale);
}

extern "C" float dse_ecs_get_time_scale(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return 1.0f;
    const auto* ts = w->registry().try_get<dse::TimeScaleComponent>(TE(e));
    return ts ? ts->scale : 1.0f;
}

// ============================================================
// Transform / Parent / Script
// ============================================================

extern "C" void dse_ecs_add_transform(uint32_t e, float x, float y, float z,
                                      float sx, float sy, float sz) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& transform = w->registry().emplace_or_replace<TransformComponent>(TE(e));
    transform.position = glm::vec3(x, y, z);
    transform.scale = glm::vec3(sx, sy, sz);
    transform.dirty = true;
}

extern "C" void dse_ecs_add_parent(uint32_t e, uint32_t parent) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& pc = w->registry().emplace_or_replace<ParentComponent>(TE(e));
    pc.parent = TE(parent);
}

extern "C" void dse_ecs_set_parent(uint32_t e, uint32_t parent) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto* pc = w->registry().try_get<ParentComponent>(TE(e));
    if (pc) pc->parent = TE(parent);
}

extern "C" uint32_t dse_ecs_get_parent(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return 0xFFFFFFFFu;
    const auto* pc = w->registry().try_get<ParentComponent>(TE(e));
    if (!pc || pc->parent == entt::null) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(pc->parent);
}

extern "C" void dse_ecs_clear_parent(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    if (w->registry().all_of<ParentComponent>(TE(e))) {
        w->registry().remove<ParentComponent>(TE(e));
    }
}

extern "C" void dse_ecs_add_script(uint32_t e, const char* path) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e)) || !path) return;
    auto& sc = w->registry().emplace_or_replace<ScriptComponent>(TE(e));
    sc.script_path = path;
    sc.enabled = true;
}

extern "C" void dse_ecs_set_script_path(uint32_t e, const char* path) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e)) || !path) return;
    auto* sc = w->registry().try_get<ScriptComponent>(TE(e));
    if (sc) sc->script_path = path;
}

extern "C" int dse_ecs_get_script_path(uint32_t e, char* out, int cap) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return -1;
    const auto* sc = w->registry().try_get<ScriptComponent>(TE(e));
    if (!sc) return -1;
    return CopyStr(sc->script_path, out, cap);
}

extern "C" void dse_ecs_set_script_enabled(uint32_t e, int enabled) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto* sc = w->registry().try_get<ScriptComponent>(TE(e));
    if (sc) sc->enabled = (enabled != 0);
}

extern "C" int dse_ecs_get_script_enabled(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return -1;
    const auto* sc = w->registry().try_get<ScriptComponent>(TE(e));
    if (!sc) return -1;
    return sc->enabled ? 1 : 0;
}
