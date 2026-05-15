/**
 * @file module.h
 * @brief 引擎动态模块化接口定义，允许功能模块（如 3D/物理/网络）解耦并按需加载
 */

#pragma once

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"

#include <memory>
#include <vector>

class AssetManager;

namespace dse {
namespace render {
class RenderGraph;
class IRenderPass;
struct RenderPassContext;
} // namespace render
} // namespace dse

namespace dse {
namespace core {

/**
 * @class IModule
 * @brief 所有动态加载模块必须实现的基类接口
 */
class IModule {
public:
    virtual ~IModule() = default;

    /**
     * @brief 获取模块的唯一名称
     */
    virtual const char* GetName() const = 0;

    /**
     * @brief 模块初始化，引擎启动或模块被加载时调用
     */
    virtual bool OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) = 0;

    /**
     * @brief 模块帧更新 (逻辑阶段)
     */
    virtual void OnUpdate(World& world, float delta_time) = 0;

    /**
     * @brief 模块固定帧更新 (物理/固定逻辑阶段)
     */
    virtual void OnFixedUpdate(World& world, float fixed_delta_time) = 0;

    /**
     * @brief 模块渲染：PreZ (深度预渲染阶段)
     */
    virtual void OnRenderPreZ(World& world, CommandBuffer& cmd_buffer) {}

    /**
     * @brief 模块渲染：Shadow (阴影贴图渲染阶段)
     */
    virtual void OnRenderShadow(World& world, CommandBuffer& cmd_buffer, int cascade_index, const glm::mat4& light_view, const glm::mat4& light_proj) {}

    /**
     * @brief 模块渲染：Scene (主场景渲染阶段，仅不透明物体)
     */
    virtual void OnRenderScene(World& world, CommandBuffer& cmd_buffer, const glm::mat4& clip_correction = glm::mat4(1.0f)) {}

    /**
     * @brief 模块渲染：Transparent (WBOIT 透明物体渲染阶段)
     *
     * ForwardScenePass 结束后由 WBOITPass 调用，仅渲染 blend_mode != Opaque 的半透明物体。
     * @param wboit_mode 1=accumulation, 2=revealage
     */
    virtual void OnRenderTransparent(World& world, CommandBuffer& cmd_buffer, const glm::mat4& clip_correction, int wboit_mode) {}

    /**
     * @brief 模块渲染：UI (独立于场景的 UI 渲染阶段)
     *
     * UI 渲染通常使用独立的正交投影和 RenderTarget，
     * 与 Scene Pass 分离以保证 UI 不被深度测试影响
     */
    virtual void OnRenderUI(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height, const glm::mat4& clip_correction = glm::mat4(1.0f)) {}

    /**
     * @brief 模块向 RenderGraph 注册自定义渲染 Pass
     *
     * 引擎在 BuildRenderGraph 时调用此方法，模块可创建自己的 IRenderPass 实例
     * 并添加到 out_passes。引擎将自动调用 Setup() 和管理生命周期。
     * @param graph 当前帧的渲染图
     * @param ctx 共享渲染上下文
     * @param out_passes 模块创建的 Pass 实例输出容器
     */
    virtual void RegisterRenderPasses(
        dse::render::RenderGraph& graph,
        dse::render::RenderPassContext& ctx,
        std::vector<std::unique_ptr<dse::render::IRenderPass>>& out_passes) {}

    /**
     * @brief 模块关闭，释放资源
     */
    virtual void OnShutdown(World& world) = 0;
};

} // namespace core
} // namespace dse
