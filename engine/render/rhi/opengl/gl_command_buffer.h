/**
 * @file gl_command_buffer.h
 * @brief OpenGL 命令缓冲实现 — 立即转发到 OpenGLRhiDevice
 *
 * 从 rhi_device.h 拆分：rhi_device.h 仅保留 CommandBuffer 抽象基类。
 * 本实现继承 ForwardingCommandBuffer，每个方法直接委托到
 * OpenGLRhiDevice 的 Real* 入口。
 */

#ifndef DSE_GL_COMMAND_BUFFER_H
#define DSE_GL_COMMAND_BUFFER_H

#include "engine/render/rhi/forwarding_command_buffer.h"

namespace dse {
namespace render {

class OpenGLRhiDevice;

/**
 * @class OpenGLCommandBuffer
 * @brief OpenGL 命令缓冲实现 — 立即转发模式
 *
 * 与 DX11/Vulkan CommandBuffer 统一为立即转发架构。
 * 所有绘制命令直接委托到 OpenGLRhiDevice::Real* 方法。
 */
class OpenGLCommandBuffer final : public ForwardingCommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc& render_pass) override;
    void EndRenderPass() override;
    void ClearColor(const glm::vec4& color) override;
    void SetViewport(int x, int y, int width, int height) override;
    void ClearDepth(float depth = 1.0f) override;

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

    // --- 通用 compute 原语 (B-compute) ---
    // GL 后端无 compute 路径：GetBloomComputeShader() 恒返回 0，消费者（BloomRenderer 等）
    // 据此回退全屏 quad，DispatchComputePass 在 GL 上不应被调用。提供显式 no-op 以满足纯虚契约。
    void DispatchComputePass(const ComputeDispatch& dispatch) override;

    /// 设置所属设备（由 OpenGLRhiDevice::CreateCommandBuffer 注入）
    void SetDevice(OpenGLRhiDevice* device);

    /// 重置命令缓冲状态
    void Reset();

private:
    OpenGLRhiDevice* device_ = nullptr;
};

} // namespace render
} // namespace dse

#endif // DSE_GL_COMMAND_BUFFER_H
