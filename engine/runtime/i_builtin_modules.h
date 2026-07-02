/**
 * @file i_builtin_modules.h
 * @brief FramePipeline 内置模块抽象接口
 *
 * 将 engine/ 对 modules/ 具体类型的依赖隔离到纯虚接口之后。
 * 具体实现由 modules/runtime_bridge/builtin_modules_impl.cpp 提供。
 * 依赖方向: engine/ 定义接口，modules/ 提供实现 → 符合 AGENTS.md 规定。
 */

#ifndef DSE_I_BUILTIN_MODULES_H
#define DSE_I_BUILTIN_MODULES_H

#include <cstdint>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "engine/render/hiz_types.h"
#include "engine/base/frame_update_context.h"

class World;
class AssetManager;

namespace dse {
namespace gameplay2d { class AudioSystem; }
namespace core { class IModule; }
namespace render {
class RhiDevice;
class CommandBuffer;
class RenderGraph;
class IRenderPass;
class MeshRenderer;
struct RenderPassContext;
struct RenderScene;
struct FrameContext;
} // namespace render
} // namespace dse

using dse::render::RhiDevice;
using dse::render::CommandBuffer;
using dse::render::MeshRenderer;


/**
 * @class IBuiltinModules
 * @brief engine/ 层对 gameplay_2d / gameplay_3d / mesh_render 模块的抽象视图
 */
class IBuiltinModules {
public:
    virtual ~IBuiltinModules() = default;

    // ---- Gameplay2D ----
    virtual bool InitGameplay2D(World& world, RhiDevice* rhi, AssetManager* asset_mgr) = 0;
    virtual void UpdateGameplay2D(World& world, const dse::FrameUpdateContext& frame) = 0;
    virtual void FixedUpdateGameplay2D(World& world, float dt) = 0;
    virtual void ShutdownGameplay2D(World& world) = 0;
    virtual void RenderScene2D(World& world, CommandBuffer& cmd, const dse::render::FrameContext& frame) = 0;
    virtual void RenderUI2D(World& world, CommandBuffer& cmd, int w, int h, const glm::mat4& clip) = 0;
    virtual dse::gameplay2d::AudioSystem& GetAudioSystem() = 0;

    // ---- MeshRenderSystem ----
    virtual void InitMeshSystem(AssetManager* asset_mgr) = 0;
    virtual void ShutdownMeshSystem() = 0;
    virtual void RenderMeshes(World& world, CommandBuffer& cmd, RhiDevice& device, MeshRenderer& renderer, const dse::render::FrameContext& frame) = 0;
    virtual void BuildRenderQueues(World& world, dse::render::RenderScene& scene, bool gameplay3d_enabled) = 0;
    virtual int  PrepareGPUScene(World& world, dse::render::RenderPassContext& ctx) = 0;
    virtual void ResetGPUSceneState() = 0;
    virtual const std::vector<dse::gameplay3d::HiZAABB>& CachedAABBs() const = 0;
    virtual int  CachedAABBCount() const = 0;
    virtual void SetHiZVisibility(const std::vector<uint32_t>& visibility) = 0;
    virtual void CleanupGPUResources(RhiDevice* rhi) = 0;

    // ---- Gameplay3D ----
    // 2D/3D 双路径的选择完全封装在 modules 层实现内：
    // DSE_ENABLE_3D=ON 时转发 Gameplay3DModule，OFF 时转发最小 fallback
    // （particle/steering/animator）。engine 侧管线主流程零 #ifdef。
    virtual bool InitGameplay3D(World& world, RhiDevice* rhi, AssetManager* asset_mgr) = 0;
    virtual void UpdateGameplay3D(World& world, const dse::FrameUpdateContext& frame) = 0;
    virtual void FixedUpdateGameplay3D(World& world, float dt) = 0;
    virtual void ShutdownGameplay3D(World& world) = 0;
    virtual dse::core::IModule* GetGameplay3DModule() = 0;
    virtual void RegisterGameplay3DPasses(
        dse::render::RenderGraph& graph,
        dse::render::RenderPassContext& ctx,
        std::vector<std::unique_ptr<dse::render::IRenderPass>>& out_passes) = 0;

};

/// 工厂函数 — 声明在 engine/，实现在 modules/runtime_bridge/builtin_modules_impl.cpp
std::unique_ptr<IBuiltinModules> CreateBuiltinModules();

#endif // DSE_I_BUILTIN_MODULES_H
