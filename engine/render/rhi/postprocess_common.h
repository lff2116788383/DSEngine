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
#include <cstddef>

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

} // namespace render
} // namespace dse

#endif // DSE_RENDER_POSTPROCESS_COMMON_H
