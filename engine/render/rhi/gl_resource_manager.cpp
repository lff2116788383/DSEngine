/**
 * @file gl_resource_manager.cpp
 * @brief GLResourceManager 实现 - GPU 资源管理器
 */

#include "engine/render/rhi/gl_resource_manager.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include <glad/gl.h>
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

void GLResourceManager::DestroyAllRenderTargets() {
    for (auto& [handle, target] : render_targets_) {
        if (target.fbo_handle != 0) {
            glDeleteFramebuffers(1, &target.fbo_handle);
            resource_ledger_.framebuffers_destroyed += 1;
            target.fbo_handle = 0;
        }
        if (target.color_texture_handle != 0) {
            glDeleteTextures(1, &target.color_texture_handle);
            resource_ledger_.textures_destroyed += 1;
            target.color_texture_handle = 0;
        }
        if (target.depth_texture_handle != 0) {
            glDeleteTextures(1, &target.depth_texture_handle);
            resource_ledger_.textures_destroyed += 1;
            target.depth_texture_handle = 0;
        }
    }
    resource_ledger_.render_targets_destroyed += render_targets_.size();
    render_targets_.clear();
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
    const std::size_t live_textures = resource_ledger_.textures_created - resource_ledger_.textures_destroyed;
    const std::size_t live_fbos = resource_ledger_.framebuffers_created - resource_ledger_.framebuffers_destroyed;
    const std::size_t live_programs = resource_ledger_.shader_programs_created - resource_ledger_.shader_programs_destroyed;
    const std::size_t live_vaos = resource_ledger_.vertex_arrays_created - resource_ledger_.vertex_arrays_destroyed;
    const std::size_t live_buffers = resource_ledger_.buffers_created - resource_ledger_.buffers_destroyed;
    const std::size_t live_targets = resource_ledger_.render_targets_created - resource_ledger_.render_targets_destroyed;
    const std::size_t live_pipelines = resource_ledger_.pipeline_states_created - resource_ledger_.pipeline_states_destroyed;
    DEBUG_LOG_INFO("RHI resource ledger: textures={}/{}, framebuffers={}/{}, shader_programs={}/{}, vaos={}/{}, buffers={}/{}, render_targets={}/{}, pipeline_states={}/{}",
                   resource_ledger_.textures_destroyed, resource_ledger_.textures_created,
                   resource_ledger_.framebuffers_destroyed, resource_ledger_.framebuffers_created,
                   resource_ledger_.shader_programs_destroyed, resource_ledger_.shader_programs_created,
                   resource_ledger_.vertex_arrays_destroyed, resource_ledger_.vertex_arrays_created,
                   resource_ledger_.buffers_destroyed, resource_ledger_.buffers_created,
                   resource_ledger_.render_targets_destroyed, resource_ledger_.render_targets_created,
                   resource_ledger_.pipeline_states_destroyed, resource_ledger_.pipeline_states_created);
    if (live_textures != 0 || live_fbos != 0 || live_programs != 0 || live_vaos != 0 || live_buffers != 0 || live_targets != 0 || live_pipelines != 0) {
        DEBUG_LOG_WARN("RHI resource ledger detected live GL objects: textures={}, framebuffers={}, shader_programs={}, vaos={}, buffers={}, render_targets={}, pipeline_states={}",
                       live_textures, live_fbos, live_programs, live_vaos, live_buffers, live_targets, live_pipelines);
    }
}

} // namespace render
} // namespace dse
