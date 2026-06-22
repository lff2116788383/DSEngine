/**
 * @file hair_renderer.cpp
 * @brief HairRenderer 实现 — 见头文件说明。
 */

#include "engine/render/hair_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

#include <glm/gtc/matrix_inverse.hpp>

namespace dse {
namespace render {

namespace {

// std140 组合 HairUniforms 块（320 字节），布局须与 hair.vert/hair.frag 的 HairUniforms 一致：
// 3 个 mat4 + 8 个 vec4（标量参数打包进 params0/params1）。
struct HairUniformsUBO {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 camera_pos;    // xyz = camera world pos
    glm::vec4 light_dir;     // xyz = light direction
    glm::vec4 light_color;   // xyz = light color
    glm::vec4 root_color;    // rgba
    glm::vec4 tip_color;     // rgba
    glm::vec4 spec_color;    // xyz = specular color
    glm::vec4 params0;       // x=light_intensity y=ambient_intensity z=opacity w=spec_primary
    glm::vec4 params1;       // x=spec_secondary y=spec_strength1 z=spec_strength2 w=pad
};
static_assert(sizeof(HairUniformsUBO) == 320, "HairUniformsUBO std140 = 320 bytes");

}  // namespace

void HairRenderer::EnsureResources(RhiDevice& device) {
    if (init_) return;

    // 半透明混合 PSO：测深度（被不透明几何遮挡）但不写深度、不剔除（线带双面可见）。
    // 拓扑 LINE_STRIP 烘焙进 PSO，逐 strand 的 Draw 画一条连续线带。
    PipelineStateDesc desc;
    desc.blend_enabled = true;
    desc.blend_src = BlendFactor::SrcAlpha;
    desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    desc.alpha_blend_src = BlendFactor::SrcAlpha;
    desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
    desc.depth_test_enabled = true;
    desc.depth_write_enabled = false;
    desc.culling_enabled = false;
    desc.topology = PrimitiveTopology::LineStrip;
    pso_ = device.CreatePipelineState(desc);

    GpuBufferDesc u_desc;
    u_desc.size = sizeof(HairUniformsUBO);
    u_desc.usage = GpuBufferUsage::kUniform;
    u_desc.is_dynamic = true;
    hair_ubo_ = device.CreateGpuBuffer(u_desc, nullptr);

    init_ = true;
}

void HairRenderer::Draw(CommandBuffer& cmd, RhiDevice& device,
                        const std::vector<HairDrawItem>& items,
                        const glm::mat4& view, const glm::mat4& proj) {
    if (items.empty()) return;
    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::HairStrand);
    if (program == 0) return;  // SSBO 不支持的上下文（如 WebGL2）→ 静默跳过
    EnsureResources(device);
    if (!hair_ubo_) return;

    const glm::vec3 camera_pos = glm::vec3(glm::inverse(view)[3]);

    cmd.SetPipelineState(pso_);
    cmd.BindShaderProgram(program);

    for (const auto& item : items) {
        if (item.strand_count == 0 || item.total_vertex_count == 0) continue;
        if (!item.strand_firsts || !item.strand_counts) continue;
        if (!item.position_ssbo || !item.tangent_ssbo) continue;

        HairUniformsUBO u{};
        u.model = item.world_transform;
        u.view = view;
        u.projection = proj;
        u.camera_pos = glm::vec4(camera_pos, 1.0f);
        u.light_dir = glm::vec4(item.light_direction, 0.0f);
        u.light_color = glm::vec4(item.light_color, 1.0f);
        u.root_color = item.root_color;
        u.tip_color = item.tip_color;
        u.spec_color = glm::vec4(item.specular_color, 1.0f);
        u.params0 = glm::vec4(item.light_intensity, item.ambient_intensity,
                              item.opacity, item.specular_primary);
        u.params1 = glm::vec4(item.specular_secondary, item.specular_strength_primary,
                              item.specular_strength_secondary, 0.0f);
        device.UpdateGpuBuffer(hair_ubo_, 0, sizeof(u), &u);

        // 组合 HairUniforms UBO\@set0.b0（VS/FS 共享）+ position/tangent SSBO\@set7.b0/b1。
        cmd.BindUniformBuffer(0u, hair_ubo_.raw());
        cmd.BindStorageBuffer(0u, item.position_ssbo.raw(), 0u, 0u);
        cmd.BindStorageBuffer(1u, item.tangent_ssbo.raw(), 0u, 0u);

        // 每个 strand 是一条 LINE_STRIP：first_vertex=strand_firsts[s]，count=strand_counts[s]。
        // gl_VertexIndex（DX SV_VertexID / GL gl_VertexID / VK gl_VertexIndex）= first + i，
        // 三后端非索引 Draw 均加 first_vertex，直接命中 SSBO 绝对索引。
        for (uint32_t s = 0; s < item.strand_count; ++s) {
            const int first = item.strand_firsts[s];
            const int count = item.strand_counts[s];
            if (count <= 0) continue;
            cmd.Draw(static_cast<uint32_t>(count), static_cast<uint32_t>(first));
        }
    }
}

void HairRenderer::Shutdown(RhiDevice& device) {
    if (hair_ubo_) device.DeleteGpuBuffer(hair_ubo_);
    hair_ubo_ = BufferHandle{};
    pso_ = 0;
    init_ = false;
}

}  // namespace render
}  // namespace dse
