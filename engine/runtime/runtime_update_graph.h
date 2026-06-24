#ifndef DSE_RUNTIME_UPDATE_GRAPH_H
#define DSE_RUNTIME_UPDATE_GRAPH_H

#include "engine/base/frame_update_context.h"

class FramePipeline;

namespace dse::runtime {

void RunRuntimeUpdateGraph(::FramePipeline& pipeline, const dse::FrameUpdateContext& frame);
void RunRuntimeFixedUpdateGraph(::FramePipeline& pipeline, float fixed_delta_time);

} // namespace dse::runtime

#endif
