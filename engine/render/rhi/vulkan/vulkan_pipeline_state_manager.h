/**
 * @file vulkan_pipeline_state_manager.h
 * @brief Vulkan 管线状态管理器 - VkPipeline/VkRenderPass 缓存与应用
 *
 * 职责：
 * 1. 将 PipelineStateDesc 转换为 Vulkan VkPipeline 创建信息
 * 2. VkGraphicsPipeline 缓存与复用（基于状态 hash）
 * 3. VkRenderPass 缓存（基于 attachment 描述 hash）
 * 4. 管线状态 Diff 机制（对标 GL 版本）
 */

#ifndef DSE_RENDER_VULKAN_PIPELINE_STATE_MANAGER_H
#define DSE_RENDER_VULKAN_PIPELINE_STATE_MANAGER_H

#include "engine/render/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>

namespace dse {
namespace render {

class VulkanContext;
class VulkanShaderManager;

/// Vulkan 管线状态完整信息
struct VulkanPipelineState {
    PipelineStateDesc desc;          ///< RHI 无关的管线状态描述
    VkPipeline pipeline = VK_NULL_HANDLE;       ///< 已创建的 Vulkan 管线对象
    VkRenderPass render_pass = VK_NULL_HANDLE;   ///< 关联的 RenderPass
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE; ///< 关联的 PipelineLayout
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT; ///< MSAA 采样数
    unsigned int shader_program_handle = 0;      ///< 关联的着色器程序
};

/**
 * @class VulkanPipelineStateManager
 * @brief Vulkan 管线状态管理器
 *
 * 与 GL 版本不同，Vulkan 的管线状态在创建时就绑定了着色器、
 * RenderPass 和顶点格式，因此需要更多的绑定信息。
 */
class VulkanPipelineStateManager {
public:
    VulkanPipelineStateManager() = default;
    ~VulkanPipelineStateManager() = default;

    /// 初始化
    void Init(VulkanContext* context, VulkanShaderManager* shader_mgr);

    /// 清理所有管线状态资源
    void Shutdown();

    /// 创建管线状态并返回句柄
    unsigned int CreatePipelineState(const PipelineStateDesc& desc);

    /// 延迟创建或获取 VkPipeline（首次 Draw 时按需创建）
    /// @param handle 管线状态句柄
    /// @param shader_program 关联的着色器程序
    /// @param render_pass 目标 RenderPass
    /// @param vertex_bindings 顶点绑定描述
    /// @param vertex_attributes 顶点属性描述
    /// @param extent 渲染区域大小
    VkPipeline GetOrCreateVkPipeline(
        unsigned int handle,
        const struct VulkanShaderProgram* shader_program,
        VkRenderPass render_pass,
        const std::vector<VkVertexInputBindingDescription>& vertex_bindings,
        const std::vector<VkVertexInputAttributeDescription>& vertex_attributes,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        uint32_t color_attachment_count = 1);

    /// 查询管线状态
    const PipelineStateDesc* GetPipelineState(unsigned int handle) const;

    /// 获取完整的 Vulkan 管线状态（包含 VkPipeline 等）
    const VulkanPipelineState* GetVulkanPipelineState(unsigned int handle) const;

    /// 将 PipelineStateDesc 中的混合/深度/剔除转换为 Vulkan 常量
    static VkBlendFactor ToVkBlendFactor(BlendFactor factor);
    static VkCompareOp ToVkCompareOp(CompareFunc func);
    static VkCullModeFlagBits ToVkCullMode(CullFace face);
    static VkFrontFace ToVkFrontFace();

    /// 获取或创建 VkRenderPass（基于附件描述的缓存）
    struct RenderPassKey {
        bool has_color = false;
        bool has_depth = false;
        bool color_clear = false;
        bool depth_clear = false;
        bool operator==(const RenderPassKey& o) const {
            return has_color == o.has_color && has_depth == o.has_depth &&
                   color_clear == o.color_clear && depth_clear == o.depth_clear;
        }
    };
    VkRenderPass GetOrCreateRenderPass(const RenderPassKey& key);

    /// 设置活跃管线状态（追踪当前绑定）
    void set_active_pipeline_state(unsigned int handle) { active_pipeline_state_ = handle; }
    unsigned int active_pipeline_state() const { return active_pipeline_state_; }

    /// 当前管线状态数量
    std::size_t pipeline_state_count() const { return pipeline_states_.size(); }

private:
    VulkanContext* context_ = nullptr;
    VulkanShaderManager* shader_mgr_ = nullptr;

    std::unordered_map<unsigned int, VulkanPipelineState> pipeline_states_;
    unsigned int next_handle_ = 530000;
    unsigned int active_pipeline_state_ = 0;

    /// Pipeline 复合缓存键：(handle, renderPass, samples)
    struct PipelineCacheKey {
        unsigned int handle;
        VkRenderPass render_pass;
        VkSampleCountFlagBits samples;
        bool operator==(const PipelineCacheKey& o) const {
            return handle == o.handle && render_pass == o.render_pass && samples == o.samples;
        }
    };
    struct PipelineCacheKeyHash {
        size_t operator()(const PipelineCacheKey& k) const {
            size_t h = std::hash<unsigned int>()(k.handle);
            h ^= std::hash<uint64_t>()(reinterpret_cast<uint64_t>(k.render_pass)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(static_cast<int>(k.samples)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<PipelineCacheKey, VkPipeline, PipelineCacheKeyHash> pipeline_cache_;

    /// VkRenderPass 缓存
    struct RenderPassKeyHash {
        size_t operator()(const RenderPassKey& k) const;
    };
    std::unordered_map<RenderPassKey, VkRenderPass, RenderPassKeyHash> render_pass_cache_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_PIPELINE_STATE_MANAGER_H
