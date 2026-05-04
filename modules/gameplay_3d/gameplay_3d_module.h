#pragma once

#include "engine/core/module.h"

// 3D Systems
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "modules/gameplay_3d/rendering/terrain_system.h"
#include "modules/gameplay_3d/rendering/frustum_culling_system.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "modules/gameplay_3d/camera/free_camera_controller_system.h"
#include "modules/gameplay_3d/ai/steering_system.h"

namespace dse {
namespace gameplay3d {

/**
 * @class Gameplay3DModule
 * @brief 将所有的 3D 功能系统打包为一个独立模块，实现与引擎核心的解耦
 */
class Gameplay3DModule : public core::IModule {
public:
    const char* GetName() const override { return "Gameplay3D"; }

    bool OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) override;
    void OnUpdate(World& world, float delta_time) override;
    void OnFixedUpdate(World& world, float fixed_delta_time) override;
    void OnRenderPreZ(World& world, CommandBuffer& cmd_buffer) override;
    void OnRenderShadow(World& world, CommandBuffer& cmd_buffer, int cascade_index, const glm::mat4& light_view, const glm::mat4& light_proj) override;
    void OnRenderScene(World& world, CommandBuffer& cmd_buffer) override;
    void OnShutdown(World& world) override;

private:
    MeshRenderSystem mesh_render_system_;
    TerrainSystem terrain_system_;
    FrustumCullingSystem frustum_culling_system_;
    AnimatorSystem animator_system_;
    Particle3DSystem particle3d_system_;
    FreeCameraControllerSystem free_camera_controller_system_;
    SteeringSystem steering_system_;
};

} // namespace gameplay3d
} // namespace dse
