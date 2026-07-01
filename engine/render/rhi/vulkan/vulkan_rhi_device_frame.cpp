/**
 * @file vulkan_rhi_device_frame.cpp
 * @brief VulkanRhiDevice frame management, GPU-driven, VAO, debug modes.
 */

#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#include "engine/render/rhi/vulkan/vulkan_rhi_device_internal.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"

#include <algorithm>
#include <cstring>

namespace dse {
namespace render {

void VulkanRhiDevice::ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
    if (!initialized_ || !dst || size == 0) return;
    const auto* ssbo = resource_mgr_.GetSSBO(handle);
    if (!ssbo || !ssbo->buffer) return;

    // Staging buffer è¯»å›ž
    VkDevice device = context_.device();
    VkBuffer staging;
    VkDeviceMemory staging_mem;

    VkBufferCreateInfo buf_ci{};
    buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_ci.size = size;
    buf_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buf_ci, nullptr, &staging) != VK_SUCCESS) return;

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, staging, &mem_req);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = resource_mgr_.FindMemoryType(
        mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &alloc_info, nullptr, &staging_mem) != VK_SUCCESS) {
        vkDestroyBuffer(device, staging, nullptr);
        return;
    }
    vkBindBufferMemory(device, staging, staging_mem, 0);

    VkCommandBuffer cmd = resource_mgr_.BeginSingleTimeCommands();
    VkBufferCopy copy_region{};
    copy_region.srcOffset = offset;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkCmdCopyBuffer(cmd, ssbo->buffer, staging, 1, &copy_region);
    resource_mgr_.EndSingleTimeCommands(cmd);

    void* mapped = nullptr;
    if (vkMapMemory(device, staging_mem, 0, size, 0, &mapped) == VK_SUCCESS) {
        memcpy(dst, mapped, size);
        vkUnmapMemory(device, staging_mem);
    }

    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, staging_mem, nullptr);
}

VertexArrayHandle VulkanRhiDevice::CreateVertexArray() {
    // Vulkan ä¸éœ€è¦ VAO æ¦‚å¿µï¼Œé¡¶ç‚¹æ ¼å¼åœ¨ VkPipeline åˆ›å»ºæ—¶æŒ‡å®š
    // è¿”å›žå ä½å¥æŸ„ä»¥å…¼å®¹ RhiDevice æŽ¥å£
    static unsigned int vao_counter = 600000;
    return VertexArrayHandle{vao_counter++};
}

void VulkanRhiDevice::DeleteVertexArray(VertexArrayHandle handle) {
    // Vulkan ä¸éœ€è¦ VAO æ¦‚å¿µï¼Œno-op
    (void)handle;
}

std::shared_ptr<CommandBuffer> VulkanRhiDevice::CreateCommandBuffer() {
    auto cmd = std::make_shared<VulkanCommandBuffer>();
    cmd->SetDevice(this);

    // ä»Žå‘½ä»¤ç¼“å†²æ± èŽ·å–ï¼ˆé¿å…é€å¸§ vkAllocateCommandBuffers å¼€é”€ï¼‰
    VkCommandBuffer vk_cmd = resource_mgr_.AcquireCommandBuffer();
    cmd->SetVkCommandBuffer(vk_cmd);

    // ç«‹å³å¼€å§‹å½•åˆ¶
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(vk_cmd, &begin_info);

    return cmd;
}

void VulkanRhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    if (!initialized_) return;

    auto* vk_cmd = dynamic_cast<VulkanCommandBuffer*>(cmd_buffer.get());
    if (!vk_cmd || vk_cmd->GetVkCommandBuffer() == VK_NULL_HANDLE) return;

    // ç»“æŸå‘½ä»¤ç¼“å†²å½•åˆ¶
    DEBUG_LOG_TRACE("[Vulkan] Submit: vkEndCommandBuffer");
    VkResult end_result = vkEndCommandBuffer(vk_cmd->GetVkCommandBuffer());
    if (end_result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] vkEndCommandBuffer failed: {}", static_cast<int>(end_result));
        return;
    }
    DEBUG_LOG_TRACE("[Vulkan] Submit: vkEndCommandBuffer OK");

    // æ”¶é›†æœ¬å¸§æ‰€æœ‰å·²æäº¤çš„å‘½ä»¤ç¼“å†²
    pending_command_buffers_.push_back(vk_cmd->GetVkCommandBuffer());
    current_frame_stats_.draw_calls++;
}

void VulkanRhiDevice::EndFrame() {
    if (!initialized_) return;

    draw_executor_.EndFrame();

    // æäº¤æœ¬å¸§æ‰€æœ‰å½•åˆ¶çš„å‘½ä»¤ç¼“å†² + present
    VkResult present_result = VK_SUCCESS;
    // ä¿å­˜æœ¬å¸§å‘½ä»¤ç¼“å†²åˆ—è¡¨ï¼Œç”¨äºŽæäº¤åŽå½’è¿˜åˆ°æ± 
    std::vector<VkCommandBuffer> frame_cmd_buffers = std::move(pending_command_buffers_);
    pending_command_buffers_.clear();

    if (!frame_cmd_buffers.empty()) {
        DEBUG_LOG_TRACE("[Vulkan] EndFrame: PresentFrame ({} cmd bufs)", frame_cmd_buffers.size());
        present_result = context_.PresentFrame(frame_cmd_buffers);
        DEBUG_LOG_TRACE("[Vulkan] EndFrame: PresentFrame result={}", static_cast<int>(present_result));
    } else {
        auto clear_cmd = CreateCommandBuffer();
        if (clear_cmd) {
            clear_cmd->BeginRenderPass(RenderPassDesc{0, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), true});
            clear_cmd->EndRenderPass();
            Submit(clear_cmd);
        }
        if (!pending_command_buffers_.empty()) {
            frame_cmd_buffers = std::move(pending_command_buffers_);
            pending_command_buffers_.clear();
            present_result = context_.PresentFrame(frame_cmd_buffers);
        }
    }
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
        (present_result == VK_SUBOPTIMAL_KHR && !swapchain_recreated_this_frame_)) {
        swapchain_needs_recreate_ = true;
    }

    context_.AdvanceFrame();
    // å½’è¿˜æœ¬å¸§å‘½ä»¤ç¼“å†²åˆ°æ± ï¼ˆAdvanceFrame å·²é€šè¿‡ fence ä¿è¯ GPU å®Œæˆï¼‰
    for (auto& cb : frame_cmd_buffers) {
        resource_mgr_.ReleaseCommandBuffer(cb);
    }
    const auto& ex_stats = draw_executor_.last_frame_stats();
    last_frame_stats_ = {};
    last_frame_stats_.draw_calls = ex_stats.draw_calls;
    last_frame_stats_.sprite_count = ex_stats.sprite_count;
    last_frame_stats_.mesh_count = ex_stats.mesh_count;
    last_frame_stats_.render_passes = ex_stats.render_passes;
    last_frame_stats_.shadow_passes = ex_stats.shadow_passes;
    last_frame_stats_.material_switches = ex_stats.material_switches;
    last_frame_stats_.instanced_draw_calls = ex_stats.instanced_draw_calls;
    last_frame_stats_.instanced_mesh_count = ex_stats.instanced_mesh_count;
    last_frame_stats_.particle_count = ex_stats.particle_count;
    last_frame_stats_.max_batch_sprites = ex_stats.max_batch_sprites;

    // GPU Timestamp Query: æ”¶é›†ä¸Šä¸€å¸§ç»“æžœ
    gpu_timer_.ResolveGpuTimers();
}

void VulkanRhiDevice::ResetGpuTimers() {
    if (!initialized_) return;
    gpu_timer_.ResetGpuTimers();
}

void VulkanRhiDevice::FlushPendingGpuTimerReset(VkCommandBuffer cmd) {
    gpu_timer_.FlushPendingQueryPoolReset(cmd);
}

const RenderStats& VulkanRhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

// ============================================================
// Mega Buffer (GPU Driven)
// ============================================================

VertexArrayHandle VulkanRhiDevice::CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                                  BufferHandle& out_vbo, BufferHandle& out_ibo) {
    unsigned int vbo_h = resource_mgr_.CreateBuffer(vbo_size_bytes, nullptr, true, false);
    unsigned int ibo_h = resource_mgr_.CreateBuffer(ibo_size_bytes, nullptr, true, true);
    if (vbo_h == 0 || ibo_h == 0) {
        if (vbo_h) resource_mgr_.DeleteBuffer(vbo_h);
        if (ibo_h) resource_mgr_.DeleteBuffer(ibo_h);
        out_vbo = {}; out_ibo = {};
        return {};
    }
    unsigned int vao_id = next_vao_id_++;
    vao_bindings_[vao_id] = {vbo_h, ibo_h};
    out_vbo = BufferHandle{vbo_h};
    out_ibo = BufferHandle{ibo_h};
    return VertexArrayHandle{vao_id};
}

void VulkanRhiDevice::UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) {
    if (!vbo || size == 0 || !data) return;
    resource_mgr_.UpdateBuffer(vbo.raw(), offset, size, data);
}

void VulkanRhiDevice::UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) {
    if (!ibo || size == 0 || !data) return;
    resource_mgr_.UpdateBuffer(ibo.raw(), offset, size, data);
}

void VulkanRhiDevice::DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) {
    if (vbo) resource_mgr_.DeleteBuffer(vbo.raw());
    if (ibo) resource_mgr_.DeleteBuffer(ibo.raw());
    vao_bindings_.erase(vao.raw());
}

void VulkanRhiDevice::BindMegaVAO(VertexArrayHandle vao) {
    auto it = vao_bindings_.find(vao.raw());
    if (it == vao_bindings_.end()) return;
    if (active_render_cmd_ == VK_NULL_HANDLE) return;

    const VulkanBuffer* vbo_buf = resource_mgr_.GetBuffer(it->second.vbo_handle);
    const VulkanBuffer* ibo_buf = resource_mgr_.GetBuffer(it->second.ibo_handle);
    if (vbo_buf && vbo_buf->buffer != VK_NULL_HANDLE) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(active_render_cmd_, 0, 1, &vbo_buf->buffer, &offset);
    }
    if (ibo_buf && ibo_buf->buffer != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(active_render_cmd_, ibo_buf->buffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

void VulkanRhiDevice::UnbindVAO() {
    // Vulkan æ— éœ€æ˜¾å¼è§£ç»‘
}

// ============================================================
// Static Mesh VAO
// ============================================================

VertexArrayHandle VulkanRhiDevice::CreateStaticMeshVAO(
    const void* vertex_data, size_t vertex_bytes,
    const std::vector<const void*>& ebo_datas,
    const std::vector<size_t>& ebo_sizes,
    BufferHandle& out_vbo,
    std::vector<BufferHandle>& out_ebos)
{
    if (!vertex_data || vertex_bytes == 0) { out_vbo = {}; out_ebos.clear(); return {}; }
    if (ebo_datas.size() != ebo_sizes.size()) { out_vbo = {}; out_ebos.clear(); return {}; }

    unsigned int vbo_h = resource_mgr_.CreateBuffer(vertex_bytes, vertex_data, false, false);
    if (vbo_h == 0) { out_vbo = {}; out_ebos.clear(); return {}; }

    out_ebos.resize(ebo_datas.size());
    unsigned int first_ebo = 0;
    for (size_t i = 0; i < ebo_datas.size(); ++i) {
        unsigned int ebo_h = resource_mgr_.CreateBuffer(ebo_sizes[i], ebo_datas[i], false, true);
        out_ebos[i] = BufferHandle{ebo_h};
        if (i == 0) first_ebo = ebo_h;
    }

    unsigned int vao_id = next_vao_id_++;
    vao_bindings_[vao_id] = {vbo_h, first_ebo};
    out_vbo = BufferHandle{vbo_h};
    return VertexArrayHandle{vao_id};
}

void VulkanRhiDevice::DeleteStaticMeshVAO(VertexArrayHandle vao, BufferHandle vbo,
                                            const std::vector<BufferHandle>& ebos) {
    for (auto& ebo : ebos) {
        if (ebo) resource_mgr_.DeleteBuffer(ebo.raw());
    }
    if (vbo) resource_mgr_.DeleteBuffer(vbo.raw());
    vao_bindings_.erase(vao.raw());
}

void VulkanRhiDevice::BindVAOWithEBO(VertexArrayHandle vao, BufferHandle ebo) {
    auto it = vao_bindings_.find(vao.raw());
    if (it == vao_bindings_.end()) return;
    if (active_render_cmd_ == VK_NULL_HANDLE) return;

    const VulkanBuffer* vbo_buf = resource_mgr_.GetBuffer(it->second.vbo_handle);
    if (vbo_buf && vbo_buf->buffer != VK_NULL_HANDLE) {
        VkDeviceSize vk_offset = 0;
        vkCmdBindVertexBuffers(active_render_cmd_, 0, 1, &vbo_buf->buffer, &vk_offset);
    }
    const VulkanBuffer* ebo_buf = resource_mgr_.GetBuffer(ebo.raw());
    if (ebo_buf && ebo_buf->buffer != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(active_render_cmd_, ebo_buf->buffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

// ============================================================
// GPU-Driven PBR Shader Setup
// ============================================================

bool VulkanRhiDevice::HasGPUDrivenPBRShader() const {
    return shader_mgr_.gpu_driven_pbr_shader_handle() != 0;
}

void VulkanRhiDevice::SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                                const glm::vec3& camera_pos,
                                                const glm::vec3& light_dir, const glm::vec3& light_color,
                                                float light_intensity, float ambient_intensity,
                                                float shadow_strength) {
    if (active_render_cmd_ == VK_NULL_HANDLE) return;
    draw_executor_.SetBoundSSBOs(bound_ssbos_);
    draw_executor_.SetupGPUDrivenPBR(active_render_cmd_, view, proj, camera_pos,
                                      light_dir, light_color,
                                      light_intensity, ambient_intensity,
                                      shadow_strength,
                                      state_mgr_, shader_mgr_);
}

void VulkanRhiDevice::SetupGPUDrivenShadowShader(const glm::mat4& light_view, const glm::mat4& light_proj) {
    if (active_render_cmd_ == VK_NULL_HANDLE) return;
    draw_executor_.SetupGPUDrivenShadow(active_render_cmd_, light_view, light_proj,
                                         state_mgr_, shader_mgr_);
}

void VulkanRhiDevice::BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                                              unsigned int metallic_roughness,
                                              unsigned int emissive, unsigned int occlusion) {
    if (active_render_cmd_ == VK_NULL_HANDLE) return;
    // åŒæ­¥ bound_ssbos_ åˆ° draw executorï¼ˆGPU-driven è·¯å¾„ä¸èµ° DrawMeshBatchï¼Œéœ€æ‰‹åŠ¨åŒæ­¥ï¼‰
    draw_executor_.SetBoundSSBOs(bound_ssbos_);
    draw_executor_.BindGPUDrivenTextures(active_render_cmd_, albedo, normal,
                                          metallic_roughness, emissive, occlusion,
                                          resource_mgr_);
}

void VulkanRhiDevice::CacheGPUDrivenInstanceData(const void* models, const void* cmds, int count) {
    cached_gpu_models_ = models;
    cached_gpu_cmds_   = cmds;
    cached_gpu_count_  = count;
}

void VulkanRhiDevice::UpdateGPUDrivenMaterial(const void* mat_data) {
    if (!mat_data) return;
    draw_executor_.UpdateGPUDrivenMaterial(mat_data);
}

// --- ç¼–è¾‘å™¨åœºæ™¯è§†å›¾æ¨¡å¼ ---

void VulkanRhiDevice::SetWireframeMode(bool enable) {
    global_render_state_.wireframe_mode = enable;
    state_mgr_.SetWireframeMode(enable);
}

void VulkanRhiDevice::SetForceUnlit(bool enable) {
    global_render_state_.force_unlit = enable;
}

void VulkanRhiDevice::SetOverdrawMode(bool enable) {
    global_render_state_.overdraw_mode = enable;
    state_mgr_.SetOverdrawMode(enable);
}

void VulkanRhiDevice::OnWindowResized(int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) return;
    context_.RecreateSwapchain(width, height);
    swapchain_needs_recreate_ = false;
}

} // namespace render
} // namespace dse

