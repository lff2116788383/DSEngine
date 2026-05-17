/**
 * @file game_application.cpp
 * @brief GameApplication 基类实现
 */

#include "engine/scripting/cpp/game_application.h"
#include "engine/scripting/cpp/cpp_business_runtime.h"

namespace dse::runtime {

int GameApplication::Run(EngineRunConfig config) {
    config.business_mode = BusinessMode::Cpp;

    ConfigureCppBusinessHooks({
        [this](World& w, AssetManager& am) { Bootstrap(w, am); },
        [this](World& w, float dt)         { Tick(w, dt); },
        [this]()                            { ShutdownInternal(); }
    });

    return RunEngine(config);
}

void GameApplication::Bootstrap(World& world, AssetManager& asset_manager) {
    world_ = &world;
    asset_manager_ = &asset_manager;
    OnInit();
}

void GameApplication::Tick(World& world, float delta_time) {
    OnUpdate(delta_time);
}

void GameApplication::ShutdownInternal() {
    OnShutdown();
    world_ = nullptr;
    asset_manager_ = nullptr;
}

// ─── ECS 基础操作 ──────────────────────────────────

Entity GameApplication::CreateEntity() {
    return world_->CreateEntity();
}

void GameApplication::DestroyEntity(Entity e) {
    world_->DestroyEntity(e);
}

// ─── 实体工厂 ──────────────────────────────────────

Entity GameApplication::CreateEntityAt(const glm::vec3& position,
                                       const glm::vec3& scale) {
    Entity e = CreateEntity();
    auto& t = Emplace<TransformComponent>(e);
    t.position = position;
    t.scale = scale;
    t.dirty = true;
    return e;
}

Entity GameApplication::CreateCamera3D(const glm::vec3& position,
                                       float fov,
                                       float near_clip,
                                       float far_clip) {
    Entity e = CreateEntityAt(position);
    auto& cam = Emplace<Camera3DComponent>(e);
    cam.fov = fov;
    cam.near_clip = near_clip;
    cam.far_clip = far_clip;
    cam.enabled = true;
    Emplace<FreeCameraControllerComponent>(e).move_speed = 8.0f;
    return e;
}

Entity GameApplication::CreateDirectionalLight(const glm::vec3& direction,
                                               const glm::vec3& color,
                                               float intensity,
                                               bool cast_shadow) {
    Entity e = CreateEntityAt(glm::vec3(0.0f));
    auto& light = Emplace<DirectionalLight3DComponent>(e);
    light.direction = glm::normalize(direction);
    light.color = color;
    light.intensity = intensity;
    light.cast_shadow = cast_shadow;
    return e;
}

Entity GameApplication::CreatePointLight(const glm::vec3& position,
                                         const glm::vec3& color,
                                         float intensity,
                                         float radius) {
    Entity e = CreateEntityAt(position);
    auto& light = Emplace<PointLightComponent>(e);
    light.color = color;
    light.intensity = intensity;
    light.radius = radius;
    return e;
}

Entity GameApplication::CreateMesh(const glm::vec3& position,
                                   const std::string& mesh_path,
                                   const glm::vec3& scale) {
    Entity e = CreateEntityAt(position, scale);
    auto& mesh = Emplace<MeshRendererComponent>(e);
    mesh.mesh_path = mesh_path;
    mesh.shader_variant = "MESH_PBR";
    mesh.visible = true;
    mesh.receive_shadow = true;
    return e;
}

// ─── 资产快捷方法 ──────────────────────────────────

unsigned int GameApplication::LoadTexture(const std::string& path) {
    auto tex = asset_manager_->LoadTexture(path);
    return tex ? tex->GetHandle() : 0;
}

} // namespace dse::runtime
