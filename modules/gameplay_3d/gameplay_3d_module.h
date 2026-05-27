#pragma once

#include "engine/core/module.h"
#include "engine/core/event_bus.h"
#include "engine/physics/physics3d/i_physics3d_system.h"

// 3D Systems
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "modules/gameplay_3d/rendering/terrain_system.h"
#include "modules/gameplay_3d/rendering/grass_system.h"
#include "modules/gameplay_3d/rendering/hair_system.h"
#include "modules/gameplay_3d/rendering/frustum_culling_system.h"
#include "modules/gameplay_3d/rendering/lod_system.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#include "modules/gameplay_3d/animation/anim_layer_blend_system.h"
#include "modules/gameplay_3d/animation/ik_solver_system.h"
#include "modules/gameplay_3d/animation/foot_ik_system.h"
#include "modules/gameplay_3d/bone_attachment_system.h"
#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "modules/gameplay_3d/camera/free_camera_controller_system.h"
#include "modules/gameplay_3d/ai/steering_system.h"
#ifdef DSE_ENABLE_NAVMESH
#include "modules/gameplay_3d/ai/nav_agent_system.h"
#endif
#ifdef DSE_HAS_PHYSICS3D
#include "modules/gameplay_3d/destruction/fracture_system.h"
#include "modules/gameplay_3d/ragdoll/ragdoll_system.h"
#include "modules/gameplay_3d/vehicle/vehicle_system.h"
#include "modules/gameplay_3d/buoyancy/buoyancy_system.h"
#endif
#include "modules/gameplay_3d/cloth/cloth_system.h"
#include "modules/gameplay_3d/fluid/fluid_system.h"
#include "modules/gameplay_3d/softbody/softbody_system.h"
#include "modules/gameplay_3d/rope/rope_system.h"
#include "modules/gameplay_3d/sky/day_night_cycle_system.h"
#include "modules/gameplay_3d/weather/weather_system.h"
#include "modules/gameplay_3d/snow/snow_cover_system.h"

namespace dse {
namespace render {
struct RenderScene;
} // namespace render
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
    void OnRenderScene(World& world, CommandBuffer& cmd_buffer, const glm::mat4& clip_correction = glm::mat4(1.0f)) override;
    void OnRenderTransparent(World& world, CommandBuffer& cmd_buffer, const glm::mat4& clip_correction, int wboit_mode) override;
    void OnShutdown(World& world) override;
    void BuildRenderQueues(World& world, dse::render::RenderScene& scene);

    MeshRenderSystem& mesh_render_system() { return mesh_render_system_; }
    const MeshRenderSystem& mesh_render_system() const { return mesh_render_system_; }

private:
    MeshRenderSystem mesh_render_system_;
    TerrainSystem terrain_system_;
    GrassSystem grass_system_;
    HairSystem hair_system_;
    FrustumCullingSystem frustum_culling_system_;
    LODSystem lod_system_;
    AnimatorSystem animator_system_;
    AnimLayerBlendSystem anim_layer_blend_system_;
    IKSolverSystem ik_solver_system_;
    FootIKSystem foot_ik_system_;
    Particle3DSystem particle3d_system_;
    FreeCameraControllerSystem free_camera_controller_system_;
    SteeringSystem steering_system_;
#ifdef DSE_ENABLE_NAVMESH
    NavAgentSystem nav_agent_system_;
#endif
#ifdef DSE_HAS_PHYSICS3D
    FractureSystem fracture_system_;
    RagdollSystem ragdoll_system_;
    VehicleSystem vehicle_system_;
    BuoyancySystem buoyancy_system_;
#endif
    ClothSystem cloth_system_;
    FluidSystem fluid_system_;
    SoftBodySystem softbody_system_;
    RopeSystem rope_system_;
    DayNightCycleSystem day_night_cycle_system_;
    WeatherSystem weather_system_;
    SnowCoverSystem snow_cover_system_;

    World* world_cache_ = nullptr;
    dse::core::SubscriptionHandle origin_rebase_handle_;
};

} // namespace gameplay3d
} // namespace dse
