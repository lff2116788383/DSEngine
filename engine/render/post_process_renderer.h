/**
 * @file post_process_renderer.h
 * @brief 生产级后处理渲染器（后端无关，仅用通用绘制原语）。
 *
 * 后端无关的后处理渲染器（阶段 2b 已全面取代旧 ABI CommandBuffer::DrawPostProcess）。语义：
 *   - 全屏 quad（clip-space，pos\@0 vec2 + uv\@1 vec2）→ postprocess.vert；
 *   - 每效果取 gen 着色器程序（device.GetGenPPShaderProgram(effect)），
 *     绑源纹理 + 额外纹理 + 参数 UBO（set=2,binding=0）后 DrawIndexed(6)。
 *   - PSO 关剔除/深度；按 req.blend_enabled 选不混合 / alpha 混合。
 *
 * 资源（全屏 VB / IB / 参数 UBO 池 / PSO）首帧懒建并跨帧复用。
 *
 * Draw 仅处理已由后端 GetGenPPShaderProgram 支持（返回非 0）的效果；其余返回 false
 * （未知/未实现效果，调用方按需跳过）。bloom mip 链经 BloomRenderer 走 compute 原语。
 */

#ifndef DSE_RENDER_POST_PROCESS_RENDERER_H
#define DSE_RENDER_POST_PROCESS_RENDERER_H

#include <vector>

#include "engine/render/rhi/rhi_handle.h"
#include "engine/render/rhi/postprocess_common.h"

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/**
 * @class PostProcessRenderer
 * @brief 用通用原语执行单个后处理效果。资源帧间懒建并复用。
 */
class PostProcessRenderer {
public:
    /// 执行一次后处理。req.effect_name 必须已被后端 GetGenPPShaderProgram 支持，
    /// 否则返回 false（未知/未实现效果）。须在 BeginRenderPass 内调用。
    bool Draw(CommandBuffer& cmd, RhiDevice& device, const PostProcessRequest& req);

    /// 新一帧开始：复位参数 UBO 池游标（缓冲本身跨帧持久复用）。
    void BeginFrame();

    /// 释放内部 GPU 资源（析构/重置时调用）。
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device);
    unsigned int PsoFor(RhiDevice& device, bool blend);
    BufferHandle NextParamUbo(RhiDevice& device, size_t bytes);

    BufferHandle quad_vbo_;   ///< 全屏 quad 顶点（clip-space，pos2+uv2，静态）
    BufferHandle quad_ibo_;   ///< 全屏 quad 索引（0,1,2,0,2,3，静态）

    /// 参数 UBO 池（每次带参 Draw 取一个独立缓冲，跨帧持久——满足 Vulkan
    /// 「提交前不可别名/删除」生命周期约束）。BeginFrame 复位游标循环复用。
    std::vector<BufferHandle> param_ubos_;
    size_t param_ubo_next_ = 0;

    unsigned int pso_opaque_ = 0;  ///< 关剔除/深度/混合
    unsigned int pso_blend_  = 0;  ///< 关剔除/深度 + alpha 混合
    bool init_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_POST_PROCESS_RENDERER_H
