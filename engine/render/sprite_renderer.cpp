/**
 * @file sprite_renderer.cpp
 * @brief SpriteRenderer 实现 — 见头文件说明。
 */

#include "engine/render/sprite_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

#include <array>
#include <cstring>
#include <vector>

namespace dse {
namespace render {

namespace {

// 顶点布局与 sprite2d 着色器一致：pos\@0(vec3) + color\@1(vec4) + uv\@2(vec2)。
struct SpriteVertex {
    float px, py, pz;
    float r, g, b, a;
    float u, v;
};
static_assert(sizeof(SpriteVertex) == 36, "SpriteVertex must be tightly packed (3+4+2 floats)");

// sprite2d 的 std140 PerFrame 块：mat4 vp + mat4 view + vec4 camera_pos +
// vec4 foliage_wind + vec4 foliage_push = 176 字节。着色器仅用 vp。
struct SpritePerFrameUBO {
    glm::mat4 vp;
    glm::mat4 view;
    glm::vec4 camera_pos;
    glm::vec4 foliage_wind;
    glm::vec4 foliage_push;
};
static_assert(sizeof(SpritePerFrameUBO) == 176, "SpritePerFrameUBO std140 layout = 176 bytes");

} // namespace

void SpriteRenderer::EnsureResources(RhiDevice& device) {
    if (init_) return;

    // 2D overlay PSO：不写/不测深度、不剔除、alpha 混合。
    PipelineStateDesc desc;
    desc.blend_enabled = true;
    desc.blend_src = BlendFactor::SrcAlpha;
    desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    desc.depth_test_enabled = false;
    desc.depth_write_enabled = false;
    desc.culling_enabled = false;
    pso_ = device.CreatePipelineState(desc);

    // 索引化 quad：4 顶点 / 6 索引（动态 VB，便于每帧改 tint）。
    GpuBufferDesc vb_desc;
    vb_desc.size = sizeof(SpriteVertex) * 4;
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = true;
    vbo_ = device.CreateGpuBuffer(vb_desc, nullptr);

    const std::array<uint16_t, 6> indices = {0, 1, 2, 0, 2, 3};
    GpuBufferDesc ib_desc;
    ib_desc.size = sizeof(indices);
    ib_desc.usage = GpuBufferUsage::kIndex;
    ibo_ = device.CreateGpuBuffer(ib_desc, indices.data());

    GpuBufferDesc ub_desc;
    ub_desc.size = sizeof(SpritePerFrameUBO);
    ub_desc.usage = GpuBufferUsage::kUniform;
    ub_desc.is_dynamic = true;
    ubo_ = device.CreateGpuBuffer(ub_desc, nullptr);

    init_ = true;
}

void SpriteRenderer::Draw(CommandBuffer& cmd, RhiDevice& device, unsigned int texture_handle,
                          const glm::mat4& vp, float half_extent, const glm::vec4& tint) {
    if (texture_handle == 0) return;
    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::Sprite2D);
    if (program == 0) return;  // 该后端未提供 sprite2d 内建着色器

    EnsureResources(device);
    if (!vbo_ || !ibo_ || !ubo_) return;

    // 顶点（裁剪空间 quad，绕序 CCW：左下→右下→右上→左上）
    const float h = half_extent;
    const SpriteVertex verts[4] = {
        {-h, -h, 0.0f, tint.r, tint.g, tint.b, tint.a, 0.0f, 0.0f},
        { h, -h, 0.0f, tint.r, tint.g, tint.b, tint.a, 1.0f, 0.0f},
        { h,  h, 0.0f, tint.r, tint.g, tint.b, tint.a, 1.0f, 1.0f},
        {-h,  h, 0.0f, tint.r, tint.g, tint.b, tint.a, 0.0f, 1.0f},
    };
    device.UpdateGpuBuffer(vbo_, 0, sizeof(verts), verts);

    SpritePerFrameUBO uniforms{};
    uniforms.vp = vp;
    uniforms.view = glm::mat4(1.0f);
    device.UpdateGpuBuffer(ubo_, 0, sizeof(uniforms), &uniforms);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
    };

    cmd.SetPipelineState(pso_);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, ubo_.raw());                       // PerFrame @ set0.b0
    cmd.BindTexture(0u, texture_handle, TextureDim::Tex2D);      // u_texture @ set2.b1
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(SpriteVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(6u, 0u, 0);
}

void SpriteRenderer::Shutdown(RhiDevice& device) {
    if (vbo_) device.DeleteGpuBuffer(vbo_);
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    if (ubo_) device.DeleteGpuBuffer(ubo_);
    vbo_ = ibo_ = ubo_ = BufferHandle{};
    init_ = false;
}

} // namespace render
} // namespace dse
