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
    if (auto* device = dynamic_cast<OpenGLRhiDevice*>(pipeline.runtime_context_.rhi_device.get())) {
        for (int i = 0; i < CSM_CASCADES; ++i) {
            device->SetGlobalShadowMap(i, pipeline.runtime_context_.rhi_device->GetRenderTargetDepthTexture(pipeline.render_resources_.shadow_render_target[i]));
        }
        for (int i = 0; i < 4; ++i) {
            device->SetGlobalSpotShadowMap(static_cast<unsigned int>(i), pipeline.runtime_context_.rhi_device->GetRenderTargetDepthTexture(pipeline.render_resources_.spot_shadow_render_target[i]));
        }
    }
}

void SubmitAndEndRuntimeRenderFrame(::FramePipeline& pipeline, std::shared_ptr<CommandBuffer> cmd_buffer) {
    pipeline.runtime_context_.rhi_device->Submit(cmd_buffer);
    pipeline.runtime_context_.rhi_device->EndFrame();
}

void FinalizeRuntimeRenderFrame(::FramePipeline& pipeline) {
    const auto& frame_stats = pipeline.runtime_context_.rhi_device->LastFrameStats();
    pipeline.last_draw_calls_ = static_cast<int>(frame_stats.draw_calls);
    pipeline.last_material_switches_ = static_cast<int>(frame_stats.material_switches);
    pipeline.last_max_batch_sprites_ = static_cast<int>(frame_stats.max_batch_sprites);
    pipeline.last_sprite_count_ = static_cast<int>(frame_stats.sprite_count);
}

} // namespace dse::runtime
