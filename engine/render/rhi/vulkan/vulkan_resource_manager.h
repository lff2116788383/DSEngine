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
#include "engine/render/rhi/rhi_types.h"

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
    VkBufferUsageFlags usage_flags = 0;      ///< 创建时的 usage（同帧重写重建用）
    uint64_t last_update_frame = UINT64_MAX; ///< 最近一次 UpdateBuffer 的帧号
    VkDeviceSize frame_write_lo = 0;         ///< 本帧已写区间下界（copy-on-write 重叠检测用）
    VkDeviceSize frame_write_hi = 0;         ///< 本帧已写区间上界（exclusive）
    bool skip_host_sync = false;             ///< per-in-flight ring 缓冲：host 写入跳过跨帧总闸（SyncHostWriteWithGpu）
};

/// Vulkan 纹理资源封装
struct VulkanTexture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    int width = 0;
    int height = 0;
    int depth = 1;
    int channels = 4;
    bool is_3d = false;
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

    int color_attachment_count = 1;  ///< MRT 颜色附件数量

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;       ///< RenderTarget 关联的 RenderPass（loadOp=CLEAR）
    VkRenderPass render_pass_load = VK_NULL_HANDLE;  ///< loadOp=LOAD 变体，用于后续不清除的渲染
    VulkanTexture color_texture;       ///< 1x SRV（MSAA 时为 resolve 目标），兼容 = color_textures[0]
    VulkanTexture msaa_color_texture;  ///< MSAA 颜色附件（仅 is_msaa=true 时有效）
    VulkanTexture depth_texture;
    std::vector<VulkanTexture> color_textures; ///< MRT: 所有颜色附件
    std::vector<unsigned int> mrt_texture_handles; ///< MRT: 每个颜色附件对应的独立纹理 handle
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
    void ResetCommandPool();

    // --- 纹理 ---
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter);
    unsigned int CreateComputeWriteTexture2D(int width, int height);
    unsigned int CreateCompressedTexture2D(CompressedTextureFormat format,
                                           const std::vector<CompressedMipLevel>& mips,
                                           bool linear_filter);
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter);
    unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter);
    void DeleteTexture(unsigned int handle);
    const VulkanTexture* GetTexture(unsigned int handle) const;

    // --- 缓冲区 ---
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index);
    /// 创建 uniform buffer（VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT，host-visible 持久映射，B0 通用 UBO 原语用）
    unsigned int CreateUniformBuffer(size_t size, const void* data, bool is_dynamic);
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data);
    void DeleteBuffer(unsigned int handle);

    /// 帧开始时推进帧计数并回收已过 in-flight 窗口的退役缓冲
    void BeginFrameBufferGC(uint32_t frames_in_flight);
    const VulkanBuffer* GetBuffer(unsigned int handle) const;

    // --- SSBO (Storage Buffer) ---
    unsigned int CreateSSBO(size_t size, const void* data);
    void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data);
    void DeleteSSBO(unsigned int handle);
    const VulkanBuffer* GetSSBO(unsigned int handle) const;

    /// 标记 SSBO/indirect 缓冲为 per-in-flight ring 管理（host 写入跳过跨帧总闸）。
    /// is_indirect=true 查 indirect_buffers_，否则查 ssbos_。
    void SetSkipHostSync(unsigned int handle, bool is_indirect);

    // --- Indirect Draw Buffer ---
    unsigned int CreateIndirectBuffer(size_t size, const void* data);
    void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data);
    void DeleteIndirectBuffer(unsigned int handle);
    const VulkanBuffer* GetIndirectBuffer(unsigned int handle) const;

    // --- 渲染目标 ---
    unsigned int CreateRenderTarget(int width, int height, bool has_color, bool has_depth,
                                     bool generate_mipmaps, bool cube_map,
                                     int msaa_samples = 1, bool allow_uav = false,
                                     int color_attachment_count = 1);
    void DeleteRenderTarget(unsigned int handle);
    const VulkanRenderTarget* GetRenderTarget(unsigned int handle) const;

    /// 获取渲染目标的颜色附件 ImageView
    VkImageView GetRenderTargetColorImageView(unsigned int handle) const;

    /// 获取渲染目标的深度附件 ImageView
    VkImageView GetRenderTargetDepthImageView(unsigned int handle) const;

    // --- 默认采样器 ---
    VkSampler default_sampler() const { return default_sampler_; }

    // --- 材质采样器（linear repeat，供网格材质贴图使用，与 GL/D3D11 默认 wrap 对齐）---
    VkSampler material_sampler() const { return material_sampler_; }

    // --- 阴影比较采样器（compareEnable=VK_TRUE，用于 sampler2DShadow PCF）---
    VkSampler shadow_comparison_sampler() const { return shadow_comparison_sampler_; }

    // --- Descriptor Pool & Set ---
    VkDescriptorPool descriptor_pool() const {
        const auto& pools = descriptor_pools_[current_pool_index_];
        return active_pool_slot_ < pools.size() ? pools[active_pool_slot_] : VK_NULL_HANDLE;
    }

    /// 从当前帧的 DescriptorPool 分配 DescriptorSet
    /// @param layout 需要匹配的 VkDescriptorSetLayout
    /// @return 新分配的 VkDescriptorSet（VK_NULL_HANDLE 表示失败）
    VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);

    /// 重置指定帧的 DescriptorPool（fence 等待后调用，仅释放该帧的 DescriptorSet）
    void ResetDescriptorPool(uint32_t frame_index);

    /// 创建 DescriptorPool（在 Init 中自动调用）
    bool CreateDescriptorPool();

private:
    /// 创建单个 DescriptorPool（供按需扩容使用）
    VkDescriptorPool CreateOneDescriptorPool();

public:

    // --- 命令缓冲池 ---
    /// 从池中获取可复用的 VkCommandBuffer（池空时创建新缓冲）
    VkCommandBuffer AcquireCommandBuffer();
    /// 归还命令缓冲到池中（reset 后复用）
    void ReleaseCommandBuffer(VkCommandBuffer cmd);
    /// 销毁池中所有空闲命令缓冲
    void ClearCommandBufferPool();

    // --- 句柄生成 ---
    unsigned int AllocateTextureHandle();
    unsigned int AllocateRenderTargetHandle();

    /// 查找合适的内存类型
    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

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

    VulkanContext* context_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;

    // 命令池
    VkCommandPool command_pool_ = VK_NULL_HANDLE;

    /// 命令缓冲池（避免逐帧 vkAllocateCommandBuffers）
    std::vector<VkCommandBuffer> free_cmd_buffers_;

    // 默认采样器（linear clamp，供 Compute Shader 使用）
    VkSampler default_sampler_ = VK_NULL_HANDLE;

    // 材质采样器（linear repeat，供网格材质贴图使用，匹配 GL/D3D11 默认 wrap=REPEAT）
    VkSampler material_sampler_ = VK_NULL_HANDLE;

    // 阴影比较采样器（compareEnable=VK_TRUE，compareOp=LESS_OR_EQUAL，供 sampler2DShadow PCF）
    VkSampler shadow_comparison_sampler_ = VK_NULL_HANDLE;

    // Descriptor Pool（per-frame，避免帧间同步冲突）
    // 每帧维护一个 pool 列表：单帧 set 数超过单个 pool 容量时按需追加新 pool，
    // 避免 VK_ERROR_OUT_OF_POOL_MEMORY 导致 draw 用空 descriptor set 触发 DEVICE_LOST。
    static constexpr uint32_t kMaxFramesInFlight = 2;
    std::vector<VkDescriptorPool> descriptor_pools_[kMaxFramesInFlight];
    uint32_t current_pool_index_ = 0;
    size_t active_pool_slot_ = 0;

    // 资源存储
    std::unordered_map<unsigned int, VulkanTexture> textures_;
    std::unordered_map<unsigned int, VulkanBuffer> buffers_;
    std::unordered_map<unsigned int, VulkanBuffer> ssbos_;
    std::unordered_map<unsigned int, VulkanBuffer> indirect_buffers_;
    std::unordered_map<unsigned int, VulkanRenderTarget> render_targets_;

    // 句柄计数器
    unsigned int next_texture_handle_ = 400000;
    unsigned int next_buffer_handle_ = 410000;
    unsigned int next_ssbo_handle_ = 415000;
    unsigned int next_indirect_handle_ = 418000;
    unsigned int next_render_target_handle_ = 420000;

    /// 同帧多次覆写动态缓冲时退役的旧 VkBuffer（命令缓冲仍引用，需过 in-flight 窗口后销毁）
    struct RetiredBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        uint64_t retired_frame = 0;
    };
    std::vector<RetiredBuffer> retired_buffers_;
    uint64_t frame_counter_ = 0;

    /// host 直写持久映射 SSBO/indirect 缓冲前，每帧至多同步一次在飞帧（见 SyncHostWriteWithGpu）
    uint64_t host_write_synced_frame_ = 0;
    void SyncHostWriteWithGpu();

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_RESOURCE_MANAGER_H

