/**
 * @file sprite_batch_renderer.cpp
 * @brief SpriteBatchRenderer 实现 — 见头文件说明。
 */

#include "engine/render/sprite_batch_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstddef>
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

// Sprite3D 的 PerFrame UBO（std140，160B）。比 2D 版少 foliage，但多一个 viewport 字段，
// 因为 Screen billboard 模式在裁剪空间做像素偏移需要视口尺寸。
struct Sprite3DPerFrameUBO {
    glm::mat4 vp;
    glm::mat4 view;
    glm::vec4 camera_pos;
    glm::vec4 viewport;  // xy = width/height, zw = reserved
};
static_assert(sizeof(Sprite3DPerFrameUBO) == 160, "Sprite3DPerFrameUBO std140 layout = 160 bytes");

// Sprite3D 顶点：CPU 只做去 camera_offset 和打包，billboard 展开在顶点着色器中完成。
// 属性 location 必须与 shaders/src/sprite3d.vert 保持一致。
struct Sprite3DVertex {
    float px, py, pz;      // world position (already camera-relative), location 0
    float cx, cy;          // local quad corner: x in [-0.5,0.5], y in [0,1], location 1
    float sw, sh;          // world size, location 2
    float anchor;          // anchor_y, location 3
    float billboard;       // 0=None 1=Yaw 2=YawPitch 3=Screen, location 4
    float r, g, b, a;      // tint * opacity, location 5
    float u, v;            // UV, location 6
    float ax, ay, az;      // None-mode local right axis, location 7
    float bx, by, bz;      // None-mode local up axis, location 8
    float zoff;            // world-space z offset, location 9
};
static_assert(sizeof(Sprite3DVertex) == 88, "Sprite3DVertex must be tightly packed (22 floats)");

const glm::vec2 kSprite3DCorner[4] = {
    {-0.5f, 0.0f},
    { 0.5f, 0.0f},
    { 0.5f, 1.0f},
    {-0.5f, 1.0f},
};

struct Batch3D {
    size_t start_quad = 0;
    size_t quad_count = 0;
    TextureHandle texture;
    unsigned int blend_mode = 0;
};

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
    TextureHandle texture;
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
        a.sdf_outline_width != b.sdf_outline_width || a.sdf_shadow_softness != b.sdf_shadow_softness ||
        a.sdf_outline_color != b.sdf_outline_color) {
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

PipelineHandle SpriteBatchRenderer::PsoForBlend(RhiDevice& device, unsigned int blend_mode) {
    auto make = [&](BlendFactor src, BlendFactor dst) {
        PipelineStateDesc desc{};
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
        PipelineStateDesc desc{};
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

PipelineHandle SpriteBatchRenderer::PsoForBlend3D(RhiDevice& device, unsigned int blend_mode,
                                                  bool foreground) {
    auto make = [&](BlendFactor src, BlendFactor dst) {
        PipelineStateDesc desc{};
        desc.blend_enabled = true;
        desc.blend_src = src;
        desc.blend_dst = dst;
        desc.alpha_blend_src = src;
        desc.alpha_blend_dst = dst;
        // Normal Sprite3D items participate in scene depth. Foreground items
        // are an explicit overlay layer: depth test/write off so a physically
        // farther canopy/roof can still cover a closer character, matching the
        // "sorting_bias < 0 => foreground" design contract.
        desc.depth_test_enabled = !foreground;
        desc.depth_write_enabled = !foreground;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = false;
        return device.CreatePipelineState(desc);
    };
    if (foreground) {
        if (blend_mode == 1u) {  // additive
            if (!pso3d_fg_additive_) pso3d_fg_additive_ = make(BlendFactor::SrcAlpha, BlendFactor::One);
            return pso3d_fg_additive_;
        }
        if (blend_mode == 2u) {  // multiply
            if (!pso3d_fg_multiply_) pso3d_fg_multiply_ = make(BlendFactor::DstColor, BlendFactor::Zero);
            return pso3d_fg_multiply_;
        }
        if (!pso3d_fg_alpha_) {
            pso3d_fg_alpha_ = make(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha);
        }
        return pso3d_fg_alpha_;
    }
    if (blend_mode == 1u) {  // additive
        if (!pso3d_additive_) pso3d_additive_ = make(BlendFactor::SrcAlpha, BlendFactor::One);
        return pso3d_additive_;
    }
    if (blend_mode == 2u) {  // multiply
        if (!pso3d_multiply_) pso3d_multiply_ = make(BlendFactor::DstColor, BlendFactor::Zero);
        return pso3d_multiply_;
    }
    // Alpha-test default. SrcAlpha/OneMinusSrcAlpha per M1 task; transparent
    // pixels are discarded in the fragment shader so they do not write depth.
    if (!pso3d_alpha_) {
        pso3d_alpha_ = make(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha);
    }
    return pso3d_alpha_;
}
void SpriteBatchRenderer::Draw(CommandBuffer& cmd, RhiDevice& device,
                               const std::vector<SpriteDrawItem>& items,
                               const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) return;

    ShaderHandle sprite_prog = device.GetBuiltinProgram(BuiltinProgram::Sprite2D);
    if (!sprite_prog) return;  // 该后端未提供 sprite2d 内建着色器

    EnsureResources(device, items.size());
    if (!ibo_ || !white_tex_) return;

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
        const TextureHandle tex = item.texture_handle ? item.texture_handle : white_tex_;

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

    ShaderHandle sdf_prog, vfx_prog;
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
        const PipelineHandle pso = PsoForBlend(device, blend);

        const int path = path_of(b);
        ShaderHandle prog = sprite_prog;
        BufferHandle ubo_handle = ubo;

        if (path != 0) {
            const SpriteDrawItem& rep = items[b.start_quad];
            ShaderHandle fx_prog = (path == 1) ? sdf_prog : vfx_prog;
            if (fx_prog) {
                SpriteFxUBO fx{};
                fx.vp = uniforms.vp;
                if (path == 1) {  // SDF
                    fx.p0 = glm::vec4(rep.sdf_threshold, rep.sdf_smoothing,
                                      rep.sdf_outline_width, rep.sdf_shadow_softness);
                    fx.p1 = rep.sdf_outline_color;  // 描边颜色（sprite_fx_sdf.frag 从 p1 读取）
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
                ubo_handle = fx_ubo;
            }
            // fx_prog==0：保持默认程序 + PerFrame UBO（回退）。
        }

        cmd.BindPipeline(device.GetGraphicsPipeline(pso, prog));
        cmd.BindUniformBuffer(0u, ubo_handle);
        cmd.BindTexture(0u, b.texture, TextureDim::Tex2D);
        cmd.BindVertexBuffer(0u, vbo, static_cast<uint32_t>(sizeof(SpriteVertex)), kAttrs);
        cmd.BindIndexBuffer(ibo_, IndexType::UInt16);
        cmd.DrawIndexed(static_cast<uint32_t>(b.quad_count * 6),
                        static_cast<uint32_t>(b.start_quad * 6), 0);
    }
}

void SpriteBatchRenderer::DrawSprite3D(CommandBuffer& cmd, RhiDevice& device,
                                       const std::vector<SpriteDrawItem>& items,
                                       const glm::mat4& view, const glm::mat4& projection,
                                       const glm::vec2& viewport_size,
                                       const glm::vec3& camera_offset,
                                       bool foreground) {
    if (items.empty()) return;

    ShaderHandle sprite_prog = device.GetBuiltinProgram(BuiltinProgram::Sprite3D);
    if (!sprite_prog) return;  // backend does not provide Sprite3D yet

    EnsureResources(device, items.size());
    if (!ibo_ || !white_tex_) return;

    BufferHandle vbo = vbo3d_.Acquire(device, sizeof(Sprite3DVertex) * 4 * items.size(),
                                      GpuBufferUsage::kVertex);
    BufferHandle ubo = ubo3d_.Acquire(device, sizeof(Sprite3DPerFrameUBO), GpuBufferUsage::kUniform);
    if (!vbo || !ubo) return;

    std::vector<Sprite3DVertex> verts;
    verts.reserve(items.size() * 4);
    std::vector<Batch3D> batches;

    for (size_t i = 0; i < items.size(); ++i) {
        const SpriteDrawItem& item = items[i];
        const TextureHandle tex = item.texture_handle ? item.texture_handle : white_tex_;

        if (batches.empty() || batches.back().texture != tex ||
            batches.back().blend_mode != item.blend_mode) {
            Batch3D b;
            b.start_quad = i;
            b.quad_count = 0;
            b.texture = tex;
            b.blend_mode = item.blend_mode;
            batches.push_back(b);
        }
        batches.back().quad_count += 1;

        glm::vec3 base = glm::vec3(item.model[3]) - camera_offset;

        glm::vec3 axis_x(1.0f, 0.0f, 0.0f);
        glm::vec3 axis_y(0.0f, 1.0f, 0.0f);
        if (item.sprite3d_billboard == 0) {
            const glm::vec3 mx(item.model[0]);
            const glm::vec3 my(item.model[1]);
            const float lx = glm::length(mx);
            const float ly = glm::length(my);
            if (lx > 1.0e-6f) axis_x = mx / lx;
            if (ly > 1.0e-6f) axis_y = my / ly;
        }

        const float u0 = item.uv.x;
        const float v0 = item.uv.y;
        const float u1 = item.uv.z;
        const float v1 = item.uv.w;
        const glm::vec2 uvs[4] = {
            {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1},
        };

        for (int k = 0; k < 4; ++k) {
            Sprite3DVertex v;
            v.px = base.x; v.py = base.y; v.pz = base.z;
            v.cx = kSprite3DCorner[k].x;
            v.cy = kSprite3DCorner[k].y;
            v.sw = item.sprite3d_size.x;
            v.sh = item.sprite3d_size.y;
            v.anchor = item.sprite3d_anchor_y;
            v.billboard = static_cast<float>(item.sprite3d_billboard);
            v.r = item.color.r; v.g = item.color.g;
            v.b = item.color.b; v.a = item.color.a;
            v.u = uvs[k].x; v.v = uvs[k].y;
            v.ax = axis_x.x; v.ay = axis_x.y; v.az = axis_x.z;
            v.bx = axis_y.x; v.by = axis_y.y; v.bz = axis_y.z;
            v.zoff = item.sprite3d_z_offset;
            verts.push_back(v);
        }
    }

    device.UpdateGpuBuffer(vbo, 0, verts.size() * sizeof(Sprite3DVertex), verts.data());

    glm::vec2 vp = viewport_size;
    if (vp.x <= 0.0f || vp.y <= 0.0f) vp = glm::vec2(1.0f, 1.0f);
    Sprite3DPerFrameUBO uniforms{};
    uniforms.vp = projection * view;
    uniforms.view = view;
    uniforms.camera_pos = glm::vec4(0.0f);
    uniforms.viewport = glm::vec4(vp.x, vp.y, 0.0f, 0.0f);
    device.UpdateGpuBuffer(ubo, 0, sizeof(uniforms), &uniforms);

    static const std::vector<VertexAttr> kAttrs3D = {
        VertexAttr{0u, 3u, static_cast<uint32_t>(offsetof(Sprite3DVertex, px))},
        VertexAttr{1u, 2u, static_cast<uint32_t>(offsetof(Sprite3DVertex, cx))},
        VertexAttr{2u, 2u, static_cast<uint32_t>(offsetof(Sprite3DVertex, sw))},
        VertexAttr{3u, 1u, static_cast<uint32_t>(offsetof(Sprite3DVertex, anchor))},
        VertexAttr{4u, 1u, static_cast<uint32_t>(offsetof(Sprite3DVertex, billboard))},
        VertexAttr{5u, 4u, static_cast<uint32_t>(offsetof(Sprite3DVertex, r))},
        VertexAttr{6u, 2u, static_cast<uint32_t>(offsetof(Sprite3DVertex, u))},
        VertexAttr{7u, 3u, static_cast<uint32_t>(offsetof(Sprite3DVertex, ax))},
        VertexAttr{8u, 3u, static_cast<uint32_t>(offsetof(Sprite3DVertex, bx))},
        VertexAttr{9u, 1u, static_cast<uint32_t>(offsetof(Sprite3DVertex, zoff))},
    };

    for (const Batch3D& b : batches) {
        const PipelineHandle pso = PsoForBlend3D(device, b.blend_mode, foreground);
        cmd.BindPipeline(device.GetGraphicsPipeline(pso, sprite_prog));
        cmd.BindUniformBuffer(0u, ubo);
        cmd.BindTexture(0u, b.texture, TextureDim::Tex2D);
        cmd.BindVertexBuffer(0u, vbo, static_cast<uint32_t>(sizeof(Sprite3DVertex)), kAttrs3D);
        cmd.BindIndexBuffer(ibo_, IndexType::UInt16);
        cmd.DrawIndexed(static_cast<uint32_t>(b.quad_count * 6),
                        static_cast<uint32_t>(b.start_quad * 6), 0);
    }
}

void SpriteBatchRenderer::Shutdown(RhiDevice& device) {
    vbo_.Shutdown(device);
    ubo_.Shutdown(device);
    vbo3d_.Shutdown(device);
    ubo3d_.Shutdown(device);
    for (PerInFlightBuffer& b : fx_ubos_) b.Shutdown(device);
    fx_ubos_.clear();
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    if (white_tex_) device.DeleteTexture(white_tex_);
    ibo_ = BufferHandle{};
    white_tex_ = {};
    ibo_cap_quads_ = 0;
    init_ = false;
}

}  // namespace render
}  // namespace dse
