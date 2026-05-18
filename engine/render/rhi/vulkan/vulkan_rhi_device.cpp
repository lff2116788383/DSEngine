/**
 * @file vulkan_rhi_device.cpp
 * @brief VulkanRhiDevice 实现 — Vulkan 后端的 RhiDevice 接口实现
 *
 * 架构对标 OpenGLRhiDevice，五个子系统协同工作：
 * - VulkanContext：Instance/Device/Swapchain 生命周期
 * - VulkanResourceManager：纹理/Buffer/RenderTarget 创建与销毁
 * - VulkanPipelineStateManager：VkPipeline/VkRenderPass 缓存
 * - VulkanShaderManager：GLSL→SPIR-V 编译与反射
 * - VulkanDrawExecutor：绘制命令执行
 */

#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#include "engine/base/debug.h"

#include <algorithm>
#include <cstring>

namespace dse {
namespace render {

// ============================================================
// VulkanCommandBuffer 实现
// ============================================================

void VulkanCommandBuffer::SetDevice(VulkanRhiDevice* device) {
    device_ = device;
    base_device_ = device;
}

void VulkanCommandBuffer::BeginRenderPass(const RenderPassDesc& render_pass) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().BeginRenderPass(
        vk_command_buffer_, render_pass,
        device_->resource_mgr(), device_->state_mgr());
}

void VulkanCommandBuffer::EndRenderPass() {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().EndRenderPass(vk_command_buffer_);
}

void VulkanCommandBuffer::SetPipelineState(unsigned int pipeline_state_handle) {
    if (!device_) return;
    device_->state_mgr().set_active_pipeline_state(pipeline_state_handle);
}

void VulkanCommandBuffer::DrawMeshBatch(const std::vector<MeshDrawItem>& items) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    DispatchPendingLightArrays();
    device_->draw_executor().SetBoundSSBOs(device_->bound_ssbos());
    device_->draw_executor().DrawMeshBatch(
        vk_command_buffer_, items, view_, projection_,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void VulkanCommandBuffer::DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawSpriteBatch(
        vk_command_buffer_, items, view_, projection_,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void VulkanCommandBuffer::ClearColor(const glm::vec4& color) {
    // Vulkan 中清除在 BeginRenderPass 时通过 VkClearValue 处理
    // 此处为空操作，或在已开启 RenderPass 时用 vkCmdClearAttachments
    (void)color;
}

void VulkanCommandBuffer::DrawSkybox(unsigned int cubemap_texture_handle) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawSkybox(
        vk_command_buffer_, cubemap_texture_handle, view_, projection_,
        device_->state_mgr(), device_->shader_mgr());
}

void VulkanCommandBuffer::DrawPostProcess(PostProcessRequest request) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawPostProcess(
        vk_command_buffer_, request,
        device_->state_mgr(), device_->shader_mgr());
}

void VulkanCommandBuffer::DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawParticles3D(
        vk_command_buffer_, items, view, projection,
        device_->state_mgr(), device_->shader_mgr());
}

void VulkanCommandBuffer::DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawHairStrands(
        vk_command_buffer_, items, view, projection,
        device_->state_mgr(), device_->shader_mgr());
}

void VulkanCommandBuffer::Reset() {
    vk_command_buffer_ = VK_NULL_HANDLE;
    ResetBase();
}

// ============================================================
// VulkanRhiDevice 实现
// ============================================================

bool VulkanRhiDevice::InitDevice(void* window_handle, int width, int height) {
    return InitVulkan(window_handle, width, height, true); // TODO: validation ON for debugging
}

void VulkanRhiDevice::EnsureInitialized() {
    if (initialized_) return;

    // 初始化子系统
    state_mgr_.Init(&context_, &shader_mgr_);
    shader_mgr_.Init(&context_);

    // 编译内置着色器
    shader_mgr_.InitBuiltinPBRShader();
    shader_mgr_.InitSkyboxShader();
    shader_mgr_.InitParticleShader();
    shader_mgr_.InitSpriteShader();
    shader_mgr_.InitPostProcessShader();
    shader_mgr_.InitBloomComputeShaders();

    draw_executor_.InitGeometryBuffers(&context_, &resource_mgr_);

    initialized_ = true;
    DEBUG_LOG_INFO("[Vulkan] All subsystems initialized");
}

bool VulkanRhiDevice::InitVulkan(void* window_handle, int width, int height, bool enable_validation) {
    if (initialized_) return true;

    // 1. 初始化 Vulkan 上下文
    if (!context_.Init(window_handle, width, height, enable_validation)) {
        DEBUG_LOG_ERROR("[Vulkan] Context init failed");
        return false;
    }
    KeepAlive();

    // 2. 初始化资源管理器
    if (!resource_mgr_.Init(&context_)) {
        DEBUG_LOG_ERROR("[Vulkan] ResourceManager init failed");
        return false;
    }

    // 3. 初始化其余子系统
    state_mgr_.Init(&context_, &shader_mgr_);
    shader_mgr_.Init(&context_);
    KeepAlive();

    // 4. 编译内置着色器（PBR/天空盒/粒子/精灵/后处理）
    shader_mgr_.InitBuiltinPBRShader();
    KeepAlive();
    shader_mgr_.InitSkyboxShader();
    shader_mgr_.InitParticleShader();
    shader_mgr_.InitSpriteShader();
    KeepAlive();
    shader_mgr_.InitPostProcessShader();
    shader_mgr_.InitBloomComputeShaders();
    KeepAlive();

    // 5. 初始化几何缓冲区和 UBO
    draw_executor_.InitGeometryBuffers(&context_, &resource_mgr_);

    initialized_ = true;
    DEBUG_LOG_INFO("[Vulkan] RhiDevice initialized with all subsystems");
    return true;
}

void VulkanRhiDevice::Shutdown() {
    if (!initialized_) return;

    context_.WaitIdle();

    // 按依赖逆序关闭子系统
    draw_executor_.ShutdownGeometryBuffers();
    shader_mgr_.Shutdown();
    state_mgr_.Shutdown();
    resource_mgr_.Shutdown();
    context_.Shutdown();

    external_shader_programs_.clear();
    initialized_ = false;
    DEBUG_LOG_INFO("[Vulkan] RhiDevice shutdown");
}

void VulkanRhiDevice::BeginFrame() {
    current_frame_stats_ = RenderStats{};
    pending_command_buffers_.clear();
    draw_executor_.BeginFrame();
    // 获取下一帧 swapchain image（内部等待 fence，确保 GPU 完成上一帧）
    context_.AcquireNextImage();
    // fence 已完成后，安全重置 descriptor pool
    resource_mgr_.ResetDescriptorPool();
}

unsigned int VulkanRhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    return resource_mgr_.CreateRenderTarget(desc.width, desc.height, desc.has_color, desc.has_depth,
                                             desc.generate_mipmaps, desc.cube_map,
                                             desc.msaa_samples, desc.allow_uav,
                                             desc.color_attachment_count);
}

unsigned int VulkanRhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    // Vulkan 中纹理句柄概念不同，返回 RenderTarget handle 作为代理
    return render_target_handle;
}

unsigned int VulkanRhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const {
    const auto* rt = resource_mgr_.GetRenderTarget(render_target_handle);
    if (rt && !rt->mrt_texture_handles.empty()) {
        if (index >= 0 && index < static_cast<int>(rt->mrt_texture_handles.size()))
            return rt->mrt_texture_handles[index];
        return 0;
    }
    return (index == 0) ? render_target_handle : 0;
}

unsigned int VulkanRhiDevice::GetRenderTargetDepthTexture(unsigned int render_target_handle) const {
    return render_target_handle; // 代理
}

std::vector<unsigned char> VulkanRhiDevice::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const {
    auto result = ReadRenderTargetColorRgba8WithSize(render_target_handle);
    return std::move(result.pixels);
}

RenderTargetReadback VulkanRhiDevice::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const {
    // const_cast: 底层读回操作在语义上是只读的，但 Vulkan 命令提交需要非 const 访问
    auto& resource_mgr = const_cast<VulkanResourceManager&>(resource_mgr_);
    const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(render_target_handle);
    if (!rt || !rt->has_color) {
        return {};
    }

    auto device = context_.device();
    auto physical_device = context_.physical_device();

    int width = rt->width;
    int height = rt->height;
    const bool is_hdr = (rt->color_texture.format == VK_FORMAT_R16G16B16A16_SFLOAT);
    const size_t bytes_per_pixel = is_hdr ? 8 : 4;
    size_t data_size = width * height * bytes_per_pixel;

    // 创建回读缓冲区
    VkBuffer readback_buffer = VK_NULL_HANDLE;
    VkDeviceMemory readback_memory = VK_NULL_HANDLE;

    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = data_size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buf_info, nullptr, &readback_buffer) != VK_SUCCESS) {
        return {};
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, readback_buffer, &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    uint32_t memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            memory_type_index = i;
            break;
        }
    }
    if (memory_type_index == UINT32_MAX) {
        vkDestroyBuffer(device, readback_buffer, nullptr);
        return {};
    }

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = memory_type_index;

    if (vkAllocateMemory(device, &alloc_info, nullptr, &readback_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, readback_buffer, nullptr);
        return {};
    }
    vkBindBufferMemory(device, readback_buffer, readback_memory, 0);

    // 录制命令：transition image → copy → transition back
    VkCommandBuffer cmd = resource_mgr.BeginSingleTimeCommands();

    // 过渡颜色附件到 TRANSFER_SRC
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rt->color_texture.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // 拷贝 image → buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyImageToBuffer(cmd, rt->color_texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback_buffer, 1, &region);

    // 过渡回 SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    resource_mgr.EndSingleTimeCommands(cmd);

    // 回读像素
    RenderTargetReadback result;
    result.width = width;
    result.height = height;
    result.pixels.resize(width * height * 4);

    void* mapped = nullptr;
    if (vkMapMemory(device, readback_memory, 0, data_size, 0, &mapped) == VK_SUCCESS) {
        if (is_hdr) {
            // R16G16B16A16_SFLOAT → RGBA8: half-float → clamp [0,1] → uint8
            const auto half_to_float = [](uint16_t h) -> float {
                uint32_t sign = static_cast<uint32_t>(h >> 15) << 31;
                uint32_t exponent = (h >> 10) & 0x1Fu;
                uint32_t mantissa = h & 0x3FFu;
                uint32_t f;
                if (exponent == 0) {
                    if (mantissa == 0) { f = sign; }
                    else {
                        exponent = 1;
                        while (!(mantissa & 0x400u)) { mantissa <<= 1; exponent--; }
                        mantissa &= 0x3FFu;
                        f = sign | ((exponent + 112u) << 23) | (mantissa << 13);
                    }
                } else if (exponent == 31) {
                    f = sign | 0x7F800000u | (mantissa << 13);
                } else {
                    f = sign | ((exponent + 112u) << 23) | (mantissa << 13);
                }
                float r; memcpy(&r, &f, 4); return r;
            };
            const uint16_t* src = static_cast<const uint16_t*>(mapped);
            unsigned char* dst = result.pixels.data();
            for (int i = 0; i < width * height * 4; ++i) {
                float v = half_to_float(src[i]);
                v = (std::max)(0.0f, (std::min)(1.0f, v));
                dst[i] = static_cast<unsigned char>(v * 255.0f + 0.5f);
            }
        } else {
            memcpy(result.pixels.data(), mapped, data_size);
        }
        vkUnmapMemory(device, readback_memory);
    }

    // 清理
    vkDestroyBuffer(device, readback_buffer, nullptr);
    vkFreeMemory(device, readback_memory, nullptr);

    return result;
}

unsigned int VulkanRhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    return resource_mgr_.CreateTexture2D(width, height, rgba8_data, linear_filter);
}

unsigned int VulkanRhiDevice::CreateCompressedTexture2D(CompressedTextureFormat format,
                                                         const std::vector<CompressedMipLevel>& mips,
                                                         bool linear_filter) {
    return resource_mgr_.CreateCompressedTexture2D(format, mips, linear_filter);
}

unsigned int VulkanRhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    return resource_mgr_.CreateTextureCube(width, height, rgba8_faces, linear_filter);
}

unsigned int VulkanRhiDevice::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    return resource_mgr_.CreateTexture3D(width, height, depth, rgba8_data, linear_filter);
}

void VulkanRhiDevice::DeleteTexture(unsigned int texture_handle) {
    resource_mgr_.DeleteTexture(texture_handle);
}

unsigned int VulkanRhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    unsigned int handle = shader_mgr_.CreateProgram(vert_src, frag_src);
    if (handle != 0) {
        external_shader_programs_.insert(handle);
    }
    return handle;
}

void VulkanRhiDevice::DeleteShaderProgram(unsigned int program_handle) {
    shader_mgr_.DeleteProgram(program_handle);
    external_shader_programs_.erase(program_handle);
}

unsigned int VulkanRhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    return state_mgr_.CreatePipelineState(desc);
}

unsigned int VulkanRhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    return resource_mgr_.CreateBuffer(size, data, is_dynamic, is_index);
}

void VulkanRhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    resource_mgr_.UpdateBuffer(handle, offset, size, data);
    (void)is_index;
}

void VulkanRhiDevice::DeleteBuffer(unsigned int handle) {
    resource_mgr_.DeleteBuffer(handle);
}

// --- SSBO ---

unsigned int VulkanRhiDevice::CreateSSBO(size_t size, const void* data) {
    return resource_mgr_.CreateSSBO(size, data);
}

void VulkanRhiDevice::UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) {
    resource_mgr_.UpdateSSBO(handle, offset, size, data);
}

void VulkanRhiDevice::DeleteSSBO(unsigned int handle) {
    resource_mgr_.DeleteSSBO(handle);
}

// --- Compute Shader ---

unsigned int VulkanRhiDevice::CreateComputeShader(const std::string& source) {
    return shader_mgr_.CreateComputeProgram(source);
}

void VulkanRhiDevice::DeleteComputeShader(unsigned int handle) {
    (void)handle;
}

void VulkanRhiDevice::BeginComputePass() {
    if (!initialized_ || in_compute_pass_) return;

    compute_cmd_buffer_ = resource_mgr_.BeginSingleTimeCommands();
    in_compute_pass_ = true;
    pending_compute_images_.clear();
    pending_compute_samplers_.clear();
}

void VulkanRhiDevice::EndComputePass() {
    if (!in_compute_pass_ || compute_cmd_buffer_ == VK_NULL_HANDLE) return;

    resource_mgr_.EndSingleTimeCommands(compute_cmd_buffer_);
    compute_cmd_buffer_ = VK_NULL_HANDLE;
    in_compute_pass_ = false;
    pending_compute_images_.clear();
    pending_compute_samplers_.clear();
}

void VulkanRhiDevice::DispatchCompute(unsigned int shader_handle,
                                       unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
    if (!initialized_ || shader_handle == 0) return;

    const auto* prog = shader_mgr_.GetComputeProgram(shader_handle);
    if (!prog || prog->pipeline == VK_NULL_HANDLE) return;

    // 确定录制目标 cmd buffer
    const bool batched = in_compute_pass_ && compute_cmd_buffer_ != VK_NULL_HANDLE;
    VkCommandBuffer cmd = batched ? compute_cmd_buffer_ : resource_mgr_.BeginSingleTimeCommands();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prog->pipeline);

    // 绑定 SSBO descriptor set
    if (prog->descriptor_set_layout != VK_NULL_HANDLE && !bound_ssbos_.empty()) {
        VkDescriptorSet ds = resource_mgr_.AllocateDescriptorSet(prog->descriptor_set_layout);
        if (ds != VK_NULL_HANDLE) {
            std::vector<VkWriteDescriptorSet> writes;
            std::vector<VkDescriptorBufferInfo> buffer_infos;
            buffer_infos.reserve(bound_ssbos_.size());

            for (auto& [binding, ssbo_handle] : bound_ssbos_) {
                const auto* ssbo = resource_mgr_.GetSSBO(ssbo_handle);
                if (!ssbo) continue;

                VkDescriptorBufferInfo buf_info{};
                buf_info.buffer = ssbo->buffer;
                buf_info.offset = 0;
                buf_info.range = ssbo->size;
                buffer_infos.push_back(buf_info);

                VkWriteDescriptorSet w{};
                w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet = ds;
                w.dstBinding = binding;
                w.descriptorCount = 1;
                w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                w.pBufferInfo = &buffer_infos.back();
                writes.push_back(w);
            }

            if (!writes.empty()) {
                vkUpdateDescriptorSets(context_.device(),
                    static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    prog->pipeline_layout, 0, 1, &ds, 0, nullptr);
            }
        }
    }

    // Push constants
    if (prog->push_constant_size > 0 && !compute_push_constants_.empty()) {
        uint32_t size = std::min(prog->push_constant_size,
                                 static_cast<uint32_t>(compute_push_constants_.size()));
        vkCmdPushConstants(cmd, prog->pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, size,
                           compute_push_constants_.data());
    }

    vkCmdDispatch(cmd, groups_x, groups_y, groups_z);

    if (!batched) {
        // 单次模式：插入 barrier + 提交
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);
        resource_mgr_.EndSingleTimeCommands(cmd);
    }
}

void VulkanRhiDevice::ComputeMemoryBarrier() {
    if (!in_compute_pass_ || compute_cmd_buffer_ == VK_NULL_HANDLE) return;

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(compute_cmd_buffer_,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void VulkanRhiDevice::SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) {
    pending_compute_images_[binding] = { texture_handle, read_only, -1, false };
}

void VulkanRhiDevice::SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                                 int mip_level, bool read_only, bool r32f) {
    pending_compute_images_[binding] = { texture_handle, read_only, mip_level, r32f };
}

void VulkanRhiDevice::SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
    pending_compute_samplers_[unit] = texture_handle;
}

unsigned int VulkanRhiDevice::CreateHiZTexture(int width, int height) {
    // TODO: Vulkan Hi-Z — 创建 VK_FORMAT_R32_SFLOAT 纹理，完整 mip chain
    (void)width; (void)height;
    return 0;
}

void VulkanRhiDevice::DeleteHiZTexture(unsigned int handle) {
    (void)handle;
}

int VulkanRhiDevice::GetHiZMipCount(unsigned int handle) const {
    (void)handle;
    return 0;
}

unsigned int VulkanRhiDevice::GetHiZGpuTexture(unsigned int handle) const {
    (void)handle;
    return 0;
}

// Push constant 辅助：确保缓冲区足够大，写入数据到指定偏移
static void EnsurePushConstantCapacity(std::vector<uint8_t>& buf, size_t offset, size_t write_size) {
    size_t needed = offset + write_size;
    if (buf.size() < needed) buf.resize(needed, 0);
}

void VulkanRhiDevice::SetComputeUniformInt(unsigned int shader, const char* name, int value) {
    (void)shader; (void)name;
    // Vulkan compute uniform 通过 push constant 传递，后续可扩展为 name→offset 映射
    // 当前简化：顺序追加到 push constant buffer
    size_t offset = compute_push_constants_.size();
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(int));
    memcpy(compute_push_constants_.data() + offset, &value, sizeof(int));
}
void VulkanRhiDevice::SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
    (void)shader; (void)name;
    size_t offset = compute_push_constants_.size();
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(float));
    memcpy(compute_push_constants_.data() + offset, &value, sizeof(float));
}
void VulkanRhiDevice::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
    (void)shader; (void)name;
    int data[2] = { x, y };
    size_t offset = compute_push_constants_.size();
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
    (void)shader; (void)name;
    float data[2] = { x, y };
    size_t offset = compute_push_constants_.size();
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
    (void)shader; (void)name;
    float data[4] = { x, y, z, w };
    size_t offset = compute_push_constants_.size();
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    (void)shader; (void)name;
    size_t offset = compute_push_constants_.size();
    EnsurePushConstantCapacity(compute_push_constants_, offset, 64);
    memcpy(compute_push_constants_.data() + offset, data, 64);
}
void VulkanRhiDevice::ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
    if (!initialized_) return;
    const auto* ssbo = resource_mgr_.GetSSBO(handle);
    if (!ssbo || !ssbo->buffer) return;

    // Staging buffer 读回
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

unsigned int VulkanRhiDevice::CreateVertexArray() {
    // Vulkan 不需要 VAO 概念，顶点格式在 VkPipeline 创建时指定
    // 返回占位句柄以兼容 RhiDevice 接口
    static unsigned int vao_counter = 600000;
    return vao_counter++;
}

void VulkanRhiDevice::DeleteVertexArray(unsigned int handle) {
    // Vulkan 不需要 VAO 概念，no-op
    (void)handle;
}

std::shared_ptr<CommandBuffer> VulkanRhiDevice::CreateCommandBuffer() {
    auto cmd = std::make_shared<VulkanCommandBuffer>();
    cmd->SetDevice(this);

    // 分配 VkCommandBuffer
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = resource_mgr_.command_pool();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer vk_cmd;
    if (vkAllocateCommandBuffers(context_.device(), &alloc_info, &vk_cmd) == VK_SUCCESS) {
        cmd->SetVkCommandBuffer(vk_cmd);

        // 立即开始录制
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(vk_cmd, &begin_info);
    }

    return cmd;
}

void VulkanRhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    if (!initialized_) return;

    auto* vk_cmd = dynamic_cast<VulkanCommandBuffer*>(cmd_buffer.get());
    if (!vk_cmd || vk_cmd->GetVkCommandBuffer() == VK_NULL_HANDLE) return;

    // 结束命令缓冲录制
    DEBUG_LOG_TRACE("[Vulkan] Submit: vkEndCommandBuffer");
    VkResult end_result = vkEndCommandBuffer(vk_cmd->GetVkCommandBuffer());
    if (end_result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] vkEndCommandBuffer failed: {}", static_cast<int>(end_result));
        return;
    }
    DEBUG_LOG_TRACE("[Vulkan] Submit: vkEndCommandBuffer OK");

    // 收集本帧所有已提交的命令缓冲
    pending_command_buffers_.push_back(vk_cmd->GetVkCommandBuffer());
    current_frame_stats_.draw_calls++;
}

void VulkanRhiDevice::EndFrame() {
    if (!initialized_) return;

    draw_executor_.EndFrame();

    // 提交本帧所有录制的命令缓冲 + present
    if (!pending_command_buffers_.empty()) {
        DEBUG_LOG_TRACE("[Vulkan] EndFrame: PresentFrame ({} cmd bufs)", pending_command_buffers_.size());
        context_.PresentFrame(pending_command_buffers_);
        DEBUG_LOG_TRACE("[Vulkan] EndFrame: PresentFrame OK");
        pending_command_buffers_.clear();
    }

    context_.AdvanceFrame();
    // 合并 DrawExecutor 统计到 device 层
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
}

const RenderStats& VulkanRhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

} // namespace render
} // namespace dse
