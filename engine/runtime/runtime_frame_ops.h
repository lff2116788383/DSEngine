#pragma once

class FramePipeline;
class CommandBuffer;

namespace dse::runtime {

void RunFrameUpdate(FramePipeline& pipeline, float delta_time);
void RunFrameFixedUpdate(FramePipeline& pipeline, float fixed_delta_time);
void RunFrameRender(FramePipeline& pipeline);
void BuildFrameRenderGraph(FramePipeline& pipeline);
void ExecuteFrameRenderGraph(FramePipeline& pipeline, CommandBuffer& cmd_buffer);

} // namespace dse::runtime
