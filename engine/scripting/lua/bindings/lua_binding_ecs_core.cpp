/**
 * @file lua_binding_ecs_core.cpp
 * @brief ECS Lua 绑定 — 实体创建、场景加载、实体查询、层级、脚本
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/script.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/physics_2d.h"
#include "engine/ecs/gameplay.h"
#include "engine/ecs/components_3d_foliage.h"
#include "engine/scene/scene.h"
#include "engine/scene/sub_scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/ecs/uuid_component.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"

#include <glm/glm.hpp>
#include <cstring>
#include <string>
#include <unordered_map>
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

scene::SceneManager* GetSceneManager() {
    return core::ServiceLocator::Instance().Get<scene::SceneManager>();
}

// 将相对路径解析为基于 data root 的完整路径（与 load_sub_scene 行为一致）
std::string ResolveScenePath(const char* path) {
    AssetManager& am = GetAssetManager();
    std::string resolved = am.GetDataRoot();
    if (!resolved.empty() && resolved.back() != '/' && resolved.back() != '\\') {
        resolved += '/';
    }
    resolved += path;
    return resolved;
}

int L_EcsCreateEntity(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    Entity e = world->CreateEntity();
    helper::PushEntity(L, e);
    return 1;
}

int L_EcsLoadScene(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "world_unavailable");
        return 2;
    }

    const char* scene_path = luaL_checkstring(L, 1);
    scene::Scene scene_loader("lua_runtime_scene_loader");
    scene_loader.BindWorld(world);
    const bool ok = scene_loader.Deserialize(scene_path);
    scene_loader.UnbindWorld();

    lua_pushboolean(L, ok ? 1 : 0);
    if (ok) {
        lua_pushstring(L, "");
    } else {
        lua_pushstring(L, "scene_deserialize_failed");
    }
    return 2;
}

int L_EcsLoadSubScene(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "world_unavailable");
        return 2;
    }
    const char* path = luaL_checkstring(L, 1);
    AssetManager& am = GetAssetManager();
    std::string resolved = am.GetDataRoot();
    if (!resolved.empty() && resolved.back() != '/' && resolved.back() != '\\') {
        resolved += '/';
    }
    resolved += path;
    scene::SubScene sub;
    const bool ok = sub.Load(*world, am, resolved);
    lua_pushboolean(L, ok ? 1 : 0);
    if (ok) {
        lua_pushinteger(L, static_cast<lua_Integer>(sub.EntityCount()));
    } else {
        lua_pushstring(L, "sub_scene_load_failed");
    }
    return 2;
}

int L_EcsDestroyEntity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    if (world->registry().valid(e)) {
        world->DestroyEntity(e);
    }
    return 0;
}

int L_EcsFindEntitiesByMeshPath(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_newtable(L);
        return 1;
    }

    const char* mesh_path = luaL_checkstring(L, 1);
    lua_newtable(L);
    int index = 1;
    auto view = world->registry().view<MeshRendererComponent>();
    for (auto entity : view) {
        const auto& mesh = view.get<MeshRendererComponent>(entity);
        if (mesh.mesh_path == mesh_path) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, index++);
        }
    }
    return 1;
}

int L_EcsAddTransform(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float x  = helper::CheckFloat(L, 2);
    float y  = helper::CheckFloat(L, 3);
    float z  = helper::OptFloat(L, 4, 0.0f);
    float sx = helper::OptFloat(L, 5, 1.0f);
    float sy = helper::OptFloat(L, 6, 1.0f);
    float sz = helper::OptFloat(L, 7, 1.0f);
    auto& transform = world->registry().emplace_or_replace<TransformComponent>(e);
    transform.position = glm::vec3(x, y, z);
    transform.scale    = glm::vec3(sx, sy, sz);
    transform.dirty    = true;
    return 0;
}

// ============================================================
// ParentComponent（层级）
// ============================================================

int L_EcsAddParent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    Entity parent_e = helper::CheckEntity(L, 2);
    auto& pc = world->registry().emplace_or_replace<ParentComponent>(e);
    pc.parent = parent_e;
    return 0;
}

int L_EcsSetParent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    Entity parent_e = helper::CheckEntity(L, 2);
    auto* pc = helper::TryGetComponent<ParentComponent>(*world, e);
    if (!pc) return 0;
    pc->parent = parent_e;
    return 0;
}

int L_EcsGetParent(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnil(L); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* pc = helper::TryGetComponentConst<ParentComponent>(*world, e);
    if (!pc || pc->parent == entt::null) {
        lua_pushnil(L);
        return 1;
    }
    helper::PushEntity(L, pc->parent);
    return 1;
}

int L_EcsClearParent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    if (world->registry().valid(e) && world->registry().all_of<ParentComponent>(e)) {
        world->registry().remove<ParentComponent>(e);
    }
    return 0;
}

// ============================================================
// ScriptComponent
// ============================================================

int L_EcsAddScript(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* script_path = luaL_checkstring(L, 2);
    auto& sc = world->registry().emplace_or_replace<ScriptComponent>(e);
    sc.script_path = script_path;
    sc.enabled = true;
    return 0;
}

int L_EcsSetScriptPath(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* path = luaL_checkstring(L, 2);
    auto* sc = helper::TryGetComponent<ScriptComponent>(*world, e);
    if (!sc) return 0;
    sc->script_path = path;
    return 0;
}

int L_EcsGetScriptPath(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnil(L); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* sc = helper::TryGetComponentConst<ScriptComponent>(*world, e);
    if (!sc) { lua_pushnil(L); return 1; }
    lua_pushstring(L, sc->script_path.c_str());
    return 1;
}

int L_EcsSetScriptEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool enabled = helper::CheckBool(L, 2);
    auto* sc = helper::TryGetComponent<ScriptComponent>(*world, e);
    if (!sc) return 0;
    sc->enabled = enabled;
    return 0;
}

int L_EcsGetScriptEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* sc = helper::TryGetComponentConst<ScriptComponent>(*world, e);
    if (!sc) { lua_pushboolean(L, 0); return 1; }
    helper::PushBool(L, sc->enabled);
    return 1;
}

// ============================================================
// SceneManager（异步加载 / 卸载 / 查询 / 场景过渡）
// ============================================================

// ecs.load_sub_scene_async(path)
int L_EcsLoadSubSceneAsync(lua_State* L) {
    auto* sm = GetSceneManager();
    if (!sm) { lua_pushboolean(L, 0); return 1; }
    const char* path = luaL_checkstring(L, 1);
    sm->LoadSubSceneAsync(ResolveScenePath(path));
    lua_pushboolean(L, 1);
    return 1;
}

// ecs.unload_sub_scene(path)
int L_EcsUnloadSubScene(lua_State* L) {
    auto* sm = GetSceneManager();
    if (!sm) return 0;
    const char* path = luaL_checkstring(L, 1);
    sm->UnloadSubScene(ResolveScenePath(path));
    return 0;
}

// ecs.unload_all_sub_scenes()
int L_EcsUnloadAllSubScenes(lua_State* L) {
    auto* sm = GetSceneManager();
    if (sm) sm->UnloadAll();
    return 0;
}

// ecs.is_sub_scene_loaded(path) -> bool
int L_EcsIsSubSceneLoaded(lua_State* L) {
    auto* sm = GetSceneManager();
    if (!sm) { lua_pushboolean(L, 0); return 1; }
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, sm->IsSubSceneLoaded(ResolveScenePath(path)) ? 1 : 0);
    return 1;
}

// ecs.get_loaded_sub_scenes() -> table（完整解析后的路径）
int L_EcsGetLoadedSubScenes(lua_State* L) {
    auto* sm = GetSceneManager();
    lua_newtable(L);
    if (!sm) return 1;
    auto paths = sm->GetLoadedSubScenes();
    for (size_t i = 0; i < paths.size(); ++i) {
        lua_pushstring(L, paths[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

// ecs.get_sub_scene_count() -> int
int L_EcsGetSubSceneCount(lua_State* L) {
    auto* sm = GetSceneManager();
    lua_pushinteger(L, sm ? static_cast<lua_Integer>(sm->LoadedCount()) : 0);
    return 1;
}

// ecs.get_pending_scene_count() -> int
int L_EcsGetPendingSceneCount(lua_State* L) {
    auto* sm = GetSceneManager();
    lua_pushinteger(L, sm ? static_cast<lua_Integer>(sm->PendingCount()) : 0);
    return 1;
}

// ecs.transition_to(path, [mode="fade"], [fade_duration=0.5])
//   mode: "instant" | "additive" | "fade"，或整数 0/1/2
int L_EcsTransitionTo(lua_State* L) {
    auto* sm = GetSceneManager();
    if (!sm) return 0;
    const char* path = luaL_checkstring(L, 1);
    scene::TransitionMode mode = scene::TransitionMode::Fade;
    if (lua_isnumber(L, 2)) {
        int m = static_cast<int>(lua_tointeger(L, 2));
        if (m == 0) mode = scene::TransitionMode::Instant;
        else if (m == 1) mode = scene::TransitionMode::Additive;
        else mode = scene::TransitionMode::Fade;
    } else if (lua_isstring(L, 2)) {
        const char* s = lua_tostring(L, 2);
        if (std::strcmp(s, "instant") == 0) mode = scene::TransitionMode::Instant;
        else if (std::strcmp(s, "additive") == 0) mode = scene::TransitionMode::Additive;
        else mode = scene::TransitionMode::Fade;
    }
    float fade = static_cast<float>(luaL_optnumber(L, 3, 0.5));
    sm->TransitionTo(ResolveScenePath(path), mode, fade);
    return 0;
}

// ecs.get_transition_state() -> string ("idle"|"fading_out"|"loading"|"fading_in")
int L_EcsGetTransitionState(lua_State* L) {
    auto* sm = GetSceneManager();
    const char* s = "idle";
    if (sm) {
        switch (sm->GetTransitionState()) {
            case scene::TransitionState::FadingOut: s = "fading_out"; break;
            case scene::TransitionState::Loading:   s = "loading";    break;
            case scene::TransitionState::FadingIn:  s = "fading_in";  break;
            case scene::TransitionState::Idle:
            default:                                s = "idle";       break;
        }
    }
    lua_pushstring(L, s);
    return 1;
}

// ecs.get_fade_progress() -> number [0,1]
int L_EcsGetFadeProgress(lua_State* L) {
    auto* sm = GetSceneManager();
    lua_pushnumber(L, sm ? static_cast<lua_Number>(sm->GetFadeProgress()) : 0.0);
    return 1;
}

// ecs.get_active_scene() -> string
int L_EcsGetActiveScene(lua_State* L) {
    auto* sm = GetSceneManager();
    lua_pushstring(L, sm ? sm->GetActiveScenePath().c_str() : "");
    return 1;
}

// ============================================================
// UUIDComponent（跨场景稳定引用）
// ============================================================

// ecs.get_uuid(e) -> string|nil（16 位十六进制）
int L_EcsGetUuid(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnil(L); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* uc = helper::TryGetComponentConst<UUIDComponent>(*world, e);
    if (!uc || uc->uuid == 0) { lua_pushnil(L); return 1; }
    lua_pushstring(L, uc->ToString().c_str());
    return 1;
}

// ecs.set_uuid(e, [uuid_str]) -> string
//   省略 uuid_str 时自动生成；返回最终的 UUID 十六进制字符串
int L_EcsSetUuid(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnil(L); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    uint64_t uuid;
    if (lua_isstring(L, 2)) {
        uuid = UUIDComponent::FromString(lua_tostring(L, 2));
    } else {
        uuid = UUIDComponent::Generate();
    }
    auto& uc = world->registry().emplace_or_replace<UUIDComponent>(e);
    uc.uuid = uuid;
    lua_pushstring(L, uc.ToString().c_str());
    return 1;
}

// ecs.resolve_uuid(uuid_str) -> entity|nil
//   仅能解析经由 SceneManager 加载的子场景中带 UUIDComponent 的实体
int L_EcsResolveUuid(lua_State* L) {
    auto* sm = GetSceneManager();
    if (!sm) { lua_pushnil(L); return 1; }
    uint64_t uuid = 0;
    if (lua_isstring(L, 1)) {
        uuid = UUIDComponent::FromString(lua_tostring(L, 1));
    } else {
        uuid = static_cast<uint64_t>(luaL_checkinteger(L, 1));
    }
    Entity e = sm->ResolveReference(uuid);
    if (e == entt::null) { lua_pushnil(L); return 1; }
    helper::PushEntity(L, e);
    return 1;
}

// ============================================================
// 通用 ECS 组件查询（按组件名字符串）
// ============================================================

template <typename T>
void CollectComponentView(World* w, lua_State* L, int& idx) {
    auto view = w->registry().view<T>();
    for (auto e : view) {
        helper::PushEntity(L, e);
        lua_rawseti(L, -2, idx++);
    }
}

template <typename T>
size_t CountComponentView(World* w) {
    return w->registry().view<T>().size();
}

template <typename T>
bool HasComponentT(World* w, Entity e) {
    return w->registry().all_of<T>(e);
}

struct ComponentOps {
    void (*collect)(World*, lua_State*, int&);
    size_t (*count)(World*);
    bool (*has)(World*, Entity);
};

template <typename T>
ComponentOps MakeOps() {
    return ComponentOps{ &CollectComponentView<T>, &CountComponentView<T>, &HasComponentT<T> };
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

// ecs.find_entities_with(component_name) -> table（持有该组件的全部实体）
//   component_name 见文档 §5.1 支持列表；未知名抛出 Lua 错误。
int L_EcsFindEntitiesWith(lua_State* L) {
    World* world = GetWorld();
    const char* name = luaL_checkstring(L, 1);
    const auto& table = ComponentOpsTable();
    auto it = table.find(name);
    if (it == table.end()) {
        return luaL_error(L, "find_entities_with: unknown component '%s'", name);
    }
    lua_newtable(L);
    if (!world) return 1;
    int idx = 1;
    it->second.collect(world, L, idx);
    return 1;
}

// ecs.count_entities_with(component_name) -> int
int L_EcsCountEntitiesWith(lua_State* L) {
    World* world = GetWorld();
    const char* name = luaL_checkstring(L, 1);
    const auto& table = ComponentOpsTable();
    auto it = table.find(name);
    if (it == table.end()) {
        return luaL_error(L, "count_entities_with: unknown component '%s'", name);
    }
    lua_pushinteger(L, world ? static_cast<lua_Integer>(it->second.count(world)) : 0);
    return 1;
}

// ecs.has_component(entity, component_name) -> bool
int L_EcsHasComponent(lua_State* L) {
    World* world = GetWorld();
    Entity e = helper::CheckEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    const auto& table = ComponentOpsTable();
    auto it = table.find(name);
    if (it == table.end()) {
        return luaL_error(L, "has_component: unknown component '%s'", name);
    }
    lua_pushboolean(L, (world && it->second.has(world, e)) ? 1 : 0);
    return 1;
}

// ecs.get_queryable_components() -> table（全部支持查询的组件名）
int L_EcsGetQueryableComponents(lua_State* L) {
    const auto& table = ComponentOpsTable();
    lua_newtable(L);
    int idx = 1;
    for (const auto& kv : table) {
        lua_pushstring(L, kv.first.c_str());
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

// ============================================================
// 场景 / 预制体 保存（写盘，补齐此前只读的加载侧）
// ============================================================

// ecs.save_scene(path) -> bool, string
//   把当前 World 完整序列化到 path（路径按字面使用，不做 data root 拼接）。
int L_EcsSaveScene(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "world_unavailable");
        return 2;
    }
    const char* path = luaL_checkstring(L, 1);
    scene::Scene saver("lua_runtime_scene_saver");
    saver.BindWorld(world);
    const bool ok = saver.Serialize(path);
    saver.UnbindWorld();
    lua_pushboolean(L, ok ? 1 : 0);
    lua_pushstring(L, ok ? "" : "scene_serialize_failed");
    return 2;
}

// ecs.save_prefab(entity, path) -> bool
int L_EcsSavePrefab(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const char* path = luaL_checkstring(L, 2);
    lua_pushboolean(L, scene::SaveEntityAsPrefab(*world, e, path) ? 1 : 0);
    return 1;
}

// ecs.instantiate_prefab(path, [x, y, z]) -> entity|nil
//   省略坐标时使用预制体内置 Transform。
int L_EcsInstantiatePrefab(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnil(L); return 1; }
    const char* path = luaL_checkstring(L, 1);
    Entity e;
    if (lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
        scene::PrefabInstantiateOptions opts;
        opts.override_position = true;
        opts.position = glm::vec3(static_cast<float>(lua_tonumber(L, 2)),
                                  static_cast<float>(lua_tonumber(L, 3)),
                                  static_cast<float>(lua_tonumber(L, 4)));
        e = scene::InstantiatePrefab(*world, path, opts);
    } else {
        e = scene::InstantiatePrefab(*world, path);
    }
    if (e == entt::null) { lua_pushnil(L); return 1; }
    helper::PushEntity(L, e);
    return 1;
}

} // namespace

void RegisterEcsCoreBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"create_entity",              L_EcsCreateEntity},
        {"destroy_entity",             L_EcsDestroyEntity},
        {"load_scene",                 L_EcsLoadScene},
        {"load_sub_scene",             L_EcsLoadSubScene},
        // SceneManager（异步/卸载/查询/过渡）
        {"load_sub_scene_async",       L_EcsLoadSubSceneAsync},
        {"unload_sub_scene",           L_EcsUnloadSubScene},
        {"unload_all_sub_scenes",      L_EcsUnloadAllSubScenes},
        {"is_sub_scene_loaded",        L_EcsIsSubSceneLoaded},
        {"get_loaded_sub_scenes",      L_EcsGetLoadedSubScenes},
        {"get_sub_scene_count",        L_EcsGetSubSceneCount},
        {"get_pending_scene_count",    L_EcsGetPendingSceneCount},
        {"transition_to",              L_EcsTransitionTo},
        {"get_transition_state",       L_EcsGetTransitionState},
        {"get_fade_progress",          L_EcsGetFadeProgress},
        {"get_active_scene",           L_EcsGetActiveScene},
        // UUIDComponent（跨场景稳定引用）
        {"get_uuid",                   L_EcsGetUuid},
        {"set_uuid",                   L_EcsSetUuid},
        {"resolve_uuid",               L_EcsResolveUuid},
        {"find_entities_by_mesh_path", L_EcsFindEntitiesByMeshPath},
        // 通用组件查询
        {"find_entities_with",         L_EcsFindEntitiesWith},
        {"count_entities_with",        L_EcsCountEntitiesWith},
        {"has_component",              L_EcsHasComponent},
        {"get_queryable_components",   L_EcsGetQueryableComponents},
        // 场景 / 预制体保存（写盘）
        {"save_scene",                 L_EcsSaveScene},
        {"save_prefab",                L_EcsSavePrefab},
        {"instantiate_prefab",         L_EcsInstantiatePrefab},
        {"add_transform",              L_EcsAddTransform},
        // ParentComponent
        {"add_parent",                 L_EcsAddParent},
        {"set_parent",                 L_EcsSetParent},
        {"get_parent",                 L_EcsGetParent},
        {"clear_parent",               L_EcsClearParent},
        // ScriptComponent
        {"add_script",                 L_EcsAddScript},
        {"set_script_path",            L_EcsSetScriptPath},
        {"get_script_path",            L_EcsGetScriptPath},
        {"set_script_enabled",         L_EcsSetScriptEnabled},
        {"get_script_enabled",         L_EcsGetScriptEnabled},
    });
}

} // namespace dse::runtime::lua_binding
