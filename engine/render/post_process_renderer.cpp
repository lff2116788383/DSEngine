/**
 * @file post_process_renderer.cpp
 * @brief PostProcessRenderer 实现 — 见头文件说明。
 */

#include "engine/render/post_process_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

#include <cstring>

namespace dse {
namespace render {

namespace {

// 全屏 quad 顶点：clip-space pos(vec2) + uv(vec2)，与 postprocess.vert（aPos\@0/aTexCoords\@1）一致。
// 左下→右下→右上→左上；uv 原点 (0,0) 在左下。
struct PPVertex {
    float px, py;
    float u, v;
};
static_assert(sizeof(PPVertex) == 16, "PPVertex must be tightly packed (2+2 floats)");

const PPVertex kQuad[4] = {
    {-1.0f, -1.0f, 0.0f, 0.0f},
    { 1.0f, -1.0f, 1.0f, 0.0f},
    { 1.0f,  1.0f, 1.0f, 1.0f},
    {-1.0f,  1.0f, 0.0f, 1.0f},
};
const uint16_t kQuadIndices[6] = {0, 1, 2, 0, 2, 3};

}  // namespace

void PostProcessRenderer::EnsureResources(RhiDevice& device) {
    if (init_) return;

    GpuBufferDesc vb_desc;
    vb_desc.size = sizeof(kQuad);
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = false;
    quad_vbo_ = device.CreateGpuBuffer(vb_desc, kQuad);

    GpuBufferDesc ib_desc;
    ib_desc.size = sizeof(kQuadIndices);
    ib_desc.usage = GpuBufferUsage::kIndex;
    ib_desc.is_dynamic = false;
    quad_ibo_ = device.CreateGpuBuffer(ib_desc, kQuadIndices);

    init_ = true;
}

unsigned int PostProcessRenderer::PsoFor(RhiDevice& device, bool blend) {
    // 后处理全屏 quad：关背面剔除（否则默认 cull-back 丢整屏）、关深度测试/写入。
    if (blend) {
        if (!pso_blend_) {
            PipelineStateDesc desc;
            desc.blend_enabled = true;
            desc.blend_src = BlendFactor::SrcAlpha;
            desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
            desc.alpha_blend_src = BlendFactor::One;
            desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
            desc.depth_test_enabled = false;
            desc.depth_write_enabled = false;
            desc.culling_enabled = false;
            pso_blend_ = device.CreatePipelineState(desc);
        }
        return pso_blend_;
    }
    if (!pso_opaque_) {
        PipelineStateDesc desc;
        desc.blend_enabled = false;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        pso_opaque_ = device.CreatePipelineState(desc);
    }
    return pso_opaque_;
}

BufferHandle PostProcessRenderer::NextParamUbo(RhiDevice& device, size_t bytes) {
    if (param_ubo_next_ >= param_ubos_.size()) {
        GpuBufferDesc desc;
        // 固定上限尺寸，避免按 effect 频繁重建；std140 参数块远小于此。
        desc.size = 256;
        desc.usage = GpuBufferUsage::kUniform;
        desc.is_dynamic = true;
        param_ubos_.push_back(device.CreateGpuBuffer(desc, nullptr));
    }
    BufferHandle h = param_ubos_[param_ubo_next_++];
    (void)bytes;
    return h;
}

void PostProcessRenderer::BeginFrame() {
    param_ubo_next_ = 0;
}

bool PostProcessRenderer::Draw(CommandBuffer& cmd, RhiDevice& device,
                               const PostProcessRequest& req) {
    const unsigned int prog = device.GetGenPPShaderProgram(req.effect_name);
    if (prog == 0) return false;  // 该效果尚未迁移到 PostProcessRenderer
    // 注：source_texture==0 是合法的（如 atmosphere_transmittance_lut 仅由参数程序化生成，
    // 不采样输入纹理）；此时跳过主输入绑定即可，不再据此提前返回。

    EnsureResources(device);
    if (!quad_vbo_ || !quad_ibo_) return false;

    cmd.SetPipelineState(PsoFor(device, req.blend_enabled));
    cmd.BindShaderProgram(prog);

    // 参数块 → UBO\@set2.binding0（b0）。仅当效果着色器已采用 UBO 契约（params 非空）时上传。
    if (!req.params.empty()) {
        const size_t bytes = req.params.size() * sizeof(float);
        BufferHandle ubo = NextParamUbo(device, bytes);
        device.UpdateGpuBuffer(ubo, 0, bytes, req.params.data());
        cmd.BindUniformBuffer(0u, ubo.raw());
    }

    // 主输入纹理：GLSL binding=N → t<N-1>（b0 被参数 UBO 占用，纹理从 t0 起）。
    // 无源纹理效果（source_texture==0）跳过该绑定。
    if (req.source_texture != 0) {
        const uint32_t src_slot =
            req.source_binding > 0 ? static_cast<uint32_t>(req.source_binding - 1) : 0u;
        cmd.BindTexture(src_slot, req.source_texture, TextureDim::Tex2D);
    }

    // 额外纹理：slot 存 GLSL binding，同样映射到 t<binding-1>。
    // 3D 纹理（color_grading / tonemapping 的 LUT 等）经 is_3d 标志走 TextureDim::Tex3D。
    for (const PPTextureBinding& t : req.textures) {
        if (t.handle == 0) break;
        const uint32_t slot = t.slot > 0 ? t.slot - 1 : 0u;
        cmd.BindTexture(slot, t.handle, t.is_3d ? TextureDim::Tex3D : TextureDim::Tex2D);
    }

    static const std::vector<VertexAttr> kAttrs = {
        VertexAttr{0u, 2u, 0u},   // pos
        VertexAttr{1u, 2u, 8u},   // uv
    };
    cmd.BindVertexBuffer(quad_vbo_.raw(), static_cast<uint32_t>(sizeof(PPVertex)), kAttrs);
    cmd.BindIndexBuffer(quad_ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(6, 0, 0);
    return true;
}

void PostProcessRenderer::Shutdown(RhiDevice& device) {
    if (quad_vbo_) device.DeleteGpuBuffer(quad_vbo_);
    if (quad_ibo_) device.DeleteGpuBuffer(quad_ibo_);
    for (BufferHandle& h : param_ubos_) {
        if (h) device.DeleteGpuBuffer(h);
    }
    param_ubos_.clear();
    param_ubo_next_ = 0;
    quad_vbo_ = quad_ibo_ = BufferHandle{};
    pso_opaque_ = pso_blend_ = 0;
    init_ = false;
}

}  // namespace render
}  // namespace dse
