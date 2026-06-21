/**
 * @file bloom_renderer.cpp
 * @brief BloomRenderer 实现 — 见头文件说明。
 */

#include "engine/render/bloom_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

namespace dse {
namespace render {

void BloomRenderer::BeginFrame() {
    quad_.BeginFrame();
}

void BloomRenderer::Downsample(CommandBuffer& cmd, RhiDevice& device,
                               unsigned int source_tex, float src_res_x, float src_res_y) {
    const unsigned int cs = device.GetBloomComputeShader(/*upsample=*/false);
    if (cs != 0) {
        cmd.DispatchComputePass(ComputeDispatch{cs, source_tex, 1.0f});
        return;
    }
    // GL 等无 compute 后端：全屏 quad（srcResolution 标量化为 2 float，对应 bloom_downsample.frag UBO）。
    quad_.Draw(cmd, device, PostProcessRequest{"bloom_downsample", source_tex, {src_res_x, src_res_y}});
}

void BloomRenderer::Upsample(CommandBuffer& cmd, RhiDevice& device,
                             unsigned int source_tex, float filter_radius, float blend_weight) {
    const unsigned int cs = device.GetBloomComputeShader(/*upsample=*/true);
    if (cs != 0) {
        cmd.DispatchComputePass(ComputeDispatch{cs, source_tex, blend_weight});
        return;
    }
    // GL 等无 compute 后端：全屏 quad，alpha 混合累加（blend func 与旧 GL bloom upsample 一致）。
    PostProcessRequest req{"bloom_upsample", source_tex, {filter_radius, blend_weight}};
    req.blend_enabled = true;
    quad_.Draw(cmd, device, req);
}

void BloomRenderer::Shutdown(RhiDevice& device) {
    quad_.Shutdown(device);
}

} // namespace render
} // namespace dse
