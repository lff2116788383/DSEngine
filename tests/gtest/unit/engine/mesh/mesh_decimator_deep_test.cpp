/**
 * @file mesh_decimator_deep_test.cpp
 * @brief P8: MeshDecimator 深度测试 — 减面比例、边界保护、LOD 生成、边界情况
 */

#include <gtest/gtest.h>
#include "engine/mesh/mesh_decimator.h"
#include <cmath>
#include <vector>

using namespace dse::mesh;

class MeshDecimatorDeepTest : public ::testing::Test {
protected:
    MeshDecimator decimator;

    // 6x6 的 grid plane = 5x5 quads = 50 triangles, 36 vertices
    DecimationInput MakeGridPlane(int grid_n = 6) {
        positions_.clear();
        normals_.clear();
        texcoords_.clear();
        indices_.clear();

        for (int z = 0; z < grid_n; ++z) {
            for (int x = 0; x < grid_n; ++x) {
                float fx = static_cast<float>(x);
                float fz = static_cast<float>(z);
                positions_.emplace_back(fx, 0.0f, fz);
                normals_.emplace_back(0.0f, 1.0f, 0.0f);
                texcoords_.emplace_back(fx / (grid_n - 1), fz / (grid_n - 1));
            }
        }

        for (int z = 0; z < grid_n - 1; ++z) {
            for (int x = 0; x < grid_n - 1; ++x) {
                uint32_t tl = z * grid_n + x;
                uint32_t tr = tl + 1;
                uint32_t bl = tl + grid_n;
                uint32_t br = bl + 1;
                indices_.push_back(tl);
                indices_.push_back(bl);
                indices_.push_back(tr);
                indices_.push_back(tr);
                indices_.push_back(bl);
                indices_.push_back(br);
            }
        }

        DecimationInput input;
        input.positions = positions_.data();
        input.normals = normals_.data();
        input.texcoords = texcoords_.data();
        input.vertex_count = static_cast<uint32_t>(positions_.size());
        input.indices = indices_.data();
        input.index_count = static_cast<uint32_t>(indices_.size());
        return input;
    }

    std::vector<glm::vec3> positions_;
    std::vector<glm::vec3> normals_;
    std::vector<glm::vec2> texcoords_;
    std::vector<uint32_t> indices_;
};

TEST_F(MeshDecimatorDeepTest, HalfReduction) {
    auto input = MakeGridPlane(10);
    uint32_t orig_tris = input.index_count / 3;

    DecimationConfig config;
    config.target_ratio = 0.5f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.original_triangle_count, orig_tris);
    EXPECT_LE(result.result_triangle_count, orig_tris);
    EXPECT_GT(result.result_triangle_count, 0u);
}

TEST_F(MeshDecimatorDeepTest, QuarterReduction) {
    auto input = MakeGridPlane(10);
    DecimationConfig config;
    config.target_ratio = 0.25f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    EXPECT_LT(result.result_triangle_count, result.original_triangle_count);
}

TEST_F(MeshDecimatorDeepTest, FullPreservation) {
    auto input = MakeGridPlane(6);
    DecimationConfig config;
    config.target_ratio = 1.0f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.result_triangle_count, result.original_triangle_count);
}

TEST_F(MeshDecimatorDeepTest, VeryAggressiveReduction) {
    auto input = MakeGridPlane(10);
    DecimationConfig config;
    config.target_ratio = 0.1f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    EXPECT_LT(result.result_triangle_count, result.original_triangle_count / 2);
}

TEST_F(MeshDecimatorDeepTest, LockBoundary) {
    auto input = MakeGridPlane(6);
    DecimationConfig config;
    config.target_ratio = 0.5f;
    config.lock_boundary = true;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
}

TEST_F(MeshDecimatorDeepTest, HighSeamWeight) {
    auto input = MakeGridPlane(8);
    DecimationConfig config;
    config.target_ratio = 0.5f;
    config.seam_weight = 100.0f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
}

TEST_F(MeshDecimatorDeepTest, NormalsPreserved) {
    auto input = MakeGridPlane(6);
    DecimationConfig config;
    config.target_ratio = 0.8f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.normals.size(), result.positions.size());
}

TEST_F(MeshDecimatorDeepTest, TexcoordsPreserved) {
    auto input = MakeGridPlane(6);
    DecimationConfig config;
    config.target_ratio = 0.8f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.texcoords.size(), result.positions.size());
}

TEST_F(MeshDecimatorDeepTest, IndicesValid) {
    auto input = MakeGridPlane(8);
    DecimationConfig config;
    config.target_ratio = 0.5f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    for (uint32_t idx : result.indices) {
        EXPECT_LT(idx, static_cast<uint32_t>(result.positions.size()));
    }
}

TEST_F(MeshDecimatorDeepTest, MaxErrorThreshold) {
    auto input = MakeGridPlane(10);
    DecimationConfig config;
    config.target_ratio = 0.1f;
    config.max_error = 0.001f;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    // With very strict error threshold, fewer triangles should be removed
    EXPECT_GT(result.result_triangle_count, 0u);
}

TEST_F(MeshDecimatorDeepTest, TargetTriangleCount) {
    auto input = MakeGridPlane(10);
    DecimationConfig config;
    config.target_triangle_count = 20;
    auto result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    EXPECT_LE(result.result_triangle_count, 30u);
}

// ═══════════════════════════════════════════════════════════
// LOD 生成
// ═══════════════════════════════════════════════════════════

TEST_F(MeshDecimatorDeepTest, GenerateMultipleLods) {
    auto input = MakeGridPlane(10);
    LodGenerationConfig lod_config;
    lod_config.level_ratios = {0.5f, 0.25f, 0.125f};
    lod_config.base_config.protect_uv_seams = true;

    auto result = decimator.GenerateLods(input, lod_config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.levels.size(), 3u);

    for (size_t i = 0; i < result.levels.size(); ++i) {
        EXPECT_TRUE(result.levels[i].success) << "LOD level " << i;
        EXPECT_GT(result.levels[i].result_triangle_count, 0u) << "LOD level " << i;
    }

    for (size_t i = 1; i < result.levels.size(); ++i) {
        EXPECT_LE(result.levels[i].result_triangle_count,
                  result.levels[i - 1].result_triangle_count) << "LOD " << i;
    }
}

TEST_F(MeshDecimatorDeepTest, SingleLodLevel) {
    auto input = MakeGridPlane(8);
    LodGenerationConfig lod_config;
    lod_config.level_ratios = {0.5f};

    auto result = decimator.GenerateLods(input, lod_config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.levels.size(), 1u);
}

// ═══════════════════════════════════════════════════════════
// DecimationConfig 默认值
// ═══════════════════════════════════════════════════════════

TEST(DecimationConfigTest, Defaults) {
    DecimationConfig c;
    EXPECT_FLOAT_EQ(c.target_ratio, 0.5f);
    EXPECT_EQ(c.target_triangle_count, 0u);
    EXPECT_FLOAT_EQ(c.max_error, 0.0f);
    EXPECT_FLOAT_EQ(c.seam_weight, 2.0f);
    EXPECT_FLOAT_EQ(c.boundary_weight, 10.0f);
    EXPECT_FLOAT_EQ(c.normal_flip_threshold, 0.2f);
    EXPECT_FALSE(c.lock_boundary);
    EXPECT_TRUE(c.protect_uv_seams);
    EXPECT_FLOAT_EQ(c.attribute_weight, 1.0f);
}

// ═══════════════════════════════════════════════════════════
// 边界：最小 mesh (1 triangle)
// ═══════════════════════════════════════════════════════════

TEST_F(MeshDecimatorDeepTest, SingleTriangle) {
    positions_ = {
        glm::vec3(0, 0, 0),
        glm::vec3(1, 0, 0),
        glm::vec3(0, 0, 1),
    };
    normals_ = {
        glm::vec3(0, 1, 0),
        glm::vec3(0, 1, 0),
        glm::vec3(0, 1, 0),
    };
    texcoords_ = {
        glm::vec2(0, 0),
        glm::vec2(1, 0),
        glm::vec2(0, 1),
    };
    indices_ = {0, 1, 2};

    DecimationInput input;
    input.positions = positions_.data();
    input.normals = normals_.data();
    input.texcoords = texcoords_.data();
    input.vertex_count = 3;
    input.indices = indices_.data();
    input.index_count = 3;

    DecimationConfig config;
    config.target_ratio = 0.5f;
    auto result = decimator.Decimate(input, config);
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.result_triangle_count, 1u);
}

// ═══════════════════════════════════════════════════════════
// 无法线/UV 的减面
// ═══════════════════════════════════════════════════════════

TEST_F(MeshDecimatorDeepTest, PositionsOnly) {
    auto input = MakeGridPlane(6);
    input.normals = nullptr;
    input.texcoords = nullptr;

    DecimationConfig config;
    config.target_ratio = 0.5f;
    auto result = decimator.Decimate(input, config);
    EXPECT_TRUE(result.success);
}
