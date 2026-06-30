/**
 * @file procedural_generator_test.cpp
 * @brief 程序化生成系统单元测试
 */

#include <gtest/gtest.h>
#include "engine/procedural/procedural_generator.h"
#include <cmath>
#include <set>

using namespace dse::procedural;

// ─── PCG Random ─────────────────────────────────────────────────────────────

TEST(PCGRandomTest, Deterministic_SameSeed) {
    PCGRandom a(42);
    PCGRandom b(42);

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(a.Next(), b.Next());
    }
}

TEST(PCGRandomTest, DifferentSeeds_DifferentSequence) {
    PCGRandom a(1);
    PCGRandom b(2);

    int same_count = 0;
    for (int i = 0; i < 100; ++i) {
        if (a.Next() == b.Next()) ++same_count;
    }
    EXPECT_LT(same_count, 5); // should be very different
}

TEST(PCGRandomTest, NextFloat_InRange) {
    PCGRandom rng(123);
    for (int i = 0; i < 1000; ++i) {
        float f = rng.NextFloat();
        EXPECT_GE(f, 0.0f);
        EXPECT_LT(f, 1.0f);
    }
}

TEST(PCGRandomTest, Range_InBounds) {
    PCGRandom rng(456);
    for (int i = 0; i < 1000; ++i) {
        float v = rng.Range(-5.0f, 10.0f);
        EXPECT_GE(v, -5.0f);
        EXPECT_LT(v, 10.0f);
    }
}

// ─── Noise Functions ────────────────────────────────────────────────────────

TEST(NoiseTest, PerlinNoise2D_BoundedOutput) {
    for (int i = 0; i < 100; ++i) {
        float x = static_cast<float>(i) * 0.37f;
        float y = static_cast<float>(i) * 0.53f;
        float v = PerlinNoise2D(x, y);
        EXPECT_GE(v, -2.0f); // Perlin 理论范围 ≈ [-1, 1]，实际可能略超
        EXPECT_LE(v, 2.0f);
    }
}

TEST(NoiseTest, PerlinNoise2D_Deterministic) {
    float a = PerlinNoise2D(1.5f, 2.3f, 42);
    float b = PerlinNoise2D(1.5f, 2.3f, 42);
    EXPECT_FLOAT_EQ(a, b);
}

TEST(NoiseTest, PerlinNoise2D_DifferentSeedsDifferentValues) {
    float a = PerlinNoise2D(1.5f, 2.3f, 1);
    float b = PerlinNoise2D(1.5f, 2.3f, 99);
    EXPECT_NE(a, b);
}

TEST(NoiseTest, SimplexNoise2D_BoundedOutput) {
    for (int i = 0; i < 100; ++i) {
        float x = static_cast<float>(i) * 0.47f;
        float y = static_cast<float>(i) * 0.63f;
        float v = SimplexNoise2D(x, y);
        EXPECT_GE(v, -2.0f);
        EXPECT_LE(v, 2.0f);
    }
}

TEST(NoiseTest, WorleyNoise2D_NonNegative) {
    for (int i = 0; i < 100; ++i) {
        float x = static_cast<float>(i) * 0.31f;
        float y = static_cast<float>(i) * 0.71f;
        float v = WorleyNoise2D(x, y);
        EXPECT_GE(v, 0.0f);
    }
}

TEST(NoiseTest, FBM2D_ReasonableRange) {
    FBMParams params;
    params.octaves = 6;
    params.frequency = 1.0f;
    params.seed = 123;

    for (int i = 0; i < 100; ++i) {
        float x = static_cast<float>(i) * 0.1f;
        float y = static_cast<float>(i) * 0.15f;
        float v = FBM2D(x, y, params);
        EXPECT_GE(v, -2.0f);
        EXPECT_LE(v, 2.0f);
    }
}

// ─── Terrain Generation ─────────────────────────────────────────────────────

TEST(TerrainGenTest, GenerateHeightmap_CorrectSize) {
    std::vector<float> heights;
    TerrainGenConfig config;
    config.height_scale = 100.0f;

    GenerateHeightmap(heights, 0.0f, 0.0f, 16, 16, 1.0f, config);

    EXPECT_EQ(heights.size(), 256u);
}

TEST(TerrainGenTest, GenerateHeightmap_BoundedValues) {
    std::vector<float> heights;
    TerrainGenConfig config;
    config.height_scale = 100.0f;
    config.seed = 99;

    GenerateHeightmap(heights, 0.0f, 0.0f, 32, 32, 1.0f, config);

    for (float h : heights) {
        EXPECT_GE(h, 0.0f);
        EXPECT_LE(h, config.height_scale * 1.5f); // allow some margin
    }
}

TEST(TerrainGenTest, GenerateHeightmap_Deterministic) {
    std::vector<float> a, b;
    TerrainGenConfig config;
    config.seed = 42;

    GenerateHeightmap(a, 100.0f, 200.0f, 8, 8, 2.0f, config);
    GenerateHeightmap(b, 100.0f, 200.0f, 8, 8, 2.0f, config);

    EXPECT_EQ(a, b);
}

// ─── Scatter Generation ─────────────────────────────────────────────────────

TEST(ScatterTest, GenerateScatter_ProducesInstances) {
    ScatterConfig config;
    config.seed = 42;
    config.base_spacing = 5.0f;

    ScatterType tree;
    tree.asset_path = "meshes/tree.dmesh";
    tree.density = 0.5f;
    config.types.push_back(tree);

    std::vector<ScatterInstance> instances;
    auto height_fn = [](float, float) { return 10.0f; };

    GenerateScatter(instances, 0.0f, 0.0f, 50.0f, 50.0f, config, height_fn);

    EXPECT_GT(instances.size(), 0u);
    EXPECT_LT(instances.size(), 200u); // reasonable count for 50x50 area with spacing 5
}

TEST(ScatterTest, GenerateScatter_RespectsHeightFilter) {
    ScatterConfig config;
    config.seed = 42;
    config.base_spacing = 3.0f;

    ScatterType type;
    type.asset_path = "meshes/bush.dmesh";
    type.density = 1.0f;
    type.min_height = 5.0f;
    type.max_height = 15.0f;
    config.types.push_back(type);

    std::vector<ScatterInstance> instances;
    // Height = 20, above max_height → no instances
    auto height_fn = [](float, float) { return 20.0f; };

    GenerateScatter(instances, 0.0f, 0.0f, 30.0f, 30.0f, config, height_fn);
    EXPECT_EQ(instances.size(), 0u);
}

TEST(ScatterTest, GenerateScatter_Deterministic) {
    ScatterConfig config;
    config.seed = 99;
    config.base_spacing = 4.0f;

    ScatterType type;
    type.asset_path = "test";
    type.density = 0.8f;
    config.types.push_back(type);

    auto height_fn = [](float, float) { return 5.0f; };

    std::vector<ScatterInstance> a, b;
    GenerateScatter(a, 0.0f, 0.0f, 20.0f, 20.0f, config, height_fn);
    GenerateScatter(b, 0.0f, 0.0f, 20.0f, 20.0f, config, height_fn);

    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_FLOAT_EQ(a[i].position.x, b[i].position.x);
        EXPECT_FLOAT_EQ(a[i].position.z, b[i].position.z);
    }
}
