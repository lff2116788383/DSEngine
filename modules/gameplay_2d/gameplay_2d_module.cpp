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
#ifdef DSE_ENABLE_SPINE
    spine_system_.SetAssetManager(asset_manager_);
#endif
    audio_system_.Initialize(asset_manager_);
    return true;
}

void Gameplay2DModule::OnUpdate(World& world, float delta_time) {
    tilemap_system_.Update(world.registry());
    animation_system_.Update(world, delta_time);
    particle_system_.Update(world, delta_time, &physics2d_system_);
#ifdef DSE_ENABLE_SPINE
    spine_system_.Update(world.registry(), delta_time);
#endif
    transform_system_.Update(world);
    ui_logic_system_.Update(world.registry(),
                            delta_time,
                            glm::vec2(Screen::width(), Screen::height()),
                            Input::mousePosition(),
                            Input::GetMouseButton(0));
    camera_system_.Update(world, Screen::aspect_ratio());
    audio_system_.Update(world.registry(), delta_time);
}

void Gameplay2DModule::OnFixedUpdate(World& world, float fixed_delta_time) {
    physics2d_system_.FixedUpdate(world, fixed_delta_time);
}

void Gameplay2DModule::OnRenderPreZ(World& world, CommandBuffer& cmd_buffer) {
    (void)world;
    (void)cmd_buffer;
}

void Gameplay2DModule::OnRenderShadow(World& world, CommandBuffer& cmd_buffer, int cascade_index, const glm::mat4& light_view, const glm::mat4& light_proj) {
    (void)world;
    (void)cmd_buffer;
    (void)cascade_index;
    (void)light_view;
    (void)light_proj;
}

void Gameplay2DModule::OnRenderScene(World& world, CommandBuffer& cmd_buffer, const glm::mat4& clip_correction) {
    (void)clip_correction;
    sprite_render_system_.Render(world, cmd_buffer);
#ifdef DSE_ENABLE_SPINE
    spine_system_.Render(world, cmd_buffer);
#endif
    particle_system_.Render(world, cmd_buffer);
}

void Gameplay2DModule::OnRenderUI(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height, const glm::mat4& clip_correction) {
    ui_render_system_.Render(world, cmd_buffer, screen_width, screen_height, clip_correction);
}

void Gameplay2DModule::OnShutdown(World& world) {
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
