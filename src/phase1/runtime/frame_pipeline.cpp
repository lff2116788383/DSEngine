#include "phase1/runtime/frame_pipeline.h"

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
    
    Entity camera_entity = world_->CreateEntity();
    world_->registry().emplace<TransformComponent>(camera_entity);
    world_->registry().emplace<CameraComponent>(camera_entity);
    
    // Create a static ground body
    Entity ground_entity = world_->CreateEntity();
    auto& ground_transform = world_->registry().emplace<TransformComponent>(ground_entity);
    ground_transform.position = glm::vec3(0.0f, -5.0f, 0.0f);
    ground_transform.scale = glm::vec3(20.0f, 1.0f, 1.0f);
    ground_transform.dirty = true;
    
    auto& ground_rb = world_->registry().emplace<RigidBody2DComponent>(ground_entity);
    ground_rb.type = RigidBody2DType::Static;
    
    auto& ground_collider = world_->registry().emplace<BoxCollider2DComponent>(ground_entity);
    ground_collider.size = glm::vec2(20.0f, 1.0f);
    
    // Create dynamic falling boxes
    for (int i = 0; i < 64; ++i) {
        Entity entity = world_->CreateEntity();
        
        auto& transform = world_->registry().emplace<TransformComponent>(entity);
        transform.position.x = -4.0f + (i % 16) * 0.55f;
        transform.position.y = 2.0f + (i / 16) * 0.55f;
        transform.scale = glm::vec3(0.45f, 0.45f, 1.0f);
        transform.dirty = true;
        
        auto& sprite = world_->registry().emplace<SpriteRendererComponent>(entity);
        sprite.sorting_layer = 0;
        sprite.order_in_layer = i;
        sprite.color = glm::vec4(0.9f, 0.95f, 1.0f, 1.0f);
        
        auto& rigid_body = world_->registry().emplace<RigidBody2DComponent>(entity);
        rigid_body.type = RigidBody2DType::Dynamic;
        rigid_body.gravity_scale = 1.0f;
        
        auto& collider = world_->registry().emplace<BoxCollider2DComponent>(entity);
        collider.size = glm::vec2(0.45f, 0.45f);
        collider.density = 1.0f;
        collider.friction = 0.3f;
        collider.restitution = 0.5f;
    }
    initialized_ = true;
}

void Phase1FramePipeline::Update(float delta_time) {
    if (!initialized_) {
        return;
    }
    (void)delta_time;
    transform_system_.Update(*world_);
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
    
    // Instead of passing RhiDevice directly, SpriteRenderSystem should take a CommandBuffer
    sprite_render_system_.Render(*world_, *cmd_buffer);
    
    rhi_device_->Submit(cmd_buffer);
    
    rhi_device_->EndFrame();
}

Phase1World& Phase1FramePipeline::world() {
    return *world_;
}
