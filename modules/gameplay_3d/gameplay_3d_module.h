#pragma once

#include "engine/core/module.h"
#include "engine/base/frame_update_context.h"
#include "engine/core/event_bus.h"
#include "engine/render/scene_renderer.h"
#include "engine/physics/physics3d/i_physics3d_system.h"

// 3D Systems
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "modules/gameplay_3d/rendering/terrain_system.h"
#include "modules/gameplay_3d/rendering/grass_system.h"
#include "modules/gameplay_3d/rendering/tree_system.h"
#include "modules/gameplay_3d/rendering/hair_system.h"
#include "engine/render/particle_renderer.h"
#include "engine/render/impostor/impostor_system.h"
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

// AI / Cutscene systems
#include "engine/ai/behavior_tree.h"
#include "engine/cutscene/cutscene_player.h"
#include "engine/ecs/components_3d_ai.h"

// Lightmap runtime
#include "engine/render/gi/lightmap_system.h"

// Open-world systems
#include "engine/scene/world_partition.h"
#include "engine/render/hlod/hlod_system.h"
#include "engine/render/virtual_texture/virtual_texture.h"
#include "engine/terrain/geometry_clipmap.h"
#include "engine/render/sdf/global_sdf.h"
#include "engine/ai/ai_lod_scheduler.h"
#include "engine/render/particles/gpu_particle_system.h"
#include "engine/scene/world_state_persistence.h"

namespace dse {
namespace render {
struct RenderScene;
} // namespace render
namespace gameplay3d {

/**
 * @class Gameplay3DModule
 * @brief 将所有的 3D 功能系统打包为一个独立模块，实现与引擎核心的解耦
 */
class Gameplay3DModule : public core::IModule, public dse::render::ISceneRenderer {
public:
    const char* GetName() const override { return "Gameplay3D"; }

    bool OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) override;
    void OnUpdate(World& world, float delta_time) override;
    // 帧上下文感知重载：gameplay 用 scaled_dt，自由相机控制用 unscaled_dt。
    void OnUpdate(World& world, const dse::FrameUpdateContext& frame);
    void OnFixedUpdate(World& world, float fixed_delta_time) override;
    void OnShutdown(World& world) override;
    void BuildRenderQueues(World& world, dse::render::RenderScene& scene);

    // ISceneRenderer：3D 几何（terrain/grass/tree/particle/hair）的渲染贡献，
    // 由内建 PreZ / Shadow / Forward / RSM Pass 在各自渲染作用域内按阶段调用。
    void RenderPreZ(dse::render::CommandBuffer& cmd,
                    const dse::render::RenderScenePassContext& ctx) override;
    void RenderShadow(dse::render::CommandBuffer& cmd,
                      const dse::render::RenderScenePassContext& ctx) override;
    void RenderOpaque(dse::render::CommandBuffer& cmd,
                      const dse::render::RenderScenePassContext& ctx) override;

    MeshRenderSystem& mesh_render_system() { return mesh_render_system_; }
    const MeshRenderSystem& mesh_render_system() const { return mesh_render_system_; }

private:
    RhiDevice* rhi_device_ = nullptr;
    dse::render::ParticleRenderer particle_renderer_;
    MeshRenderSystem mesh_render_system_;
    TerrainSystem terrain_system_;
    GrassSystem grass_system_;
    TreeSystem tree_system_;
    HairSystem hair_system_;
    dse::render::ImpostorSystem impostor_system_;
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

    // Open-world systems
    dse::WorldPartitionSystem world_partition_system_;
    dse::render::HLODSystem hlod_system_;
    dse::vt::VirtualTextureSystem virtual_texture_system_;
    dse::terrain::GeometryClipmapSystem geometry_clipmap_system_;
    dse::render::GlobalSDFSystem global_sdf_system_;
    dse::ai::AILodScheduler ai_lod_scheduler_;
    dse::render::GpuParticleManager gpu_particle_manager_;
    dse::WorldStatePersistence world_state_persistence_;
    uint64_t frame_number_ = 0;

    // AI / Cutscene runtime
    dse::cutscene::CutscenePlayer cutscene_player_;

    // Lightmap runtime
    dse::render::LightmapSystem lightmap_system_;

    World* world_cache_ = nullptr;
    dse::core::SubscriptionHandle origin_rebase_handle_;
};

} // namespace gameplay3d
} // namespace dse
