/**
 * @file vulkan_resource_manager.h
 * @brief Vulkan GPU 资源管理器 — 纹理/Buffer/RenderTarget/DescriptorSet 生命周期管理
 *
 * 对标 GLResourceManager，但 Vulkan 需要额外的 Descriptor Set 和内存分配。
 * 当前阶段使用 VMA (Vulkan Memory Allocator) 简化内存管理。
 */

#ifndef DSE_RENDER_VULKAN_RESOURCE_MANAGER_H
#define DSE_RENDER_VULKAN_RESOURCE_MANAGER_H

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace dse {
namespace render {

class VulkanContext;

/// Vulkan Buffer 资源封装
struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;          ///< 持久映射指针（动态缓冲使用）
    bool is_dynamic = false;
};

/// Vulkan 纹理资源封装
struct VulkanTexture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    int width = 0;
    int height = 0;
    int channels = 4;
};

/// Vulkan 渲染目标资源封装
struct VulkanRenderTarget {
    int width = 0;
    int height = 0;
    bool has_color = true;
    bool has_depth = false;
    bool generate_mipmaps = false;

    bool is_msaa = false;           ///< 是否启用 MSAA
    int msaa_samples = 1;            ///< MSAA 采样数（1 或 4）
    bool allow_uav = false;          ///< 是否支持 Compute Storage 写入

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;       ///< RenderTarget 关联的 RenderPass
    VulkanTexture color_texture;       ///< 1x SRV（MSAA 时为 resolve 目标）
    VulkanTexture msaa_color_texture;  ///< MSAA 颜色附件（仅 is_msaa=true 时有效）
    VulkanTexture depth_texture;
};

/**
 * @class VulkanResourceManager
 * @brief Vulkan GPU 资源管理器
 *
 * 职责：
 * 1. 纹理创建/销毁（Texture2D / TextureCube）
 * 2. 缓冲区创建/更新/销毁
 * 3. 渲染目标（Framebuffer + 附件）创建/销毁
 * 4. 命令池管理
 * 5. Descriptor Pool 和 Descriptor Set 分配
 */
class VulkanResourceManager {
public:
    VulkanResourceManager() = default;
    ~VulkanResourceManager() = default;

    /// 初始化（在 VulkanContext 就绪后调用）
    bool Init(VulkanContext* context);

    /// 销毁所有资源
    void Shutdown();

    // --- 命令缓冲 ---
    VkCommandPool command_pool() const { return command_pool_; }

    /// 分配一次性命令缓冲（用于临时拷贝等）
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer command_buffer);

    // --- 纹理 ---
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter);
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter);
    void DeleteTexture(unsigned int handle);
    const VulkanTexture* GetTexture(unsigned int handle) const;

    // --- 缓冲区 ---
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index);
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data);
    void DeleteBuffer(unsigned int handle);

    // --- 渲染目标 ---
    unsigned int CreateRenderTarget(int width, int height, bool has_color, bool has_depth,
                                     bool generate_mipmaps, bool cube_map,
                                     int msaa_samples = 1, bool allow_uav = false);
    void DeleteRenderTarget(unsigned int handle);
    const VulkanRenderTarget* GetRenderTarget(unsigned int handle) const;

    /// 获取渲染目标的颜色附件 ImageView
    VkImageView GetRenderTargetColorImageView(unsigned int handle) const;

    // --- 默认采样器 ---
    VkSampler default_sampler() const { return default_sampler_; }

    // --- 阴影比较采样器（compareEnable=VK_TRUE，用于 sampler2DShadow PCF）---
    VkSampler shadow_comparison_sampler() const { return shadow_comparison_sampler_; }

    // --- Descriptor Pool & Set ---
    VkDescriptorPool descriptor_pool() const { return descriptor_pool_; }

    /// 从全局 DescriptorPool 分配 DescriptorSet
    /// @param layout 需要匹配的 VkDescriptorSetLayout
    /// @return 新分配的 VkDescriptorSet（VK_NULL_HANDLE 表示失败）
    VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);

    /// 创建 DescriptorPool（在 Init 中自动调用）
    bool CreateDescriptorPool();

    // --- 句柄生成 ---
    unsigned int AllocateTextureHandle();
    unsigned int AllocateRenderTargetHandle();

private:
    /// 创建 VkImage + VkDeviceMemory + VkImageView
    bool CreateVulkanImage(int width, int height, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                           VkImageAspectFlags aspect_mask, VulkanTexture& out_texture,
                           VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);

    /// 将数据上传到纹理（使用 staging buffer）
    void UploadTextureData(VulkanTexture& texture, const void* data, size_t data_size);

    /// 过渡 Image Layout
    void TransitionImageLayout(VkImage image, VkFormat format,
                                VkImageLayout old_layout, VkImageLayout new_layout);

    /// 查找合适的内存类型
    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

    VulkanContext* context_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;

    // 命令池
    VkCommandPool command_pool_ = VK_NULL_HANDLE;

    // 默认采样器（linear clamp，供 Compute Shader 使用）
    VkSampler default_sampler_ = VK_NULL_HANDLE;

    // 阴影比较采样器（compareEnable=VK_TRUE，compareOp=LESS_OR_EQUAL，供 sampler2DShadow PCF）
    VkSampler shadow_comparison_sampler_ = VK_NULL_HANDLE;

    // Descriptor Pool
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;

    // 资源存储
    std::unordered_map<unsigned int, VulkanTexture> textures_;
    std::unordered_map<unsigned int, VulkanBuffer> buffers_;
    std::unordered_map<unsigned int, VulkanRenderTarget> render_targets_;

    // 句柄计数器
    unsigned int next_texture_handle_ = 400000;
    unsigned int next_buffer_handle_ = 410000;
    unsigned int next_render_target_handle_ = 420000;

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_RESOURCE_MANAGER_H
