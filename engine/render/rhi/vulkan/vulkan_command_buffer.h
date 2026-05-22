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
    void SetPipelineState(unsigned int pipeline_state_handle) override;
    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override;
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) override;
    void ClearColor(const glm::vec4& color) override;
    void DrawSkybox(unsigned int cubemap_texture_handle) override;
    void DrawPostProcess(PostProcessRequest request) override;
    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;
    void DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;
    void BlitToScreen(unsigned int source_rt) override;

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
