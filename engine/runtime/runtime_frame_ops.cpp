#include "engine/runtime/runtime_frame_ops.h"

#include "engine/runtime/frame_pipeline.h"

namespace dse::runtime {

void RunFrameUpdate(FramePipeline& pipeline, const dse::TimeContext& time) {
    pipeline.RunUpdateInternal(time);
}

void RunFrameFixedUpdate(FramePipeline& pipeline, float fixed_delta_time) {
    pipeline.RunFixedUpdateInternal(fixed_delta_time);
}

void RunFrameRender(FramePipeline& pipeline) {
    pipeline.RunRenderInternal();
}

void BuildFrameRenderGraph(FramePipeline& pipeline) {
    pipeline.BuildRenderGraphInternal();
}

void ExecuteFrameRenderGraph(FramePipeline& pipeline, CommandBuffer& cmd_buffer) {
    pipeline.ExecuteRenderGraphInternal(cmd_buffer);
}

} // namespace dse::runtime
