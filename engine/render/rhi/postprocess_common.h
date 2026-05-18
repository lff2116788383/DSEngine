/**
 * @file postprocess_common.h
 * @brief 三后端共享的后处理参数解释层
 *
 * 从 GL/DX11/Vulkan DrawExecutor 中提取的完全相同代码：
 * - CompositeParamsView: bloom_composite 效果的参数视图
 * - BloomCompositeParams: 三端共享的 CB/push constant 布局
 *
 * 消除三个 executor 中各自维护的同名 struct，确保参数解释逻辑单源。
 */

#ifndef DSE_RENDER_POSTPROCESS_COMMON_H
#define DSE_RENDER_POSTPROCESS_COMMON_H

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

namespace dse {
namespace render {

// ============================================================
// CompositeParamsView — bloom_composite params vector 索引解释
// ============================================================

struct CompositeParamsView {
    enum Index : std::size_t {
        kBloomTex = 0,
        kExposure,
        kBloomIntensity,
        kBloomEnabled,
        kSsaoTex,
        kAutoExposureTex,
        kLutTex,
        kLutIntensity,
        kContactShadowTex,
        kContactShadowStrength,
        kVignetteEnabled,
        kVignetteIntensity,
        kVignetteRadius,
        kVignetteSoftness,
        kFilmGrainEnabled,
        kFilmGrainIntensity,
        kFilmGrainTime,
        kCount
    };

    explicit CompositeParamsView(const std::vector<float>& in_params)
        : params(in_params) {}

    bool Has(Index index) const {
        return params.size() > static_cast<std::size_t>(index);
    }

    float Float(Index index, float fallback = 0.0f) const {
        return Has(index) ? params[static_cast<std::size_t>(index)] : fallback;
    }

    unsigned int Texture(Index index) const {
        return static_cast<unsigned int>(Float(index, 0.0f));
    }

    bool Flag(Index index) const {
        return Float(index, 0.0f) != 0.0f;
    }

    bool HasRange(Index last_index) const {
        return params.size() >= static_cast<std::size_t>(last_index) + 1;
    }

    const std::vector<float>& params;
};

// ============================================================
// BloomCompositeParams — 三端共享的 CB / push constant 结构体
// 对应 DX11 bloom_composite_params_cb_ 和 Vulkan push constants
// GL 端通过遍历字段 + glUniform* 设置（名称与此一致）
// ============================================================

struct BloomCompositeParams {
    float exposure        = 1.0f;
    float bloom_intensity = 0.5f;
    int   bloom_enabled   = 0;
    int   ssao_enabled    = 0;
    int   ae_enabled      = 0;
    int   lut_enabled     = 0;
    float lut_intensity   = 0.0f;
    int   cs_enabled      = 0;
    float cs_strength     = 0.0f;
    int   vignette_enabled     = 0;
    float vignette_intensity   = 0.0f;
    float vignette_radius      = 0.75f;
    float vignette_softness    = 0.35f;
    int   film_grain_enabled   = 0;
    float film_grain_intensity = 0.0f;
    float film_grain_time      = 0.0f;
};
static_assert(sizeof(BloomCompositeParams) == 64, "BloomCompositeParams must be 64 bytes for CB/push constant alignment");

/// 从 CompositeParamsView 填充 BloomCompositeParams
inline BloomCompositeParams PrepareBloomCompositeParams(const CompositeParamsView& cv) {
    BloomCompositeParams p;
    p.exposure        = cv.Float(CompositeParamsView::kExposure, 1.0f);
    p.bloom_intensity = cv.Float(CompositeParamsView::kBloomIntensity, 0.5f);
    p.bloom_enabled   = (cv.Flag(CompositeParamsView::kBloomEnabled) &&
                         cv.Texture(CompositeParamsView::kBloomTex) != 0) ? 1 : 0;
    p.ssao_enabled    = cv.Texture(CompositeParamsView::kSsaoTex) != 0 ? 1 : 0;
    p.ae_enabled      = cv.Texture(CompositeParamsView::kAutoExposureTex) != 0 ? 1 : 0;
    p.lut_enabled     = cv.Texture(CompositeParamsView::kLutTex) != 0 ? 1 : 0;
    p.lut_intensity   = cv.Float(CompositeParamsView::kLutIntensity, 0.0f);
    p.cs_enabled      = cv.Texture(CompositeParamsView::kContactShadowTex) != 0 ? 1 : 0;
    p.cs_strength     = cv.Float(CompositeParamsView::kContactShadowStrength, 0.0f);
    p.vignette_enabled     = cv.Flag(CompositeParamsView::kVignetteEnabled) ? 1 : 0;
    p.vignette_intensity   = cv.Float(CompositeParamsView::kVignetteIntensity, 0.0f);
    p.vignette_radius      = cv.Float(CompositeParamsView::kVignetteRadius, 0.75f);
    p.vignette_softness    = cv.Float(CompositeParamsView::kVignetteSoftness, 0.35f);
    p.film_grain_enabled   = cv.Flag(CompositeParamsView::kFilmGrainEnabled) ? 1 : 0;
    p.film_grain_intensity = cv.Float(CompositeParamsView::kFilmGrainIntensity, 0.0f);
    p.film_grain_time      = cv.Float(CompositeParamsView::kFilmGrainTime, 0.0f);
    return p;
}

// ============================================================
// PostProcessRequest — 三端统一的后处理效果请求描述
//
// 将纹理句柄与着色器参数彻底分离，消除 vector<float> 位置语义：
// - source_texture / source_binding: 主输入纹理（多数效果 binding=1，light_shaft binding=0）
// - textures[]: 额外纹理（显式声明 slot + handle）
// - params:     纯着色器 uniform 数据（不再混入纹理句柄）
// - blend_enabled: 是否启用混合（decal / water / wboit_composite / ui_overlay）
// ============================================================

static constexpr int kMaxPPTextures = 8;

struct PPTextureBinding {
    uint32_t slot    = 0;   ///< 绑定点（对应 GLSL layout(binding=N) / HLSL tN / VK set2 binding）
    unsigned int handle = 0; ///< 纹理句柄，0 表示空
};

struct PostProcessRequest {
    std::string effect_name;
    unsigned int source_texture = 0;
    int source_binding          = 1;    ///< 主纹理绑定点（light_shaft = 0，其余 = 1）

    PPTextureBinding textures[kMaxPPTextures] = {};  ///< 额外纹理，遇到 handle==0 终止

    std::vector<float> params;          ///< 纯着色器 uniform 参数（不含纹理句柄）

    bool blend_enabled = false;         ///< 是否启用 alpha 混合

    PostProcessRequest() = default;

    /// 便利构造：PostProcessRequest("effect", src_tex, {p0, p1, ...})
    PostProcessRequest(std::string name, unsigned int src,
                       std::vector<float> p = {}, bool blend = false, int src_bind = 1)
        : effect_name(std::move(name)), source_texture(src),
          source_binding(src_bind), params(std::move(p)), blend_enabled(blend) {}

    /// 便利方法：添加一个额外纹理绑定
    PostProcessRequest& Tex(uint32_t slot, unsigned int handle) {
        for (auto& t : textures) {
            if (t.handle == 0) { t = {slot, handle}; return *this; }
        }
        return *this;
    }
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_POSTPROCESS_COMMON_H
