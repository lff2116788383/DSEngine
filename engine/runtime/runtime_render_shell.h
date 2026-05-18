#ifndef DSE_RUNTIME_RENDER_SHELL_H
#define DSE_RUNTIME_RENDER_SHELL_H

#include <memory>

class FramePipeline;
namespace dse::render { class CommandBuffer; }
using dse::render::CommandBuffer;

namespace dse::runtime {

void BeginRuntimeRenderFrame(::FramePipeline& pipeline);
std::shared_ptr<CommandBuffer> CreateRuntimeRenderCommandBuffer(::FramePipeline& pipeline);
void BindRuntimeShadowMaps(::FramePipeline& pipeline);
void SubmitAndEndRuntimeRenderFrame(::FramePipeline& pipeline, std::shared_ptr<CommandBuffer> cmd_buffer);
void FinalizeRuntimeRenderFrame(::FramePipeline& pipeline);

} // namespace dse::runtime

#endif
