/**
 * @file vulkan_command_state.h
 * @brief Vulkan 单条命令缓冲的录制状态（ADR-1）。
 *
 * 这些字段原来存放在 VulkanDrawExecutor 的成员上，导致两条 CommandBuffer
 * 交错录制时互相覆盖。现在由 VulkanCommandBuffer 各自持有一份，Draw* 录制时
 * 作为显式参数传入 executor。
 */

#ifndef DSE_VULKAN_COMMAND_STATE_H
#define DSE_VULKAN_COMMAND_STATE_H

#include "engine/render/rhi/rhi_types.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

namespace dse {
namespace render {

struct VulkanPrimVbBinding {
    VkBuffer buffer = VK_NULL_HANDLE;
    uint32_t stride = 0;
    std::vector<VertexAttr> attrs;
    VertexInputRate rate = VertexInputRate::PerVertex;
};

struct VulkanPrimSSBOBinding {
    unsigned int handle = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
};

inline constexpr uint32_t kVulkanPrimPushMaxBytes = 256;

/// 与一条 VkCommandBuffer 生命周期绑定的录制状态。
struct VulkanCommandState {
    // RenderPass 绑定状态
    unsigned int current_rt_handle = 0;
    VkRenderPass current_render_pass = VK_NULL_HANDLE;
    VkSampleCountFlagBits current_msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    uint32_t current_color_attachment_count = 1;
    bool skip_current_pass = false;

    // 通用绘制原语累积状态
    unsigned int prim_pipeline_state = 0;
    unsigned int prim_program_handle = 0;
    std::map<uint32_t, VulkanPrimVbBinding> prim_vbs;
    unsigned int prim_cubemap = 0;
    uint8_t prim_push_data[kVulkanPrimPushMaxBytes] = {};
    uint32_t prim_push_size = 0;
    bool prim_has_push = false;
    VkBuffer prim_index_buffer = VK_NULL_HANDLE;
    VkIndexType prim_index_type = VK_INDEX_TYPE_UINT16;
    std::unordered_map<uint32_t, unsigned int> prim_textures;
    std::unordered_map<uint32_t, unsigned int> prim_ubos;
    std::unordered_map<uint32_t, VulkanPrimSSBOBinding> prim_ssbos;
};

}  // namespace render
}  // namespace dse

#endif  // DSE_VULKAN_COMMAND_STATE_H
