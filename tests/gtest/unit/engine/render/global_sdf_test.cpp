/**
 * @file global_sdf_test.cpp
 * @brief Global SDF 系统单元测试
 */

#include <gtest/gtest.h>
#include "engine/render/sdf/global_sdf.h"

using namespace dse::render;

TEST(GlobalSDFTest, Init_CreatesCascades) {
    GlobalSDFSystem system;
    GlobalSDFConfig config;
    config.num_cascades = 4;
    config.base_resolution = 32;
    config.base_voxel_size = 0.5f;
    config.cascade_ratio = 2.0f;

    system.Init(config);

    EXPECT_EQ(system.CascadeCount(), 4);
    EXPECT_EQ(system.GetCascade(0).resolution, 32);
    EXPECT_FLOAT_EQ(system.GetCascade(0).voxel_size, 0.5f);
    EXPECT_FLOAT_EQ(system.GetCascade(1).voxel_size, 1.0f);
    EXPECT_FLOAT_EQ(system.GetCascade(2).voxel_size, 2.0f);
    EXPECT_FLOAT_EQ(system.GetCascade(3).voxel_size, 4.0f);
}

TEST(GlobalSDFTest, Init_CascadeExtent) {
    GlobalSDFSystem system;
    GlobalSDFConfig config;
    config.num_cascades = 2;
    config.base_resolution = 64;
    config.base_voxel_size = 1.0f;
    config.cascade_ratio = 2.0f;

    system.Init(config);

    // extent = voxel_size * resolution * 0.5
    EXPECT_FLOAT_EQ(system.GetCascade(0).extent, 1.0f * 64 * 0.5f);  // 32
    EXPECT_FLOAT_EQ(system.GetCascade(1).extent, 2.0f * 64 * 0.5f);  // 64
}

TEST(GlobalSDFTest, Update_SnapsCenterToGrid) {
    GlobalSDFSystem system;
    GlobalSDFConfig config;
    config.num_cascades = 2;
    config.base_resolution = 32;
    config.base_voxel_size = 1.0f;

    system.Init(config);
    system.Update(glm::vec3(10.3f, 5.7f, -3.2f));

    // Center should be snapped (snap = voxel_size * 4 = 4)
    const auto& c = system.GetCascade(0);
    EXPECT_EQ(std::fmod(c.center.x, 4.0f), 0.0f);
    EXPECT_EQ(std::fmod(c.center.y, 4.0f), 0.0f);
    EXPECT_EQ(std::fmod(c.center.z, 4.0f), 0.0f);
}

TEST(GlobalSDFTest, QueryDistance_EmptyScene_ReturnsMax) {
    GlobalSDFSystem system;
    GlobalSDFConfig config;
    config.num_cascades = 1;
    config.base_resolution = 16;
    config.base_voxel_size = 1.0f;

    system.Init(config);
    system.Update(glm::vec3(0.0f));

    // No meshes → should return max float
    float d = system.QueryDistance(glm::vec3(0.0f));
    EXPECT_GT(d, 1e30f);
}

TEST(GlobalSDFTest, GetDirtyCascades_AfterInit) {
    GlobalSDFSystem system;
    GlobalSDFConfig config;
    config.num_cascades = 3;
    config.base_resolution = 16;

    system.Init(config);

    auto dirty = system.GetDirtyCascades();
    EXPECT_EQ(dirty.size(), 3u);
}

TEST(GlobalSDFTest, ClearDirty_Works) {
    GlobalSDFSystem system;
    GlobalSDFConfig config;
    config.num_cascades = 2;
    config.base_resolution = 16;

    system.Init(config);
    system.ClearDirty(0);
    system.ClearDirty(1);

    EXPECT_TRUE(system.GetDirtyCascades().empty());
}

TEST(GlobalSDFTest, PointTriangleDistance_OnVertex) {
    // Test via QueryDistance after submitting a simple triangle
    GlobalSDFSystem system;
    GlobalSDFConfig config;
    config.num_cascades = 1;
    config.base_resolution = 8;
    config.base_voxel_size = 2.0f;

    system.Init(config);
    system.Update(glm::vec3(0.0f));

    // Submit a triangle at origin
    glm::vec3 positions[] = {
        glm::vec3(-5.0f, 0.0f, -5.0f),
        glm::vec3(5.0f, 0.0f, -5.0f),
        glm::vec3(0.0f, 0.0f, 5.0f)
    };
    uint32_t indices[] = {0, 1, 2};

    SDFMeshInput mesh;
    mesh.positions = positions;
    mesh.indices = indices;
    mesh.vertex_count = 3;
    mesh.index_count = 3;
    mesh.transform = glm::mat4(1.0f);

    system.SubmitStaticMesh(mesh);
    system.RebuildAll();

    // Point on the triangle plane should be close to 0
    float d = system.QueryDistance(glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_LT(d, 2.0f); // within voxel accuracy
}

TEST(GlobalSDFTest, Shutdown_ClearsState) {
    GlobalSDFSystem system;
    GlobalSDFConfig config;
    config.num_cascades = 3;
    config.base_resolution = 16;

    system.Init(config);
    system.Shutdown();

    EXPECT_EQ(system.CascadeCount(), 0);
}
