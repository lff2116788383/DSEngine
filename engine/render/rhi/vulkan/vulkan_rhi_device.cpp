/**
 * @file vulkan_rhi_device.cpp
 * @brief VulkanRhiDevice å®žçŽ° â€” Vulkan åŽç«¯çš„ RhiDevice æŽ¥å£å®žçŽ°
 *
 * æž¶æž„å¯¹æ ‡ OpenGLRhiDeviceï¼Œäº”ä¸ªå­ç³»ç»ŸååŒå·¥ä½œï¼š
 * - VulkanContextï¼šInstance/Device/Swapchain ç”Ÿå‘½å‘¨æœŸ
 * - VulkanResourceManagerï¼šçº¹ç†/Buffer/RenderTarget åˆ›å»ºä¸Žé”€æ¯
 * - VulkanPipelineStateManagerï¼šVkPipeline/VkRenderPass ç¼“å­˜
 * - VulkanShaderManagerï¼šGLSLâ†’SPIR-V ç¼–è¯‘ä¸Žåå°„
 * - VulkanDrawExecutorï¼šç»˜åˆ¶å‘½ä»¤æ‰§è¡Œ
 */

#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"

#include <algorithm>
#include <cstring>

namespace dse {
namespace render {

// ============================================================
// VulkanCommandBuffer å®žçŽ°
// ============================================================

void VulkanCommandBuffer::SetDevice(VulkanRhiDevice* device) {
    device_ = device;
    base_device_ = device;
}

void VulkanCommandBuffer::BeginRenderPass(const RenderPassDesc& render_pass) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->SetActiveRenderCommandBuffer(vk_command_buffer_);
    device_->FlushPendingGpuTimerReset(vk_command_buffer_);
    device_->draw_executor().BeginRenderPass(
        vk_command_buffer_, render_pass,
        device_->resource_mgr(), device_->state_mgr());
}

void VulkanCommandBuffer::EndRenderPass() {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().EndRenderPass(vk_command_buffer_);
    device_->ClearActiveRenderCommandBuffer();
}

void VulkanCommandBuffer::ClearColor(const glm::vec4& color) {
    // Vulkan ä¸­æ¸…é™¤åœ¨ BeginRenderPass æ—¶é€šè¿‡ VkClearValue å¤„ç†
    // æ­¤å¤„ä¸ºç©ºæ“ä½œï¼Œæˆ–åœ¨å·²å¼€å¯ RenderPass æ—¶ç”¨ vkCmdClearAttachments
    (void)color;
}

void VulkanCommandBuffer::DispatchComputePass(const ComputeDispatch& dispatch) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DispatchComputePass(
        vk_command_buffer_, dispatch, device_->shader_mgr());
}

// --- é€šç”¨ç»˜åˆ¶åŽŸè¯­ (A1) ---

void VulkanCommandBuffer::BindPipeline(unsigned int graphics_pipeline_handle) {
    if (!device_) return;
    const auto* desc = device_->GetGraphicsPipelineDesc(graphics_pipeline_handle);
    if (!desc) return;
    // è®¾ä¸ºæ´»åŠ¨ PSOï¼ˆç»˜åˆ¶æ—¶ä¸Ž program ä¸€èµ·æƒ°æ€§çƒ˜è¿› VkPipelineï¼‰ï¼›program!=0 æ—¶ç»‘ programï¼ˆPSO-only ç®¡çº¿ program==0ï¼‰ã€‚
    device_->state_mgr().set_active_pipeline_state(desc->pso_state);
    if (desc->program != 0) device_->draw_executor().PrimBindShaderProgram(desc->program);
}


void VulkanCommandBuffer::BindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                                           const std::vector<VertexAttr>& attrs, VertexInputRate rate) {
    if (!device_) return;
    const VulkanBuffer* buf = device_->resource_mgr().GetBuffer(buffer_handle);
    VkBuffer vk_buf = buf ? buf->buffer : VK_NULL_HANDLE;
    device_->draw_executor().PrimBindVertexBuffer(slot, vk_buf, stride, attrs, rate);
}

void VulkanCommandBuffer::PushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) {
    if (!device_) return;
    device_->draw_executor().PrimPushConstants(stage, offset, data, size);
}

void VulkanCommandBuffer::Draw(uint32_t vertex_count, uint32_t first_vertex) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().PrimDraw(
        vk_command_buffer_, vertex_count, first_vertex,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

// --- é€šç”¨ç»˜åˆ¶åŽŸè¯­ (B0) ---

void VulkanCommandBuffer::BindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    if (!device_) return;
    const VulkanBuffer* buf = device_->resource_mgr().GetBuffer(buffer_handle);
    VkBuffer vk_buf = buf ? buf->buffer : VK_NULL_HANDLE;
    device_->draw_executor().PrimBindIndexBuffer(vk_buf, type);
}

void VulkanCommandBuffer::BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    if (!device_) return;
    device_->draw_executor().PrimBindTexture(slot, texture_handle, dim);
}

void VulkanCommandBuffer::BindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                            uint32_t offset, uint32_t size) {
    if (!device_) return;
    device_->draw_executor().PrimBindUniformBuffer(slot, buffer_handle, offset, size);
}

void VulkanCommandBuffer::BindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                            uint32_t offset, uint32_t size) {
    if (!device_) return;
    device_->draw_executor().PrimBindStorageBuffer(slot, buffer_handle, offset, size);
}

void VulkanCommandBuffer::DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().PrimDrawIndexed(
        vk_command_buffer_, index_count, first_index, base_vertex,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void VulkanCommandBuffer::DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                               uint32_t first_index, int32_t base_vertex,
                                               uint32_t first_instance) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().PrimDrawIndexedInstanced(
        vk_command_buffer_, index_count, instance_count, first_index, base_vertex, first_instance,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void VulkanCommandBuffer::DrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().PrimDrawIndexedIndirect(
        vk_command_buffer_, indirect_buffer, byte_offset,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void VulkanCommandBuffer::BlitToScreen(unsigned int source_rt) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().BlitRenderTargetToSwapchain(
        vk_command_buffer_, source_rt, device_->resource_mgr());
}

void VulkanCommandBuffer::SetViewport(int x, int y, int width, int height) {
    if (vk_command_buffer_ == VK_NULL_HANDLE) return;
    VkViewport vp{};
    vp.x = static_cast<float>(x);
    vp.y = static_cast<float>(y);
    vp.width = static_cast<float>(width);
    vp.height = static_cast<float>(height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(vk_command_buffer_, 0, 1, &vp);
    VkRect2D scissor{};
    scissor.offset = {x, y};
    scissor.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    vkCmdSetScissor(vk_command_buffer_, 0, 1, &scissor);
}

void VulkanCommandBuffer::ClearDepth(float depth) {
    if (vk_command_buffer_ == VK_NULL_HANDLE) return;
    VkClearAttachment clear_att{};
    clear_att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clear_att.clearValue.depthStencil = {depth, 0};
    VkClearRect clear_rect{};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;
    // rect uses current scissor (full render area if not set)
    clear_rect.rect = {{0, 0}, {16384, 16384}};  // oversized; GPU clamps to attachment
    vkCmdClearAttachments(vk_command_buffer_, 1, &clear_att, 1, &clear_rect);
}

void VulkanCommandBuffer::Reset() {
    vk_command_buffer_ = VK_NULL_HANDLE;
    ResetBase();
}

// ============================================================
// VulkanRhiDevice å®žçŽ°
// ============================================================

struct VulkanRhiDevice::HiZImpl {
    struct HiZTextureInfo {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView full_view = VK_NULL_HANDLE;
        std::vector<VkImageView> mip_views;
        int width = 0;
        int height = 0;
        int mip_count = 0;
        unsigned int texture_handle = 0;
    };
    std::unordered_map<unsigned int, HiZTextureInfo> textures;
    unsigned int next_handle = 450000;
};

VulkanRhiDevice::VulkanRhiDevice() = default;
VulkanRhiDevice::~VulkanRhiDevice() = default;

RenderDeviceInfo VulkanRhiDevice::GetDeviceInfo() const {
    RenderDeviceInfo info;
    VkPhysicalDevice physical_device = context_.physical_device();
    if (physical_device != VK_NULL_HANDLE) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physical_device, &props);
        info.adapter_name = props.deviceName;
        // CPU ç±»åž‹ç‰©ç†è®¾å¤‡å³è½¯æ¸²ï¼ˆå¦‚ lavapipe / SwiftShaderï¼‰ã€‚
        info.is_software = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);
    }
    return info;
}

bool VulkanRhiDevice::InitDevice(void* window_handle, int width, int height) {
#ifdef NDEBUG
    return InitVulkan(window_handle, width, height, false);
#else
    return InitVulkan(window_handle, width, height, true);
#endif
}

void VulkanRhiDevice::EnsureInitialized() {
    if (initialized_) return;

    // åˆå§‹åŒ–å­ç³»ç»Ÿ
    state_mgr_.Init(&context_, &shader_mgr_);
    shader_mgr_.Init(&context_);

    // ç¼–è¯‘å†…ç½®ç€è‰²å™¨
    shader_mgr_.InitBuiltinPBRShader();
    shader_mgr_.InitSkyboxShader();
    shader_mgr_.InitSpriteShader();
    shader_mgr_.InitTextSdfShader();
    shader_mgr_.InitShadowShader();
    shader_mgr_.InitGPUDrivenPBRShader();
    shader_mgr_.InitGPUDrivenShadowShader();
    shader_mgr_.InitPostProcessShader();
    shader_mgr_.InitBloomComputeShaders();

    draw_executor_.InitGeometryBuffers(&context_, &resource_mgr_);

    initialized_ = true;
    DEBUG_LOG_INFO("[Vulkan] All subsystems initialized");
}

bool VulkanRhiDevice::InitVulkan(void* window_handle, int width, int height, bool enable_validation) {
    if (initialized_) return true;

    // 1. åˆå§‹åŒ– Vulkan ä¸Šä¸‹æ–‡
    if (!context_.Init(window_handle, width, height, enable_validation)) {
        DEBUG_LOG_ERROR("[Vulkan] Context init failed");
        return false;
    }
    KeepAlive();

    // 2. åˆå§‹åŒ–èµ„æºç®¡ç†å™¨
    if (!resource_mgr_.Init(&context_)) {
        DEBUG_LOG_ERROR("[Vulkan] ResourceManager init failed");
        return false;
    }

    // 3. åˆå§‹åŒ–å…¶ä½™å­ç³»ç»Ÿ
    state_mgr_.Init(&context_, &shader_mgr_);
    shader_mgr_.Init(&context_);
    KeepAlive();

    // 4. ç¼–è¯‘å†…ç½®ç€è‰²å™¨ï¼ˆPBR/å¤©ç©ºç›’/ç²’å­/ç²¾çµ/é˜´å½±/åŽå¤„ç†ï¼‰
    shader_mgr_.InitBuiltinPBRShader();
    KeepAlive();
    shader_mgr_.InitSkyboxShader();
    shader_mgr_.InitSpriteShader();
    shader_mgr_.InitTextSdfShader();
    shader_mgr_.InitShadowShader();
    shader_mgr_.InitGPUDrivenPBRShader();
    shader_mgr_.InitGPUDrivenShadowShader();
    KeepAlive();
    shader_mgr_.InitPostProcessShader();
    shader_mgr_.InitBloomComputeShaders();
    KeepAlive();

    // 5. åˆå§‹åŒ–å‡ ä½•ç¼“å†²åŒºå’Œ UBO
    draw_executor_.InitGeometryBuffers(&context_, &resource_mgr_);

    // 6. GPU Timestamp Query
    gpu_timer_.Init(&context_);

    initialized_ = true;
    DEBUG_LOG_INFO("[Vulkan] RhiDevice initialized with all subsystems");
    return true;
}

void VulkanRhiDevice::Shutdown() {
    if (!initialized_) return;

    WaitIdle();

    // æ¸…ç† Hi-Z èµ„æºï¼ˆå¿…é¡»åœ¨ VkDevice é”€æ¯å‰ï¼‰
    if (hiz_impl_) {
        VkDevice device = context_.device();
        for (auto& [id, info] : hiz_impl_->textures) {
            for (auto& mv : info.mip_views)
                if (mv) vkDestroyImageView(device, mv, nullptr);
            if (info.full_view) vkDestroyImageView(device, info.full_view, nullptr);
            if (info.image) vkDestroyImage(device, info.image, nullptr);
            if (info.memory) vkFreeMemory(device, info.memory, nullptr);
        }
        hiz_impl_->textures.clear();
    }

    // é”€æ¯å³æ—¶ç»˜åˆ¶ï¼ˆÂ§5.Aï¼‰åŠ¨æ€ç®¡çº¿ç¼“å­˜ï¼ˆé¡»åœ¨ VkDevice é”€æ¯å‰ï¼‰
    {
        VkDevice device = context_.device();
        for (auto& [key, pipeline] : immediate_pipelines_) {
            if (pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device, pipeline, nullptr);
        }
        immediate_pipelines_.clear();
    }

    // æŒ‰ä¾èµ–é€†åºå…³é—­å­ç³»ç»Ÿ
    gpu_timer_.Shutdown();
    draw_executor_.ShutdownGeometryBuffers();
    shader_mgr_.Shutdown();
    state_mgr_.Shutdown();
    resource_mgr_.Shutdown();
    context_.Shutdown();

    external_shader_programs_.clear();
    initialized_ = false;
    DEBUG_LOG_INFO("[Vulkan] RhiDevice shutdown");
}

void VulkanRhiDevice::WaitIdle() {
    if (!initialized_) return;
    context_.WaitIdle();
    resource_mgr_.ResetCommandPool();
    for (uint32_t i = 0; i < VulkanContext::MAX_FRAMES_IN_FLIGHT; ++i) {
        resource_mgr_.ResetDescriptorPool(i);
    }
    pending_command_buffers_.clear();
    active_render_cmd_ = VK_NULL_HANDLE;
}

void VulkanRhiDevice::BeginFrame() {
    if (!initialized_) return;

    current_frame_stats_ = RenderStats{};
    pending_command_buffers_.clear();
    draw_executor_.BeginFrame();

    if (swapchain_needs_recreate_) {
        const int w = Screen::width() > 0 ? Screen::width() : 1280;
        const int h = Screen::height() > 0 ? Screen::height() : 720;
        context_.RecreateSwapchain(w, h);
        swapchain_needs_recreate_ = false;
        swapchain_recreated_this_frame_ = true;
    } else {
        swapchain_recreated_this_frame_ = false;
    }

    VkResult acquire_result = context_.AcquireNextImage();
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        const int w = Screen::width() > 0 ? Screen::width() : 1280;
        const int h = Screen::height() > 0 ? Screen::height() : 720;
        context_.RecreateSwapchain(w, h);
        swapchain_recreated_this_frame_ = true;
        acquire_result = context_.AcquireNextImage();
        if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
            DEBUG_LOG_WARN("[Vulkan] BeginFrame: AcquireNextImage failed after swapchain recreate ({}), skipping frame",
                           static_cast<int>(acquire_result));
            return;
        }
    }

    resource_mgr_.ResetDescriptorPool(context_.current_frame());

    gpu_timer_.ResetGpuTimers();
}

uint32_t VulkanRhiDevice::FramesInFlight() const {
    return VulkanContext::MAX_FRAMES_IN_FLIGHT;
}

uint32_t VulkanRhiDevice::CurrentFrameSlot() const {
    // BeginFrameâ€¦EndFrame é—´ç¨³å®šï¼ˆAdvanceFrame åœ¨ EndFrame æŽ¨è¿›ï¼‰ï¼Œå…¶ fence å·²åœ¨
    // AcquireNextImage ç­‰å¾… â†’ è¯¥æ§½ä½çš„ä¸Šä¸€å ç”¨å¸§å·²å®Œæˆï¼Œå¯å®‰å…¨è¦†å†™/é‡å»ºã€‚
    return context_.current_frame();
}

unsigned int VulkanRhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    return resource_mgr_.CreateRenderTarget(desc.width, desc.height, desc.has_color, desc.has_depth,
                                             desc.generate_mipmaps, desc.cube_map,
                                             desc.msaa_samples, desc.allow_uav,
                                             desc.color_attachment_count);
}

void VulkanRhiDevice::DeleteRenderTarget(unsigned int render_target_handle) {
    resource_mgr_.DeleteRenderTarget(render_target_handle);
}

unsigned int VulkanRhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    // Vulkan ä¸­çº¹ç†å¥æŸ„æ¦‚å¿µä¸åŒï¼Œè¿”å›ž RenderTarget handle ä½œä¸ºä»£ç†
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
    return render_target_handle; // ä»£ç†
}

std::vector<unsigned char> VulkanRhiDevice::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const {
    auto result = ReadRenderTargetColorRgba8WithSize(render_target_handle);
    return std::move(result.pixels);
}

RenderTargetReadback VulkanRhiDevice::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const {
    // const_cast: åº•å±‚è¯»å›žæ“ä½œåœ¨è¯­ä¹‰ä¸Šæ˜¯åªè¯»çš„ï¼Œä½† Vulkan å‘½ä»¤æäº¤éœ€è¦éž const è®¿é—®
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

    // åˆ›å»ºå›žè¯»ç¼“å†²åŒº
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

    // å½•åˆ¶å‘½ä»¤ï¼štransition image â†’ copy â†’ transition back
    VkCommandBuffer cmd = resource_mgr.BeginSingleTimeCommands();

    // è¿‡æ¸¡é¢œè‰²é™„ä»¶åˆ° TRANSFER_SRC
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

    // æ‹·è´ image â†’ buffer
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

    // è¿‡æ¸¡å›ž SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    resource_mgr.EndSingleTimeCommands(cmd);

    // å›žè¯»åƒç´ 
    RenderTargetReadback result;
    result.width = width;
    result.height = height;
    result.pixels.resize(width * height * 4);

    void* mapped = nullptr;
    if (vkMapMemory(device, readback_memory, 0, data_size, 0, &mapped) == VK_SUCCESS) {
        if (is_hdr) {
            // R16G16B16A16_SFLOAT â†’ RGBA8: half-float â†’ clamp [0,1] â†’ uint8
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

    // æ¸…ç†
    vkDestroyBuffer(device, readback_buffer, nullptr);
    vkFreeMemory(device, readback_memory, nullptr);

    return result;
}

RenderTargetDepthReadback VulkanRhiDevice::ReadRenderTargetDepthFloatWithSize(unsigned int render_target_handle) const {
    // const_cast: åº•å±‚è¯»å›žæ“ä½œè¯­ä¹‰åªè¯»ï¼Œä½† Vulkan å‘½ä»¤æäº¤éœ€éž const è®¿é—®ã€‚
    auto& resource_mgr = const_cast<VulkanResourceManager&>(resource_mgr_);
    const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(render_target_handle);
    if (!rt || !rt->has_depth || rt->depth_texture.image == VK_NULL_HANDLE) {
        return {};
    }

    auto device = context_.device();
    auto physical_device = context_.physical_device();

    const int width = rt->width;
    const int height = rt->height;
    // D24_UNORM_S8ï¼šæ‹·è´æ·±åº¦ aspect æ—¶æ¯ texel æ‰“åŒ…ä¸º 32 ä½ï¼ˆæ·±åº¦åœ¨ä½Ž 24 ä½ï¼‰ã€‚
    const size_t data_size = static_cast<size_t>(width) * height * 4;

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

    VkCommandBuffer cmd = resource_mgr.BeginSingleTimeCommands();

    // æ·±åº¦é™„ä»¶ pass ç»“æŸåŽå¤„äºŽ DEPTH_STENCIL_READ_ONLY_OPTIMAL â†’ è¿‡æ¸¡åˆ° TRANSFER_SRCã€‚
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rt->depth_texture.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyImageToBuffer(cmd, rt->depth_texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readback_buffer, 1, &region);

    // è¿‡æ¸¡å›ž DEPTH_STENCIL_READ_ONLY_OPTIMALã€‚
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    resource_mgr.EndSingleTimeCommands(cmd);

    RenderTargetDepthReadback result;
    result.width = width;
    result.height = height;
    result.depth.resize(static_cast<size_t>(width) * height, 1.0f);

    void* mapped = nullptr;
    if (vkMapMemory(device, readback_memory, 0, data_size, 0, &mapped) == VK_SUCCESS) {
        constexpr float kInv24 = 1.0f / 16777215.0f;  // 1/(2^24-1)
        const uint32_t* src = static_cast<const uint32_t*>(mapped);
        for (int i = 0; i < width * height; ++i) {
            result.depth[i] = static_cast<float>(src[i] & 0x00FFFFFFu) * kInv24;
        }
        vkUnmapMemory(device, readback_memory);
    }

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

// --- å†…å»ºèµ„æºè®¿é—®å™¨ ---

unsigned int VulkanRhiDevice::GetBuiltinProgram(BuiltinProgram program) {
    EnsureInitialized();
    switch (program) {
        case BuiltinProgram::Skybox:
            if (shader_mgr_.skybox_shader_handle() == 0) shader_mgr_.InitSkyboxShader();
            return shader_mgr_.skybox_shader_handle();
        case BuiltinProgram::Sprite2D:
            if (shader_mgr_.sprite2d_shader_handle() == 0) shader_mgr_.InitSprite2DShader();
            return shader_mgr_.sprite2d_shader_handle();
        case BuiltinProgram::SpriteFxSdf:
            if (shader_mgr_.sprite_fx_sdf_shader_handle() == 0) shader_mgr_.InitSpriteFxSdfShader();
            return shader_mgr_.sprite_fx_sdf_shader_handle();
        case BuiltinProgram::SpriteFxVfx:
            if (shader_mgr_.sprite_fx_vfx_shader_handle() == 0) shader_mgr_.InitSpriteFxVfxShader();
            return shader_mgr_.sprite_fx_vfx_shader_handle();
        case BuiltinProgram::ForwardPbr:
            if (shader_mgr_.forward_pbr_shader_handle() == 0) shader_mgr_.InitForwardPbrShader();
            return shader_mgr_.forward_pbr_shader_handle();
        case BuiltinProgram::ForwardPbrSkinned:
            if (shader_mgr_.forward_pbr_skinned_shader_handle() == 0) shader_mgr_.InitForwardPbrSkinnedShader();
            return shader_mgr_.forward_pbr_skinned_shader_handle();
        case BuiltinProgram::ForwardPbrInstanced:
            if (shader_mgr_.forward_pbr_instanced_shader_handle() == 0) shader_mgr_.InitForwardPbrInstancedShader();
            return shader_mgr_.forward_pbr_instanced_shader_handle();
        case BuiltinProgram::ForwardPbrDepth:
            if (shader_mgr_.forward_pbr_depth_shader_handle() == 0) shader_mgr_.InitForwardPbrDepthShader();
            return shader_mgr_.forward_pbr_depth_shader_handle();
        case BuiltinProgram::ForwardInstancedDepth:
            if (shader_mgr_.forward_instanced_depth_shader_handle() == 0) shader_mgr_.InitForwardInstancedDepthShader();
            return shader_mgr_.forward_instanced_depth_shader_handle();
        case BuiltinProgram::Particle3D:
            if (shader_mgr_.particle3d_shader_handle() == 0) shader_mgr_.InitParticle3DShader();
            return shader_mgr_.particle3d_shader_handle();
        case BuiltinProgram::HairStrand:
            if (shader_mgr_.hair_strand_shader_handle() == 0) shader_mgr_.InitHairStrandShader();
            return shader_mgr_.hair_strand_shader_handle();
        case BuiltinProgram::ForwardShaded:
            if (shader_mgr_.forward_shaded_shader_handle() == 0) shader_mgr_.InitForwardShadedShader();
            return shader_mgr_.forward_shaded_shader_handle();
        case BuiltinProgram::ForwardSkinnedShaded:
            if (shader_mgr_.forward_skinned_shaded_shader_handle() == 0) shader_mgr_.InitForwardSkinnedShadedShader();
            return shader_mgr_.forward_skinned_shaded_shader_handle();
        case BuiltinProgram::ForwardInstancedShaded:
            if (shader_mgr_.forward_instanced_shaded_shader_handle() == 0) shader_mgr_.InitForwardInstancedShadedShader();
            return shader_mgr_.forward_instanced_shaded_shader_handle();
        case BuiltinProgram::ForwardSkinnedInstancedShaded:
            if (shader_mgr_.forward_skinned_instanced_shaded_shader_handle() == 0) shader_mgr_.InitForwardSkinnedInstancedShadedShader();
            return shader_mgr_.forward_skinned_instanced_shaded_shader_handle();
        case BuiltinProgram::ForwardMorphShaded:
            if (shader_mgr_.forward_morph_shaded_shader_handle() == 0) shader_mgr_.InitForwardMorphShadedShader();
            return shader_mgr_.forward_morph_shaded_shader_handle();
        case BuiltinProgram::GBufferMesh:
            return shader_mgr_.gbuffer_mesh_shader_handle();  // InitBuiltinShaders é˜¶æ®µå·²é¢„ç¼–è¯‘
        case BuiltinProgram::Impostor:
            if (shader_mgr_.impostor_shader_handle() == 0) shader_mgr_.InitImpostorShader();
            return shader_mgr_.impostor_shader_handle();
    }
    return 0;
}

unsigned int VulkanRhiDevice::GetGenPPShaderProgram(const std::string& effect_name) {
    EnsureInitialized();
    // æ— å‚ sampler-only æ•ˆæžœå…±ç”¨å†…å»º passthroughï¼ˆfullscreen quad é‡‡æ ·æºçº¹ç†ï¼‰ã€‚
    // PostProcessRenderer æŒ‰ effect åå– gen-PP ç¨‹åºå¥æŸ„ï¼›æœªæ˜ å°„æ•ˆæžœè¿”å›ž 0ï¼ˆè°ƒç”¨æ–¹è·³è¿‡ï¼‰ã€‚
    // InitPostProcessShader ä¸€æ¬¡åˆ›å»º passthrough/fxaa ç­‰å…¨éƒ¨ PP æ•ˆæžœç€è‰²å™¨ã€‚
    if (shader_mgr_.postprocess_shader_handle() == 0) shader_mgr_.InitPostProcessShader();
    if (effect_name == "postprocess_passthrough" || effect_name == "copy" ||
        effect_name == "ui_overlay") {
        return shader_mgr_.postprocess_shader_handle();
    }
    if (effect_name == "fxaa") return shader_mgr_.fxaa_shader_handle();
    if (effect_name == "bloom_extract") return shader_mgr_.bloom_extract_shader_handle();
    if (effect_name == "lum_compute") return shader_mgr_.lum_compute_shader_handle();
    if (effect_name == "ssao_blur") return shader_mgr_.ssao_blur_shader_handle();
    if (effect_name == "ssao") return shader_mgr_.ssao_shader_handle();
    if (effect_name == "contact_shadow") return shader_mgr_.contact_shadow_shader_handle();
    if (effect_name == "edge_detect") return shader_mgr_.edge_detect_shader_handle();
    if (effect_name == "lum_adapt") return shader_mgr_.lum_adapt_shader_handle();
    if (effect_name == "dof") return shader_mgr_.dof_shader_handle();
    if (effect_name == "motion_blur") return shader_mgr_.motion_blur_shader_handle();
    if (effect_name == "ssr") return shader_mgr_.ssr_shader_handle();
    if (effect_name == "taa_resolve") return shader_mgr_.taa_resolve_shader_handle();
    if (effect_name == "motion_vector") return shader_mgr_.motion_vector_shader_handle();
    if (effect_name == "volumetric_fog") return shader_mgr_.volumetric_fog_shader_handle();
    if (effect_name == "volumetric_cloud") return shader_mgr_.volumetric_cloud_shader_handle();
    if (effect_name == "water") return shader_mgr_.water_shader_handle();
    if (effect_name == "decal") return shader_mgr_.decal_shader_handle();
    if (effect_name == "wboit_composite") return shader_mgr_.wboit_composite_shader_handle();
    if (effect_name == "weather_particle") return shader_mgr_.weather_particle_shader_handle();
    if (effect_name == "sss_blur") return shader_mgr_.sss_blur_shader_handle();
    if (effect_name == "tonemapping") return shader_mgr_.tonemapping_shader_handle();
    if (effect_name == "ssao_apply") return shader_mgr_.ssao_apply_shader_handle();
    if (effect_name == "light_shaft") return shader_mgr_.light_shaft_shader_handle();
    if (effect_name == "atmosphere_transmittance_lut") return shader_mgr_.atmosphere_transmittance_lut_shader_handle();
    if (effect_name == "atmosphere_sky") return shader_mgr_.atmosphere_sky_shader_handle();
    if (effect_name == "bloom_composite") return shader_mgr_.bloom_composite_ssao_ae_shader_handle();
    return 0;
}

unsigned int VulkanRhiDevice::GetBloomComputeShader(bool upsample) const {
    return upsample ? shader_mgr_.bloom_upsample_cs_handle()
                    : shader_mgr_.bloom_downsample_cs_handle();
}

unsigned int VulkanRhiDevice::GetSkyboxCubeVertexBuffer() {
    EnsureInitialized();
    if (skybox_cube_vbo_handle_ == 0) {
        static const float kSkyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
        };
        skybox_cube_vbo_handle_ = resource_mgr_.CreateBuffer(sizeof(kSkyboxVertices), kSkyboxVertices, false, false);
    }
    return skybox_cube_vbo_handle_;
}

BufferHandle VulkanRhiDevice::CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) {
    // kUniformï¼ˆéž storage/indirect/indexï¼‰â†’ VK_BUFFER_USAGE_UNIFORM_BUFFERï¼ˆhost-visible æŒä¹…æ˜ å°„ï¼‰
    if (has(desc.usage, GpuBufferUsage::kUniform) &&
        !has(desc.usage, GpuBufferUsage::kStorage) &&
        !has(desc.usage, GpuBufferUsage::kIndirect) &&
        !has(desc.usage, GpuBufferUsage::kIndex)) {
        BufferHandle h{resource_mgr_.CreateUniformBuffer(desc.size, initial_data, desc.is_dynamic)};
        if (h) gpu_buffer_usage_map_[h.raw()] = desc.usage;
        return h;
    }
    return RhiDevice::CreateGpuBuffer(desc, initial_data);
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

// --- Indirect Draw Buffer ---

unsigned int VulkanRhiDevice::CreateIndirectBuffer(size_t size, const void* data) {
    return resource_mgr_.CreateIndirectBuffer(size, data);
}

void VulkanRhiDevice::UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) {
    resource_mgr_.UpdateIndirectBuffer(handle, offset, size, data);
}

void VulkanRhiDevice::DeleteIndirectBuffer(unsigned int handle) {
    resource_mgr_.DeleteIndirectBuffer(handle);
}

void VulkanRhiDevice::MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride, size_t byte_offset) {
    if (draw_count <= 0 || indirect_buffer == 0) return;
    if (active_render_cmd_ == VK_NULL_HANDLE) return;

    // æŸ¥æ‰¾ VkBufferï¼šå…ˆæŸ¥ indirect buffer mapï¼Œå†æŸ¥ SSBO mapï¼ˆdraw cmd SSBO æœ‰ INDIRECT_BUFFER_BITï¼‰
    const VulkanBuffer* buf = resource_mgr_.GetIndirectBuffer(indirect_buffer);
    if (!buf || buf->buffer == VK_NULL_HANDLE) {
        buf = resource_mgr_.GetSSBO(indirect_buffer);
    }
    if (!buf || buf->buffer == VK_NULL_HANDLE) return;
    draw_executor_.SetBoundSSBOs(bound_ssbos_);
    draw_executor_.BindGPUDrivenInstanceSet(active_render_cmd_, resource_mgr_);
    vkCmdDrawIndexedIndirect(active_render_cmd_, buf->buffer, static_cast<VkDeviceSize>(byte_offset),
                             static_cast<uint32_t>(draw_count), static_cast<uint32_t>(stride));
}

// --- Compute Shader ---




} // namespace render
} // namespace dse

