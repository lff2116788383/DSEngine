/**
 * @file vulkan_command_buffer.h
 * @brief Vulkan 命令缓冲实现 — 立即转发到 VulkanRhiDevice
 *
 * 从 vulkan_rhi_device.h 拆分：vulkan_rhi_device.h 仅保留 VulkanRhiDevice。
 * 本实现继承 ForwardingCommandBuffer，使用 VkCommandBuffer 录制命令。
 */

#ifndef DSE_VULKAN_COMMAND_BUFFER_H
#define DSE_VULKAN_COMMAND_BUFFER_H

#include "engine/render/rhi/forwarding_command_buffer.h"
#include <vulkan/vulkan.h>

namespace dse {
namespace render {

class VulkanRhiDevice;

/**
 * @class VulkanCommandBuffer
 * @brief Vulkan 命令缓冲实现 — 立即转发模式
 *
 * 与 OpenGL/DX11 CommandBuffer 统一为立即转发架构。
 * 所有绘制命令通过 VkCommandBuffer 录制到 VulkanDrawExecutor。
 */
class VulkanCommandBuffer final : public ForwardingCommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc& render_pass) override;
    void EndRenderPass() override;
    void ClearColor(const glm::vec4& color) override;
    void DispatchComputePass(const ComputeDispatch& dispatch) override;
    void SetViewport(int x, int y, int width, int height) override;
    void ClearDepth(float depth = 1.0f) override;
    void BlitToScreen(unsigned int source_rt) override;

    // --- 通用绘制原语 (A1) ---
    void BindPipeline(unsigned int graphics_pipeline_handle) override;
    void BindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                          const std::vector<VertexAttr>& attrs,
                          VertexInputRate rate = VertexInputRate::PerVertex) override;
    void PushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) override;
    void Draw(uint32_t vertex_count, uint32_t first_vertex) override;

    // --- 通用绘制原语 (B0) ---
    void BindIndexBuffer(unsigned int buffer_handle, IndexType type) override;
    void BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) override;
    void BindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                           uint32_t offset, uint32_t size) override;
    void BindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                           uint32_t offset, uint32_t size) override;
    void DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) override;

    // --- 通用绘制原语 (B2b 前置) ---
    void DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                              uint32_t first_index, int32_t base_vertex,
                              uint32_t first_instance) override;

    // --- 通用绘制原语 (B2b-5): GPU-driven 间接索引绘制 ---
    void DrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) override;

    /// 获取底层 VkCommandBuffer
    VkCommandBuffer GetVkCommandBuffer() const { return vk_command_buffer_; }
    void SetVkCommandBuffer(VkCommandBuffer cmd) { vk_command_buffer_ = cmd; }

    /// 重置命令缓冲状态
    void Reset();

    /// 设置所属设备（由 VulkanRhiDevice::CreateCommandBuffer 注入）
    void SetDevice(VulkanRhiDevice* device);

private:
    VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;
    VulkanRhiDevice* device_ = nullptr;
};

} // namespace render
} // namespace dse

#endif // DSE_VULKAN_COMMAND_BUFFER_H
