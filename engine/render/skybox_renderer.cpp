/**
 * @file skybox_renderer.cpp
 * @brief SkyboxRenderer 实现 — 见头文件说明。
 */

#include "engine/render/skybox_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <vector>

namespace dse {
namespace render {

void SkyboxRenderer::Draw(CommandBuffer& cmd, RhiDevice& device, unsigned int cubemap_handle,
                          const glm::mat4& view, const glm::mat4& projection) {
    if (cubemap_handle == 0) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::Skybox);
    unsigned int vbo = device.GetSkyboxCubeVertexBuffer();
    if (program == 0 || vbo == 0) return;

    // 天空盒专用管线状态：深度 LEQUAL、不写深度、不剔除、不混合。
    if (!pso_init_) {
        PipelineStateDesc desc;
        desc.blend_enabled = false;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = false;
        desc.depth_func = CompareFunc::LessEqual;
        desc.culling_enabled = false;
        pso_ = device.CreatePipelineState(desc);
        pso_init_ = true;
    }

    // 移除 view 平移分量（仅保留旋转），与原 DrawSkybox 一致。
    const glm::mat4 skybox_view = glm::mat4(glm::mat3(view));
    const glm::mat4 vp = projection * skybox_view;

    cmd.SetPipelineState(pso_);
    cmd.BindShaderProgram(program);
    cmd.PushConstantsMat4(vp);
    cmd.BindTexture(0, cubemap_handle, TextureDim::TexCube);

    const std::vector<VertexAttr> attrs = { VertexAttr{0u, 3u, 0u} };
    cmd.BindVertexBuffer(vbo, static_cast<uint32_t>(sizeof(float) * 3), attrs);
    cmd.Draw(36u, 0u);
}

} // namespace render
} // namespace dse
