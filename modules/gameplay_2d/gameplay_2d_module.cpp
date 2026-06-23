#include "modules/gameplay_2d/gameplay_2d_module.h"

#include "engine/assets/asset_manager.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"
#include <glm/vec2.hpp>

namespace dse::gameplay2d {

bool Gameplay2DModule::OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) {
    (void)world;
    asset_manager_ = asset_manager;
    rhi_device_ = rhi_device;
    if (asset_manager_ == nullptr) {
        return false;
    }

    physics2d_system_.Init(world);
    // 注入 RhiDevice：2D sprite/UI/particle 渲染均经 SpriteBatchRenderer 通用原语路径。
    sprite_render_system_.SetRhiDevice(rhi_device_);
    ui_render_system_.SetRhiDevice(rhi_device_);
    particle_system_.SetRhiDevice(rhi_device_);
#ifdef DSE_ENABLE_SPINE
    spine_system_.SetAssetManager(asset_manager_);
    // 注入 RhiDevice：spine 2D 渲染经 MeshRenderer::DrawUnlit2D 通用原语路径。
    spine_system_.SetRhiDevice(rhi_device_);
#endif
    audio_system_.Initialize(asset_manager_);
    return true;
}

void Gameplay2DModule::OnUpdate(World& world, float delta_time) {
    OnUpdate(world, dse::TimeContext{delta_time, delta_time, 1.0f});
}

void Gameplay2DModule::OnUpdate(World& world, const dse::TimeContext& time) {
    const float scaled = time.scaled_dt;      // gameplay/动画/粒子（逐实体缩放在各系统内部应用）
    const float unscaled = time.unscaled_dt;  // UI / 音频走真实时间（scale=0 时仍响应）
    tilemap_system_.Update(world.registry());
    animation_system_.Update(world, scaled);
    particle_system_.Update(world, scaled, &physics2d_system_);
#ifdef DSE_ENABLE_SPINE
    spine_system_.Update(world.registry(), scaled);
#endif
    transform_system_.Update(world);
    ui_logic_system_.Update(world.registry(),
                            unscaled,
                            glm::vec2(Screen::width(), Screen::height()),
                            Input::mousePosition(),
                            Input::GetMouseButton(0));
    camera_system_.Update(world, Screen::aspect_ratio());
    audio_system_.Update(world.registry(), unscaled);
}

void Gameplay2DModule::OnFixedUpdate(World& world, float fixed_delta_time) {
    physics2d_system_.FixedUpdate(world, fixed_delta_time);
}

void Gameplay2DModule::RenderScene2D(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame, const glm::mat4& clip_correction) {
    (void)clip_correction;
    sprite_render_system_.Render(world, cmd_buffer, frame);
#ifdef DSE_ENABLE_SPINE
    spine_system_.Render(world, cmd_buffer, frame);
#endif
    particle_system_.Render(world, cmd_buffer, frame);
}

void Gameplay2DModule::RenderUI2D(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height, const glm::mat4& clip_correction) {
    ui_render_system_.Render(world, cmd_buffer, screen_width, screen_height, clip_correction);
}

void Gameplay2DModule::OnShutdown(World& world) {
    // 释放 SpriteBatchRenderer GPU 资源（须在 rhi_device_ 置空前）。
    sprite_render_system_.Shutdown();
    ui_render_system_.Shutdown();
    particle_system_.Shutdown();
    audio_system_.Shutdown();
    physics2d_system_.Shutdown();
#ifdef DSE_ENABLE_SPINE
    spine_system_.Shutdown(world.registry());
    spine_system_.SetAssetManager(nullptr);
#endif
    asset_manager_ = nullptr;
    rhi_device_ = nullptr;
}

} // namespace dse::gameplay2d
