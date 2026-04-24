/**
 * @file gl_resource_manager.cpp
 * @brief GLResourceManager 实现 - GPU 资源管理器
 */

#include "engine/render/rhi/gl_resource_manager.h"
#include "engine/render/rhi/rhi_device.h"
#include <cstdio>

namespace dse {
namespace render {

unsigned int GLResourceManager::AllocateRenderTargetHandle() {
    return next_render_target_handle_++;
}

unsigned int GLResourceManager::AllocateTextureHandle() {
    return next_texture_handle_++;
}

unsigned int GLResourceManager::AllocatePipelineStateHandle() {
    return next_pipeline_state_handle_++;
}

void GLResourceManager::StoreRenderTarget(unsigned int handle, const RenderTargetResource& rt) {
    render_targets_[handle] = rt;
    resource_ledger_.render_targets_created++;
}

const RenderTargetResource* GLResourceManager::GetRenderTarget(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? &it->second : nullptr;
}

void GLResourceManager::RemoveRenderTarget(unsigned int handle) {
    if (render_targets_.erase(handle) > 0) {
        resource_ledger_.render_targets_destroyed++;
    }
}

void GLResourceManager::StorePipelineState(unsigned int handle, const PipelineStateDesc& desc) {
    pipeline_states_[handle] = desc;
    resource_ledger_.pipeline_states_created++;
}

const PipelineStateDesc* GLResourceManager::GetPipelineState(unsigned int handle) const {
    auto it = pipeline_states_.find(handle);
    return it != pipeline_states_.end() ? &it->second : nullptr;
}

ResourceLedger& GLResourceManager::ledger() {
    return resource_ledger_;
}

const ResourceLedger& GLResourceManager::ledger() const {
    return resource_ledger_;
}

void GLResourceManager::LogResourceLedger() const {
    std::printf("[GLResourceManager] Resource Ledger:\n");
    std::printf("  Textures:        created=%zu destroyed=%zu alive=%zu\n",
        resource_ledger_.textures_created, resource_ledger_.textures_destroyed,
        resource_ledger_.textures_created - resource_ledger_.textures_destroyed);
    std::printf("  Framebuffers:    created=%zu destroyed=%zu alive=%zu\n",
        resource_ledger_.framebuffers_created, resource_ledger_.framebuffers_destroyed,
        resource_ledger_.framebuffers_created - resource_ledger_.framebuffers_destroyed);
    std::printf("  RenderTargets:   created=%zu destroyed=%zu alive=%zu\n",
        resource_ledger_.render_targets_created, resource_ledger_.render_targets_destroyed,
        resource_ledger_.render_targets_created - resource_ledger_.render_targets_destroyed);
    std::printf("  PipelineStates:  created=%zu destroyed=%zu alive=%zu\n",
        resource_ledger_.pipeline_states_created, resource_ledger_.pipeline_states_destroyed,
        resource_ledger_.pipeline_states_created - resource_ledger_.pipeline_states_destroyed);
    std::printf("  ShaderPrograms:  created=%zu destroyed=%zu alive=%zu\n",
        resource_ledger_.shader_programs_created, resource_ledger_.shader_programs_destroyed,
        resource_ledger_.shader_programs_created - resource_ledger_.shader_programs_destroyed);
    std::printf("  VertexArrays:    created=%zu destroyed=%zu alive=%zu\n",
        resource_ledger_.vertex_arrays_created, resource_ledger_.vertex_arrays_destroyed,
        resource_ledger_.vertex_arrays_created - resource_ledger_.vertex_arrays_destroyed);
    std::printf("  Buffers:         created=%zu destroyed=%zu alive=%zu\n",
        resource_ledger_.buffers_created, resource_ledger_.buffers_destroyed,
        resource_ledger_.buffers_created - resource_ledger_.buffers_destroyed);
    std::fflush(stdout);
}

} // namespace render
} // namespace dse
