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

// SDF/VFX 路径的 SpriteFx push-block（std140，128B）。vp 供顶点变换，p0..p3 载效果参数：
//   SDF: p0 = (threshold, smoothing, outline_width, shadow_softness)
//   VFX: p0 = gradient_start, p1 = gradient_end,
//        p2 = (rect_w, rect_h, corner_radius, gradient_dir), p3 = (blur_radius, blur_intensity, 0, 0)
struct SpriteFxUBO {
    glm::mat4 vp;
    glm::vec4 p0;
    glm::vec4 p1;
    glm::vec4 p2;
    glm::vec4 p3;
};
static_assert(sizeof(SpriteFxUBO) == 128, "SpriteFxUBO std140 layout = 128 bytes");

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

const SpriteVisualEffect& Vfx(const SpriteDrawItem& i) { return i.visual_effect; }

// 游程合批要求批内所有项的「程序选择 + push-block 参数」一致，否则取代表项参数会出错。
// 故除 texture/material/variant/blend 外，还比较 SDF 与 VFX 全部参数。
bool SameState(const SpriteDrawItem& a, const SpriteDrawItem& b) {
    if (a.texture_handle != b.texture_handle ||
        a.material_instance_id != b.material_instance_id ||
        a.shader_variant_key != b.shader_variant_key ||
        a.blend_mode != b.blend_mode) {
        return false;
    }
    if (a.sdf_threshold != b.sdf_threshold || a.sdf_smoothing != b.sdf_smoothing ||
        a.sdf_outline_width != b.sdf_outline_width || a.sdf_shadow_softness != b.sdf_shadow_softness) {
        return false;
    }
    const SpriteVisualEffect& va = Vfx(a);
    const SpriteVisualEffect& vb = Vfx(b);
    return va.enabled == vb.enabled &&
           va.corner_radius == vb.corner_radius &&
           va.gradient_direction == vb.gradient_direction &&
           va.blur_radius == vb.blur_radius &&
           va.blur_intensity == vb.blur_intensity &&
           va.rect_size == vb.rect_size &&
           va.gradient_start == vb.gradient_start &&
           va.gradient_end == vb.gradient_end;
}

}  // namespace

void SpriteBatchRenderer::EnsureResources(RhiDevice& device, size_t needed_quads) {
    if (!init_) {
        // 白纹理：无纹理项（texture_handle==0）的回退采样源。一次性创建，跨帧复用。
        const unsigned char white[4] = {255, 255, 255, 255};
        white_tex_ = device.CreateTexture2D(1, 1, white, false);
        init_ = true;
    }

    if (needed_quads == 0) needed_quads = 1;

    // 动态顶点缓冲 vbo_ 与 PerFrame 缓冲 ubo_ 改由 Draw 经 PerInFlightBuffer::Acquire
    // 取当前在飞槽位（每帧覆写 → 须每在飞帧缓冲，D9）；此处只建非每帧写的静态资源。

    // 索引缓冲（静态，绝对索引 4i+{0,1,2,0,2,3}）按需重建。非每帧写 → 单缓冲即可。
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

void SpriteBatchRenderer::EnsureFxUbos(RhiDevice& device, size_t needed) {
    // 每 fx 批需独立逻辑缓冲（参数互异，且延迟后端提交前不可覆写）。池跨帧持久、按需增长；
    // 每个 PerInFlightBuffer 的物理槽位在 Acquire 时按当前在飞帧惰性建。
    (void)device;
    if (fx_ubos_.size() < needed) fx_ubos_.resize(needed);
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

    unsigned int sprite_prog = device.GetBuiltinProgram(BuiltinProgram::Sprite2D);
    if (sprite_prog == 0) return;  // 该后端未提供 sprite2d 内建着色器

    EnsureResources(device, items.size());
    if (!ibo_ || white_tex_ == 0) return;

    // 取本在飞帧的动态顶点 / PerFrame 缓冲槽位（2 帧在飞下与上一帧不别名，D9）。
    BufferHandle vbo = vbo_.Acquire(device, sizeof(SpriteVertex) * 4 * items.size(),
                                    GpuBufferUsage::kVertex);
    BufferHandle ubo = ubo_.Acquire(device, sizeof(SpritePerFrameUBO), GpuBufferUsage::kUniform);
    if (!vbo || !ubo) return;

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

    device.UpdateGpuBuffer(vbo, 0, verts.size() * sizeof(SpriteVertex), verts.data());

    SpritePerFrameUBO uniforms{};
    uniforms.vp = projection * view;
    uniforms.view = view;
    device.UpdateGpuBuffer(ubo, 0, sizeof(uniforms), &uniforms);

    static const std::vector<VertexAttr> kAttrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
    };

    // 路径分类：SDF（变体 key）/ VFX（visual_effect.enabled）/ 默认。fx 程序懒取，
    // 缺失（后端未提供）则回退默认程序，绘出带纹理 quad 而非崩溃。
    auto path_of = [&](const Batch& b) -> int {
        const SpriteDrawItem& rep = items[b.start_quad];
        if (b.shader_variant == kSdfVariantKey) return 1;       // SDF
        if (rep.visual_effect.enabled) return 2;                // VFX
        return 0;                                               // 默认
    };

    unsigned int sdf_prog = 0, vfx_prog = 0;
    size_t fx_batch_count = 0;
    for (const Batch& b : batches) if (path_of(b) != 0) ++fx_batch_count;
    if (fx_batch_count > 0) {
        sdf_prog = device.GetBuiltinProgram(BuiltinProgram::SpriteFxSdf);
        vfx_prog = device.GetBuiltinProgram(BuiltinProgram::SpriteFxVfx);
        EnsureFxUbos(device, fx_batch_count);
    }

    size_t fx_idx = 0;
    for (const Batch& b : batches) {
        const unsigned int blend = (b.shader_variant == kAdditiveVariantKey) ? 1u : b.blend_mode;
        const unsigned int pso = PsoForBlend(device, blend);

        const int path = path_of(b);
        unsigned int prog = sprite_prog;
        unsigned int ubo_handle = ubo.raw();

        if (path != 0) {
            const SpriteDrawItem& rep = items[b.start_quad];
            unsigned int fx_prog = (path == 1) ? sdf_prog : vfx_prog;
            if (fx_prog != 0) {
                SpriteFxUBO fx{};
                fx.vp = uniforms.vp;
                if (path == 1) {  // SDF
                    fx.p0 = glm::vec4(rep.sdf_threshold, rep.sdf_smoothing,
                                      rep.sdf_outline_width, rep.sdf_shadow_softness);
                } else {          // VFX
                    const SpriteVisualEffect& ve = rep.visual_effect;
                    fx.p0 = ve.gradient_start;
                    fx.p1 = ve.gradient_end;
                    fx.p2 = glm::vec4(ve.rect_size.x, ve.rect_size.y,
                                      ve.corner_radius, ve.gradient_direction);
                    fx.p3 = glm::vec4(ve.blur_radius, ve.blur_intensity, 0.0f, 0.0f);
                }
                BufferHandle fx_ubo = fx_ubos_[fx_idx++].Acquire(
                    device, sizeof(SpriteFxUBO), GpuBufferUsage::kUniform);
                device.UpdateGpuBuffer(fx_ubo, 0, sizeof(fx), &fx);
                prog = fx_prog;
                ubo_handle = fx_ubo.raw();
            }
            // fx_prog==0：保持默认程序 + PerFrame UBO（回退）。
        }

        cmd.BindPipeline(device.GetGraphicsPipeline(pso, prog));
        cmd.BindUniformBuffer(0u, ubo_handle);
        cmd.BindTexture(0u, b.texture, TextureDim::Tex2D);
        cmd.BindVertexBuffer(0u, vbo.raw(), static_cast<uint32_t>(sizeof(SpriteVertex)), kAttrs);
        cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
        cmd.DrawIndexed(static_cast<uint32_t>(b.quad_count * 6),
                        static_cast<uint32_t>(b.start_quad * 6), 0);
    }
}

void SpriteBatchRenderer::Shutdown(RhiDevice& device) {
    vbo_.Shutdown(device);
    ubo_.Shutdown(device);
    for (PerInFlightBuffer& b : fx_ubos_) b.Shutdown(device);
    fx_ubos_.clear();
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    if (white_tex_) device.DeleteTexture(white_tex_);
    ibo_ = BufferHandle{};
    white_tex_ = 0;
    ibo_cap_quads_ = 0;
    init_ = false;
}

}  // namespace render
}  // namespace dse
