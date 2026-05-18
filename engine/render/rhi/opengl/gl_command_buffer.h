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
    void SetPipelineState(unsigned int pipeline_state_handle) override;
    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override;
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) override;
    void ClearColor(const glm::vec4& color) override;
    void DrawSkybox(unsigned int cubemap_texture_handle) override;
    void DrawPostProcess(PostProcessRequest request) override;
    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;
    void DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;

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
