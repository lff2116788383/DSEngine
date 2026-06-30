/**
 * @file meshlet_builder_test.cpp
 * @brief Meshlet Builder 单元测试
 */

#include <gtest/gtest.h>
#include "engine/render/meshlet/meshlet_builder.h"
#include "engine/render/meshlet/meshlet_types.h"
#include <cstdio>
#include <filesystem>

using namespace dse::render;

class MeshletBuilderTest : public ::testing::Test {
protected:
    // Simple quad: 2 triangles, 4 vertices
    std::vector<glm::vec3> quad_positions = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };
    std::vector<uint32_t> quad_indices = {0, 1, 2, 0, 2, 3};

    // Grid mesh for testing multiple meshlets
    std::vector<glm::vec3> grid_positions;
    std::vector<uint32_t> grid_indices;

    void SetUp() override {
        // Create a 20x20 grid mesh (400 vertices, 722 triangles)
        const int grid_size = 20;
        for (int y = 0; y <= grid_size; ++y) {
            for (int x = 0; x <= grid_size; ++x) {
                grid_positions.push_back(glm::vec3(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    0.0f));
            }
        }
        for (int y = 0; y < grid_size; ++y) {
            for (int x = 0; x < grid_size; ++x) {
                uint32_t tl = y * (grid_size + 1) + x;
                uint32_t tr = tl + 1;
                uint32_t bl = (y + 1) * (grid_size + 1) + x;
                uint32_t br = bl + 1;
                grid_indices.push_back(tl);
                grid_indices.push_back(tr);
                grid_indices.push_back(bl);
                grid_indices.push_back(tr);
                grid_indices.push_back(br);
                grid_indices.push_back(bl);
            }
        }
    }
};

TEST_F(MeshletBuilderTest, Build_SimpleQuad) {
    MeshletBuilder builder;
    auto result = builder.Build(quad_positions, quad_indices);

    EXPECT_EQ(result.meshlets.size(), 1u);
    EXPECT_EQ(result.meshlets[0].triangle_count, 2u);
    EXPECT_EQ(result.meshlets[0].vertex_count, 4u);
    EXPECT_EQ(result.global_indices.size(), 6u);
    EXPECT_EQ(result.draw_ranges.size(), 1u);
    EXPECT_EQ(result.draw_ranges[0].index_count, 6u);
}

TEST_F(MeshletBuilderTest, Build_EmptyMesh) {
    MeshletBuilder builder;
    std::vector<glm::vec3> empty_pos;
    std::vector<uint32_t> empty_idx;
    auto result = builder.Build(empty_pos, empty_idx);

    EXPECT_TRUE(result.meshlets.empty());
    EXPECT_TRUE(result.global_indices.empty());
}

TEST_F(MeshletBuilderTest, Build_GridCreatesMultipleMeshlets) {
    MeshletBuilder builder;
    MeshletBuildConfig config;
    config.max_vertices = 64;
    config.max_triangles = 64; // Small meshlets to force multiple

    auto result = builder.Build(grid_positions, grid_indices, config);

    // 800 triangles / 64 max ≈ at least 12 meshlets
    EXPECT_GT(result.meshlets.size(), 1u);

    // Verify all triangles are covered
    uint32_t total_tris = 0;
    for (const auto& m : result.meshlets) {
        total_tris += m.triangle_count;
        EXPECT_LE(m.triangle_count, config.max_triangles);
        EXPECT_LE(m.vertex_count, config.max_vertices);
    }
    EXPECT_EQ(total_tris * 3, static_cast<uint32_t>(grid_indices.size()));
}

TEST_F(MeshletBuilderTest, Build_BoundingSphereValid) {
    MeshletBuilder builder;
    auto result = builder.Build(quad_positions, quad_indices);

    const auto& m = result.meshlets[0];
    // Center should be approximately in the middle of the quad
    EXPECT_NEAR(m.center.x, 0.5f, 0.3f);
    EXPECT_NEAR(m.center.y, 0.5f, 0.3f);
    EXPECT_NEAR(m.center.z, 0.0f, 0.01f);
    // Radius should encompass all vertices
    EXPECT_GT(m.radius, 0.0f);
    EXPECT_LT(m.radius, 2.0f);
}

TEST_F(MeshletBuilderTest, Build_NormalConeValid) {
    MeshletBuilder builder;
    auto result = builder.Build(quad_positions, quad_indices);

    const auto& m = result.meshlets[0];
    // Flat quad: normal cone should point +Z
    EXPECT_NEAR(m.cone_axis.z, 1.0f, 0.01f);
    // Tight cone: cutoff should be ~1.0 (all normals align)
    EXPECT_GT(m.cone_cutoff, 0.9f);
}

TEST_F(MeshletBuilderTest, Build_CustomConfig) {
    MeshletBuilder builder;
    MeshletBuildConfig config;
    config.max_vertices = 32;
    config.max_triangles = 16;

    auto result = builder.Build(grid_positions, grid_indices, config);

    for (const auto& m : result.meshlets) {
        EXPECT_LE(m.vertex_count, 32u);
        EXPECT_LE(m.triangle_count, 16u);
    }
}

TEST_F(MeshletBuilderTest, Build_DrawRangesConsistent) {
    MeshletBuilder builder;
    auto result = builder.Build(grid_positions, grid_indices);

    uint32_t total_indices = 0;
    for (size_t i = 0; i < result.draw_ranges.size(); ++i) {
        const auto& range = result.draw_ranges[i];
        EXPECT_EQ(range.index_count, result.meshlets[i].triangle_count * 3);
        total_indices += range.index_count;
    }
    EXPECT_EQ(total_indices, static_cast<uint32_t>(result.global_indices.size()));
}

TEST_F(MeshletBuilderTest, Serialize_Deserialize_Roundtrip) {
    MeshletBuilder builder;
    auto original = builder.Build(grid_positions, grid_indices);
    original.name = "test_grid";

    std::string path = "test_meshlet_roundtrip.dmeshlet";
    ASSERT_TRUE(MeshletBuilder::Serialize(original, path));

    MeshletMesh loaded;
    ASSERT_TRUE(MeshletBuilder::Deserialize(path, loaded));

    EXPECT_EQ(loaded.meshlets.size(), original.meshlets.size());
    EXPECT_EQ(loaded.positions.size(), original.positions.size());
    EXPECT_EQ(loaded.global_indices.size(), original.global_indices.size());
    EXPECT_EQ(loaded.meshlet_vertices.size(), original.meshlet_vertices.size());
    EXPECT_EQ(loaded.meshlet_triangles.size(), original.meshlet_triangles.size());

    // Verify meshlet data matches
    for (size_t i = 0; i < loaded.meshlets.size(); ++i) {
        EXPECT_EQ(loaded.meshlets[i].triangle_count, original.meshlets[i].triangle_count);
        EXPECT_EQ(loaded.meshlets[i].vertex_count, original.meshlets[i].vertex_count);
        EXPECT_NEAR(loaded.meshlets[i].radius, original.meshlets[i].radius, 1e-5f);
    }

    // Cleanup
    std::remove(path.c_str());
}

TEST_F(MeshletBuilderTest, Deserialize_InvalidFile) {
    MeshletMesh mesh;
    EXPECT_FALSE(MeshletBuilder::Deserialize("nonexistent.dmeshlet", mesh));
}

TEST_F(MeshletBuilderTest, BuildFromFullMesh) {
    // Simulate BatchVertex-like data: pos(3) + color(4) + uv(2) + normal(3) + tangent(3) + weights(4) + joints(4) = 23 floats
    const uint32_t stride = 23;
    std::vector<float> vertex_data(quad_positions.size() * stride, 0.0f);
    for (size_t i = 0; i < quad_positions.size(); ++i) {
        vertex_data[i * stride + 0] = quad_positions[i].x;
        vertex_data[i * stride + 1] = quad_positions[i].y;
        vertex_data[i * stride + 2] = quad_positions[i].z;
    }

    MeshletBuilder builder;
    auto result = builder.BuildFromFullMesh(vertex_data.data(),
                                            static_cast<uint32_t>(quad_positions.size()),
                                            stride,
                                            quad_indices.data(),
                                            static_cast<uint32_t>(quad_indices.size()));

    EXPECT_EQ(result.meshlets.size(), 1u);
    EXPECT_EQ(result.meshlets[0].triangle_count, 2u);
}
