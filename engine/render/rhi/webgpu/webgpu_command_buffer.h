/**
 * @file webgpu_command_buffer.h
 * @brief WebGPU 命令缓冲（manager 拆分：原 webgpu_rhi_device.cpp 匿名 ns 原样移出）。
 *
 * 把后端无关的录制接口逐调用转发到 WebGPURhiDevice 的设备级 Cmd*（orchestrator 再转发到
 * draw_executor），由设备直接录制到本帧 frame_encoder_（立即转发，非缓存重放）。所有接口
 * 显式实现，不依赖基类静默默认，避免漏实现时无声吞掉绘制。
 */

#ifndef DSE_WEBGPU_COMMAND_BUFFER_H
#define DSE_WEBGPU_COMMAND_BUFFER_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/webgpu/webgpu_rhi_device.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace dse {
namespace render {

class WebGPUCommandBuffer final : public CommandBuffer {
public:
    explicit WebGPUCommandBuffer(WebGPURhiDevice* device) : device_(device) {}

    void BeginRenderPass(const RenderPassDesc& render_pass) override { device_->CmdBeginRenderPass(render_pass); }
    void EndRenderPass() override { device_->CmdEndRenderPass(); }
    void ClearColor(const glm::vec4& color) override { device_->CmdClearColor(color); }
    void SetViewport(int x, int y, int width, int height) override { device_->CmdSetViewport(x, y, width, height); }

    void BindGlobalShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalShadowMap(index, texture_handle); }
    void BindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalSpotShadowMap(index, texture_handle); }
    void BindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalPointShadowMap(index, texture_handle); }

    void BindPipeline(unsigned int graphics_pipeline_handle) override { device_->CmdBindPipeline(graphics_pipeline_handle); }
    void BindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                          const std::vector<VertexAttr>& attrs,
                          VertexInputRate rate) override {
        device_->CmdBindVertexBuffer(slot, buffer_handle, stride, attrs, rate);
    }
    void PushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) override {
        device_->CmdPushConstants(stage, offset, data, size);
    }
    void Draw(uint32_t vertex_count, uint32_t first_vertex) override { device_->CmdDraw(vertex_count, first_vertex); }

    void BindIndexBuffer(unsigned int buffer_handle, IndexType type) override { device_->CmdBindIndexBuffer(buffer_handle, type); }
    void BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) override {
        device_->CmdBindTexture(slot, texture_handle, dim);
    }
    void BindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) override {
        device_->CmdBindUniformBuffer(slot, buffer_handle, offset, size);
    }
    void BindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) override {
        device_->CmdBindStorageBuffer(slot, buffer_handle, offset, size);
    }
    void DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) override {
        device_->CmdDrawIndexed(index_count, first_index, base_vertex);
    }
    void DispatchComputePass(const ComputeDispatch& dispatch) override { device_->CmdDispatchComputePass(dispatch); }
    void DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                              uint32_t first_index, int32_t base_vertex,
                              uint32_t first_instance) override {
        device_->CmdDrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
    }
    void DrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) override {
        device_->CmdDrawIndexedIndirect(indirect_buffer, byte_offset);
    }

private:
    WebGPURhiDevice* device_;
};

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif  // DSE_WEBGPU_COMMAND_BUFFER_H
