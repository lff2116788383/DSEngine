/**
 * @file webgpu_pipeline_state_manager.cpp
 * @brief WebGPUPipelineStateManager 实现（机械抽自 webgpu_rhi_device.cpp）。
 */

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/webgpu/webgpu_pipeline_state_manager.h"

namespace dse {
namespace render {

const PipelineStateDesc* WebGPUPipelineStateManager::FindPipelineState(unsigned int handle) const {
    auto it = pipeline_states_.find(handle);
    return it != pipeline_states_.end() ? &it->second : nullptr;
}

unsigned int WebGPUPipelineStateManager::CreatePipelineState(const PipelineStateDesc& desc) {
    // 登记 PSO 子状态（光栅/混合/深度/拓扑）。WGPURenderPipeline 由
    // (pso, program, RT 颜色/深度格式, 顶点布局) 惰性组装并缓存（着色器就绪后）。
    const unsigned int h = NextHandle();
    pipeline_states_[h] = desc;
    return h;
}

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
