#include "samples/cpp/phase1_demo_logic.h"
#include "samples/cpp/phase1_demo_config.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include <algorithm>
#include <iostream>

namespace dse::samples::cpp_demo {
namespace {
struct DemoState {
    bool initialized = false;
    int frame_counter = 0;
    int spawn_index = 0;
    unsigned int texture_handle = 0;
    unsigned int normal_handle = 0;
    Entity camera = entt::null;
    Entity ground = entt::null;
    Entity dlight = entt::null;
    config::StressSettings settings = config::kDemo2D;
};

DemoState& State() {
    static DemoState state;
    return state;
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
    state.initialized = true;
    std::cout << "[3D-Smoke][CPP] bootstrap_ok camera=1 light=1 ground=1 total_boxes=" << state.settings.total_boxes
              << " spawn_per_frame=" << state.settings.spawn_per_frame << std::endl;
}

void Tick(World& world, float delta_time) {
    (void)delta_time;
    auto& state = State();
    if (!state.initialized) {
        return;
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
