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
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"

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

void VulkanCommandBuffer::ClearColor(const glm::vec4& color) {
    // Vulkan 中清除在 BeginRenderPass 时通过 VkClearValue 处理
    // 此处为空操作，或在已开启 RenderPass 时用 vkCmdClearAttachments
    (void)color;
}

void VulkanCommandBuffer::DrawPostProcess(PostProcessRequest request) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawPostProcess(
        vk_command_buffer_, request,
        device_->state_mgr(), device_->shader_mgr());
}

// --- 通用绘制原语 (A1) ---

void VulkanCommandBuffer::BindShaderProgram(unsigned int program_handle) {
    if (!device_) return;
    device_->draw_executor().PrimBindShaderProgram(program_handle);
}

void VulkanCommandBuffer::BindVertexBuffer(unsigned int buffer_handle, uint32_t stride,
                                           const std::vector<VertexAttr>& attrs) {
    if (!device_) return;
    const VulkanBuffer* buf = device_->resource_mgr().GetBuffer(buffer_handle);
    VkBuffer vk_buf = buf ? buf->buffer : VK_NULL_HANDLE;
    device_->draw_executor().PrimBindVertexBuffer(vk_buf, stride, attrs);
}

void VulkanCommandBuffer::BindTextureCube(unsigned int slot, unsigned int cubemap_handle) {
    if (!device_) return;
    device_->draw_executor().PrimBindTextureCube(slot, cubemap_handle);
}

void VulkanCommandBuffer::PushConstantsMat4(const glm::mat4& value) {
    if (!device_) return;
    device_->draw_executor().PrimPushConstantsMat4(value);
}

void VulkanCommandBuffer::Draw(uint32_t vertex_count, uint32_t first_vertex) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().PrimDraw(
        vk_command_buffer_, vertex_count, first_vertex,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

// --- 通用绘制原语 (B0) ---

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

void VulkanCommandBuffer::DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || vk_command_buffer_ == VK_NULL_HANDLE) return;
    device_->draw_executor().DrawHairStrands(
        vk_command_buffer_, items, view, projection,
        device_->state_mgr(), device_->shader_mgr());
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
// VulkanRhiDevice 实现
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
        // CPU 类型物理设备即软渲（如 lavapipe / SwiftShader）。
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

    // 初始化子系统
    state_mgr_.Init(&context_, &shader_mgr_);
    shader_mgr_.Init(&context_);

    // 编译内置着色器
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

    // 4. 编译内置着色器（PBR/天空盒/粒子/精灵/阴影/后处理）
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

    // 5. 初始化几何缓冲区和 UBO
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

    // 清理 Hi-Z 资源（必须在 VkDevice 销毁前）
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

    // 按依赖逆序关闭子系统
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

RenderTargetDepthReadback VulkanRhiDevice::ReadRenderTargetDepthFloatWithSize(unsigned int render_target_handle) const {
    // const_cast: 底层读回操作语义只读，但 Vulkan 命令提交需非 const 访问。
    auto& resource_mgr = const_cast<VulkanResourceManager&>(resource_mgr_);
    const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(render_target_handle);
    if (!rt || !rt->has_depth || rt->depth_texture.image == VK_NULL_HANDLE) {
        return {};
    }

    auto device = context_.device();
    auto physical_device = context_.physical_device();

    const int width = rt->width;
    const int height = rt->height;
    // D24_UNORM_S8：拷贝深度 aspect 时每 texel 打包为 32 位（深度在低 24 位）。
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

    // 深度附件 pass 结束后处于 DEPTH_STENCIL_READ_ONLY_OPTIMAL → 过渡到 TRANSFER_SRC。
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

    // 过渡回 DEPTH_STENCIL_READ_ONLY_OPTIMAL。
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

// --- 内建资源访问器 ---

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
        case BuiltinProgram::ForwardShaded:
            if (shader_mgr_.forward_shaded_shader_handle() == 0) shader_mgr_.InitForwardShadedShader();
            return shader_mgr_.forward_shaded_shader_handle();
        case BuiltinProgram::ForwardSkinnedShaded:
            if (shader_mgr_.forward_skinned_shaded_shader_handle() == 0) shader_mgr_.InitForwardSkinnedShadedShader();
            return shader_mgr_.forward_skinned_shaded_shader_handle();
        case BuiltinProgram::ForwardInstancedShaded:
            if (shader_mgr_.forward_instanced_shaded_shader_handle() == 0) shader_mgr_.InitForwardInstancedShadedShader();
            return shader_mgr_.forward_instanced_shaded_shader_handle();
        case BuiltinProgram::ForwardMorphShaded:
            if (shader_mgr_.forward_morph_shaded_shader_handle() == 0) shader_mgr_.InitForwardMorphShadedShader();
            return shader_mgr_.forward_morph_shaded_shader_handle();
    }
    return 0;
}

unsigned int VulkanRhiDevice::GetGenPPShaderProgram(const std::string& effect_name) {
    EnsureInitialized();
    // 无参 sampler-only 效果共用内建 passthrough（fullscreen quad 采样源纹理）。
    // 其余效果尚未迁到 PostProcessRenderer，返回 0 → 调用方继续走 DrawPostProcess ABI。
    // InitPostProcessShader 一次创建 passthrough/fxaa 等全部 PP 效果着色器。
    if (shader_mgr_.postprocess_shader_handle() == 0) shader_mgr_.InitPostProcessShader();
    if (effect_name == "postprocess_passthrough" || effect_name == "copy" ||
        effect_name == "ui_overlay") {
        return shader_mgr_.postprocess_shader_handle();
    }
    if (effect_name == "fxaa") return shader_mgr_.fxaa_shader_handle();
    return 0;
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
    // kUniform（非 storage/indirect/index）→ VK_BUFFER_USAGE_UNIFORM_BUFFER（host-visible 持久映射）
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

    // 查找 VkBuffer：先查 indirect buffer map，再查 SSBO map（draw cmd SSBO 有 INDIRECT_BUFFER_BIT）
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

unsigned int VulkanRhiDevice::CreateComputeShader(const std::string& source) {
    if (!initialized_) return 0u;
    return shader_mgr_.CreateComputeProgram(source);
}

void VulkanRhiDevice::DeleteComputeShader(unsigned int handle) {
    shader_mgr_.DeleteComputeProgram(handle);
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

    // 绑定 descriptor set（SSBO + storage image + sampler）
    if (prog->descriptor_set_layout != VK_NULL_HANDLE) {
        VkDescriptorSet ds = resource_mgr_.AllocateDescriptorSet(prog->descriptor_set_layout);
        if (ds != VK_NULL_HANDLE) {
            std::vector<VkWriteDescriptorSet> writes;
            std::vector<VkDescriptorBufferInfo> buffer_infos;
            std::vector<VkDescriptorImageInfo>  image_infos;
            buffer_infos.reserve(bound_ssbos_.size());
            image_infos.reserve(pending_compute_images_.size() + pending_compute_samplers_.size());

            // SSBO 绑定（binding 0..ssbo_binding_count-1）
            for (auto& [binding, ssbo_handle] : bound_ssbos_) {
                if (binding >= prog->ssbo_binding_count) continue;
                const auto* ssbo = resource_mgr_.GetSSBO(ssbo_handle);
                if (!ssbo) continue;
                VkDescriptorBufferInfo buf_info{};
                buf_info.buffer = ssbo->buffer;
                buf_info.offset = 0;
                buf_info.range  = ssbo->size;
                buffer_infos.push_back(buf_info);
                VkWriteDescriptorSet w{};
                w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet          = ds;
                w.dstBinding      = binding;
                w.descriptorCount = 1;
                w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                w.pBufferInfo     = &buffer_infos.back();
                writes.push_back(w);
            }

            // Storage image 绑定（layout binding = ssbo_count + user_binding）
            uint32_t img_base = prog->ssbo_binding_count;
            uint32_t total_bindings = prog->ssbo_binding_count + prog->storage_image_count + prog->sampler_count;
            for (auto& [binding, img_bind] : pending_compute_images_) {
                if (img_base + binding >= total_bindings) continue;
                VkImageView view = VK_NULL_HANDLE;
                // 检查 Hi-Z 纹理
                if (hiz_impl_) {
                    auto hit = hiz_impl_->textures.find(img_bind.texture_handle);
                    if (hit != hiz_impl_->textures.end()) {
                        auto& hiz = hit->second;
                        int mip = img_bind.mip_level >= 0 ? img_bind.mip_level : 0;
                        if (mip < static_cast<int>(hiz.mip_views.size()))
                            view = hiz.mip_views[mip];
                    }
                }
                // 普通纹理
                if (view == VK_NULL_HANDLE) {
                    const auto* tex = resource_mgr_.GetTexture(img_bind.texture_handle);
                    if (tex) view = tex->image_view;
                }
                if (view == VK_NULL_HANDLE) continue;
                VkDescriptorImageInfo ii{};
                ii.imageView   = view;
                ii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_infos.push_back(ii);
                VkWriteDescriptorSet w{};
                w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet          = ds;
                w.dstBinding      = img_base + binding;
                w.descriptorCount = 1;
                w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                w.pImageInfo      = &image_infos.back();
                writes.push_back(w);
            }

            // Sampler 绑定（layout binding = ssbo_count + storage_image_count + user_unit）
            uint32_t smp_base = prog->ssbo_binding_count + prog->storage_image_count;
            for (auto& [unit, tex_handle] : pending_compute_samplers_) {
                if (smp_base + unit >= total_bindings) continue;
                VkImageView view = VK_NULL_HANDLE;
                VkSampler sampler = VK_NULL_HANDLE;
                // 检查 Hi-Z 纹理（storage image 使用后保持 GENERAL layout）
                bool is_hiz_texture = false;
                if (hiz_impl_) {
                    auto hit = hiz_impl_->textures.find(tex_handle);
                    if (hit != hiz_impl_->textures.end()) {
                        view = hit->second.full_view;
                        sampler = resource_mgr_.default_sampler();
                        is_hiz_texture = true;
                    }
                }
                // 普通纹理
                if (view == VK_NULL_HANDLE) {
                    const auto* tex = resource_mgr_.GetTexture(tex_handle);
                    if (tex && tex->image_view != VK_NULL_HANDLE) {
                        view = tex->image_view;
                        sampler = tex->sampler != VK_NULL_HANDLE ? tex->sampler : resource_mgr_.default_sampler();
                    }
                }
                // Render target depth attachment（Hi-Z 使用 PreZ depth）
                bool is_depth_attachment = false;
                if (view == VK_NULL_HANDLE) {
                    VkImageView depth_view = resource_mgr_.GetRenderTargetDepthImageView(tex_handle);
                    if (depth_view != VK_NULL_HANDLE) {
                        view = depth_view;
                        sampler = resource_mgr_.default_sampler();
                        is_depth_attachment = true;
                    }
                }
                if (view == VK_NULL_HANDLE) continue;
                VkDescriptorImageInfo ii{};
                ii.sampler     = sampler;
                ii.imageView   = view;
                ii.imageLayout = is_depth_attachment
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : (is_hiz_texture ? VK_IMAGE_LAYOUT_GENERAL
                                      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                image_infos.push_back(ii);
                VkWriteDescriptorSet w{};
                w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet          = ds;
                w.dstBinding      = smp_base + unit;
                w.descriptorCount = 1;
                w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w.pImageInfo      = &image_infos.back();
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
    // Dispatch 后清空状态缓存，避免跨 dispatch 累积
    compute_push_constants_.clear();
    compute_uniform_layouts_.clear();
    compute_uniform_next_offset_ = 0;
    pending_compute_images_.clear();
    pending_compute_samplers_.clear();

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

// ============================================================
// RenderGraph 自动屏障：精确 VkImageMemoryBarrier
// ============================================================

namespace {

struct VkBarrierMapping {
    VkImageLayout layout;
    VkAccessFlags access;
    VkPipelineStageFlags stage;
};

VkBarrierMapping MapResourceState(ResourceState state) {
    switch (state) {
    case ResourceState::RenderTarget:
        return { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    case ResourceState::DepthWrite:
        return { VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT };
    case ResourceState::DepthRead:
        return { VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT };
    case ResourceState::ShaderRead:
        return { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
    case ResourceState::UnorderedAccess:
        return { VK_IMAGE_LAYOUT_GENERAL,
                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
    case ResourceState::CopySource:
        return { VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VK_ACCESS_TRANSFER_READ_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT };
    case ResourceState::CopyDest:
        return { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT };
    case ResourceState::Present:
        return { VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                 0,
                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
    case ResourceState::Undefined:
    default:
        return { VK_IMAGE_LAYOUT_UNDEFINED,
                 0,
                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
    }
}

} // anonymous namespace

void VulkanRhiDevice::TransitionRenderTarget(unsigned int rt_handle,
                                              ResourceState from, ResourceState to) {
    if (from == to) return;

    const auto* rt = resource_mgr_.GetRenderTarget(rt_handle);
    if (!rt) return;

    // 确定要转换的 VkImage 和 aspect mask
    VkImage image = VK_NULL_HANDLE;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    bool is_depth_transition = (from == ResourceState::DepthWrite || from == ResourceState::DepthRead ||
                                to == ResourceState::DepthWrite || to == ResourceState::DepthRead);
    if (is_depth_transition && rt->has_depth && rt->depth_texture.image != VK_NULL_HANDLE) {
        image = rt->depth_texture.image;
        // VUID-VkImageMemoryBarrier-image-03320: D24S8/D32S8 必须同时声明 DEPTH+STENCIL aspect
        const VkFormat fmt = rt->depth_texture.format;
        const bool has_stencil = (fmt == VK_FORMAT_D24_UNORM_S8_UINT ||
                                  fmt == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                  fmt == VK_FORMAT_D16_UNORM_S8_UINT);
        aspect = has_stencil ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                             : VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (rt->has_color && rt->color_texture.image != VK_NULL_HANDLE) {
        image = rt->color_texture.image;
    } else {
        return;
    }

    auto src = MapResourceState(from);
    auto dst = MapResourceState(to);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = src.layout;
    barrier.newLayout = dst.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    barrier.srcAccessMask = src.access;
    barrier.dstAccessMask = dst.access;

    // 优先使用活跃渲染命令缓冲；不可用时走 single-time 命令
    VkCommandBuffer cmd = active_render_cmd_;
    if (cmd != VK_NULL_HANDLE) {
        vkCmdPipelineBarrier(cmd, src.stage, dst.stage,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    } else {
        VkCommandBuffer one_shot = resource_mgr_.BeginSingleTimeCommands();
        vkCmdPipelineBarrier(one_shot, src.stage, dst.stage,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        resource_mgr_.EndSingleTimeCommands(one_shot);
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
    if (!initialized_ || width <= 0 || height <= 0) return 0;
    if (!hiz_impl_) hiz_impl_ = std::make_unique<HiZImpl>();

    VkDevice device = context_.device();

    int mip_count = 1;
    {
        int w = width, h = height;
        while (w > 1 || h > 1) {
            w = (std::max)(1, w / 2);
            h = (std::max)(1, h / 2);
            ++mip_count;
        }
    }

    // 创建 VkImage（R32_SFLOAT，完整 mip chain）
    VkImageCreateInfo img_ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    img_ci.imageType = VK_IMAGE_TYPE_2D;
    img_ci.format = VK_FORMAT_R32_SFLOAT;
    img_ci.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    img_ci.mipLevels = static_cast<uint32_t>(mip_count);
    img_ci.arrayLayers = 1;
    img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    img_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    HiZImpl::HiZTextureInfo info{};
    info.width = width;
    info.height = height;
    info.mip_count = mip_count;

    if (vkCreateImage(device, &img_ci, nullptr, &info.image) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create Hi-Z image");
        return 0;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, info.image, &mem_reqs);
    VkMemoryAllocateInfo alloc_ci{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc_ci.allocationSize = mem_reqs.size;
    alloc_ci.memoryTypeIndex = resource_mgr_.FindMemoryType(
        mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &alloc_ci, nullptr, &info.memory) != VK_SUCCESS) {
        vkDestroyImage(device, info.image, nullptr);
        DEBUG_LOG_ERROR("[Vulkan] Failed to allocate Hi-Z memory");
        return 0;
    }
    vkBindImageMemory(device, info.image, info.memory, 0);

    // Layout transition: UNDEFINED → GENERAL（所有 mip 级别）
    {
        VkCommandBuffer cmd = resource_mgr_.BeginSingleTimeCommands();
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = info.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = static_cast<uint32_t>(mip_count);
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        resource_mgr_.EndSingleTimeCommands(cmd);
    }

    // 全 mip view（用于采样）
    VkImageViewCreateInfo view_ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_ci.image = info.image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = VK_FORMAT_R32_SFLOAT;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.baseMipLevel = 0;
    view_ci.subresourceRange.levelCount = static_cast<uint32_t>(mip_count);
    view_ci.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &view_ci, nullptr, &info.full_view);

    // 每个 mip level 一个 view（用于 compute storage image 写入）
    info.mip_views.resize(mip_count);
    for (int i = 0; i < mip_count; ++i) {
        VkImageViewCreateInfo mip_view_ci = view_ci;
        mip_view_ci.subresourceRange.baseMipLevel = static_cast<uint32_t>(i);
        mip_view_ci.subresourceRange.levelCount = 1;
        vkCreateImageView(device, &mip_view_ci, nullptr, &info.mip_views[i]);
    }

    // 注册为纹理资源（供 GetHiZGpuTexture 通过 handle 返回）
    // 使用 resource_mgr_ 的 compute write texture 创建方式简化
    // 这里直接返回一个自管理 handle
    unsigned int handle = hiz_impl_->next_handle++;
    info.texture_handle = handle;
    hiz_impl_->textures[handle] = std::move(info);

    DEBUG_LOG_INFO("[Vulkan] Hi-Z texture created: handle={} {}x{} mips={}", handle, width, height, mip_count);
    return handle;
}

void VulkanRhiDevice::DeleteHiZTexture(unsigned int handle) {
    if (!hiz_impl_) return;
    auto it = hiz_impl_->textures.find(handle);
    if (it == hiz_impl_->textures.end()) return;

    VkDevice device = context_.device();
    auto& info = it->second;
    for (auto& mv : info.mip_views) {
        if (mv) vkDestroyImageView(device, mv, nullptr);
    }
    if (info.full_view) vkDestroyImageView(device, info.full_view, nullptr);
    if (info.image) vkDestroyImage(device, info.image, nullptr);
    if (info.memory) vkFreeMemory(device, info.memory, nullptr);
    hiz_impl_->textures.erase(it);
}

int VulkanRhiDevice::GetHiZMipCount(unsigned int handle) const {
    if (!hiz_impl_) return 0;
    auto it = hiz_impl_->textures.find(handle);
    return it != hiz_impl_->textures.end() ? it->second.mip_count : 0;
}

unsigned int VulkanRhiDevice::GetHiZGpuTexture(unsigned int handle) const {
    if (!hiz_impl_) return 0;
    auto it = hiz_impl_->textures.find(handle);
    return it != hiz_impl_->textures.end() ? handle : 0;
}

static void EnsurePushConstantCapacity(std::vector<uint8_t>& buf, size_t offset, size_t write_size) {
    size_t needed = offset + write_size;
    if (buf.size() < needed) buf.resize(needed, 0);
}

size_t VulkanRhiDevice::GetOrCreateUniformOffset(unsigned int shader, const char* name, size_t data_size) {
    auto& layout = compute_uniform_layouts_[shader];
    auto it = layout.name_to_offset.find(name);
    if (it != layout.name_to_offset.end()) {
        return it->second;
    }
    // 16-byte 对齐（Vulkan push constant 布局要求）
    size_t offset = (compute_uniform_next_offset_ + 15) & ~size_t(15);
    layout.name_to_offset[name] = offset;
    compute_uniform_next_offset_ = offset + data_size;
    return offset;
}

void VulkanRhiDevice::SetComputeUniformInt(unsigned int shader, const char* name, int value) {
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(int));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(int));
    memcpy(compute_push_constants_.data() + offset, &value, sizeof(int));
}
void VulkanRhiDevice::SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(float));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(float));
    memcpy(compute_push_constants_.data() + offset, &value, sizeof(float));
}
void VulkanRhiDevice::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
    int data[2] = { x, y };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
    float data[2] = { x, y };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) {
    float data[3] = { x, y, z };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) {
    int data[3] = { x, y, z };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
    float data[4] = { x, y, z, w };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    size_t offset = GetOrCreateUniformOffset(shader, name, 64);
    EnsurePushConstantCapacity(compute_push_constants_, offset, 64);
    memcpy(compute_push_constants_.data() + offset, data, 64);
}
unsigned int VulkanRhiDevice::CreateComputeShaderEx(
    const std::string& /*gl_src*/, const std::string& vk_src, const std::string& /*hlsl_src*/,
    uint32_t ssbo_count, uint32_t storage_image_count, uint32_t sampler_count,
    uint32_t push_constant_bytes) {
    if (!initialized_) return 0u;
    if (ssbo_count == 0 && storage_image_count == 0 && sampler_count == 0)
        return shader_mgr_.CreateComputeProgramSSBO(vk_src, 0, push_constant_bytes);
    return shader_mgr_.CreateComputeProgramFull(
        vk_src, ssbo_count, storage_image_count, sampler_count, push_constant_bytes);
}

unsigned int VulkanRhiDevice::CreateComputeWriteTexture2D(int width, int height) {
    if (!initialized_) return 0;
    return resource_mgr_.CreateComputeWriteTexture2D(width, height);
}

void VulkanRhiDevice::ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
    if (!initialized_ || !dst || size == 0) return;
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

VertexArrayHandle VulkanRhiDevice::CreateVertexArray() {
    // Vulkan 不需要 VAO 概念，顶点格式在 VkPipeline 创建时指定
    // 返回占位句柄以兼容 RhiDevice 接口
    static unsigned int vao_counter = 600000;
    return VertexArrayHandle{vao_counter++};
}

void VulkanRhiDevice::DeleteVertexArray(VertexArrayHandle handle) {
    // Vulkan 不需要 VAO 概念，no-op
    (void)handle;
}

std::shared_ptr<CommandBuffer> VulkanRhiDevice::CreateCommandBuffer() {
    auto cmd = std::make_shared<VulkanCommandBuffer>();
    cmd->SetDevice(this);

    // 从命令缓冲池获取（避免逐帧 vkAllocateCommandBuffers 开销）
    VkCommandBuffer vk_cmd = resource_mgr_.AcquireCommandBuffer();
    cmd->SetVkCommandBuffer(vk_cmd);

    // 立即开始录制
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
    VkResult present_result = VK_SUCCESS;
    // 保存本帧命令缓冲列表，用于提交后归还到池
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
    // 归还本帧命令缓冲到池（AdvanceFrame 已通过 fence 保证 GPU 完成）
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

    // GPU Timestamp Query: 收集上一帧结果
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
    // Vulkan 无需显式解绑
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
    // 同步 bound_ssbos_ 到 draw executor（GPU-driven 路径不走 DrawMeshBatch，需手动同步）
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

// --- 编辑器场景视图模式 ---

void VulkanRhiDevice::SetWireframeMode(bool enable) {
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

