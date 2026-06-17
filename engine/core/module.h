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
     * @brief 模块向 RenderGraph 注册自定义渲染 Pass
     *
     * 这是模块参与渲染的唯一入口。模块不再实现固定阶段渲染回调
     * （历史上的 OnRenderPreZ/Shadow/Scene/Transparent/UI 已移除）；
     * 渲染贡献应封装为 IRenderPass，由 RenderGraph 统一编排排序，
     * 管线实现细节（如 WBOIT 的 accumulation/revealage）留在 Pass 内部。
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
