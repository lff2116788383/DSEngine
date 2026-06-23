/**
 * @file builtin_modules_impl.h
 * @brief IBuiltinModules 具体实现 — 持有 Gameplay2D/3D/MeshRender 具体类型
 *
 * 此文件位于 modules/ 层，仅被 modules/ .cpp 包含，engine/ 不可引用。
 */

#ifndef DSE_BUILTIN_MODULES_IMPL_H
#define DSE_BUILTIN_MODULES_IMPL_H

#include "engine/runtime/i_builtin_modules.h"

#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "modules/gameplay_2d/camera/camera_system.h"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "modules/gameplay_2d/animation/animation_system.h"
#include "modules/gameplay_2d/particle/particle_system.h"
#ifdef DSE_ENABLE_SPINE
#include "modules/gameplay_2d/spine/spine_system.h"
#endif
#include "modules/gameplay_2d/gameplay_2d_module.h"
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#ifdef DSE_ENABLE_3D
#include "modules/gameplay_3d/gameplay_3d_module.h"
#else
#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "modules/gameplay_3d/ai/steering_system.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#endif

class BuiltinModulesImpl final : public IBuiltinModules {
public:
    // ---- Gameplay2D ----
    bool InitGameplay2D(World& world, RhiDevice* rhi, AssetManager* asset_mgr) override;
    void UpdateGameplay2D(World& world, const dse::TimeContext& time) override;
    void FixedUpdateGameplay2D(World& world, float dt) override;
    void ShutdownGameplay2D(World& world) override;
    void RenderScene2D(World& world, CommandBuffer& cmd, const dse::render::FrameContext& frame) override;
    void RenderUI2D(World& world, CommandBuffer& cmd, int w, int h, const glm::mat4& clip) override;
    dse::gameplay2d::AudioSystem& GetAudioSystem() override;

    // ---- MeshRenderSystem ----
    void InitMeshSystem(AssetManager* asset_mgr) override;
    void ShutdownMeshSystem() override;
    void RenderMeshes(World& world, CommandBuffer& cmd, RhiDevice& device, MeshRenderer& renderer, const dse::render::FrameContext& frame) override;
    void BuildRenderQueues(World& world, dse::render::RenderScene& scene) override;
    int  PrepareGPUScene(World& world, dse::render::RenderPassContext& ctx) override;
    void ResetGPUSceneState() override;
    const std::vector<dse::gameplay3d::HiZAABB>& CachedAABBs() const override;
    int  CachedAABBCount() const override;
    void SetHiZVisibility(const std::vector<uint32_t>& visibility) override;
    void CleanupGPUResources(RhiDevice* rhi) override;

    // ---- Gameplay3D ----
    bool InitGameplay3D(World& world, RhiDevice* rhi, AssetManager* asset_mgr) override;
    void UpdateGameplay3D(World& world, const dse::TimeContext& time) override;
    void FixedUpdateGameplay3D(World& world, float dt) override;
    void ShutdownGameplay3D(World& world) override;
    dse::core::IModule* GetGameplay3DModule() override;
    void RegisterGameplay3DPasses(
        dse::render::RenderGraph& graph,
        dse::render::RenderPassContext& ctx,
        std::vector<std::unique_ptr<dse::render::IRenderPass>>& out_passes) override;

    // ---- Fallback 3D ----
    void InitFallback3D(World& world, RhiDevice* rhi, AssetManager* asset_mgr) override;
    void UpdateFallback3D(World& world, const dse::TimeContext& time) override;
    void ShutdownFallback3D(World& world) override;

private:
    dse::gameplay2d::Gameplay2DModule gameplay2d_module_;
    dse::gameplay3d::MeshRenderSystem mesh_render_system_;
#ifdef DSE_ENABLE_3D
    dse::gameplay3d::Gameplay3DModule gameplay3d_module_;
#else
    dse::gameplay3d::Particle3DSystem particle3d_system_;
    dse::gameplay3d::SteeringSystem steering_system_;
    dse::gameplay3d::AnimatorSystem animator3d_system_;
#endif
};

#endif // DSE_BUILTIN_MODULES_IMPL_H
