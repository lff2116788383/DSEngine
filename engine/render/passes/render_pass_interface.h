/**
 * @file render_pass_interface.h
 * @brief 渲染 Pass 基类接口，所有具体 Pass 继承此接口
 */

#ifndef DSE_RENDER_PASSES_RENDER_PASS_INTERFACE_H
#define DSE_RENDER_PASSES_RENDER_PASS_INTERFACE_H

class World;

namespace dse {
namespace render {

class RenderGraph;
class CommandBuffer;

/**
 * @class IRenderPass
 * @brief 渲染 Pass 抽象接口
 *
 * 每个实现类负责：
 * 1. Setup() — 在 RenderGraph 上声明资源依赖
 * 2. Execute() — 录制渲染命令到 CommandBuffer
 * 3. WarmUpECS() — (可选) 声明 Execute 中访问的 ECS 组件类型，
 *    由主线程在并行执行前调用，确保 EnTT 组件池已分配。
 */
class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    /// 在 RenderGraph 上声明读写资源及设置执行回调
    virtual void Setup(RenderGraph& graph) = 0;

    /// 录制渲染命令
    virtual void Execute(CommandBuffer& cmd_buffer) = 0;

    /// Pass 名称（用于调试和日志）
    virtual const char* GetName() const = 0;

    /// 预热 ECS 组件池（主线程调用，并行执行前）。
    /// 实现中应对所有 Execute() 内 registry.view<T>() 用到的 T 调用一次 view。
    /// 默认空实现 — 不访问 ECS 的 Pass 无需重写。
    virtual void WarmUpECS(World& /*world*/) {}
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PASSES_RENDER_PASS_INTERFACE_H
