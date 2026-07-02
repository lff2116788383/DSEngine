/**
 * @file procedural_deep_test.cpp
 * @brief P5: 程序化生成系统深度测试 — 噪声函数特性、确定性、散布、PCG
 */

#include <gtest/gtest.h>
#include "engine/procedural/procedural_generator.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <vector>

using namespace dse::procedural;

// ═══════════════════════════════════════════════════════════
// 噪声函数基础特性
// ═══════════════════════════════════════════════════════════

TEST(NoiseDeepTest, PerlinRange) {
    for (float x = -10.0f; x <= 10.0f; x += 0.37f) {
        for (float y = -10.0f; y <= 10.0f; y += 0.37f) {
            float v = PerlinNoise2D(x, y);
            EXPECT_GE(v, -1.0f) << "x=" << x << " y=" << y;
            EXPECT_LE(v, 1.0f) << "x=" << x << " y=" << y;
        }
    }
}

TEST(NoiseDeepTest, SimplexRange) {
    for (float x = -10.0f; x <= 10.0f; x += 0.37f) {
        for (float y = -10.0f; y <= 10.0f; y += 0.37f) {
            float v = SimplexNoise2D(x, y);
            EXPECT_GE(v, -1.0f) << "x=" << x << " y=" << y;
            EXPECT_LE(v, 1.0f) << "x=" << x << " y=" << y;
        }
    }
}

TEST(NoiseDeepTest, WorleyRange) {
    for (float x = -10.0f; x <= 10.0f; x += 0.37f) {
        for (float y = -10.0f; y <= 10.0f; y += 0.37f) {
            float v = WorleyNoise2D(x, y);
            EXPECT_GE(v, 0.0f) << "x=" << x << " y=" << y;
            EXPECT_LE(v, 1.0f) << "x=" << x << " y=" << y;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// 噪声确定性
// ═══════════════════════════════════════════════════════════

TEST(NoiseDeepTest, PerlinDeterministic) {
    float a = PerlinNoise2D(3.14f, 2.71f, 42);
    float b = PerlinNoise2D(3.14f, 2.71f, 42);
    EXPECT_FLOAT_EQ(a, b);
}

TEST(NoiseDeepTest, SimplexDeterministic) {
    float a = SimplexNoise2D(3.14f, 2.71f, 42);
    float b = SimplexNoise2D(3.14f, 2.71f, 42);
    EXPECT_FLOAT_EQ(a, b);
}

TEST(NoiseDeepTest, WorleyDeterministic) {
    float a = WorleyNoise2D(3.14f, 2.71f, 42);
    float b = WorleyNoise2D(3.14f, 2.71f, 42);
    EXPECT_FLOAT_EQ(a, b);
}

TEST(NoiseDeepTest, DifferentSeedsProduceDifferentValues) {
    bool any_diff = false;
    for (float x = 0.0f; x < 10.0f; x += 1.7f) {
        for (float y = 0.0f; y < 10.0f; y += 1.7f) {
            float a = PerlinNoise2D(x, y, 1);
            float b = PerlinNoise2D(x, y, 999);
            if (std::abs(a - b) > 1e-6f) any_diff = true;
        }
    }
    EXPECT_TRUE(any_diff);
}

// ═══════════════════════════════════════════════════════════
// FBM 测试
// ═══════════════════════════════════════════════════════════

TEST(FBMDeepTest, OutputRange) {
    FBMParams params;
    params.octaves = 6;
    params.seed = 123;
    for (float x = -5.0f; x <= 5.0f; x += 0.5f) {
        for (float y = -5.0f; y <= 5.0f; y += 0.5f) {
            float v = FBM2D(x, y, params);
            EXPECT_GE(v, -2.0f);
            EXPECT_LE(v, 2.0f);
        }
    }
}

TEST(FBMDeepTest, MoreOctavesMoreDetail) {
    FBMParams p1;
    p1.octaves = 1;
    p1.seed = 42;

    FBMParams p2;
    p2.octaves = 8;
    p2.seed = 42;

    float sum_diff = 0.0f;
    int count = 0;
    for (float x = 0.0f; x < 10.0f; x += 0.1f) {
        float v1 = FBM2D(x, 0.0f, p1);
        float v2 = FBM2D(x, 0.0f, p2);
        sum_diff += std::abs(v1 - v2);
        ++count;
    }
    EXPECT_GT(sum_diff / count, 0.0f);
}

TEST(FBMDeepTest, DomainWarpDeterministic) {
    FBMParams params;
    params.seed = 77;
    float a = DomainWarpedFBM(1.0f, 2.0f, params, 0.3f);
    float b = DomainWarpedFBM(1.0f, 2.0f, params, 0.3f);
    EXPECT_FLOAT_EQ(a, b);
}

// ═══════════════════════════════════════════════════════════
// 地形高度图生成
// ═══════════════════════════════════════════════════════════

TEST(HeightmapGenDeepTest, CorrectOutputSize) {
    std::vector<float> heights;
    TerrainGenConfig config;
    config.seed = 100;
    GenerateHeightmap(heights, 0.0f, 0.0f, 32, 32, 1.0f, config);
    EXPECT_EQ(heights.size(), 32u * 32u);
}

TEST(HeightmapGenDeepTest, DeterministicOutput) {
    std::vector<float> h1, h2;
    TerrainGenConfig config;
    config.seed = 200;
    GenerateHeightmap(h1, 0.0f, 0.0f, 16, 16, 1.0f, config);
    GenerateHeightmap(h2, 0.0f, 0.0f, 16, 16, 1.0f, config);
    ASSERT_EQ(h1.size(), h2.size());
    for (size_t i = 0; i < h1.size(); ++i) {
        EXPECT_FLOAT_EQ(h1[i], h2[i]) << "index=" << i;
    }
}

TEST(HeightmapGenDeepTest, DifferentOriginsDifferentHeights) {
    std::vector<float> h1, h2;
    TerrainGenConfig config;
    config.seed = 300;
    GenerateHeightmap(h1, 0.0f, 0.0f, 16, 16, 1.0f, config);
    GenerateHeightmap(h2, 1000.0f, 1000.0f, 16, 16, 1.0f, config);

    bool any_diff = false;
    for (size_t i = 0; i < h1.size(); ++i) {
        if (h1[i] != h2[i]) { any_diff = true; break; }
    }
    EXPECT_TRUE(any_diff);
}

TEST(HeightmapGenDeepTest, HeightScaleAffectsRange) {
    std::vector<float> h_small, h_big;
    TerrainGenConfig c1;
    c1.seed = 400;
    c1.height_scale = 10.0f;
    GenerateHeightmap(h_small, 0.0f, 0.0f, 32, 32, 1.0f, c1);

    TerrainGenConfig c2;
    c2.seed = 400;
    c2.height_scale = 1000.0f;
    GenerateHeightmap(h_big, 0.0f, 0.0f, 32, 32, 1.0f, c2);

    float max_small = *std::max_element(h_small.begin(), h_small.end());
    float max_big = *std::max_element(h_big.begin(), h_big.end());
    EXPECT_GT(max_big, max_small);
}

// ═══════════════════════════════════════════════════════════
// 散布生成
// ═══════════════════════════════════════════════════════════

TEST(ScatterDeepTest, GeneratesInstances) {
    std::vector<ScatterInstance> instances;
    ScatterConfig config;
    config.seed = 500;
    config.base_spacing = 3.0f;
    ScatterType tree;
    tree.asset_path = "tree.glb";
    tree.density = 1.0f;
    config.types.push_back(tree);

    GenerateScatter(instances, 0.0f, 0.0f, 50.0f, 50.0f, config,
                    [](float, float) { return 0.0f; });
    EXPECT_GT(instances.size(), 0u);
}

TEST(ScatterDeepTest, DeterministicOutput) {
    ScatterConfig config;
    config.seed = 600;
    config.base_spacing = 5.0f;
    ScatterType grass;
    grass.asset_path = "grass.glb";
    config.types.push_back(grass);

    auto height_fn = [](float, float) { return 0.0f; };

    std::vector<ScatterInstance> a, b;
    GenerateScatter(a, 0.0f, 0.0f, 20.0f, 20.0f, config, height_fn);
    GenerateScatter(b, 0.0f, 0.0f, 20.0f, 20.0f, config, height_fn);

    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_FLOAT_EQ(a[i].position.x, b[i].position.x);
        EXPECT_FLOAT_EQ(a[i].position.z, b[i].position.z);
    }
}

TEST(ScatterDeepTest, HeightFilterExcludes) {
    ScatterConfig config;
    config.seed = 700;
    config.base_spacing = 2.0f;
    ScatterType t;
    t.asset_path = "rock.glb";
    t.min_height = 5.0f;
    t.max_height = 10.0f;
    config.types.push_back(t);

    std::vector<ScatterInstance> instances;
    GenerateScatter(instances, 0.0f, 0.0f, 30.0f, 30.0f, config,
                    [](float, float) { return 0.0f; });
    EXPECT_EQ(instances.size(), 0u);
}

// ═══════════════════════════════════════════════════════════
// PCGRandom 测试
// ═══════════════════════════════════════════════════════════

TEST(PCGDeepTest, DeterministicSequence) {
    PCGRandom a(42);
    PCGRandom b(42);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(a.Next(), b.Next()) << "iteration=" << i;
    }
}

TEST(PCGDeepTest, FloatRange) {
    PCGRandom rng(99);
    for (int i = 0; i < 1000; ++i) {
        float v = rng.NextFloat();
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }
}

TEST(PCGDeepTest, RangeFunction) {
    PCGRandom rng(123);
    for (int i = 0; i < 500; ++i) {
        float v = rng.Range(-5.0f, 5.0f);
        EXPECT_GE(v, -5.0f);
        EXPECT_LT(v, 5.0f);
    }
}

TEST(PCGDeepTest, DifferentSeedsDiffer) {
    PCGRandom a(1);
    PCGRandom b(999);
    bool any_diff = false;
    for (int i = 0; i < 10; ++i) {
        if (a.Next() != b.Next()) { any_diff = true; break; }
    }
    EXPECT_TRUE(any_diff);
}

TEST(PCGDeepTest, Distribution) {
    PCGRandom rng(55);
    int below_half = 0;
    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        if (rng.NextFloat() < 0.5f) ++below_half;
    }
    float ratio = static_cast<float>(below_half) / N;
    EXPECT_NEAR(ratio, 0.5f, 0.05f);
}
