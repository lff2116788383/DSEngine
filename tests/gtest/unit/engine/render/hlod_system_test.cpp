/**
 * @file hlod_system_test.cpp
 * @brief HLOD 系统单元测试
 */

#include <gtest/gtest.h>
#include "engine/render/hlod/hlod_system.h"
#include <filesystem>
#include <cmath>

using namespace dse::render;

// ─── HLODBuilder 测试 ──────────────────────────────────────────────────────

class HLODBuilderTest : public ::testing::Test {
protected:
    std::vector<HLODBuildMesh> CreateTestMeshes(int count, float spacing = 10.0f) {
        std::vector<HLODBuildMesh> meshes;
        for (int i = 0; i < count; ++i) {
            HLODBuildMesh m;
            float offset = static_cast<float>(i) * spacing;
            // Simple quad
            m.positions = {
                glm::vec3(offset, 0, 0), glm::vec3(offset + 1, 0, 0),
                glm::vec3(offset + 1, 1, 0), glm::vec3(offset, 1, 0)
            };
            m.normals = {
                glm::vec3(0, 0, 1), glm::vec3(0, 0, 1),
                glm::vec3(0, 0, 1), glm::vec3(0, 0, 1)
            };
            m.texcoords = {
                glm::vec2(0, 0), glm::vec2(1, 0),
                glm::vec2(1, 1), glm::vec2(0, 1)
            };
            m.indices = {0, 1, 2, 0, 2, 3};
            m.transform = glm::mat4(1.0f);
            m.entity_index = static_cast<uint32_t>(i);
            meshes.push_back(m);
        }
        return meshes;
    }
};

TEST_F(HLODBuilderTest, Build_EmptyInput) {
    std::vector<HLODBuildMesh> meshes;
    HLODBuildConfig config;
    auto result = HLODBuilder::Build(meshes, config);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.clusters.empty());
}

TEST_F(HLODBuilderTest, Build_SingleMesh) {
    auto meshes = CreateTestMeshes(1);
    HLODBuildConfig config;
    config.hlod_levels = 2;
    auto result = HLODBuilder::Build(meshes, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.clusters.size(), 1u);
    EXPECT_EQ(result.clusters[0].levels.size(), 2u);
    EXPECT_EQ(result.clusters[0].source_entities.size(), 1u);
}

TEST_F(HLODBuilderTest, Build_MultipleMeshes_SingleCluster) {
    // 所有 mesh 在同一 cluster_radius 内
    auto meshes = CreateTestMeshes(5, 5.0f);  // spacing=5, all within radius=64
    HLODBuildConfig config;
    config.cluster_radius = 64.0f;
    config.hlod_levels = 3;
    auto result = HLODBuilder::Build(meshes, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.clusters.size(), 1u);
    EXPECT_EQ(result.clusters[0].source_entities.size(), 5u);
    EXPECT_EQ(result.clusters[0].levels.size(), 3u);
}

TEST_F(HLODBuilderTest, Build_MultipleMeshes_MultipleClusters) {
    // Mesh 间距超过 cluster_radius
    auto meshes = CreateTestMeshes(4, 200.0f);  // spacing=200, radius=64 → 每个单独一簇
    HLODBuildConfig config;
    config.cluster_radius = 64.0f;
    config.hlod_levels = 2;
    auto result = HLODBuilder::Build(meshes, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.clusters.size(), 4u);
    for (const auto& cluster : result.clusters) {
        EXPECT_EQ(cluster.source_entities.size(), 1u);
        EXPECT_EQ(cluster.levels.size(), 2u);
    }
}

TEST_F(HLODBuilderTest, Build_LevelDistances_Increasing) {
    auto meshes = CreateTestMeshes(3, 5.0f);
    HLODBuildConfig config;
    config.hlod_levels = 3;
    config.base_distance = 100.0f;
    config.level_distance_multiplier = 2.0f;
    auto result = HLODBuilder::Build(meshes, config);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.clusters[0].levels.size(), 3u);
    EXPECT_FLOAT_EQ(result.clusters[0].levels[0].switch_distance, 100.0f);
    EXPECT_FLOAT_EQ(result.clusters[0].levels[1].switch_distance, 200.0f);
    EXPECT_FLOAT_EQ(result.clusters[0].levels[2].switch_distance, 400.0f);
}

// ─── 序列化 Round-trip 测试 ────────────────────────────────────────────────

TEST_F(HLODBuilderTest, SaveLoad_RoundTrip) {
    auto meshes = CreateTestMeshes(6, 10.0f);
    HLODBuildConfig config;
    config.cluster_radius = 64.0f;
    config.hlod_levels = 2;
    auto result = HLODBuilder::Build(meshes, config);
    ASSERT_TRUE(result.success);

    const std::string tmp_path = "__test_hlod_roundtrip.dhlod";
    ASSERT_TRUE(HLODBuilder::SaveToFile(result.clusters, tmp_path));

    std::vector<HLODCluster> loaded;
    ASSERT_TRUE(HLODBuilder::LoadFromFile(tmp_path, loaded));

    EXPECT_EQ(loaded.size(), result.clusters.size());
    for (size_t i = 0; i < loaded.size(); ++i) {
        EXPECT_EQ(loaded[i].name, result.clusters[i].name);
        EXPECT_EQ(loaded[i].source_entities.size(), result.clusters[i].source_entities.size());
        EXPECT_EQ(loaded[i].levels.size(), result.clusters[i].levels.size());
        for (size_t l = 0; l < loaded[i].levels.size(); ++l) {
            EXPECT_EQ(loaded[i].levels[l].mesh_path, result.clusters[i].levels[l].mesh_path);
            EXPECT_FLOAT_EQ(loaded[i].levels[l].switch_distance, result.clusters[i].levels[l].switch_distance);
        }
    }

    std::filesystem::remove(tmp_path);
}

TEST_F(HLODBuilderTest, LoadFromFile_InvalidPath) {
    std::vector<HLODCluster> clusters;
    EXPECT_FALSE(HLODBuilder::LoadFromFile("nonexistent_file.dhlod", clusters));
}

// ─── HLODProxy 默认值 ──────────────────────────────────────────────────────

TEST(HLODProxyTest, DefaultValues) {
    HLODProxy proxy;
    EXPECT_TRUE(proxy.mesh_path.empty());
    EXPECT_FLOAT_EQ(proxy.switch_distance, 0.0f);
    EXPECT_EQ(proxy.triangle_count, 0u);
}

// ─── HLODConfigComponent 默认值 ──────────────────────────────────────────

TEST(HLODConfigTest, DefaultValues) {
    HLODConfigComponent config;
    EXPECT_TRUE(config.enabled);
    EXPECT_TRUE(config.hlod_data_path.empty());
    EXPECT_FLOAT_EQ(config.distance_scale, 1.0f);
    EXPECT_FLOAT_EQ(config.hysteresis, 0.1f);
}

// ─── HLODMemberComponent 默认值 ──────────────────────────────────────────

TEST(HLODMemberTest, DefaultValues) {
    HLODMemberComponent member;
    EXPECT_EQ(member.cluster_index, 0u);
    EXPECT_FALSE(member.hidden_by_hlod);
}
