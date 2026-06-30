/**
 * @file impostor_renderer.cpp
 * @brief ImpostorRenderer 实现 — 见头文件说明。
 */

#include "engine/render/impostor/impostor_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace dse {
namespace render {

namespace {

// std140 PerFrame 块（176 字节），布局须与 impostor.vert 的 PerFrame 一致
struct ImpostorPerFrameUBO {
    glm::mat4 vp;
    glm::mat4 view;
    glm::vec4 camera_pos;
    glm::vec4 foliage_wind;  // 占位
    glm::vec4 foliage_push;  // 占位
};
static_assert(sizeof(ImpostorPerFrameUBO) == 176, "ImpostorPerFrameUBO std140 = 176 bytes");

// ImpostorParams UBO（32 字节）
struct ImpostorParamsUBO {
    glm::vec4 light_dir;      // xyz = light dir, w = normal_strength
    glm::vec4 ambient_color;  // xyz = ambient, w = alpha_cutoff
};
static_assert(sizeof(ImpostorParamsUBO) == 32, "ImpostorParamsUBO std140 = 32 bytes");

struct QuadVertex {
    float px, py, pz;
    float u, v;
};
static_assert(sizeof(QuadVertex) == 20, "QuadVertex must be tightly packed");

// 每实例 SSBO 步长：std430 { vec4 pos_size; vec4 frame_info; vec4 pivot_fade; } = 48 字节。
struct ImpostorGpuInstance {
    glm::vec4 pos_size;    // xyz=world pos, w=half_size
    glm::vec4 frame_info;  // x=frame_x, y=frame_y, z=total_x, w=total_y
    glm::vec4 pivot_fade;  // xyz=pivot_offset, w=fade
};
static_assert(sizeof(ImpostorGpuInstance) == 48, "ImpostorGpuInstance std430 = 48 bytes");

constexpr int kInitialCapacity = 256;

}  // namespace

void ImpostorRenderer::EnsureResources(RhiDevice& device) {
    if (init_) return;

    // Alpha 混合 PSO：测深度但不写深度（billboard 透明边缘需混合）、不剔除
    PipelineStateDesc desc;
    desc.blend_enabled = true;
    desc.blend_src = BlendFactor::SrcAlpha;
    desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    desc.alpha_blend_src = BlendFactor::One;
    desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
    desc.depth_test_enabled = true;
    desc.depth_write_enabled = false;
    desc.culling_enabled = false;
    pso_ = device.CreatePipelineState(desc);

    // 1x1 白纹理回退
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
    f_desc.size = sizeof(ImpostorPerFrameUBO);
    f_desc.usage = GpuBufferUsage::kUniform;
    f_desc.is_dynamic = true;
    per_frame_ubo_ = device.CreateGpuBuffer(f_desc, nullptr);

    GpuBufferDesc p_desc;
    p_desc.size = sizeof(ImpostorParamsUBO);
    p_desc.usage = GpuBufferUsage::kUniform;
    p_desc.is_dynamic = true;
    params_ubo_ = device.CreateGpuBuffer(p_desc, nullptr);

    // 预分配实例 SSBO
    GpuBufferDesc s_desc;
    s_desc.size = static_cast<uint32_t>(kInitialCapacity * sizeof(ImpostorGpuInstance));
    s_desc.usage = GpuBufferUsage::kStorage;
    s_desc.is_dynamic = true;
    instance_ssbo_ = device.CreateGpuBuffer(s_desc, nullptr);
    instance_ssbo_capacity_ = kInitialCapacity;

    init_ = true;
}

void ImpostorRenderer::DrawImpostors(CommandBuffer& cmd, RhiDevice& device,
                                     const std::vector<ImpostorBatchItem>& batches,
                                     const glm::mat4& view, const glm::mat4& proj,
                                     const glm::vec3& camera_pos,
                                     const glm::vec3& light_dir,
                                     const glm::vec3& ambient_color) {
    if (batches.empty()) return;
    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::Impostor);
    if (program == 0) return;  // 不支持 → 静默跳过
    EnsureResources(device);
    if (!per_frame_ubo_ || !quad_vbo_ || !quad_ibo_) return;

    // 更新 PerFrame UBO
    ImpostorPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},   // aPos
        VertexAttr{1u, 2u, 12u},  // aTexCoord
    };

    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());  // PerFrame @ set0.b0
    cmd.BindVertexBuffer(0u, quad_vbo_.raw(), static_cast<uint32_t>(sizeof(QuadVertex)), attrs);
    cmd.BindIndexBuffer(quad_ibo_.raw(), IndexType::UInt16);

    for (const auto& batch : batches) {
        if (batch.instances.empty()) continue;
        const int count = static_cast<int>(batch.instances.size());

        // 确保 SSBO 容量
        if (count > instance_ssbo_capacity_) {
            if (instance_ssbo_) device.DeleteGpuBuffer(instance_ssbo_);
            GpuBufferDesc s_desc;
            s_desc.size = static_cast<uint32_t>(count * sizeof(ImpostorGpuInstance));
            s_desc.usage = GpuBufferUsage::kStorage;
            s_desc.is_dynamic = true;
            instance_ssbo_ = device.CreateGpuBuffer(s_desc, nullptr);
            instance_ssbo_capacity_ = count;
        }

        // 填充 GPU 实例数据
        std::vector<ImpostorGpuInstance> gpu_data(count);
        for (int i = 0; i < count; ++i) {
            const auto& src = batch.instances[i];
            gpu_data[i].pos_size = glm::vec4(src.world_position, src.half_size);
            gpu_data[i].frame_info = glm::vec4(
                static_cast<float>(src.frame_x),
                static_cast<float>(src.frame_y),
                static_cast<float>(src.frames_x_total),
                static_cast<float>(src.frames_y_total));
            gpu_data[i].pivot_fade = glm::vec4(src.pivot_offset, src.fade);
        }
        device.UpdateGpuBuffer(instance_ssbo_, 0,
                               static_cast<uint32_t>(count * sizeof(ImpostorGpuInstance)),
                               gpu_data.data());

        // 更新 ImpostorParams UBO
        ImpostorParamsUBO params{};
        params.light_dir = glm::vec4(light_dir, batch.normal_strength);
        params.ambient_color = glm::vec4(ambient_color, batch.alpha_cutoff);
        device.UpdateGpuBuffer(params_ubo_, 0, sizeof(params), &params);
        cmd.BindUniformBuffer(1u, params_ubo_.raw());  // ImpostorParams @ set1.b0

        // 绑定纹理
        cmd.BindTexture(0u, batch.atlas_texture ? batch.atlas_texture : white_tex_, TextureDim::Tex2D);
        cmd.BindTexture(1u, batch.normal_atlas_texture ? batch.normal_atlas_texture : white_tex_, TextureDim::Tex2D);

        // 绑定实例 SSBO
        const uint32_t inst_bytes = static_cast<uint32_t>(count) * static_cast<uint32_t>(sizeof(ImpostorGpuInstance));
        cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, inst_bytes);

        // 绘制
        cmd.DrawIndexedInstanced(6u, static_cast<uint32_t>(count), 0u, 0, 0u);
    }
}

void ImpostorRenderer::Shutdown(RhiDevice& device) {
    if (quad_vbo_) device.DeleteGpuBuffer(quad_vbo_);
    if (quad_ibo_) device.DeleteGpuBuffer(quad_ibo_);
    if (per_frame_ubo_) device.DeleteGpuBuffer(per_frame_ubo_);
    if (params_ubo_) device.DeleteGpuBuffer(params_ubo_);
    if (instance_ssbo_) device.DeleteGpuBuffer(instance_ssbo_);
    if (white_tex_) device.DeleteTexture(white_tex_);
    quad_vbo_ = quad_ibo_ = per_frame_ubo_ = params_ubo_ = instance_ssbo_ = BufferHandle{};
    white_tex_ = 0;
    pso_ = 0;
    instance_ssbo_capacity_ = 0;
    init_ = false;
}

} // namespace render
} // namespace dse
