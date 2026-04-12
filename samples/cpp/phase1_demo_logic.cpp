#include "samples/cpp/phase1_demo_logic.h"
#include "samples/cpp/phase1_demo_config.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/assets/asset_manager.h"
#include "engine/scene/scene.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "engine/input/input.h"
#include "engine/input/key_code.h"

namespace dse::samples::cpp_demo {
namespace {
struct DemoState {
    bool initialized = false;
    bool reference_demo_15_9_mode = false;
    bool reference_demo_15_9_materials_initialized = false;
    AssetManager* runtime_asset_manager = nullptr;
    int frame_counter = 0;
    int spawn_index = 0;
    unsigned int texture_handle = 0;
    unsigned int normal_handle = 0;
    unsigned int reference_material0_id = 430001;
    unsigned int reference_material1_id = 430002;
    float reference_specular_pow0 = 8.0f;
    float reference_specular_pow1 = 4.0f;
    Entity camera = entt::null;
    Entity ground = entt::null;
    Entity dlight = entt::null;
    config::StressSettings settings = config::kDemo2D;
};

DemoState& State() {
    static DemoState state;
    return state;
}

const char* ResolveStartupScenePath() {
    if (const char* env = std::getenv(config::kStartupSceneEnv)) {
        return env;
    }
    return nullptr;
}

float Clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

float MapSpecularPowToRoughness(float specular_pow) {
    const float normalized = Clamp01((specular_pow - 1.0f) / 16.0f);
    return 1.0f - normalized;
}

float MapSpecularPowToEmissive(float specular_pow) {
    const float normalized = Clamp01((specular_pow - 1.0f) / 16.0f);
    return 0.02f + normalized * 0.18f;
}

void EnsureReferenceDemo159Materials(AssetManager& asset_manager) {
    auto& state = State();
    if (state.reference_demo_15_9_materials_initialized) {
        return;
    }

    auto material0 = asset_manager.GetMaterialInstance(state.reference_material0_id);
    if (!material0) {
        material0 = asset_manager.CreateMaterialInstance("reference_demo_15_9_mesh_0");
    }
    auto material1 = asset_manager.GetMaterialInstance(state.reference_material1_id);
    if (!material1) {
        material1 = asset_manager.CreateMaterialInstance("reference_demo_15_9_mesh_1");
    }

    if (material0) {
        material0->SetShaderVariant("MESH_PBR");
        material0->SetBlendMode(MaterialBlendMode::Opaque);
        material0->SetBaseColor(glm::vec4(0.95f, 0.95f, 1.0f, 1.0f));
    }
    if (material1) {
        material1->SetShaderVariant("MESH_PBR");
        material1->SetBlendMode(MaterialBlendMode::Opaque);
        material1->SetBaseColor(glm::vec4(1.0f, 0.9f, 0.85f, 1.0f));
    }

    state.reference_material0_id = material0 ? material0->GetId() : state.reference_material0_id;
    state.reference_material1_id = material1 ? material1->GetId() : state.reference_material1_id;
    state.reference_demo_15_9_materials_initialized = material0 != nullptr && material1 != nullptr;
}

void ApplyReferenceDemo159MaterialParameters(AssetManager& asset_manager) {
    auto& state = State();
    auto material0 = asset_manager.GetMaterialInstance(state.reference_material0_id);
    auto material1 = asset_manager.GetMaterialInstance(state.reference_material1_id);
    if (!material0 || !material1) {
        return;
    }

    MaterialAsset::ScalarOverrides scalars0 = material0->GetScalarOverrides();
    scalars0.metallic = 0.08f;
    scalars0.roughness = MapSpecularPowToRoughness(state.reference_specular_pow0);
    scalars0.ao = 1.0f;
    scalars0.normal_strength = 1.0f;
    material0->SetScalarOverrides(scalars0);
    material0->SetEmissiveColor(glm::vec3(0.0f, 0.02f, MapSpecularPowToEmissive(state.reference_specular_pow0)));

    MaterialAsset::ScalarOverrides scalars1 = material1->GetScalarOverrides();
    scalars1.metallic = 0.12f;
    scalars1.roughness = MapSpecularPowToRoughness(state.reference_specular_pow1);
    scalars1.ao = 1.0f;
    scalars1.normal_strength = 1.0f;
    material1->SetScalarOverrides(scalars1);
    material1->SetEmissiveColor(glm::vec3(MapSpecularPowToEmissive(state.reference_specular_pow1), 0.01f, 0.0f));
}

void HandleReferenceDemo159Input(AssetManager& asset_manager) {
    auto& state = State();
    bool changed = false;
    if (Input::GetKeyDown(KEY_CODE_MINUS)) {
        state.reference_specular_pow0 = std::max(1.0f, state.reference_specular_pow0 - 0.1f);
        changed = true;
    }
    if (Input::GetKeyDown(KEY_CODE_EQUAL)) {
        state.reference_specular_pow0 = std::min(32.0f, state.reference_specular_pow0 + 0.1f);
        changed = true;
    }
    if (Input::GetKeyDown(KEY_CODE_LEFT_BRACKET)) {
        state.reference_specular_pow1 = std::max(1.0f, state.reference_specular_pow1 - 0.1f);
        changed = true;
    }
    if (Input::GetKeyDown(KEY_CODE_RIGHT_BRACKET)) {
        state.reference_specular_pow1 = std::min(32.0f, state.reference_specular_pow1 + 0.1f);
        changed = true;
    }

    if (!changed) {
        return;
    }

    ApplyReferenceDemo159MaterialParameters(asset_manager);
    std::cout << "[3D-Smoke][CPP] reference_demo_15_9_material_update spec0=" << state.reference_specular_pow0
              << " roughness0=" << MapSpecularPowToRoughness(state.reference_specular_pow0)
              << " spec1=" << state.reference_specular_pow1
              << " roughness1=" << MapSpecularPowToRoughness(state.reference_specular_pow1) << std::endl;
}

void ReportMvpResourceDiagnostics(World& world) {
    auto mesh_view = world.registry().view<MeshRendererComponent>();
    for (auto entity : mesh_view) {
        const auto& mesh = mesh_view.get<MeshRendererComponent>(entity);
        if (!mesh.mesh_path.empty() && !std::filesystem::exists(mesh.mesh_path)) {
            std::cout << "[3D-Smoke][CPP] mvp_resource_missing type=mesh path=" << mesh.mesh_path << std::endl;
        }
    }

    auto skybox_view = world.registry().view<SkyboxComponent>();
    for (auto entity : skybox_view) {
        const auto& skybox = skybox_view.get<SkyboxComponent>(entity);
        if (!skybox.cubemap_path.empty() && !std::filesystem::exists(skybox.cubemap_path)) {
            std::cout << "[3D-Smoke][CPP] mvp_resource_missing type=skybox path=" << skybox.cubemap_path << std::endl;
        }
    }

    auto terrain_view = world.registry().view<TerrainComponent>();
    for (auto entity : terrain_view) {
        const auto& terrain = terrain_view.get<TerrainComponent>(entity);
        if (!terrain.heightmap_path.empty() && !std::filesystem::exists(terrain.heightmap_path)) {
            std::cout << "[3D-Smoke][CPP] mvp_resource_missing type=terrain_heightmap path=" << terrain.heightmap_path << std::endl;
        }
    }
}

bool TryBootstrapFromStartupScene(World& world, AssetManager& asset_manager) {
    const char* startup_scene = ResolveStartupScenePath();
    std::cout << "[3D-Smoke][CPP] startup_scene_env=" << (startup_scene ? startup_scene : "<null>") << std::endl;
    if (startup_scene == nullptr || startup_scene[0] == '\0') {
        return false;
    }

    scene::Scene startup("cpp-startup-scene");
    if (!startup.Deserialize(startup_scene)) {
        std::cout << "[3D-Smoke][CPP] startup_scene_failed path=" << startup_scene << std::endl;
        return false;
    }

    world.Clear();
    for (auto entity : startup.GetWorld().registry().storage<entt::entity>()) {
        if (!startup.GetWorld().registry().valid(entity)) continue;
        const auto new_ent = world.CreateEntity();
        if (startup.GetWorld().registry().all_of<TransformComponent>(entity)) {
            world.registry().emplace<TransformComponent>(new_ent, startup.GetWorld().registry().get<TransformComponent>(entity));
        }
        if (startup.GetWorld().registry().all_of<MeshRendererComponent>(entity)) {
            world.registry().emplace<MeshRendererComponent>(new_ent, startup.GetWorld().registry().get<MeshRendererComponent>(entity));
        }
        if (startup.GetWorld().registry().all_of<Camera3DComponent>(entity)) {
            world.registry().emplace<Camera3DComponent>(new_ent, startup.GetWorld().registry().get<Camera3DComponent>(entity));
        }
        if (startup.GetWorld().registry().all_of<DirectionalLight3DComponent>(entity)) {
            world.registry().emplace<DirectionalLight3DComponent>(new_ent, startup.GetWorld().registry().get<DirectionalLight3DComponent>(entity));
        }
        if (startup.GetWorld().registry().all_of<PointLightComponent>(entity)) {
            world.registry().emplace<PointLightComponent>(new_ent, startup.GetWorld().registry().get<PointLightComponent>(entity));
        }
        if (startup.GetWorld().registry().all_of<SkyboxComponent>(entity)) {
            world.registry().emplace<SkyboxComponent>(new_ent, startup.GetWorld().registry().get<SkyboxComponent>(entity));
        }
        if (startup.GetWorld().registry().all_of<Animator3DComponent>(entity)) {
            auto animator = startup.GetWorld().registry().get<Animator3DComponent>(entity);
            animator.state_machine.reset();
            animator.final_bone_matrices.clear();
            world.registry().emplace<Animator3DComponent>(new_ent, std::move(animator));
        }
        if (startup.GetWorld().registry().all_of<TerrainComponent>(entity)) {
            auto terrain = startup.GetWorld().registry().get<TerrainComponent>(entity);
            terrain.is_dirty = true;
            world.registry().emplace<TerrainComponent>(new_ent, std::move(terrain));
        }
    }

    auto& state = State();
    state.initialized = true;
    state.runtime_asset_manager = &asset_manager;
    state.spawn_index = 0;
    state.frame_counter = 0;
    state.settings.total_boxes = 0;
    state.settings.spawn_per_frame = 0;
    state.settings.columns = 0;
    state.reference_demo_15_9_mode = std::string(startup_scene) == "assets/scenes/reference_demo_15_9.scene.json";
    if (state.reference_demo_15_9_mode) {
        EnsureReferenceDemo159Materials(asset_manager);
        ApplyReferenceDemo159MaterialParameters(asset_manager);
        auto mesh_view = world.registry().view<MeshRendererComponent>();
        size_t reference_mesh_index = 0;
        for (auto entity : mesh_view) {
            auto& mesh = mesh_view.get<MeshRendererComponent>(entity);
            if (mesh.material_instance_id == 430001 || mesh.material_instance_id == 430002) {
                mesh.material_instance_id = reference_mesh_index == 0 ? state.reference_material0_id : state.reference_material1_id;
                ++reference_mesh_index;
            }
        }
        std::cout << "[3D-Smoke][CPP] reference_demo_15_9_material_bootstrap material0=" << state.reference_material0_id
                  << " spec0=" << state.reference_specular_pow0
                  << " material1=" << state.reference_material1_id
                  << " spec1=" << state.reference_specular_pow1 << std::endl;
    }
    std::cout << "[3D-Smoke][CPP] startup_scene_loaded path=" << startup_scene << std::endl;
    ReportMvpResourceDiagnostics(world);
    return true;
}

Entity SpawnBox(World& world, int index, const DemoState& state) {
    const float start_x = -0.5f * static_cast<float>(state.settings.columns - 1) * state.settings.spacing;
    const float x = start_x + static_cast<float>(index % state.settings.columns) * state.settings.spacing;
    const float y = state.settings.start_y + static_cast<float>(index / state.settings.columns) * state.settings.spacing;

    const Entity entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(x, y, 0.0f);
    transform.scale = glm::vec3(state.settings.box_scale, state.settings.box_scale, 1.0f);
    transform.dirty = true;

    auto& mesh = world.registry().emplace<MeshRendererComponent>(entity);
    mesh.mesh_path = "models/cube.dmesh";
    mesh.color = glm::vec4(1.0f);
    mesh.shader_variant = "MESH_PBR";
    mesh.metallic = 0.1f;
    mesh.roughness = 0.5f;
    mesh.receive_shadow = true;
    mesh.visible = true;

    return entity;
}
}

void Bootstrap(World& world, AssetManager& asset_manager) {
    auto& state = State();
    state = {};
    state.settings = config::kDemo2D;
    state.settings.columns = 10;
    state.settings.total_boxes = 100;
    state.settings.spawn_per_frame = 10;
    if (state.initialized) {
        return;
    }

    if (TryBootstrapFromStartupScene(world, asset_manager)) {
        return;
    }

    state.camera = world.CreateEntity();
    auto& camera_transform = world.registry().emplace<TransformComponent>(state.camera);
    camera_transform.position = glm::vec3(0.0f, 5.0f, 15.0f);
    camera_transform.dirty = true;
    auto& camera = world.registry().emplace<Camera3DComponent>(state.camera);
    camera.fov = 60.0f;
    camera.near_clip = 0.1f;
    camera.far_clip = 1000.0f;
    
    auto& free_cam = world.registry().emplace<FreeCameraControllerComponent>(state.camera);
    free_cam.move_speed = 10.0f;

    auto texture = asset_manager.LoadTexture("models/CesiumLogoFlat.png");
    state.texture_handle = texture ? texture->GetHandle() : 0;

    state.dlight = world.CreateEntity();
    auto& light = world.registry().emplace<DirectionalLight3DComponent>(state.dlight);
    light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.5f));
    light.color = glm::vec3(1.0f, 0.9f, 0.8f);
    light.intensity = 2.0f;
    light.cast_shadow = true;

    state.ground = world.CreateEntity();
    auto& ground_transform = world.registry().emplace<TransformComponent>(state.ground);
    ground_transform.position = glm::vec3(0.0f, -2.0f, 0.0f);
    ground_transform.scale = glm::vec3(40.0f, 1.0f, 40.0f);
    ground_transform.dirty = true;

    auto& ground_mesh = world.registry().emplace<MeshRendererComponent>(state.ground);
    ground_mesh.mesh_path = "models/cube.dmesh";
    ground_mesh.color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    ground_mesh.shader_variant = "MESH_PBR";
    ground_mesh.metallic = 0.0f;
    ground_mesh.roughness = 0.9f;
    ground_mesh.receive_shadow = true;
    ground_mesh.visible = true;

    state.spawn_index = 0;
    state.frame_counter = 0;
    state.runtime_asset_manager = &asset_manager;
    state.initialized = true;
    std::cout << "[3D-Smoke][CPP] bootstrap_ok camera=1 light=1 ground=1 total_boxes=" << state.settings.total_boxes
              << " spawn_per_frame=" << state.settings.spawn_per_frame << std::endl;
}

void Tick(World& world, float delta_time) {
    auto& state = State();
    if (!state.initialized) {
        return;
    }
    if (state.reference_demo_15_9_mode && state.runtime_asset_manager != nullptr) {
        HandleReferenceDemo159Input(*state.runtime_asset_manager);
    }

    if (state.spawn_index < state.settings.total_boxes) {
        const int remaining = state.settings.total_boxes - state.spawn_index;
        const int batch = std::min(state.settings.spawn_per_frame, remaining);
        for (int i = 0; i < batch; ++i) {
            SpawnBox(world, state.spawn_index, state);
            state.spawn_index += 1;
        }
        if (state.spawn_index == state.settings.total_boxes) {
            std::cout << "[3D-Smoke][CPP] setup_finished boxes=" << state.settings.total_boxes << std::endl;
        }
    }

    state.frame_counter += 1;
    if (state.frame_counter % 60 != 0) {
        return;
    }

    const int draw_calls = FramePipeline::Instance().LastDrawCalls();
    const int max_batch = FramePipeline::Instance().LastMaxBatchSprites();
    const int sprite_count = FramePipeline::Instance().LastSpriteCount();
    const char* status = draw_calls > 9 ? "FAIL" : "PASS";
    std::cout << "[3D-Smoke][CPP] draw_calls=" << draw_calls
              << " max_batch=" << max_batch
              << " sprites=" << sprite_count
              << " spawned=" << state.spawn_index << "/" << state.settings.total_boxes
              << " status=" << status << std::endl;
}

void Shutdown() {
    State() = {};
}
}
