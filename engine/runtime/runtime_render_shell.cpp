#include "engine/runtime/runtime_render_shell.h"

#include "engine/runtime/frame_pipeline.h"

namespace dse::runtime {

void BeginRuntimeRenderFrame(::FramePipeline& pipeline) {
    pipeline.runtime_context_.rhi_device->BeginFrame();
}

std::shared_ptr<CommandBuffer> CreateRuntimeRenderCommandBuffer(::FramePipeline& pipeline) {
    return pipeline.runtime_context_.rhi_device->CreateCommandBuffer();
}

void BindRuntimeShadowMaps(::FramePipeline& pipeline) {
    for (int i = 0; i < CSM_CASCADES; ++i) {
        pipeline.runtime_context_.rhi_device->SetGlobalShadowMap(i, pipeline.runtime_context_.rhi_device->GetRenderTargetDepthTexture(pipeline.render_resources_.shadow_render_target[i]));
    }
    for (int i = 0; i < 4; ++i) {
        pipeline.runtime_context_.rhi_device->SetGlobalSpotShadowMap(static_cast<unsigned int>(i), pipeline.runtime_context_.rhi_device->GetRenderTargetDepthTexture(pipeline.render_resources_.spot_shadow_render_target[i]));
        pipeline.runtime_context_.rhi_device->SetGlobalPointShadowMap(static_cast<unsigned int>(i), pipeline.runtime_context_.rhi_device->GetRenderTargetDepthTexture(pipeline.render_resources_.point_shadow_render_target[i]));
    }
}

void SubmitAndEndRuntimeRenderFrame(::FramePipeline& pipeline, std::shared_ptr<CommandBuffer> cmd_buffer) {
    pipeline.runtime_context_.rhi_device->Submit(cmd_buffer);
    pipeline.runtime_context_.rhi_device->EndFrame();
}

void FinalizeRuntimeRenderFrame(::FramePipeline& pipeline) {
    const auto& frame_stats = pipeline.runtime_context_.rhi_device->LastFrameStats();
    pipeline.stats_.SetLastFrameStats(
        static_cast<int>(frame_stats.draw_calls),
        static_cast<int>(frame_stats.material_switches),
        static_cast<int>(frame_stats.max_batch_sprites),
        static_cast<int>(frame_stats.sprite_count));
    pipeline.stats_.SetGpuDrivenStats(
        pipeline.render_pass_context_.gpu_driven_active_this_frame ? 1 : 0,
        pipeline.render_pass_context_.gpu_indirect_draw_count,
        pipeline.render_pass_context_.gpu_total_instances);
}

} // namespace dse::runtime
