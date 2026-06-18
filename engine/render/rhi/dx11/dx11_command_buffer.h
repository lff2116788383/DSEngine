/**
 * @file dx11_command_buffer.h
 * @brief D3D11 命令缓冲实现 — 立即转发到 DX11RhiDevice
 *
 * 从 dx11_rhi_device.h 拆分：dx11_rhi_device.h 仅保留 DX11RhiDevice。
 * 本实现继承 ForwardingCommandBuffer，每个方法直接委托到
 * DX11RhiDevice 内部的 DX11DrawExecutor。
 */

#ifndef DSE_DX11_COMMAND_BUFFER_H
#define DSE_DX11_COMMAND_BUFFER_H

#include "engine/render/rhi/forwarding_command_buffer.h"

namespace dse {
namespace render {

class DX11RhiDevice;

/**
 * @class DX11CommandBuffer
 * @brief D3D11 命令缓冲实现 — 立即转发模式
 *
 * 与 OpenGL/Vulkan CommandBuffer 统一为立即转发架构。
 * 所有绘制命令直接委托到 DX11DrawExecutor。
 */
class DX11CommandBuffer final : public ForwardingCommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc& render_pass) override;
    void EndRenderPass() override;
    void SetPipelineState(unsigned int pipeline_state_handle) override;
    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override;
    void ClearColor(const glm::vec4& color) override;
    void DrawPostProcess(PostProcessRequest request) override;
    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;
    void DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;
    void SetViewport(int x, int y, int width, int height) override;
    void ClearDepth(float depth = 1.0f) override;

    // --- 通用绘制原语 (A1) ---
    void BindShaderProgram(unsigned int program_handle) override;
    void BindVertexBuffer(unsigned int buffer_handle, uint32_t stride,
                          const std::vector<VertexAttr>& attrs) override;
    void BindTextureCube(unsigned int slot, unsigned int cubemap_handle) override;
    void PushConstantsMat4(const glm::mat4& value) override;
    void Draw(uint32_t vertex_count, uint32_t first_vertex) override;

    // --- 通用绘制原语 (B0): 索引 / 2D 纹理 / UBO / 索引绘制 ---
    void BindIndexBuffer(unsigned int buffer_handle, IndexType type) override;
    void BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) override;
    void BindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                           uint32_t offset, uint32_t size) override;
    void DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) override;

    /// 重置命令缓冲状态
    void Reset();

    /// 设置所属设备（由 DX11RhiDevice::CreateCommandBuffer 注入）
    void SetDevice(DX11RhiDevice* device);

private:
    DX11RhiDevice* device_ = nullptr;
};

} // namespace render
} // namespace dse

#endif // DSE_DX11_COMMAND_BUFFER_H
