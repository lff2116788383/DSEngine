#ifndef DSE_RUNTIME_RENDER_SHELL_H
#define DSE_RUNTIME_RENDER_SHELL_H

class FramePipeline;
class CommandBuffer;

namespace dse::runtime {

void BindRuntimeShadowMaps(::FramePipeline& pipeline);
void FinalizeRuntimeRenderFrame(::FramePipeline& pipeline);

} // namespace dse::runtime

#endif
