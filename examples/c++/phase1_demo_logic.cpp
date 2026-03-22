#include "c++/phase1_demo_logic.h"
#include "c++/phase1_demo_config.h"
#include "phase1/runtime/frame_pipeline.h"
#include "phase1/asset/asset_manager.h"
#include "phase1/ecs/components_2d.h"
#include <algorithm>
#include <iostream>

namespace phase1::examples::cpp_demo {
namespace {
struct DemoState {
    bool initialized = false;
    int frame_counter = 0;
    int spawn_index = 0;
    unsigned int texture_handle = 0;
    Entity camera = entt::null;
    Entity ground = entt::null;
    config::StressSettings settings = config::kPhase1_2D;
};

DemoState& State() {
    static DemoState state;
    return state;
}

Entity SpawnBox(Phase1World& world, int index, const DemoState& state) {
    const float start_x = -0.5f * static_cast<float>(state.settings.columns - 1) * state.settings.spacing;
    const float x = start_x + static_cast<float>(index % state.settings.columns) * state.settings.spacing;
    const float y = state.settings.start_y + static_cast<float>(index / state.settings.columns) * state.settings.spacing;

    const Entity entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(x, y, 0.0f);
    transform.scale = glm::vec3(state.settings.box_scale, state.settings.box_scale, 1.0f);
    transform.dirty = true;

    auto& sprite = world.registry().emplace<SpriteRendererComponent>(entity);
    sprite.color = glm::vec4(0.9f, 0.95f, 1.0f, 1.0f);
    sprite.order_in_layer = index;
    sprite.texture_handle = state.texture_handle;
    sprite.visible = true;

    auto& rb = world.registry().emplace<RigidBody2DComponent>(entity);
    rb.type = RigidBody2DType::Dynamic;
    rb.gravity_scale = 1.0f;
    rb.fixed_rotation = false;

    auto& collider = world.registry().emplace<BoxCollider2DComponent>(entity);
    collider.size = glm::vec2(state.settings.box_scale, state.settings.box_scale);
    collider.density = 1.0f;
    collider.friction = 0.3f;
    collider.restitution = 0.5f;
    return entity;
}
}

void Bootstrap(Phase1World& world) {
    auto& state = State();
    state = {};
    state.settings = config::kPhase1_2D;
    if (state.initialized) {
        return;
    }

    state.camera = world.CreateEntity();
    auto& camera_transform = world.registry().emplace<TransformComponent>(state.camera);
    camera_transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    camera_transform.dirty = true;
    auto& camera = world.registry().emplace<CameraComponent>(state.camera);
    camera.orthographic = true;
    camera.orthographic_size = state.settings.camera_ortho_size;

    auto texture = Phase1AssetManager::Instance().LoadTexture("mirror_assets/Resources/item/1.png");
    if (!texture) {
        texture = Phase1AssetManager::Instance().LoadTexture("data/mirror_assets/Resources/item/1.png");
    }
    state.texture_handle = texture ? texture->GetHandle() : 0;

    state.ground = world.CreateEntity();
    auto& ground_transform = world.registry().emplace<TransformComponent>(state.ground);
    ground_transform.position = glm::vec3(0.0f, -5.0f, 0.0f);
    ground_transform.scale = glm::vec3(40.0f, 1.0f, 1.0f);
    ground_transform.dirty = true;

    auto& ground_sprite = world.registry().emplace<SpriteRendererComponent>(state.ground);
    ground_sprite.color = glm::vec4(0.3f, 0.8f, 0.3f, 1.0f);
    ground_sprite.texture_handle = state.texture_handle;
    ground_sprite.visible = true;

    auto& ground_rb = world.registry().emplace<RigidBody2DComponent>(state.ground);
    ground_rb.type = RigidBody2DType::Static;
    ground_rb.gravity_scale = 0.0f;
    ground_rb.fixed_rotation = true;

    auto& ground_collider = world.registry().emplace<BoxCollider2DComponent>(state.ground);
    ground_collider.size = glm::vec2(40.0f, 1.0f);
    ground_collider.density = 1.0f;
    ground_collider.friction = 0.4f;
    ground_collider.restitution = 0.0f;

    state.spawn_index = 0;
    state.frame_counter = 0;
    state.initialized = true;
    std::cout << "[Phase1-2D-Test][CPP] setup started: total_boxes=" << state.settings.total_boxes
              << " spawn_per_frame=" << state.settings.spawn_per_frame << std::endl;
}

void Tick(Phase1World& world, float delta_time) {
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
            std::cout << "[Phase1-2D-Test][CPP] setup finished: boxes=" << state.settings.total_boxes << std::endl;
        }
    }

    state.frame_counter += 1;
    if (state.frame_counter % 60 != 0) {
        return;
    }

    const int draw_calls = Phase1FramePipeline::Instance().LastDrawCalls();
    const int max_batch = Phase1FramePipeline::Instance().LastMaxBatchSprites();
    const int sprite_count = Phase1FramePipeline::Instance().LastSpriteCount();
    const char* status = draw_calls > 9 ? "FAIL" : "PASS";
    std::cout << "[Phase1-2D-Test][CPP] draw_calls=" << draw_calls
              << " max_batch=" << max_batch
              << " sprites=" << sprite_count
              << " spawned=" << state.spawn_index << "/" << state.settings.total_boxes
              << " status=" << status << std::endl;
}

void Shutdown() {
    State() = {};
}
}
