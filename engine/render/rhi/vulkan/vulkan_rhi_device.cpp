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

#include <cstring>

namespace dse {
namespace render {

// ============================================================
// VulkanCommandBuffer 实现
// ============================================================

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

void VulkanCommandBuffer::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    view_ = view;
    projection_ = projection;
}

void VulkanCommandBuffer::DrawBatch(const std::vector<DrawBatchItem>& items) {
    // DrawBatchItem 是 SpriteDrawItem 的别名，直接传递
    if (!items.empty()) {
        DrawSpriteBatch(items);
    }
}

void VulkanCommandBuffer::DrawMeshBatch(const std::vector<MeshDrawItem>& items) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
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

void VulkanCommandBuffer::SetGlobalMat4(const std::string& name, const glm::mat4& value) {
    pending_mat4_[name] = value;
}

void VulkanCommandBuffer::SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) {
    pending_mat4_array_[name] = values;
}

void VulkanCommandBuffer::SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) {
    pending_float_array_[name] = values;
}

void VulkanCommandBuffer::DrawSkybox(unsigned int cubemap_texture_handle) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawSkybox(
        vk_command_buffer_, cubemap_texture_handle, view_, projection_,
        device_->state_mgr(), device_->shader_mgr());
}

void VulkanCommandBuffer::DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawPostProcess(
        vk_command_buffer_, source_texture, effect_name, params,
        device_->state_mgr(), device_->shader_mgr());
}

void VulkanCommandBuffer::DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawParticles3D(
        vk_command_buffer_, items, view, projection,
        device_->state_mgr(), device_->shader_mgr());
}

void VulkanCommandBuffer::DeferSetGlobalShadowMap(unsigned int index, unsigned int texture_handle) {
    if (device_) device_->SetGlobalShadowMap(index, texture_handle);
}

void VulkanCommandBuffer::DeferSetGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) {
    if (device_) device_->SetGlobalSpotShadowMap(index, texture_handle);
}

void VulkanCommandBuffer::DeferSetGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) {
    if (device_) device_->SetGlobalPointShadowMap(index, texture_handle);
}

void VulkanCommandBuffer::Reset() {
    vk_command_buffer_ = VK_NULL_HANDLE;
    view_ = glm::mat4(1.0f);
    projection_ = glm::mat4(1.0f);
}

// ============================================================
// VulkanRhiDevice 实现
// ============================================================

bool VulkanRhiDevice::InitDevice(void* window_handle, int width, int height) {
    return InitVulkan(window_handle, width, height, false);
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

    // 2. 初始化资源管理器
    if (!resource_mgr_.Init(&context_)) {
        DEBUG_LOG_ERROR("[Vulkan] ResourceManager init failed");
        return false;
    }

    // 3. 初始化其余子系统
    state_mgr_.Init(&context_, &shader_mgr_);
    shader_mgr_.Init(&context_);

    // 4. 编译内置着色器（PBR/天空盒/粒子/精灵/后处理）
    shader_mgr_.InitBuiltinPBRShader();
    shader_mgr_.InitSkyboxShader();
    shader_mgr_.InitParticleShader();
    shader_mgr_.InitSpriteShader();
    shader_mgr_.InitPostProcessShader();
    shader_mgr_.InitBloomComputeShaders();

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
    // 获取下一帧 swapchain image
    context_.AcquireNextImage();
}

unsigned int VulkanRhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    return resource_mgr_.CreateRenderTarget(desc.width, desc.height, desc.has_color, desc.has_depth,
                                             desc.generate_mipmaps, desc.cube_map,
                                             desc.msaa_samples, desc.allow_uav);
}

unsigned int VulkanRhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    // Vulkan 中纹理句柄概念不同，返回 RenderTarget handle 作为代理
    return render_target_handle;
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
    size_t data_size = width * height * 4;

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
    result.pixels.resize(data_size);

    void* mapped = nullptr;
    if (vkMapMemory(device, readback_memory, 0, data_size, 0, &mapped) == VK_SUCCESS) {
        memcpy(result.pixels.data(), mapped, data_size);
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

unsigned int VulkanRhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    return resource_mgr_.CreateTextureCube(width, height, rgba8_faces, linear_filter);
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
    VkResult end_result = vkEndCommandBuffer(vk_cmd->GetVkCommandBuffer());
    if (end_result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] vkEndCommandBuffer failed: {}", static_cast<int>(end_result));
        return;
    }

    // 收集本帧所有已提交的命令缓冲
    pending_command_buffers_.push_back(vk_cmd->GetVkCommandBuffer());
    current_frame_stats_.draw_calls++;
}

void VulkanRhiDevice::EndFrame() {
    if (!initialized_) return;

    draw_executor_.EndFrame();

    // 提交本帧所有录制的命令缓冲 + present
    if (!pending_command_buffers_.empty()) {
        context_.PresentFrame(pending_command_buffers_);
        pending_command_buffers_.clear();
    }

    context_.AdvanceFrame();
    last_frame_stats_ = current_frame_stats_;
}

const RenderStats& VulkanRhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

} // namespace render
} // namespace dse
