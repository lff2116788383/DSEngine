/**
 * @file webgpu_pipeline_state_manager.h
 * @brief WebGPU PSO 子状态登记表（manager 拆分：薄封装）。
 *
 * 仅登记 PSO 子状态（光栅/混合/深度/拓扑）。真正的 WGPURenderPipeline 由 draw_executor
 * 按 (pso, program, RT 格式, 顶点布局) 惰性组装并缓存。本 manager 不持有任何原生对象。
 */

#ifndef DSE_WEBGPU_PIPELINE_STATE_MANAGER_H
#define DSE_WEBGPU_PIPELINE_STATE_MANAGER_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/webgpu/webgpu_common.h"
#include "engine/render/rhi/webgpu/webgpu_context.h"

#include <unordered_map>

namespace dse {
namespace render {

class WebGPUPipelineStateManager {
public:
    void Init(WebGPUContext* ctx) { ctx_ = ctx; }
    /// orchestrator Shutdown 调用：仅子状态登记表，无原生对象。
    void Shutdown() { pipeline_states_.clear(); }

    const PipelineStateDesc* FindPipelineState(unsigned int handle) const;
    unsigned int CreatePipelineState(const PipelineStateDesc& desc);

private:
    unsigned int NextHandle() { return ctx_->NextHandle(); }

    WebGPUContext* ctx_ = nullptr;
    std::unordered_map<unsigned int, PipelineStateDesc> pipeline_states_;
};

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif  // DSE_WEBGPU_PIPELINE_STATE_MANAGER_H
