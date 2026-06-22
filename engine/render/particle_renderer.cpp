/**
 * @file particle_renderer.cpp
 * @brief ParticleRenderer 实现 — 见头文件说明。
 */

#include "engine/render/particle_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

#include <cstdint>
#include <vector>

namespace dse {
namespace render {

namespace {

// std140 PerFrame 块（176 字节），布局须与 particle_instanced.vert 的 PerFrame 一致：
// mat4 vp + mat4 view + vec4 camera_pos + vec4 foliage_wind + vec4 foliage_push。
// 着色器仅用 vp / view（取相机右/上轴广告牌），其余字段置零。
struct ParticlePerFrameUBO {
    glm::mat4 vp;
    glm::mat4 view;
    glm::vec4 camera_pos;
    glm::vec4 foliage_wind;
    glm::vec4 foliage_push;
};
static_assert(sizeof(ParticlePerFrameUBO) == 176, "ParticlePerFrameUBO std140 = 176 bytes");

// 单位 quad：4 顶点 { pos(vec3) + uv(vec2) }（紧凑 20 字节），6 索引两三角。
struct QuadVertex {
    float px, py, pz;
    float u, v;
};
static_assert(sizeof(QuadVertex) == 20, "QuadVertex must be tightly packed (3+2 floats)");

// 每实例 SSBO 步长：std430 { vec4 pos_size; vec4 color; } = 32 字节。
constexpr uint32_t kInstanceStrideBytes = 32u;

}  // namespace

void ParticleRenderer::EnsureResources(RhiDevice& device) {
    if (init_) return;

    // 加性混合 PSO：测深度（被不透明几何遮挡）但不写深度、不剔除（广告牌双面可见）。
    PipelineStateDesc desc;
    desc.blend_enabled = true;
    desc.blend_src = BlendFactor::SrcAlpha;
    desc.blend_dst = BlendFactor::One;
    desc.alpha_blend_src = BlendFactor::SrcAlpha;
    desc.alpha_blend_dst = BlendFactor::One;
    desc.depth_test_enabled = true;
    desc.depth_write_enabled = false;
    desc.culling_enabled = false;
    pso_ = device.CreatePipelineState(desc);

    // 1x1 白纹理：缺省纹理槽回退（采样得 1.0 → 仅顶点色生效）。
    const unsigned char white[4] = {255, 255, 255, 255};
    white_tex_ = device.CreateTexture2D(1, 1, white, /*linear_filter=*/true);

    const QuadVertex quad[4] = {
        {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f, 0.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 0.0f, 1.0f},
    };
    const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};

    GpuBufferDesc vb_desc;
    vb_desc.size = sizeof(quad);
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = false;
    quad_vbo_ = device.CreateGpuBuffer(vb_desc, quad);

    GpuBufferDesc ib_desc;
    ib_desc.size = sizeof(idx);
    ib_desc.usage = GpuBufferUsage::kIndex;
    ib_desc.is_dynamic = false;
    quad_ibo_ = device.CreateGpuBuffer(ib_desc, idx);

    GpuBufferDesc f_desc;
    f_desc.size = sizeof(ParticlePerFrameUBO);
    f_desc.usage = GpuBufferUsage::kUniform;
    f_desc.is_dynamic = true;
    per_frame_ubo_ = device.CreateGpuBuffer(f_desc, nullptr);

    init_ = true;
}

void ParticleRenderer::DrawParticles(CommandBuffer& cmd, RhiDevice& device,
                                     const std::vector<ParticleDrawItem>& items,
                                     const glm::mat4& view, const glm::mat4& proj) {
    if (items.empty()) return;
    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::Particle3D);
    if (program == 0) return;  // SSBO 不支持的上下文（如 WebGL2）→ 静默跳过
    EnsureResources(device);
    if (!per_frame_ubo_ || !quad_vbo_ || !quad_ibo_) return;

    ParticlePerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},   // aPos
        VertexAttr{1u, 2u, 12u},  // aTexCoord
    };

    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());  // PerFrame @ set0.b0
    cmd.BindVertexBuffer(quad_vbo_.raw(), static_cast<uint32_t>(sizeof(QuadVertex)), attrs);
    cmd.BindIndexBuffer(quad_ibo_.raw(), IndexType::UInt16);

    for (const auto& item : items) {
        if (item.particle_count <= 0 || item.instance_buffer == 0) continue;
        const uint32_t inst_bytes = static_cast<uint32_t>(item.particle_count) * kInstanceStrideBytes;
        cmd.BindTexture(0u, item.texture_handle ? item.texture_handle : white_tex_, TextureDim::Tex2D);
        // 每实例 pos/size/color SSBO\@slot 0（set7.b0）。子区间 [0, count*32)。
        cmd.BindStorageBuffer(0u, item.instance_buffer, 0u, inst_bytes);
        // 契约：first_instance 恒 0，DX11 SV_InstanceID 从 0 起，0 基 SSBO 索引取数。
        cmd.DrawIndexedInstanced(6u, static_cast<uint32_t>(item.particle_count), 0u, 0, 0u);
    }
}

void ParticleRenderer::Shutdown(RhiDevice& device) {
    if (quad_vbo_) device.DeleteGpuBuffer(quad_vbo_);
    if (quad_ibo_) device.DeleteGpuBuffer(quad_ibo_);
    if (per_frame_ubo_) device.DeleteGpuBuffer(per_frame_ubo_);
    if (white_tex_) device.DeleteTexture(white_tex_);
    quad_vbo_ = quad_ibo_ = per_frame_ubo_ = BufferHandle{};
    white_tex_ = 0;
    pso_ = 0;
    init_ = false;
}

} // namespace render
} // namespace dse
