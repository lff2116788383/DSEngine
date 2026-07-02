/**
 * @file hlod_deep_test.cpp
 * @brief P3: HLOD 系统深度边界测试
 *
 * 补充 hlod_system_test.cpp 中未覆盖的场景：
 * - 大量 mesh 的聚类行为
 * - 极端配置参数（0 levels / huge radius）
 * - HLOD 层级距离计算边界
 * - mesh 在同一位置（重叠）时的聚类
 * - 大规模 SaveLoad round-trip 一致性
 */

#include <gtest/gtest.h>
#include "engine/render/hlod/hlod_system.h"
#include <cstdint>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace dse::render;

class HLODDeepTest : public ::testing::Test {
protected:
    std::vector<HLODBuildMesh> CreateTestMeshes(int count, float spacing = 10.0f) {
        std::vector<HLODBuildMesh> meshes;
        for (int i = 0; i < count; ++i) {
            HLODBuildMesh m;
            float offset = static_cast<float>(i) * spacing;
            m.positions = {
                glm::vec3(offset, 0, 0), glm::vec3(offset + 1, 0, 0),
                glm::vec3(offset + 1, 1, 0), glm::vec3(offset, 1, 0)
            };
            m.normals.resize(4, glm::vec3(0, 0, 1));
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

// ─── 大量 mesh 聚类 ────────────────────────────────────────────────────────

TEST_F(HLODDeepTest, Build_LargeMeshCount_AllInOneCluster) {
    auto meshes = CreateTestMeshes(100, 0.5f);  // 密集排列
    HLODBuildConfig config;
    config.cluster_radius = 1000.0f;
    config.hlod_levels = 2;

    auto result = HLODBuilder::Build(meshes, config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.clusters.size(), 1u);
    EXPECT_EQ(result.clusters[0].source_entities.size(), 100u);
}

TEST_F(HLODDeepTest, Build_LargeMeshCount_ManyClusters) {
    auto meshes = CreateTestMeshes(50, 500.0f);  // 极稀疏
    HLODBuildConfig config;
    config.cluster_radius = 32.0f;
    config.hlod_levels = 2;

    auto result = HLODBuilder::Build(meshes, config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.clusters.size(), 50u);
    for (const auto& c : result.clusters) {
        EXPECT_EQ(c.source_entities.size(), 1u);
    }
}

// ─── 极端配置 ──────────────────────────────────────────────────────────────

TEST_F(HLODDeepTest, Build_SingleLevel) {
    auto meshes = CreateTestMeshes(3, 5.0f);
    HLODBuildConfig config;
    config.hlod_levels = 1;

    auto result = HLODBuilder::Build(meshes, config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.clusters[0].levels.size(), 1u);
}

TEST_F(HLODDeepTest, Build_ManyLevels) {
    auto meshes = CreateTestMeshes(5, 5.0f);
    HLODBuildConfig config;
    config.hlod_levels = 8;
    config.base_distance = 50.0f;
    config.level_distance_multiplier = 1.5f;

    auto result = HLODBuilder::Build(meshes, config);
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.clusters[0].levels.size(), 8u);

    // 验证距离严格递增
    for (size_t i = 1; i < result.clusters[0].levels.size(); ++i) {
        EXPECT_GT(result.clusters[0].levels[i].switch_distance,
                  result.clusters[0].levels[i - 1].switch_distance);
    }
}

// ─── 重叠位置 mesh 的聚类 ──────────────────────────────────────────────────

TEST_F(HLODDeepTest, Build_OverlappingMeshes_SameCluster) {
    auto meshes = CreateTestMeshes(10, 0.0f);  // spacing=0, 全部重叠
    HLODBuildConfig config;
    config.cluster_radius = 64.0f;
    config.hlod_levels = 2;

    auto result = HLODBuilder::Build(meshes, config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.clusters.size(), 1u);
    EXPECT_EQ(result.clusters[0].source_entities.size(), 10u);
}

// ─── 距离乘数为 1（所有 level 相同距离） ───────────────────────────────────

TEST_F(HLODDeepTest, Build_DistanceMultiplier1_AllSameDistance) {
    auto meshes = CreateTestMeshes(2, 5.0f);
    HLODBuildConfig config;
    config.hlod_levels = 3;
    config.base_distance = 100.0f;
    config.level_distance_multiplier = 1.0f;

    auto result = HLODBuilder::Build(meshes, config);
    EXPECT_TRUE(result.success);
    for (const auto& level : result.clusters[0].levels) {
        EXPECT_FLOAT_EQ(level.switch_distance, 100.0f);
    }
}

// ─── 大规模 SaveLoad round-trip ──────────────────────────────────────────

TEST_F(HLODDeepTest, SaveLoad_LargeDataset_RoundTrip) {
    auto meshes = CreateTestMeshes(30, 10.0f);
    HLODBuildConfig config;
    config.cluster_radius = 64.0f;
    config.hlod_levels = 4;
    config.base_distance = 200.0f;
    config.level_distance_multiplier = 2.0f;

    auto result = HLODBuilder::Build(meshes, config);
    ASSERT_TRUE(result.success);
    ASSERT_GT(result.clusters.size(), 0u);

    const std::string tmp_path = "__test_hlod_deep_roundtrip.dhlod";
    ASSERT_TRUE(HLODBuilder::SaveToFile(result.clusters, tmp_path));

    std::vector<HLODCluster> loaded;
    ASSERT_TRUE(HLODBuilder::LoadFromFile(tmp_path, loaded));

    EXPECT_EQ(loaded.size(), result.clusters.size());
    for (size_t i = 0; i < loaded.size(); ++i) {
        EXPECT_EQ(loaded[i].levels.size(), result.clusters[i].levels.size());
        EXPECT_EQ(loaded[i].source_entities.size(), result.clusters[i].source_entities.size());
        for (size_t l = 0; l < loaded[i].levels.size(); ++l) {
            EXPECT_FLOAT_EQ(loaded[i].levels[l].switch_distance,
                            result.clusters[i].levels[l].switch_distance);
        }
    }

    std::filesystem::remove(tmp_path);
}

// ─── Build 后 source_entities 索引正确 ─────────────────────────────────────

TEST_F(HLODDeepTest, Build_SourceEntityIndices_Correct) {
    auto meshes = CreateTestMeshes(8, 5.0f);
    HLODBuildConfig config;
    config.cluster_radius = 1000.0f;
    config.hlod_levels = 2;

    auto result = HLODBuilder::Build(meshes, config);
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.clusters.size(), 1u);

    auto& entities = result.clusters[0].source_entities;
    EXPECT_EQ(entities.size(), 8u);

    // 验证所有 source_entities 索引都在 [0,8) 范围内
    std::vector<uint32_t> indices;
    for (auto e : entities) {
        indices.push_back(static_cast<uint32_t>(e));
    }
    std::sort(indices.begin(), indices.end());
    for (size_t i = 0; i < indices.size(); ++i) {
        EXPECT_EQ(indices[i], static_cast<uint32_t>(i));
    }
}
