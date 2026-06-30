/**
 * @file mesh_decimator_test.cpp
 * @brief QEM Mesh Decimation 单元测试
 *
 * 测试减面器核心逻辑：比例减面、误差限制、边界保护、LOD 生成。
 */

#include <gtest/gtest.h>
#include "engine/mesh/mesh_decimator.h"
#include <cmath>

using namespace dse::mesh;

namespace {

/// 构造一个简单的平面网格（NxN quad grid = 2*(N-1)^2 triangles）
void BuildPlaneGrid(int n, std::vector<glm::vec3>& positions,
                    std::vector<glm::vec3>& normals,
                    std::vector<glm::vec2>& texcoords,
                    std::vector<uint32_t>& indices) {
    positions.clear();
    normals.clear();
    texcoords.clear();
    indices.clear();

    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            positions.push_back(glm::vec3(static_cast<float>(x), 0.0f, static_cast<float>(z)));
            normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
            texcoords.push_back(glm::vec2(static_cast<float>(x) / (n - 1),
                                           static_cast<float>(z) / (n - 1)));
        }
    }

    for (int z = 0; z < n - 1; ++z) {
        for (int x = 0; x < n - 1; ++x) {
            uint32_t i00 = z * n + x;
            uint32_t i10 = z * n + x + 1;
            uint32_t i01 = (z + 1) * n + x;
            uint32_t i11 = (z + 1) * n + x + 1;

            indices.push_back(i00);
            indices.push_back(i10);
            indices.push_back(i11);

            indices.push_back(i00);
            indices.push_back(i11);
            indices.push_back(i01);
        }
    }
}

/// 构造一个简单三角形
void BuildSingleTriangle(std::vector<glm::vec3>& positions,
                         std::vector<uint32_t>& indices) {
    positions = {
        glm::vec3(0, 0, 0),
        glm::vec3(1, 0, 0),
        glm::vec3(0.5f, 0, 1)
    };
    indices = {0, 1, 2};
}

} // namespace

// ─── 基本减面测试 ───────────────────────────────────────────────────────────

TEST(MeshDecimatorTest, Decimate_ReducesTriangleCount) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;
    BuildPlaneGrid(10, positions, normals, texcoords, indices);

    DecimationInput input;
    input.positions = positions.data();
    input.normals = normals.data();
    input.texcoords = texcoords.data();
    input.vertex_count = static_cast<uint32_t>(positions.size());
    input.indices = indices.data();
    input.index_count = static_cast<uint32_t>(indices.size());

    DecimationConfig config;
    config.target_ratio = 0.5f;

    MeshDecimator decimator;
    DecimationResult result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    uint32_t original_tris = static_cast<uint32_t>(indices.size()) / 3;
    EXPECT_GT(result.original_triangle_count, 0u);
    EXPECT_EQ(result.original_triangle_count, original_tris);
    // Result should be approximately 50% of original
    EXPECT_LT(result.result_triangle_count, original_tris);
    EXPECT_GT(result.result_triangle_count, 0u);
}

TEST(MeshDecimatorTest, Decimate_OutputHasValidIndices) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;
    BuildPlaneGrid(8, positions, normals, texcoords, indices);

    DecimationInput input;
    input.positions = positions.data();
    input.normals = normals.data();
    input.texcoords = texcoords.data();
    input.vertex_count = static_cast<uint32_t>(positions.size());
    input.indices = indices.data();
    input.index_count = static_cast<uint32_t>(indices.size());

    DecimationConfig config;
    config.target_ratio = 0.3f;

    MeshDecimator decimator;
    DecimationResult result = decimator.Decimate(input, config);

    ASSERT_TRUE(result.success);

    // All indices should be within vertex count
    uint32_t vert_count = static_cast<uint32_t>(result.positions.size());
    for (uint32_t idx : result.indices) {
        EXPECT_LT(idx, vert_count);
    }

    // Index count should be multiple of 3
    EXPECT_EQ(result.indices.size() % 3, 0u);
}

TEST(MeshDecimatorTest, Decimate_PreservesAttributes) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;
    BuildPlaneGrid(6, positions, normals, texcoords, indices);

    DecimationInput input;
    input.positions = positions.data();
    input.normals = normals.data();
    input.texcoords = texcoords.data();
    input.vertex_count = static_cast<uint32_t>(positions.size());
    input.indices = indices.data();
    input.index_count = static_cast<uint32_t>(indices.size());

    DecimationConfig config;
    config.target_ratio = 0.5f;

    MeshDecimator decimator;
    DecimationResult result = decimator.Decimate(input, config);

    ASSERT_TRUE(result.success);
    // Result should have normals and texcoords
    EXPECT_EQ(result.normals.size(), result.positions.size());
    EXPECT_EQ(result.texcoords.size(), result.positions.size());
}

TEST(MeshDecimatorTest, Decimate_MinimalMesh_GracefulHandling) {
    // A single triangle cannot be further reduced
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    BuildSingleTriangle(positions, indices);

    DecimationInput input;
    input.positions = positions.data();
    input.vertex_count = static_cast<uint32_t>(positions.size());
    input.indices = indices.data();
    input.index_count = static_cast<uint32_t>(indices.size());

    DecimationConfig config;
    config.target_ratio = 0.1f; // want 10% of 1 triangle

    MeshDecimator decimator;
    DecimationResult result = decimator.Decimate(input, config);

    // Should still succeed (may not reduce further)
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.result_triangle_count, 1u);
}

TEST(MeshDecimatorTest, Decimate_RatioOne_PreservesAll) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;
    BuildPlaneGrid(5, positions, normals, texcoords, indices);

    DecimationInput input;
    input.positions = positions.data();
    input.normals = normals.data();
    input.vertex_count = static_cast<uint32_t>(positions.size());
    input.indices = indices.data();
    input.index_count = static_cast<uint32_t>(indices.size());

    DecimationConfig config;
    config.target_ratio = 1.0f; // keep all

    MeshDecimator decimator;
    DecimationResult result = decimator.Decimate(input, config);

    EXPECT_TRUE(result.success);
    uint32_t original_tris = static_cast<uint32_t>(indices.size()) / 3;
    EXPECT_EQ(result.result_triangle_count, original_tris);
}

// ─── LOD 生成测试 ───────────────────────────────────────────────────────────

TEST(MeshDecimatorTest, GenerateLods_MultipleLevel) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;
    BuildPlaneGrid(10, positions, normals, texcoords, indices);

    DecimationInput input;
    input.positions = positions.data();
    input.normals = normals.data();
    input.texcoords = texcoords.data();
    input.vertex_count = static_cast<uint32_t>(positions.size());
    input.indices = indices.data();
    input.index_count = static_cast<uint32_t>(indices.size());

    LodGenerationConfig lod_config;
    lod_config.level_ratios = {0.5f, 0.25f, 0.125f};
    lod_config.base_config.target_ratio = 0.5f; // unused but set

    MeshDecimator decimator;
    LodGenerationResult result = decimator.GenerateLods(input, lod_config);

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.levels.size(), 3u);

    // Each level should have fewer triangles than the previous
    uint32_t original_tris = static_cast<uint32_t>(indices.size()) / 3;
    EXPECT_LT(result.levels[0].result_triangle_count, original_tris);
    EXPECT_LT(result.levels[1].result_triangle_count, result.levels[0].result_triangle_count);
    EXPECT_LT(result.levels[2].result_triangle_count, result.levels[1].result_triangle_count);
}

TEST(MeshDecimatorTest, GenerateLods_AllSucceed) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;
    BuildPlaneGrid(8, positions, normals, texcoords, indices);

    DecimationInput input;
    input.positions = positions.data();
    input.normals = normals.data();
    input.vertex_count = static_cast<uint32_t>(positions.size());
    input.indices = indices.data();
    input.index_count = static_cast<uint32_t>(indices.size());

    LodGenerationConfig lod_config;
    lod_config.level_ratios = {0.5f, 0.25f};

    MeshDecimator decimator;
    LodGenerationResult result = decimator.GenerateLods(input, lod_config);

    ASSERT_TRUE(result.success);
    for (const auto& level : result.levels) {
        EXPECT_TRUE(level.success);
        EXPECT_GT(level.result_triangle_count, 0u);
    }
}

// ─── 配置测试 ───────────────────────────────────────────────────────────────

TEST(DecimationConfigTest, DefaultValues) {
    DecimationConfig config;
    EXPECT_FLOAT_EQ(config.target_ratio, 0.5f);
    EXPECT_EQ(config.target_triangle_count, 0u);
    EXPECT_FLOAT_EQ(config.max_error, 0.0f);
    EXPECT_GT(config.seam_weight, 1.0f);
    EXPECT_GT(config.boundary_weight, 1.0f);
    EXPECT_FALSE(config.lock_boundary);
    EXPECT_TRUE(config.protect_uv_seams);
}
