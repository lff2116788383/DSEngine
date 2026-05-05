/**
 * @file render_pass_interface.h
 * @brief 渲染 Pass 基类接口，所有具体 Pass 继承此接口
 */

#ifndef DSE_RENDER_PASSES_RENDER_PASS_INTERFACE_H
#define DSE_RENDER_PASSES_RENDER_PASS_INTERFACE_H

namespace dse {
namespace render {
class RenderGraph;
} // namespace render
} // namespace dse

class CommandBuffer;

namespace dse {
namespace render {

/**
 * @class IRenderPass
 * @brief 渲染 Pass 抽象接口
 *
 * 每个实现类负责：
 * 1. Setup() — 在 RenderGraph 上声明资源依赖
 * 2. Execute() — 录制渲染命令到 CommandBuffer
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
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PASSES_RENDER_PASS_INTERFACE_H
