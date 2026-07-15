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
#include "modules/gameplay_2d/parallax/parallax_system.h"
#include "modules/gameplay_2d/lighting/light_2d_system.h"
#include "modules/gameplay_2d/trail/trail_system.h"
#include "modules/gameplay_2d/line_renderer/line_renderer_system.h"
#include "modules/gameplay_2d/camera/camera_controller_system.h"
#include "modules/gameplay_2d/audio/audio_spatial_2d_system.h"
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

    // Phase 1：主线程（Prepare）从 ECS 提取全部 2D 场景/UI 绘制数据，
    // 渲染线程 RenderScene2D/RenderUI2D 仅消费快照、不访问 ECS。
    // 屏幕尺寸/裁剪矩阵在提取时从 Screen + RhiDevice 采样，供渲染线程使用。
    void ExtractSceneRenderData2D(World& world);
    void ExtractUIRenderData2D(World& world);

    // 2D 场景/UI 渲染贡献。非 IModule 虚函数 —— 由 FramePipeline 经
    // RenderPassContext 钩子调用，待 Phase 2 迁为独立的 IRenderPass。
    void RenderScene2D(CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame, const glm::mat4& clip_correction = glm::mat4(1.0f));
    void RenderUI2D(CommandBuffer& cmd_buffer, int screen_width, int screen_height, const glm::mat4& clip_correction = glm::mat4(1.0f));

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
    ParallaxSystem& parallax_system() { return parallax_system_; }
    Light2DSystem& light_2d_system() { return light_2d_system_; }
    TrailSystem& trail_system() { return trail_system_; }
    LineRendererSystem& line_renderer_system() { return line_renderer_system_; }
    CameraControllerSystem& camera_controller_system() { return camera_controller_system_; }
    AudioSpatial2DSystem& audio_spatial_2d_system() { return audio_spatial_2d_system_; }

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
    ParallaxSystem parallax_system_;
    Light2DSystem light_2d_system_;
    TrailSystem trail_system_;
    LineRendererSystem line_renderer_system_;
    CameraControllerSystem camera_controller_system_;
    AudioSpatial2DSystem audio_spatial_2d_system_;
};

} // namespace dse::gameplay2d
