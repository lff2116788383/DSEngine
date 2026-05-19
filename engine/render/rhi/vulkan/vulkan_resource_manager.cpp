/**
 * @file vulkan_resource_manager.cpp
 * @brief VulkanResourceManager 实现 — Vulkan GPU 资源的创建/销毁/更新
 */

#include "engine/render/rhi/vulkan/vulkan_resource_manager.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/base/debug.h"

#include <cstring>

namespace dse {
namespace render {

bool VulkanResourceManager::Init(VulkanContext* context) {
    context_ = context;
    device_ = context->device();

    // 创建命令池
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = context->queue_families().graphics.value();

    if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create command pool");
        return false;
    }

    // 创建 Descriptor Pool
    if (!CreateDescriptorPool()) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create descriptor pool");
        return false;
    }

    // 创建默认线性采样器（Compute Shader 使用）
    VkSamplerCreateInfo sampler_ci{};
    sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.magFilter = VK_FILTER_LINEAR;
    sampler_ci.minFilter = VK_FILTER_LINEAR;
    sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_ci.minLod = 0.0f;
    sampler_ci.maxLod = VK_LOD_CLAMP_NONE;
    sampler_ci.anisotropyEnable = VK_FALSE;
    sampler_ci.maxAnisotropy = 1.0f;
    sampler_ci.compareEnable = VK_FALSE;
    sampler_ci.compareOp = VK_COMPARE_OP_ALWAYS;
    if (vkCreateSampler(device_, &sampler_ci, nullptr, &default_sampler_) != VK_SUCCESS) {
        DEBUG_LOG_WARN("[Vulkan] Failed to create default sampler");
    }

    // 创建阴影比较采样器（sampler2DShadow PCF 要求 compareEnable=VK_TRUE）
    VkSamplerCreateInfo shadow_sampler_ci{};
    shadow_sampler_ci.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    shadow_sampler_ci.magFilter     = VK_FILTER_LINEAR;
    shadow_sampler_ci.minFilter     = VK_FILTER_LINEAR;
    shadow_sampler_ci.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadow_sampler_ci.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadow_sampler_ci.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadow_sampler_ci.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadow_sampler_ci.minLod        = 0.0f;
    shadow_sampler_ci.maxLod        = VK_LOD_CLAMP_NONE;
    shadow_sampler_ci.anisotropyEnable = VK_FALSE;
    shadow_sampler_ci.maxAnisotropy = 1.0f;
    shadow_sampler_ci.compareEnable = VK_TRUE;
    shadow_sampler_ci.compareOp     = VK_COMPARE_OP_LESS_OR_EQUAL;
    shadow_sampler_ci.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    if (vkCreateSampler(device_, &shadow_sampler_ci, nullptr, &shadow_comparison_sampler_) != VK_SUCCESS) {
        DEBUG_LOG_WARN("[Vulkan] Failed to create shadow comparison sampler");
    }

    initialized_ = true;
    DEBUG_LOG_INFO("[Vulkan] ResourceManager initialized");
    return true;
}

void VulkanResourceManager::Shutdown() {
    if (!initialized_) return;

    vkDeviceWaitIdle(device_);

    // 销毁所有渲染目标
    for (auto& [handle, rt] : render_targets_) {
        if (rt.framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, rt.framebuffer, nullptr);
        if (rt.render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(device_, rt.render_pass, nullptr);
        if (rt.is_msaa) {
            if (rt.msaa_color_texture.image_view != VK_NULL_HANDLE) vkDestroyImageView(device_, rt.msaa_color_texture.image_view, nullptr);
            if (rt.msaa_color_texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, rt.msaa_color_texture.image, nullptr);
            if (rt.msaa_color_texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, rt.msaa_color_texture.memory, nullptr);
        }
        if (rt.color_texture.image_view != VK_NULL_HANDLE) vkDestroyImageView(device_, rt.color_texture.image_view, nullptr);
        if (rt.color_texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, rt.color_texture.image, nullptr);
        if (rt.color_texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, rt.color_texture.memory, nullptr);
        if (rt.depth_texture.image_view != VK_NULL_HANDLE) vkDestroyImageView(device_, rt.depth_texture.image_view, nullptr);
        if (rt.depth_texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, rt.depth_texture.image, nullptr);
        if (rt.depth_texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, rt.depth_texture.memory, nullptr);
    }
    render_targets_.clear();

    // 销毁所有纹理
    for (auto& [handle, tex] : textures_) {
        if (tex.image_view != VK_NULL_HANDLE) vkDestroyImageView(device_, tex.image_view, nullptr);
        if (tex.image != VK_NULL_HANDLE) vkDestroyImage(device_, tex.image, nullptr);
        if (tex.memory != VK_NULL_HANDLE) vkFreeMemory(device_, tex.memory, nullptr);
    }
    textures_.clear();

    // 销毁所有缓冲
    for (auto& [handle, buf] : buffers_) {
        if (buf.mapped) vkUnmapMemory(device_, buf.memory);
        if (buf.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, buf.buffer, nullptr);
        if (buf.memory != VK_NULL_HANDLE) vkFreeMemory(device_, buf.memory, nullptr);
    }
    buffers_.clear();

    // 销毁所有 SSBO
    for (auto& [handle, buf] : ssbos_) {
        if (buf.mapped) vkUnmapMemory(device_, buf.memory);
        if (buf.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, buf.buffer, nullptr);
        if (buf.memory != VK_NULL_HANDLE) vkFreeMemory(device_, buf.memory, nullptr);
    }
    ssbos_.clear();

    if (default_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, default_sampler_, nullptr);
        default_sampler_ = VK_NULL_HANDLE;
    }

    if (shadow_comparison_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, shadow_comparison_sampler_, nullptr);
        shadow_comparison_sampler_ = VK_NULL_HANDLE;
    }

    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }

    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

    initialized_ = false;
    DEBUG_LOG_INFO("[Vulkan] ResourceManager shutdown");
}

// ============================================================
// 命令缓冲
// ============================================================

VkCommandBuffer VulkanResourceManager::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool_;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void VulkanResourceManager::EndSingleTimeCommands(VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(context_->graphics_queue(), 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_->graphics_queue());

    vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
}

// ============================================================
// 纹理
// ============================================================

unsigned int VulkanResourceManager::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    unsigned int handle = AllocateTextureHandle();
    VulkanTexture tex;
    tex.width = width;
    tex.height = height;
    tex.channels = 4;
    tex.format = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (!CreateVulkanImage(width, height, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_TILING_OPTIMAL, usage,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                           VK_IMAGE_ASPECT_COLOR_BIT, tex)) {
        return 0;
    }

    if (rgba8_data) {
        size_t data_size = width * height * 4;
        UploadTextureData(tex, rgba8_data, data_size);
    }

    textures_[handle] = tex;
    return handle;
}

unsigned int VulkanResourceManager::CreateComputeWriteTexture2D(int width, int height) {
    unsigned int handle = AllocateTextureHandle();
    VulkanTexture tex;
    tex.width    = width;
    tex.height   = height;
    tex.channels = 4;
    tex.format   = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (!CreateVulkanImage(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
                           VK_IMAGE_TILING_OPTIMAL, usage,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                           VK_IMAGE_ASPECT_COLOR_BIT, tex)) {
        return 0;
    }

    // 初始化为 VK_IMAGE_LAYOUT_GENERAL（compute shader storage image 需要此 layout）
    TransitionImageLayout(tex.image, VK_FORMAT_R16G16B16A16_SFLOAT,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // 创建默认采样器（用于后续在 PBR pass 中采样）
    VkSamplerCreateInfo samp_ci{};
    samp_ci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samp_ci.magFilter    = VK_FILTER_LINEAR;
    samp_ci.minFilter    = VK_FILTER_LINEAR;
    samp_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.maxLod       = 0.0f;
    vkCreateSampler(device_, &samp_ci, nullptr, &tex.sampler);

    textures_[handle] = tex;
    DEBUG_LOG_INFO("[Vulkan] Compute write texture created: handle={} {}x{}", handle, width, height);
    return handle;
}

unsigned int VulkanResourceManager::CreateCompressedTexture2D(CompressedTextureFormat format,
                                                                const std::vector<CompressedMipLevel>& mips,
                                                                bool linear_filter) {
    if (mips.empty()) return 0;

    VkFormat vk_fmt = VK_FORMAT_UNDEFINED;
    switch (format) {
        case CompressedTextureFormat::BC1_UNORM: vk_fmt = VK_FORMAT_BC1_RGBA_UNORM_BLOCK; break;
        case CompressedTextureFormat::BC1_SRGB:  vk_fmt = VK_FORMAT_BC1_RGBA_SRGB_BLOCK; break;
        case CompressedTextureFormat::BC2_UNORM: vk_fmt = VK_FORMAT_BC2_UNORM_BLOCK; break;
        case CompressedTextureFormat::BC3_UNORM: vk_fmt = VK_FORMAT_BC3_UNORM_BLOCK; break;
        case CompressedTextureFormat::BC3_SRGB:  vk_fmt = VK_FORMAT_BC3_SRGB_BLOCK; break;
        case CompressedTextureFormat::BC4_UNORM: vk_fmt = VK_FORMAT_BC4_UNORM_BLOCK; break;
        case CompressedTextureFormat::BC5_UNORM: vk_fmt = VK_FORMAT_BC5_UNORM_BLOCK; break;
        case CompressedTextureFormat::BC7_UNORM: vk_fmt = VK_FORMAT_BC7_UNORM_BLOCK; break;
        case CompressedTextureFormat::BC7_SRGB:  vk_fmt = VK_FORMAT_BC7_SRGB_BLOCK; break;
        default: return 0;
    }

    unsigned int handle = AllocateTextureHandle();
    VulkanTexture tex;
    tex.width = mips[0].width;
    tex.height = mips[0].height;
    tex.channels = 4;
    tex.format = vk_fmt;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = vk_fmt;
    image_info.extent.width = static_cast<uint32_t>(mips[0].width);
    image_info.extent.height = static_cast<uint32_t>(mips[0].height);
    image_info.extent.depth = 1;
    image_info.mipLevels = static_cast<uint32_t>(mips.size());
    image_info.arrayLayers = 1;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device_, &image_info, nullptr, &tex.image) != VK_SUCCESS) return 0;

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device_, tex.image, &mem_reqs);
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &alloc_info, nullptr, &tex.memory) != VK_SUCCESS) return 0;
    vkBindImageMemory(device_, tex.image, tex.memory, 0);

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = tex.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = vk_fmt;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = static_cast<uint32_t>(mips.size());
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device_, &view_info, nullptr, &tex.image_view) != VK_SUCCESS) return 0;

    size_t total_size = 0;
    for (auto& m : mips) total_size += m.size;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = total_size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device_, &buf_info, nullptr, &staging_buffer);
    VkMemoryRequirements staging_reqs;
    vkGetBufferMemoryRequirements(device_, staging_buffer, &staging_reqs);
    VkMemoryAllocateInfo staging_alloc{};
    staging_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    staging_alloc.allocationSize = staging_reqs.size;
    staging_alloc.memoryTypeIndex = FindMemoryType(staging_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device_, &staging_alloc, nullptr, &staging_memory);
    vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);

    void* mapped;
    vkMapMemory(device_, staging_memory, 0, total_size, 0, &mapped);
    size_t offset = 0;
    for (auto& m : mips) {
        memcpy(static_cast<uint8_t*>(mapped) + offset, m.data, m.size);
        offset += m.size;
    }
    vkUnmapMemory(device_, staging_memory);

    VkCommandBuffer cmd = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = static_cast<uint32_t>(mips.size());
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    std::vector<VkBufferImageCopy> regions(mips.size());
    offset = 0;
    for (size_t i = 0; i < mips.size(); ++i) {
        regions[i] = {};
        regions[i].bufferOffset = offset;
        regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel = static_cast<uint32_t>(i);
        regions[i].imageSubresource.baseArrayLayer = 0;
        regions[i].imageSubresource.layerCount = 1;
        regions[i].imageExtent = {static_cast<uint32_t>(mips[i].width), static_cast<uint32_t>(mips[i].height), 1};
        offset += mips[i].size;
    }
    vkCmdCopyBufferToImage(cmd, staging_buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(regions.size()), regions.data());

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    EndSingleTimeCommands(cmd);
    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_memory, nullptr);

    textures_[handle] = tex;
    return handle;
}

unsigned int VulkanResourceManager::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    unsigned int handle = AllocateTextureHandle();
    VulkanTexture tex;
    tex.width = width;
    tex.height = height;
    tex.channels = 4;
    tex.format = VK_FORMAT_R8G8B8A8_UNORM;

    // 创建立方体纹理图像
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 6;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device_, &image_info, nullptr, &tex.image) != VK_SUCCESS) return 0;

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device_, tex.image, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &tex.memory) != VK_SUCCESS) return 0;
    vkBindImageMemory(device_, tex.image, tex.memory, 0);

    // 上传 6 面
    if (rgba8_faces) {
        size_t face_size = width * height * 4;
        size_t total_size = face_size * 6;

        // 创建 staging buffer
        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        VkBufferCreateInfo buf_info{};
        buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_info.size = total_size;
        buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device_, &buf_info, nullptr, &staging_buffer);
        VkMemoryRequirements staging_reqs;
        vkGetBufferMemoryRequirements(device_, staging_buffer, &staging_reqs);
        VkMemoryAllocateInfo staging_alloc{};
        staging_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        staging_alloc.allocationSize = staging_reqs.size;
        staging_alloc.memoryTypeIndex = FindMemoryType(staging_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &staging_alloc, nullptr, &staging_memory);
        vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);

        void* mapped;
        vkMapMemory(device_, staging_memory, 0, total_size, 0, &mapped);
        for (int face = 0; face < 6; ++face) {
            memcpy(static_cast<unsigned char*>(mapped) + face * face_size, rgba8_faces[face], face_size);
        }
        vkUnmapMemory(device_, staging_memory);

        // 拷贝到立方体纹理
        VkCommandBuffer cmd = BeginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = tex.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 6;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        std::vector<VkBufferImageCopy> regions(6);
        for (int face = 0; face < 6; ++face) {
            regions[face].bufferOffset = face * face_size;
            regions[face].bufferRowLength = 0;
            regions[face].bufferImageHeight = 0;
            regions[face].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            regions[face].imageSubresource.mipLevel = 0;
            regions[face].imageSubresource.baseArrayLayer = face;
            regions[face].imageSubresource.layerCount = 1;
            regions[face].imageOffset = {0, 0, 0};
            regions[face].imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
        }
        vkCmdCopyBufferToImage(cmd, staging_buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions.data());

        // 转到 shader 只读布局
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndSingleTimeCommands(cmd);

        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
    }

    // 创建 ImageView
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = tex.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 6;

    if (vkCreateImageView(device_, &view_info, nullptr, &tex.image_view) != VK_SUCCESS) return 0;

    textures_[handle] = tex;
    return handle;
}

unsigned int VulkanResourceManager::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    if (width <= 0 || height <= 0 || depth <= 0) return 0;

    unsigned int handle = AllocateTextureHandle();
    VulkanTexture tex;
    tex.width    = width;
    tex.height   = height;
    tex.depth    = depth;
    tex.channels = 4;
    tex.format   = VK_FORMAT_R8G8B8A8_UNORM;
    tex.is_3d    = true;

    // --- 创建 VkImage (3D) ---
    VkImageCreateInfo image_info{};
    image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType     = VK_IMAGE_TYPE_3D;
    image_info.format        = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width  = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth  = static_cast<uint32_t>(depth);
    image_info.mipLevels     = 1;
    image_info.arrayLayers   = 1;
    image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples       = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device_, &image_info, nullptr, &tex.image) != VK_SUCCESS) return 0;

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device_, tex.image, &mem_reqs);
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize  = mem_reqs.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &alloc_info, nullptr, &tex.memory) != VK_SUCCESS) return 0;
    vkBindImageMemory(device_, tex.image, tex.memory, 0);

    // --- 上传数据 ---
    if (rgba8_data) {
        size_t data_size = static_cast<size_t>(width) * height * depth * 4;

        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        VkBufferCreateInfo buf_info{};
        buf_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_info.size        = data_size;
        buf_info.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device_, &buf_info, nullptr, &staging_buffer);

        VkMemoryRequirements staging_reqs;
        vkGetBufferMemoryRequirements(device_, staging_buffer, &staging_reqs);
        VkMemoryAllocateInfo staging_alloc{};
        staging_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        staging_alloc.allocationSize  = staging_reqs.size;
        staging_alloc.memoryTypeIndex = FindMemoryType(staging_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &staging_alloc, nullptr, &staging_memory);
        vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);

        void* mapped;
        vkMapMemory(device_, staging_memory, 0, data_size, 0, &mapped);
        memcpy(mapped, rgba8_data, data_size);
        vkUnmapMemory(device_, staging_memory);

        VkCommandBuffer cmd = BeginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = tex.image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.bufferOffset                    = 0;
        region.bufferRowLength                 = 0;
        region.bufferImageHeight               = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = {0, 0, 0};
        region.imageExtent                     = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth)};
        vkCmdCopyBufferToImage(cmd, staging_buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndSingleTimeCommands(cmd);
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
    }

    // --- ImageView (3D) ---
    VkImageViewCreateInfo view_info{};
    view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image                           = tex.image;
    view_info.viewType                        = VK_IMAGE_VIEW_TYPE_3D;
    view_info.format                          = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.components                      = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel   = 0;
    view_info.subresourceRange.levelCount     = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount     = 1;
    if (vkCreateImageView(device_, &view_info, nullptr, &tex.image_view) != VK_SUCCESS) return 0;

    // --- Sampler ---
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter    = linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    sampler_info.minFilter    = linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxLod       = 0.0f;
    if (vkCreateSampler(device_, &sampler_info, nullptr, &tex.sampler) != VK_SUCCESS) return 0;

    textures_[handle] = tex;
    return handle;
}

void VulkanResourceManager::DeleteTexture(unsigned int handle) {
    auto it = textures_.find(handle);
    if (it == textures_.end()) return;

    auto& tex = it->second;
    if (tex.sampler != VK_NULL_HANDLE) vkDestroySampler(device_, tex.sampler, nullptr);
    if (tex.image_view != VK_NULL_HANDLE) vkDestroyImageView(device_, tex.image_view, nullptr);
    if (tex.image != VK_NULL_HANDLE) vkDestroyImage(device_, tex.image, nullptr);
    if (tex.memory != VK_NULL_HANDLE) vkFreeMemory(device_, tex.memory, nullptr);

    textures_.erase(it);
}

const VulkanTexture* VulkanResourceManager::GetTexture(unsigned int handle) const {
    auto it = textures_.find(handle);
    return it != textures_.end() ? &it->second : nullptr;
}

// ============================================================
// 缓冲区
// ============================================================

unsigned int VulkanResourceManager::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    unsigned int handle = next_buffer_handle_++;
    VulkanBuffer buf;
    buf.size = size;
    buf.is_dynamic = is_dynamic;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = is_index ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &buffer_info, nullptr, &buf.buffer) != VK_SUCCESS) return 0;

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device_, buf.buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;

    if (is_dynamic) {
        // 动态缓冲：HOST_VISIBLE + HOST_COHERENT，持久映射
        alloc_info.memoryTypeIndex = FindMemoryType(mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    } else {
        // 静态缓冲：DEVICE_LOCAL，通过 staging 上传
        alloc_info.memoryTypeIndex = FindMemoryType(mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &buf.memory) != VK_SUCCESS) return 0;
    vkBindBufferMemory(device_, buf.buffer, buf.memory, 0);

    if (is_dynamic) {
        vkMapMemory(device_, buf.memory, 0, size, 0, &buf.mapped);
    }

    // 上传初始数据
    if (data) {
        if (is_dynamic && buf.mapped) {
            memcpy(buf.mapped, data, size);
        } else {
            // staging buffer 上传
            VkBuffer staging_buffer;
            VkDeviceMemory staging_memory;
            VkBufferCreateInfo staging_info{};
            staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            staging_info.size = size;
            staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staging_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            vkCreateBuffer(device_, &staging_info, nullptr, &staging_buffer);
            VkMemoryRequirements staging_reqs;
            vkGetBufferMemoryRequirements(device_, staging_buffer, &staging_reqs);
            VkMemoryAllocateInfo staging_alloc{};
            staging_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            staging_alloc.allocationSize = staging_reqs.size;
            staging_alloc.memoryTypeIndex = FindMemoryType(staging_reqs.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkAllocateMemory(device_, &staging_alloc, nullptr, &staging_memory);
            vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);

            void* mapped;
            vkMapMemory(device_, staging_memory, 0, size, 0, &mapped);
            memcpy(mapped, data, size);
            vkUnmapMemory(device_, staging_memory);

            VkCommandBuffer cmd = BeginSingleTimeCommands();
            VkBufferCopy copy_region{};
            copy_region.size = size;
            vkCmdCopyBuffer(cmd, staging_buffer, buf.buffer, 1, &copy_region);
            EndSingleTimeCommands(cmd);

            vkDestroyBuffer(device_, staging_buffer, nullptr);
            vkFreeMemory(device_, staging_memory, nullptr);
        }
    }

    buffers_[handle] = buf;
    return handle;
}

void VulkanResourceManager::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data) {
    auto it = buffers_.find(handle);
    if (it == buffers_.end()) return;

    auto& buf = it->second;
    if (buf.is_dynamic && buf.mapped) {
        memcpy(static_cast<unsigned char*>(buf.mapped) + offset, data, size);
    } else {
        // 非动态缓冲：staging 上传
        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        VkBufferCreateInfo staging_info{};
        staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_info.size = size;
        staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device_, &staging_info, nullptr, &staging_buffer);
        VkMemoryRequirements staging_reqs;
        vkGetBufferMemoryRequirements(device_, staging_buffer, &staging_reqs);
        VkMemoryAllocateInfo staging_alloc{};
        staging_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        staging_alloc.allocationSize = staging_reqs.size;
        staging_alloc.memoryTypeIndex = FindMemoryType(staging_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &staging_alloc, nullptr, &staging_memory);
        vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);

        void* mapped;
        vkMapMemory(device_, staging_memory, 0, size, 0, &mapped);
        memcpy(mapped, data, size);
        vkUnmapMemory(device_, staging_memory);

        VkCommandBuffer cmd = BeginSingleTimeCommands();
        VkBufferCopy copy_region{};
        copy_region.srcOffset = 0;
        copy_region.dstOffset = offset;
        copy_region.size = size;
        vkCmdCopyBuffer(cmd, staging_buffer, buf.buffer, 1, &copy_region);
        EndSingleTimeCommands(cmd);

        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
    }
}

void VulkanResourceManager::DeleteBuffer(unsigned int handle) {
    auto it = buffers_.find(handle);
    if (it == buffers_.end()) return;

    auto& buf = it->second;
    if (buf.mapped) vkUnmapMemory(device_, buf.memory);
    if (buf.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, buf.buffer, nullptr);
    if (buf.memory != VK_NULL_HANDLE) vkFreeMemory(device_, buf.memory, nullptr);

    buffers_.erase(it);
}

// ============================================================
// SSBO (Storage Buffer)
// ============================================================

unsigned int VulkanResourceManager::CreateSSBO(size_t size, const void* data) {
    unsigned int handle = next_ssbo_handle_++;
    VulkanBuffer buf;
    buf.size = size;
    buf.is_dynamic = true;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &buffer_info, nullptr, &buf.buffer) != VK_SUCCESS) return 0;

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device_, buf.buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &buf.memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buf.buffer, nullptr);
        return 0;
    }
    vkBindBufferMemory(device_, buf.buffer, buf.memory, 0);
    vkMapMemory(device_, buf.memory, 0, size, 0, &buf.mapped);

    if (data && buf.mapped) {
        memcpy(buf.mapped, data, size);
    }

    ssbos_[handle] = buf;
    return handle;
}

void VulkanResourceManager::UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) {
    auto it = ssbos_.find(handle);
    if (it == ssbos_.end()) return;
    auto& buf = it->second;
    if (buf.mapped) {
        memcpy(static_cast<unsigned char*>(buf.mapped) + offset, data, size);
    }
}

void VulkanResourceManager::DeleteSSBO(unsigned int handle) {
    auto it = ssbos_.find(handle);
    if (it == ssbos_.end()) return;
    auto& buf = it->second;
    if (buf.mapped) vkUnmapMemory(device_, buf.memory);
    if (buf.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, buf.buffer, nullptr);
    if (buf.memory != VK_NULL_HANDLE) vkFreeMemory(device_, buf.memory, nullptr);
    ssbos_.erase(it);
}

const VulkanBuffer* VulkanResourceManager::GetSSBO(unsigned int handle) const {
    auto it = ssbos_.find(handle);
    return it != ssbos_.end() ? &it->second : nullptr;
}

// ============================================================
// 渲染目标
// ============================================================

unsigned int VulkanResourceManager::CreateRenderTarget(int width, int height, bool has_color, bool has_depth,
                                                        bool generate_mipmaps, bool cube_map,
                                                        int msaa_samples, bool allow_uav,
                                                        int color_attachment_count) {
    unsigned int handle = AllocateRenderTargetHandle();
    VulkanRenderTarget rt;
    rt.width = width;
    rt.height = height;
    rt.has_color = has_color;
    rt.has_depth = has_depth;
    rt.generate_mipmaps = generate_mipmaps;
    rt.allow_uav = allow_uav;

    const int num_color = has_color ? (std::max)(1, color_attachment_count) : 0;
    rt.color_attachment_count = num_color;

    // 查询设备支持的 MSAA 采样数
    VkSampleCountFlagBits actual_samples = VK_SAMPLE_COUNT_1_BIT;
    if (msaa_samples >= 4) {
        VkPhysicalDeviceProperties phys_props;
        vkGetPhysicalDeviceProperties(context_->physical_device(), &phys_props);
        VkSampleCountFlags supported = phys_props.limits.framebufferColorSampleCounts
                                     & phys_props.limits.framebufferDepthSampleCounts;
        if (supported & VK_SAMPLE_COUNT_4_BIT) actual_samples = VK_SAMPLE_COUNT_4_BIT;
    }
    // MRT (>1 attachment) 禁用 MSAA（deferred GBuffer 不用 MSAA）
    if (num_color > 1) actual_samples = VK_SAMPLE_COUNT_1_BIT;
    const bool use_msaa = (actual_samples != VK_SAMPLE_COUNT_1_BIT);
    rt.is_msaa = use_msaa;
    rt.msaa_samples = use_msaa ? 4 : 1;

    const VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;

    // ---- 颜色附件 ----
    if (has_color) {
        VkImageUsageFlags color_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (allow_uav)        color_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (generate_mipmaps) color_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (use_msaa) {
            // MSAA 附件（仅用于渲染，不需要采样）
            rt.msaa_color_texture.format = color_format;
            rt.msaa_color_texture.width  = width;
            rt.msaa_color_texture.height = height;
            if (!CreateVulkanImage(width, height, color_format, VK_IMAGE_TILING_OPTIMAL,
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   VK_IMAGE_ASPECT_COLOR_BIT, rt.msaa_color_texture,
                                   actual_samples)) return 0;

            // Resolve 目标（1x，Shader 可读和可写）
            rt.color_texture.format = color_format;
            rt.color_texture.width  = width;
            rt.color_texture.height = height;
            if (!CreateVulkanImage(width, height, color_format, VK_IMAGE_TILING_OPTIMAL,
                                   color_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   VK_IMAGE_ASPECT_COLOR_BIT, rt.color_texture)) return 0;
            rt.color_textures.push_back(rt.color_texture);
        } else {
            rt.color_textures.resize(static_cast<size_t>(num_color));
            for (int ci = 0; ci < num_color; ++ci) {
                rt.color_textures[ci].format = color_format;
                rt.color_textures[ci].width  = width;
                rt.color_textures[ci].height = height;
                if (!CreateVulkanImage(width, height, color_format, VK_IMAGE_TILING_OPTIMAL,
                                       color_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       VK_IMAGE_ASPECT_COLOR_BIT, rt.color_textures[ci])) return 0;
            }
            rt.color_texture = rt.color_textures[0];
            // MRT: 为每个颜色附件注册独立纹理 handle，供 GetRenderTargetColorTexture(handle, index) 查询
            if (num_color > 1) {
                rt.mrt_texture_handles.resize(static_cast<size_t>(num_color));
                for (int ci = 0; ci < num_color; ++ci) {
                    unsigned int tex_h = next_texture_handle_++;
                    textures_[tex_h] = rt.color_textures[ci];
                    rt.mrt_texture_handles[ci] = tex_h;
                }
            }
        }
    }

    // ---- 深度附件（MSAA 时同样需要 4x） ----
    if (has_depth) {
        const VkFormat depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
        rt.depth_texture.format = depth_format;
        rt.depth_texture.width  = width;
        rt.depth_texture.height = height;
        if (!CreateVulkanImage(width, height, depth_format, VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               VK_IMAGE_ASPECT_DEPTH_BIT, rt.depth_texture,
                               actual_samples)) return 0;
    }

    // ---- RenderPass ----
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> color_refs;
    VkAttachmentReference depth_ref{};
    std::vector<VkAttachmentReference> resolve_refs;

    if (has_color) {
        if (use_msaa) {
            VkAttachmentDescription msaa_att{};
            msaa_att.format         = color_format;
            msaa_att.samples        = actual_samples;
            msaa_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            msaa_att.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            msaa_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            msaa_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            msaa_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            msaa_att.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.push_back(msaa_att);
            color_refs.push_back({static_cast<uint32_t>(attachments.size() - 1),
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        } else {
            for (int ci = 0; ci < num_color; ++ci) {
                VkAttachmentDescription color_att{};
                color_att.format         = color_format;
                color_att.samples        = VK_SAMPLE_COUNT_1_BIT;
                color_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
                color_att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
                color_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                color_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
                color_att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                attachments.push_back(color_att);
                color_refs.push_back({static_cast<uint32_t>(attachments.size() - 1),
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
            }
        }
    }

    if (has_depth) {
        VkAttachmentDescription depth_att{};
        depth_att.format         = rt.depth_texture.format;
        depth_att.samples        = actual_samples;
        depth_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        depth_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_att.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        attachments.push_back(depth_att);
        depth_ref = {static_cast<uint32_t>(attachments.size() - 1),
                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    }

    if (has_color && use_msaa) {
        VkAttachmentDescription resolve_att{};
        resolve_att.format         = color_format;
        resolve_att.samples        = VK_SAMPLE_COUNT_1_BIT;
        resolve_att.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve_att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        resolve_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolve_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        resolve_att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        resolve_refs.push_back({static_cast<uint32_t>(attachments.size()),
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        attachments.push_back(resolve_att);
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = static_cast<uint32_t>(color_refs.size());
    subpass.pColorAttachments       = color_refs.empty() ? nullptr : color_refs.data();
    subpass.pDepthStencilAttachment = has_depth ? &depth_ref : nullptr;
    subpass.pResolveAttachments     = resolve_refs.empty() ? nullptr : resolve_refs.data();

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                      | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                      | VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rp_info{};
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    rp_info.pAttachments    = attachments.data();
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies   = &dep;

    if (vkCreateRenderPass(device_, &rp_info, nullptr, &rt.render_pass) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create render pass for render target");
        return 0;
    }

    // ---- Framebuffer ----
    std::vector<VkImageView> fb_attachments;
    if (has_color) {
        if (use_msaa) {
            fb_attachments.push_back(rt.msaa_color_texture.image_view);
        } else {
            for (int ci = 0; ci < num_color; ++ci)
                fb_attachments.push_back(rt.color_textures[ci].image_view);
        }
    }
    if (has_depth) fb_attachments.push_back(rt.depth_texture.image_view);
    if (has_color && use_msaa) fb_attachments.push_back(rt.color_texture.image_view);

    VkFramebufferCreateInfo fb_info{};
    fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass      = rt.render_pass;
    fb_info.attachmentCount = static_cast<uint32_t>(fb_attachments.size());
    fb_info.pAttachments    = fb_attachments.data();
    fb_info.width           = static_cast<uint32_t>(width);
    fb_info.height          = static_cast<uint32_t>(height);
    fb_info.layers          = 1;

    if (vkCreateFramebuffer(device_, &fb_info, nullptr, &rt.framebuffer) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create framebuffer for render target");
        return 0;
    }

    render_targets_[handle] = rt;
    DEBUG_LOG_INFO("[Vulkan] RenderTarget created: {}x{} color_count={} msaa={} uav={} handle={}",
                   width, height, num_color, rt.msaa_samples, allow_uav ? 1 : 0, handle);
    return handle;
}

void VulkanResourceManager::DeleteRenderTarget(unsigned int handle) {
    auto it = render_targets_.find(handle);
    if (it == render_targets_.end()) return;

    auto& rt = it->second;
    if (rt.framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, rt.framebuffer, nullptr);
    if (rt.render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(device_, rt.render_pass, nullptr);
    if (rt.is_msaa) {
        if (rt.msaa_color_texture.image_view != VK_NULL_HANDLE) vkDestroyImageView(device_, rt.msaa_color_texture.image_view, nullptr);
        if (rt.msaa_color_texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, rt.msaa_color_texture.image, nullptr);
        if (rt.msaa_color_texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, rt.msaa_color_texture.memory, nullptr);
    }
    for (auto& ct : rt.color_textures) {
        if (ct.image_view != VK_NULL_HANDLE) vkDestroyImageView(device_, ct.image_view, nullptr);
        if (ct.image != VK_NULL_HANDLE) vkDestroyImage(device_, ct.image, nullptr);
        if (ct.memory != VK_NULL_HANDLE) vkFreeMemory(device_, ct.memory, nullptr);
    }
    if (rt.depth_texture.image_view != VK_NULL_HANDLE) vkDestroyImageView(device_, rt.depth_texture.image_view, nullptr);
    if (rt.depth_texture.image != VK_NULL_HANDLE) vkDestroyImage(device_, rt.depth_texture.image, nullptr);
    if (rt.depth_texture.memory != VK_NULL_HANDLE) vkFreeMemory(device_, rt.depth_texture.memory, nullptr);

    render_targets_.erase(it);
}

const VulkanRenderTarget* VulkanResourceManager::GetRenderTarget(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? &it->second : nullptr;
}

VkImageView VulkanResourceManager::GetRenderTargetColorImageView(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? it->second.color_texture.image_view : VK_NULL_HANDLE;
}

VkImageView VulkanResourceManager::GetRenderTargetDepthImageView(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? it->second.depth_texture.image_view : VK_NULL_HANDLE;
}

// ============================================================
// 句柄生成
// ============================================================

unsigned int VulkanResourceManager::AllocateTextureHandle() { return next_texture_handle_++; }
unsigned int VulkanResourceManager::AllocateRenderTargetHandle() { return next_render_target_handle_++; }

// ============================================================
// 内部工具
// ============================================================

bool VulkanResourceManager::CreateVulkanImage(int width, int height, VkFormat format, VkImageTiling tiling,
                                               VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                               VkImageAspectFlags aspect_mask, VulkanTexture& out_texture,
                                               VkSampleCountFlagBits sample_count) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent.width = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = sample_count;

    if (vkCreateImage(device_, &image_info, nullptr, &out_texture.image) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create image");
        return false;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device_, out_texture.image, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_reqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &out_texture.memory) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to allocate image memory");
        return false;
    }
    vkBindImageMemory(device_, out_texture.image, out_texture.memory, 0);

    // ImageView
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = out_texture.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    view_info.subresourceRange.aspectMask = aspect_mask;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device_, &view_info, nullptr, &out_texture.image_view) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create image view");
        return false;
    }

    return true;
}

void VulkanResourceManager::UploadTextureData(VulkanTexture& texture, const void* data, size_t data_size) {
    // staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = data_size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device_, &buf_info, nullptr, &staging_buffer);
    VkMemoryRequirements staging_reqs;
    vkGetBufferMemoryRequirements(device_, staging_buffer, &staging_reqs);
    VkMemoryAllocateInfo staging_alloc{};
    staging_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    staging_alloc.allocationSize = staging_reqs.size;
    staging_alloc.memoryTypeIndex = FindMemoryType(staging_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device_, &staging_alloc, nullptr, &staging_memory);
    vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);

    void* mapped;
    vkMapMemory(device_, staging_memory, 0, data_size, 0, &mapped);
    memcpy(mapped, data, data_size);
    vkUnmapMemory(device_, staging_memory);

    // 拷贝
    VkCommandBuffer cmd = BeginSingleTimeCommands();

    TransitionImageLayout(texture.image, texture.format,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(texture.width), static_cast<uint32_t>(texture.height), 1};

    vkCmdCopyBufferToImage(cmd, staging_buffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    TransitionImageLayout(texture.image, texture.format,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EndSingleTimeCommands(cmd);

    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_memory, nullptr);
}

void VulkanResourceManager::TransitionImageLayout(VkImage image, VkFormat format,
                                                    VkImageLayout old_layout, VkImageLayout new_layout) {
    VkCommandBuffer cmd = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    EndSingleTimeCommands(cmd);
}

uint32_t VulkanResourceManager::FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(context_->physical_device(), &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    DEBUG_LOG_ERROR("[Vulkan] Failed to find suitable memory type");
    return 0;
}

// ============================================================
// Descriptor Pool & Set
// ============================================================

bool VulkanResourceManager::CreateDescriptorPool() {
    // 池大小：覆盖所有描述符类型，每帧最大用量估算
    std::vector<VkDescriptorPoolSize> pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         2048},  // PerFrame/PerScene/PerMaterial UBO
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8192}, // 纹理采样器
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         2048}, // PointLight/SpotLight SSBO
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          64},   // Bloom Compute UAV
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 4096;  // 每帧最多 4096 个 DescriptorSet
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create descriptor pool");
        return false;
    }

    DEBUG_LOG_INFO("[Vulkan] Descriptor pool created (maxSets=4096)");
    return true;
}

void VulkanResourceManager::ResetDescriptorPool() {
    if (descriptor_pool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkResetDescriptorPool(device_, descriptor_pool_, 0);
    }
}

VkDescriptorSet VulkanResourceManager::AllocateDescriptorSet(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet descriptor_set;
    VkResult result = vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to allocate descriptor set: {}", static_cast<int>(result));
        return VK_NULL_HANDLE;
    }
    return descriptor_set;
}

} // namespace render
} // namespace dse
