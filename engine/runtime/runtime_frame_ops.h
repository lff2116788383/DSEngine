#pragma once

#include "engine/base/frame_update_context.h"

class FramePipeline;
namespace dse::render { class CommandBuffer; }
using dse::render::CommandBuffer;

namespace dse::runtime {

void RunRuntimeUpdateGraph(FramePipeline& pipeline, const dse::FrameUpdateContext& frame);
void RunRuntimeFixedUpdateGraph(FramePipeline& pipeline, float fixed_delta_time);

void RunFrameUpdate(FramePipeline& pipeline, const dse::FrameUpdateContext& frame);
void RunFrameFixedUpdate(FramePipeline& pipeline, float fixed_delta_time);
void RunFrameRender(FramePipeline& pipeline);
void BuildFrameRenderGraph(FramePipeline& pipeline);
void ExecuteFrameRenderGraph(FramePipeline& pipeline, CommandBuffer& cmd_buffer);

} // namespace dse::runtime
