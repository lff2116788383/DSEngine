#include "phase1/runtime/frame_pipeline.h"
#include "utils/debug.h"
#include "utils/time.h"
#include "utils/screen.h"
#include "phase1/asset/asset_manager.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "render_device/render_task_producer.h"

Phase1FramePipeline& Phase1FramePipeline::Instance() {
    static Phase1FramePipeline instance;
    return instance;
}

void Phase1FramePipeline::Init() {
    if (initialized_) {
        return;
    }
    world_ = &Phase1World::Instance();
    rhi_device_ = std::make_unique<OpenGLRhiDevice>();
    
    physics2d_system_.Init();
    
    // Initialize Lua Scripts
    lua_script_system_.Init(*world_);
    
    Entity camera_entity = world_->CreateEntity();
    world_->registry().emplace<TransformComponent>(camera_entity);
    world_->registry().emplace<CameraComponent>(camera_entity);
    
    // Create a static ground body
    Entity ground_entity = world_->CreateEntity();
    auto& ground_transform = world_->registry().emplace<TransformComponent>(ground_entity);
    ground_transform.position = glm::vec3(0.0f, -5.0f, 0.0f);
    ground_transform.scale = glm::vec3(40.0f, 1.0f, 1.0f);
    ground_transform.dirty = true;
    
    auto& ground_sprite = world_->registry().emplace<SpriteRendererComponent>(ground_entity);
    ground_sprite.color = glm::vec4(0.3f, 0.8f, 0.3f, 1.0f);

    auto& ground_rb = world_->registry().emplace<RigidBody2DComponent>(ground_entity);
    ground_rb.type = RigidBody2DType::Static;
    
    auto& ground_collider = world_->registry().emplace<BoxCollider2DComponent>(ground_entity);
    ground_collider.size = glm::vec2(40.0f, 1.0f);
    
    // Load a texture for the boxes
    auto box_texture = Phase1AssetManager::Instance().LoadTexture("bin/data/mirror_assets/Resources/item/1.png");
    unsigned int box_tex_handle = box_texture ? box_texture->GetHandle() : 0;
    
    int total_boxes = 512;
    int columns = 32;
    float spacing = 0.55f;
    float start_x = -0.5f * (columns - 1) * spacing;
    float start_y = 2.0f;
    for (int i = 0; i < total_boxes; ++i) {
        Entity entity = world_->CreateEntity();
        
        auto& transform = world_->registry().emplace<TransformComponent>(entity);
        transform.position.x = start_x + (i % columns) * spacing;
        transform.position.y = start_y + (i / columns) * spacing;
        transform.scale = glm::vec3(0.45f, 0.45f, 1.0f);
        transform.dirty = true;
        
        auto& sprite = world_->registry().emplace<SpriteRendererComponent>(entity);
        sprite.sorting_layer = 0;
        sprite.order_in_layer = i;
        sprite.color = glm::vec4(0.9f, 0.95f, 1.0f, 1.0f);
        sprite.texture = box_texture;
        sprite.texture_handle = box_tex_handle;
        
        auto& rigid_body = world_->registry().emplace<RigidBody2DComponent>(entity);
        rigid_body.type = RigidBody2DType::Dynamic;
        rigid_body.gravity_scale = 1.0f;
        
        auto& collider = world_->registry().emplace<BoxCollider2DComponent>(entity);
        collider.size = glm::vec2(0.45f, 0.45f);
        collider.density = 1.0f;
        collider.friction = 0.3f;
        collider.restitution = 0.5f;
    }
    
    // Create a simple UI element
    Entity ui_entity = world_->CreateEntity();
    auto& ui_comp = world_->registry().emplace<UIRendererComponent>(ui_entity);
    ui_comp.position = glm::vec2(100.0f, 100.0f);
    ui_comp.size = glm::vec2(200.0f, 50.0f);
    ui_comp.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.8f); // Red UI box
    
    initialized_ = true;
}

void Phase1FramePipeline::Update(float delta_time) {
    if (!initialized_) {
        return;
    }
    
    // ImGui new frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Phase 1 Simple UI
    ImGui::Begin("Phase 1 Debug UI");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Entity Count: %zu", world_->registry().storage<entt::entity>().size());
    ImGui::End();

    // Lua Scripts logic update
    lua_script_system_.Update(*world_, delta_time);

    transform_system_.Update(*world_);
    camera_system_.Update(*world_, Screen::aspect_ratio());
}

void Phase1FramePipeline::FixedUpdate(float fixed_delta_time) {
    if (!initialized_) {
        return;
    }
    physics2d_system_.FixedUpdate(*world_, fixed_delta_time);
}

void Phase1FramePipeline::Render() {
    if (!initialized_) {
        return;
    }
    rhi_device_->BeginFrame();
    
    auto cmd_buffer = rhi_device_->CreateCommandBuffer();
    cmd_buffer->ClearColor(glm::vec4(0.02f, 0.02f, 0.02f, 1.0f));
    
    auto camera_view = world_->registry().view<CameraComponent>();
    if (!camera_view.empty()) {
        auto& camera = camera_view.get<CameraComponent>(camera_view.front());
        cmd_buffer->SetCamera(camera.view, camera.projection);
    }
    
    // Instead of passing RhiDevice directly, SpriteRenderSystem should take a CommandBuffer
    sprite_render_system_.Render(*world_, *cmd_buffer);
    
    // UI Render Pass
    ui_render_system_.Render(*world_, *cmd_buffer, Screen::width(), Screen::height());
    
    rhi_device_->Submit(cmd_buffer);
    
    // Render ImGui
    ImGui::Render();
    
    // Instead of rendering immediately (which is in the wrong thread),
    // we should create a RenderTask for ImGui, or since this is Phase 1 standalone,
    // we can temporarily just let it run if it's single threaded. But DSEngine is multithreaded.
    // For now, let's omit ImGui_ImplOpenGL3_RenderDrawData here if it crashes,
    // but let's try to just push a custom task or use a simple hack.
    // DSEngine's old architecture used RenderTaskProducer::ProduceRenderTaskXXX.
    RenderTaskProducer::ProduceRenderTaskRenderImGui();
    
    rhi_device_->EndFrame();

    stats_accumulator_ += Time::delta_time();
    if (stats_accumulator_ >= 1.0f) {
        stats_accumulator_ = 0.0f;
        const auto& stats = rhi_device_->LastFrameStats();
        size_t entity_count = world_->EntityCount();
        size_t physics_bodies = 0;
        auto physics_view = world_->registry().view<RigidBody2DComponent>();
        for (auto entity : physics_view) {
            (void)entity;
            ++physics_bodies;
        }
        DEBUG_LOG_INFO("Phase1 stats: entities={}, sprites={}, draw_calls={}, max_batch_sprites={}, physics_bodies={}",
                       entity_count,
                       stats.sprite_count,
                       stats.draw_calls,
                       stats.max_batch_sprites,
                       physics_bodies);
    }
}

Phase1World& Phase1FramePipeline::world() {
    return *world_;
}
