/**
 * @file sprite_batch_renderer.cpp
 * @brief SpriteBatchRenderer 实现 — 见头文件说明。
 */

#include "engine/render/sprite_batch_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstring>
#include <functional>
#include <string>

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

// 着色器变体 key（与 sprite_render_system / draw_executor 一致）。
const unsigned int kSdfVariantKey =
    static_cast<unsigned int>(std::hash<std::string>{}("TEXT_SDF"));
const unsigned int kAdditiveVariantKey =
    static_cast<unsigned int>(std::hash<std::string>{}("SPRITE_ADDITIVE"));

// 单位 quad（与 GLDrawExecutor::DrawBatch 同序：左下→右下→右上→左上）。
const glm::vec4 kQuadPos[4] = {
    {-0.5f, -0.5f, 0.0f, 1.0f},
    { 0.5f, -0.5f, 0.0f, 1.0f},
    { 0.5f,  0.5f, 0.0f, 1.0f},
    {-0.5f,  0.5f, 0.0f, 1.0f},
};
const glm::vec2 kQuadUv[4] = {
    {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
};

// 一个游程批：连续同态的 [start_quad, start_quad+quad_count) 项。
struct Batch {
    size_t start_quad = 0;
    size_t quad_count = 0;
    unsigned int texture = 0;
    unsigned int blend_mode = 0;
    unsigned int shader_variant = 0;
};

bool SameState(const SpriteDrawItem& a, const SpriteDrawItem& b) {
    return a.texture_handle == b.texture_handle &&
           a.material_instance_id == b.material_instance_id &&
           a.shader_variant_key == b.shader_variant_key &&
           a.blend_mode == b.blend_mode;
}

}  // namespace

void SpriteBatchRenderer::EnsureResources(RhiDevice& device, size_t needed_quads) {
    if (!init_) {
        // 白纹理：无纹理项（texture_handle==0）的回退采样源。
        const unsigned char white[4] = {255, 255, 255, 255};
        white_tex_ = device.CreateTexture2D(1, 1, white, false);

        GpuBufferDesc ub_desc;
        ub_desc.size = sizeof(SpritePerFrameUBO);
        ub_desc.usage = GpuBufferUsage::kUniform;
        ub_desc.is_dynamic = true;
        ubo_ = device.CreateGpuBuffer(ub_desc, nullptr);

        init_ = true;
    }

    if (needed_quads == 0) needed_quads = 1;

    // 顶点缓冲按需扩容（动态）。
    if (!vbo_ || needed_quads > vbo_cap_quads_) {
        if (vbo_) device.DeleteGpuBuffer(vbo_);
        GpuBufferDesc vb_desc;
        vb_desc.size = sizeof(SpriteVertex) * 4 * needed_quads;
        vb_desc.usage = GpuBufferUsage::kVertex;
        vb_desc.is_dynamic = true;
        vbo_ = device.CreateGpuBuffer(vb_desc, nullptr);
        vbo_cap_quads_ = needed_quads;
    }

    // 索引缓冲（静态，绝对索引 4i+{0,1,2,0,2,3}）按需重建。
    if (!ibo_ || needed_quads > ibo_cap_quads_) {
        if (ibo_) device.DeleteGpuBuffer(ibo_);
        std::vector<uint16_t> indices;
        const size_t cap = needed_quads;
        indices.reserve(cap * 6);
        for (size_t q = 0; q < cap; ++q) {
            const uint16_t base = static_cast<uint16_t>(q * 4);
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
        GpuBufferDesc ib_desc;
        ib_desc.size = indices.size() * sizeof(uint16_t);
        ib_desc.usage = GpuBufferUsage::kIndex;
        ibo_ = device.CreateGpuBuffer(ib_desc, indices.data());
        ibo_cap_quads_ = needed_quads;
    }
}

unsigned int SpriteBatchRenderer::PsoForBlend(RhiDevice& device, unsigned int blend_mode) {
    auto make = [&](BlendFactor src, BlendFactor dst) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = src;
        desc.blend_dst = dst;
        desc.alpha_blend_src = src;
        desc.alpha_blend_dst = dst;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        return device.CreatePipelineState(desc);
    };
    if (blend_mode == 1) {  // additive
        if (!pso_additive_) pso_additive_ = make(BlendFactor::SrcAlpha, BlendFactor::One);
        return pso_additive_;
    }
    if (blend_mode == 2) {  // multiply
        if (!pso_multiply_) pso_multiply_ = make(BlendFactor::DstColor, BlendFactor::Zero);
        return pso_multiply_;
    }
    // alpha（默认）：分离 alpha 通道 src=One，与 DrawBatch 的
    // glBlendFuncSeparate(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA) 一致。
    if (!pso_alpha_) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::SrcAlpha;
        desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.alpha_blend_src = BlendFactor::One;
        desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        pso_alpha_ = device.CreatePipelineState(desc);
    }
    return pso_alpha_;
}

void SpriteBatchRenderer::Draw(CommandBuffer& cmd, RhiDevice& device,
                               const std::vector<SpriteDrawItem>& items,
                               const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) return;

    unsigned int sprite_prog = device.GetSprite2DShaderProgram();
    if (sprite_prog == 0) return;  // 该后端未提供 sprite2d 内建着色器

    EnsureResources(device, items.size());
    if (!vbo_ || !ibo_ || !ubo_ || white_tex_ == 0) return;

    // === 顶点：一次性构建整批（避免延迟后端的 VB 复用别名）===
    std::vector<SpriteVertex> verts;
    verts.reserve(items.size() * 4);
    std::vector<Batch> batches;

    for (size_t i = 0; i < items.size(); ++i) {
        const SpriteDrawItem& item = items[i];
        const unsigned int tex = item.texture_handle == 0 ? white_tex_ : item.texture_handle;

        if (batches.empty() || !SameState(items[batches.back().start_quad], item)) {
            Batch b;
            b.start_quad = i;
            b.quad_count = 0;
            b.texture = tex;
            b.blend_mode = item.blend_mode;
            b.shader_variant = item.shader_variant_key;
            batches.push_back(b);
        }
        batches.back().quad_count += 1;

        // UV 子矩形（与 DrawBatch 同逻辑）。
        glm::vec2 uvs[4];
        if (item.uv.z > 0.0f && item.uv.w > 0.0f) {
            const bool use_max_uv = item.uv.z > item.uv.x && item.uv.w > item.uv.y;
            const float u1 = use_max_uv ? item.uv.z : (item.uv.x + item.uv.z);
            const float v1 = use_max_uv ? item.uv.w : (item.uv.y + item.uv.w);
            uvs[0] = {item.uv.x, item.uv.y};
            uvs[1] = {u1, item.uv.y};
            uvs[2] = {u1, v1};
            uvs[3] = {item.uv.x, v1};
        } else {
            for (int k = 0; k < 4; ++k) uvs[k] = kQuadUv[k];
        }

        for (int k = 0; k < 4; ++k) {
            const glm::vec4 wp = item.model * kQuadPos[k];
            SpriteVertex v;
            v.px = wp.x; v.py = wp.y; v.pz = wp.z;
            v.r = item.color.r; v.g = item.color.g; v.b = item.color.b; v.a = item.color.a;
            v.u = uvs[k].x; v.v = uvs[k].y;
            verts.push_back(v);
        }
    }

    device.UpdateGpuBuffer(vbo_, 0, verts.size() * sizeof(SpriteVertex), verts.data());

    SpritePerFrameUBO uniforms{};
    uniforms.vp = projection * view;
    uniforms.view = view;
    device.UpdateGpuBuffer(ubo_, 0, sizeof(uniforms), &uniforms);

    static const std::vector<VertexAttr> kAttrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
    };

    for (const Batch& b : batches) {
        const unsigned int blend = (b.shader_variant == kAdditiveVariantKey) ? 1u : b.blend_mode;
        cmd.SetPipelineState(PsoForBlend(device, blend));
        cmd.BindShaderProgram(sprite_prog);
        cmd.BindUniformBuffer(0u, ubo_.raw());
        cmd.BindTexture(0u, b.texture, TextureDim::Tex2D);
        cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(SpriteVertex)), kAttrs);
        cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
        cmd.DrawIndexed(static_cast<uint32_t>(b.quad_count * 6),
                        static_cast<uint32_t>(b.start_quad * 6), 0);
    }
}

void SpriteBatchRenderer::Shutdown(RhiDevice& device) {
    if (vbo_) device.DeleteGpuBuffer(vbo_);
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    if (ubo_) device.DeleteGpuBuffer(ubo_);
    if (sdf_ubo_) device.DeleteGpuBuffer(sdf_ubo_);
    if (vfx_ubo_) device.DeleteGpuBuffer(vfx_ubo_);
    if (white_tex_) device.DeleteTexture(white_tex_);
    vbo_ = ibo_ = ubo_ = sdf_ubo_ = vfx_ubo_ = BufferHandle{};
    white_tex_ = 0;
    vbo_cap_quads_ = ibo_cap_quads_ = 0;
    init_ = false;
}

}  // namespace render
}  // namespace dse
