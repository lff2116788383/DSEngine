#ifndef DSE_RUNTIME_UPDATE_GRAPH_H
#define DSE_RUNTIME_UPDATE_GRAPH_H

class FramePipeline;

namespace dse::runtime {

void RunRuntimeUpdateGraph(::FramePipeline& pipeline, float delta_time);
void RunRuntimeFixedUpdateGraph(::FramePipeline& pipeline, float fixed_delta_time);

} // namespace dse::runtime

#endif
