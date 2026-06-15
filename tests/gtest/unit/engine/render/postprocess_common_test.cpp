/**
 * @file postprocess_common_test.cpp
 * @brief PostProcess 共享参数层纯 CPU 测试
 *
 * 测试策略：
 * - BloomCompositeParams 结构体大小与默认值
 * - CompositeParamsView 索引访问、越界 fallback
 * - PrepareBloomCompositeParams 完整参数传递
 * - PrepareBloomCompositeParams 空 / 不足参数 fallback
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/postprocess_common.h"

using namespace dse::render;

// ============================================================
// BloomCompositeParams 结构体
// ============================================================

// 测试 泛光组合参数：尺寸64
TEST(BloomCompositeParamsTest, Size64) {
    EXPECT_EQ(sizeof(BloomCompositeParams), 64u);
}

// 测试 泛光组合参数：默认值
TEST(BloomCompositeParamsTest, DefaultValues) {
    BloomCompositeParams p;
    EXPECT_FLOAT_EQ(p.exposure, 1.0f);
    EXPECT_FLOAT_EQ(p.bloom_intensity, 0.5f);
    EXPECT_EQ(p.bloom_enabled, 0);
    EXPECT_EQ(p.ssao_enabled, 0);
    EXPECT_EQ(p.ae_enabled, 0);
    EXPECT_EQ(p.lut_enabled, 0);
    EXPECT_FLOAT_EQ(p.lut_intensity, 0.0f);
    EXPECT_EQ(p.cs_enabled, 0);
    EXPECT_FLOAT_EQ(p.cs_strength, 0.0f);
    EXPECT_EQ(p.vignette_enabled, 0);
    EXPECT_FLOAT_EQ(p.vignette_intensity, 0.0f);
    EXPECT_FLOAT_EQ(p.vignette_radius, 0.75f);
    EXPECT_FLOAT_EQ(p.vignette_softness, 0.35f);
    EXPECT_EQ(p.film_grain_enabled, 0);
    EXPECT_FLOAT_EQ(p.film_grain_intensity, 0.0f);
    EXPECT_FLOAT_EQ(p.film_grain_time, 0.0f);
}

// ============================================================
// CompositeParamsView
// ============================================================

// 测试 组合参数视图：空参数拥有返回false
TEST(CompositeParamsViewTest, EmptyParameters_HasReturnsfalse) {
    std::vector<float> empty;
    CompositeParamsView cv(empty);
    EXPECT_FALSE(cv.Has(CompositeParamsView::kBloomTex));
    EXPECT_FALSE(cv.Has(CompositeParamsView::kExposure));
}

// 测试 组合参数视图：空参数浮点Returnsfallback
TEST(CompositeParamsViewTest, EmptyParameters_FloatReturnsfallback) {
    std::vector<float> empty;
    CompositeParamsView cv(empty);
    EXPECT_FLOAT_EQ(cv.Float(CompositeParamsView::kExposure, 2.5f), 2.5f);
    EXPECT_FLOAT_EQ(cv.Float(CompositeParamsView::kBloomIntensity, 0.8f), 0.8f);
}

// 测试 组合参数视图：参数拥有返回true
TEST(CompositeParamsViewTest, Parameters_HasReturnstrue) {
    std::vector<float> params(static_cast<size_t>(CompositeParamsView::kCount), 0.0f);
    CompositeParamsView cv(params);
    EXPECT_TRUE(cv.Has(CompositeParamsView::kBloomTex));
    EXPECT_TRUE(cv.Has(CompositeParamsView::kFilmGrainTime));
    EXPECT_TRUE(cv.HasRange(CompositeParamsView::kFilmGrainTime));
}

// 测试 组合参数视图：纹理且标志
TEST(CompositeParamsViewTest, TextureAndFlag) {
    std::vector<float> params(static_cast<size_t>(CompositeParamsView::kCount), 0.0f);
    params[CompositeParamsView::kBloomTex] = 42.0f;
    params[CompositeParamsView::kBloomEnabled] = 1.0f;
    CompositeParamsView cv(params);
    EXPECT_EQ(cv.Texture(CompositeParamsView::kBloomTex), 42u);
    EXPECT_TRUE(cv.Flag(CompositeParamsView::kBloomEnabled));
    EXPECT_FALSE(cv.Flag(CompositeParamsView::kVignetteEnabled));
}

// ============================================================
// PrepareBloomCompositeParams
// ============================================================

// 测试 准备泛光组合：空参数使用默认值
TEST(PrepareBloomCompositeTest, EmptyParameters_UseDefaultValues) {
    std::vector<float> empty;
    CompositeParamsView cv(empty);
    auto p = PrepareBloomCompositeParams(cv);
    EXPECT_FLOAT_EQ(p.exposure, 1.0f);
    EXPECT_FLOAT_EQ(p.bloom_intensity, 0.5f);
    EXPECT_EQ(p.bloom_enabled, 0);
    EXPECT_EQ(p.ssao_enabled, 0);
    EXPECT_EQ(p.ae_enabled, 0);
    EXPECT_FLOAT_EQ(p.vignette_radius, 0.75f);
    EXPECT_FLOAT_EQ(p.vignette_softness, 0.35f);
}

// 测试 准备泛光组合：参数正确
TEST(PrepareBloomCompositeTest, Parameters_Correct) {
    std::vector<float> params(static_cast<size_t>(CompositeParamsView::kCount), 0.0f);
    params[CompositeParamsView::kBloomTex] = 10.0f;
    params[CompositeParamsView::kExposure] = 2.0f;
    params[CompositeParamsView::kBloomIntensity] = 0.8f;
    params[CompositeParamsView::kBloomEnabled] = 1.0f;
    params[CompositeParamsView::kSsaoTex] = 20.0f;
    params[CompositeParamsView::kAutoExposureTex] = 30.0f;
    params[CompositeParamsView::kLutTex] = 40.0f;
    params[CompositeParamsView::kLutIntensity] = 0.6f;
    params[CompositeParamsView::kContactShadowTex] = 50.0f;
    params[CompositeParamsView::kContactShadowStrength] = 0.9f;
    params[CompositeParamsView::kVignetteEnabled] = 1.0f;
    params[CompositeParamsView::kVignetteIntensity] = 0.5f;
    params[CompositeParamsView::kVignetteRadius] = 0.6f;
    params[CompositeParamsView::kVignetteSoftness] = 0.4f;
    params[CompositeParamsView::kFilmGrainEnabled] = 1.0f;
    params[CompositeParamsView::kFilmGrainIntensity] = 0.3f;
    params[CompositeParamsView::kFilmGrainTime] = 1.5f;

    CompositeParamsView cv(params);
    auto p = PrepareBloomCompositeParams(cv);

    EXPECT_FLOAT_EQ(p.exposure, 2.0f);
    EXPECT_FLOAT_EQ(p.bloom_intensity, 0.8f);
    EXPECT_EQ(p.bloom_enabled, 1);
    EXPECT_EQ(p.ssao_enabled, 1);
    EXPECT_EQ(p.ae_enabled, 1);
    EXPECT_EQ(p.lut_enabled, 1);
    EXPECT_FLOAT_EQ(p.lut_intensity, 0.6f);
    EXPECT_EQ(p.cs_enabled, 1);
    EXPECT_FLOAT_EQ(p.cs_strength, 0.9f);
    EXPECT_EQ(p.vignette_enabled, 1);
    EXPECT_FLOAT_EQ(p.vignette_intensity, 0.5f);
    EXPECT_FLOAT_EQ(p.vignette_radius, 0.6f);
    EXPECT_FLOAT_EQ(p.vignette_softness, 0.4f);
    EXPECT_EQ(p.film_grain_enabled, 1);
    EXPECT_FLOAT_EQ(p.film_grain_intensity, 0.3f);
    EXPECT_FLOAT_EQ(p.film_grain_time, 1.5f);
}

// 测试 准备泛光组合：泛光Enabledbut无纹理泛光启用为0
TEST(PrepareBloomCompositeTest, BloomEnabledbutNoTexture_bloom_EnabledIs0) {
    std::vector<float> params(static_cast<size_t>(CompositeParamsView::kCount), 0.0f);
    params[CompositeParamsView::kBloomEnabled] = 1.0f;
    // kBloomTex = 0 → bloom_enabled 仍为 0
    CompositeParamsView cv(params);
    auto p = PrepareBloomCompositeParams(cv);
    EXPECT_EQ(p.bloom_enabled, 0);
}

// 测试 准备泛光组合：参数交叉Lineusefallback
TEST(PrepareBloomCompositeTest, Parameters_CrossTheLineusefallback) {
    std::vector<float> params = {10.0f, 3.0f, 0.9f}; // kBloomTex, kExposure, kBloomIntensity
    CompositeParamsView cv(params);
    auto p = PrepareBloomCompositeParams(cv);
    EXPECT_FLOAT_EQ(p.exposure, 3.0f);
    EXPECT_FLOAT_EQ(p.bloom_intensity, 0.9f);
    EXPECT_EQ(p.bloom_enabled, 0); // kBloomEnabled 越界 → false
    EXPECT_EQ(p.vignette_enabled, 0);
    EXPECT_FLOAT_EQ(p.vignette_radius, 0.75f); // fallback
}
