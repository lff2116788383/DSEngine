#pragma once

#include "engine/core/module.h"
#include "engine/base/frame_update_context.h"
#include "engine/scene/transform_system.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "modules/gameplay_2d/camera/camera_system.h"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "engine/audio/audio_system.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "modules/gameplay_2d/animation/animation_system.h"
#include "modules/gameplay_2d/particle/particle_system.h"
#ifdef DSE_ENABLE_SPINE
#include "modules/gameplay_2d/spine/spine_system.h"
#endif

class AssetManager;

namespace dse::gameplay2d {

class Gameplay2DModule : public dse::core::IModule {
public:
    const char* GetName() const override { return "Gameplay2D"; }

    bool OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) override;
    void OnUpdate(World& world, float delta_time) override;
    // 帧上下文感知重载：gameplay 子系统用 scaled_dt，UI 子系统用 unscaled_dt。
    void OnUpdate(World& world, const dse::FrameUpdateContext& frame);
    void OnFixedUpdate(World& world, float fixed_delta_time) override;
    void OnShutdown(World& world) override;

    // 2D 场景/UI 渲染贡献。非 IModule 虚函数 —— 由 FramePipeline 经
    // RenderPassContext 钩子调用，待 Phase 2 迁为独立的 IRenderPass。
    void RenderScene2D(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame, const glm::mat4& clip_correction = glm::mat4(1.0f));
    void RenderUI2D(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height, const glm::mat4& clip_correction = glm::mat4(1.0f));

    TransformSystem& transform_system() { return transform_system_; }
    CameraSystem& camera_system() { return camera_system_; }
    SpriteRenderSystem& sprite_render_system() { return sprite_render_system_; }
    UIRenderSystem& ui_render_system() { return ui_render_system_; }
    Physics2DSystem& physics2d_system() { return physics2d_system_; }
    AnimationSystem& animation_system() { return animation_system_; }
    ParticleSystem& particle_system() { return particle_system_; }
#ifdef DSE_ENABLE_SPINE
    SpineSystem& spine_system() { return spine_system_; }
#endif
    UISystem& ui_logic_system() { return ui_logic_system_; }
    AudioSystem& audio_system() { return audio_system_; }
    TilemapSystem& tilemap_system() { return tilemap_system_; }

private:
    AssetManager* asset_manager_ = nullptr;
    RhiDevice* rhi_device_ = nullptr;

    TransformSystem transform_system_;
    CameraSystem camera_system_;
    SpriteRenderSystem sprite_render_system_;
    UIRenderSystem ui_render_system_;
    Physics2DSystem physics2d_system_;
    AnimationSystem animation_system_;
    ParticleSystem particle_system_;
#ifdef DSE_ENABLE_SPINE
    SpineSystem spine_system_;
#endif
    UISystem ui_logic_system_;
    AudioSystem audio_system_;
    TilemapSystem tilemap_system_;
};

} // namespace dse::gameplay2d
