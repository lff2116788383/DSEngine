/**
 * @file bloom_renderer.h
 * @brief Bloom mip 链渲染器（后端无关，封装 compute vs 全屏 quad 的分叉）。
 *
 * bloom 降采样/升采样在各后端实现发散：DX11/Vulkan 走 compute（写 UAV/storage image），
 * GL 走全屏 quad ping-pong。BloomRenderer 用 device.GetBloomComputeShader() 判定：
 *   - 返回非 0（compute 后端）→ cmd.DispatchComputePass(ComputeDispatch)；
 *   - 返回 0（GL 等）→ 经内部 PostProcessRenderer 走 bloom_downsample/bloom_upsample
 *     全屏 quad（升采样用 alpha 混合累加，与旧 GL 路径 blend func 一致）。
 *
 * 调用方（BloomPass）负责按 mip 逐级 BeginRenderPass(目标 RT) 包裹每一步——compute 路径
 * 取当前绑定 RT 作输出，quad 路径绘制进当前 RT，两条路径结构一致。
 */

#ifndef DSE_RENDER_BLOOM_RENDERER_H
#define DSE_RENDER_BLOOM_RENDERER_H

#include "engine/render/post_process_renderer.h"

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/**
 * @class BloomRenderer
 * @brief 执行一次 bloom 降/升采样步（compute 或 quad，按后端能力分派）。
 */
class BloomRenderer {
public:
    /// 新一帧开始：复位内部 quad 渲染器的参数 UBO 池游标。
    void BeginFrame();

    /// 一次降采样步：以 source_tex 为输入，写入当前绑定 RT。
    /// src_res_x/y 为源 mip 全分辨率（quad 路径据此算 texel；compute 路径忽略，按纹理尺寸推导）。
    void Downsample(CommandBuffer& cmd, RhiDevice& device,
                    unsigned int source_tex, float src_res_x, float src_res_y);

    /// 一次升采样步：以 source_tex（更高 mip）为输入，累加进当前绑定 RT（更低 mip）。
    /// filter_radius 为 quad 路径的采样半径；blend_weight 为混合权重（两路径均用）。
    void Upsample(CommandBuffer& cmd, RhiDevice& device,
                  unsigned int source_tex, float filter_radius, float blend_weight);

    /// 释放内部 GPU 资源。
    void Shutdown(RhiDevice& device);

private:
    PostProcessRenderer quad_;  ///< GL/无 compute 后端的全屏 quad 回退路径
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_BLOOM_RENDERER_H
