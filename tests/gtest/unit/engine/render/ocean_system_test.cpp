/**
 * @file ocean_system_test.cpp
 * @brief OceanSystem 单元测试 — FFT 替换 O(N³) DFT 后的正确性验证（B-5）
 *
 * 覆盖场景：
 * - 2 的幂分辨率走 FFT 路径：Update 后高度场有限、非全零
 * - 非 2 幂分辨率走原 DFT 回退：不崩溃
 * - 确定性：相同时间两次 Update 产出相同高度
 */

#include <gtest/gtest.h>
#include "engine/render/ocean_system.h"
#include <cmath>

using namespace dse::render;

// 测试 海洋：2 的幂分辨率（FFT 路径）产出有限且非零的高度场
// 注：suite 命名避开 test_world_systems.cpp 的 OceanSystemTest（TEST_F fixture）
TEST(OceanSystemFFTTest, FFTUpdateProducesFiniteHeights) {
    OceanConfig cfg;
    cfg.fft_resolution = 16;  // 2^4 → FFT 路径
    OceanSystem ocean;
    ocean.Init(cfg);
    ocean.Update(0.5f, glm::vec3(0.0f));

    bool any_nonzero = false;
    for (int i = 0; i < 64; ++i) {
        const float x = static_cast<float>(i % 8 - 4) * 4.0f;
        const float z = static_cast<float>(i / 8 - 4) * 4.0f;
        const float h = ocean.GetHeightAt(x, z);
        ASSERT_TRUE(std::isfinite(h)) << "height NaN/Inf at (" << x << "," << z << ")";
        if (std::abs(h) > 1e-5f) any_nonzero = true;
    }
    EXPECT_TRUE(any_nonzero) << "FFT 路径产出全零高度场";
}

// 测试 海洋：非 2 幂分辨率回退原 DFT 路径，不崩溃且结果有限
TEST(OceanSystemFFTTest, NonPowerOfTwoFallsBackToDFT) {
    OceanConfig cfg;
    cfg.fft_resolution = 12;  // 非 2 幂 → 回退路径
    OceanSystem ocean;
    ocean.Init(cfg);
    ocean.Update(1.0f, glm::vec3(10.0f, 0.0f, -10.0f));

    const float h = ocean.GetHeightAt(2.5f, -3.5f);
    EXPECT_TRUE(std::isfinite(h));
}

// 测试 海洋：相同时间两次 Update 产出确定结果
TEST(OceanSystemFFTTest, UpdateIsDeterministic) {
    OceanConfig cfg;
    cfg.fft_resolution = 8;
    OceanSystem ocean;
    ocean.Init(cfg);

    ocean.Update(0.3f, glm::vec3(0.0f));
    const float h1 = ocean.GetHeightAt(1.0f, 2.0f);
    ocean.Update(0.3f, glm::vec3(0.0f));
    const float h2 = ocean.GetHeightAt(1.0f, 2.0f);

    EXPECT_FLOAT_EQ(h1, h2);
}
