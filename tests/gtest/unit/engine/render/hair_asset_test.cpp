/**
 * @file hair_asset_test.cpp
 * @brief HairAsset / HairInstance 单元测试（纯 CPU 端）
 *
 * 测试策略：
 * - HairStrandVertex / HairStrand 默认值
 * - HairAsset::IsValid() 各种边界
 * - GenerateTestHairAsset 数据完整性
 * - HairSimParams / HairRenderParams / HairLODParams 默认值
 * - HairInstance::UpdateLOD() LOD 级别切换
 */

#include <gtest/gtest.h>
#include "engine/render/hair/hair_asset.h"
#include "engine/render/hair/hair_instance.h"
#include <glm/glm.hpp>
#include <cmath>

using namespace dse::render;

// ============================================================
// HairStrandVertex
// ============================================================

TEST(HairStrandVertexTest, 默认值全零) {
    HairStrandVertex v{};
    EXPECT_FLOAT_EQ(v.position.x, 0.0f);
    EXPECT_FLOAT_EQ(v.position.w, 0.0f);
    EXPECT_FLOAT_EQ(v.tangent.x, 0.0f);
    EXPECT_FLOAT_EQ(v.tangent.w, 0.0f);
}

// ============================================================
// HairStrand
// ============================================================

TEST(HairStrandTest, 默认值) {
    HairStrand s;
    EXPECT_EQ(s.vertex_offset, 0u);
    EXPECT_EQ(s.vertex_count, 0u);
}

// ============================================================
// HairAsset::IsValid
// ============================================================

TEST(HairAssetTest, 空资产无效) {
    HairAsset asset;
    EXPECT_FALSE(asset.IsValid());
}

TEST(HairAssetTest, 顶点为空无效) {
    HairAsset asset;
    asset.strands.resize(2);
    asset.vertices_per_strand = 4;
    EXPECT_FALSE(asset.IsValid());
}

TEST(HairAssetTest, Strand为空无效) {
    HairAsset asset;
    asset.vertices.resize(8);
    asset.vertices_per_strand = 4;
    EXPECT_FALSE(asset.IsValid());
}

TEST(HairAssetTest, 顶点数不匹配无效) {
    HairAsset asset;
    asset.strands.resize(2);
    asset.vertices.resize(7);  // 应该是 2*4=8
    asset.vertices_per_strand = 4;
    EXPECT_FALSE(asset.IsValid());
}

TEST(HairAssetTest, VerticesPerStrand为零无效) {
    HairAsset asset;
    asset.strands.resize(2);
    asset.vertices.resize(0);
    asset.vertices_per_strand = 0;
    EXPECT_FALSE(asset.IsValid());
}

TEST(HairAssetTest, 有效资产) {
    HairAsset asset;
    asset.strands.resize(3);
    asset.vertices.resize(48);
    asset.vertices_per_strand = 16;
    EXPECT_TRUE(asset.IsValid());
    EXPECT_EQ(asset.num_guide_strands(), 3u);
    EXPECT_EQ(asset.num_vertices(), 48u);
}

// ============================================================
// HairAsset 默认参数
// ============================================================

TEST(HairAssetTest, 默认参数) {
    HairAsset asset;
    EXPECT_EQ(asset.vertices_per_strand, 16u);
    EXPECT_EQ(asset.num_follow_per_guide, 4u);
    EXPECT_FLOAT_EQ(asset.follow_root_offset_range, 1.5f);
    EXPECT_TRUE(asset.name.empty());
}

// ============================================================
// GenerateTestHairAsset
// ============================================================

TEST(HairAssetTest, 程序化生成_数据完整性) {
    HairAsset asset;
    GenerateTestHairAsset(10, 8, 2.0f, 1.0f, asset);

    EXPECT_TRUE(asset.IsValid());
    EXPECT_EQ(asset.num_guide_strands(), 10u);
    EXPECT_EQ(asset.num_vertices(), 80u);
    EXPECT_EQ(asset.vertices_per_strand, 8u);
    EXPECT_EQ(asset.name, "procedural_sphere_hair");
}

TEST(HairAssetTest, 程序化生成_根部在球面上) {
    HairAsset asset;
    GenerateTestHairAsset(20, 16, 3.0f, 2.0f, asset);

    for (uint32_t s = 0; s < asset.num_guide_strands(); ++s) {
        glm::vec3 root(asset.vertices[s * 16].position);
        float dist = glm::length(root);
        EXPECT_NEAR(dist, 2.0f, 0.1f) << "strand " << s << " root not on sphere";
    }
}

TEST(HairAssetTest, 程序化生成_切线已归一化) {
    HairAsset asset;
    GenerateTestHairAsset(5, 8, 2.0f, 1.0f, asset);

    for (uint32_t i = 0; i < asset.num_vertices(); ++i) {
        glm::vec3 t(asset.vertices[i].tangent);
        float len = glm::length(t);
        EXPECT_NEAR(len, 1.0f, 0.01f) << "vertex " << i << " tangent not unit";
    }
}

// ============================================================
// HairSimParams 默认值
// ============================================================

TEST(HairSimParamsTest, 默认值) {
    HairSimParams p;
    EXPECT_FLOAT_EQ(p.damping, 0.04f);
    EXPECT_FLOAT_EQ(p.stiffness_local, 0.8f);
    EXPECT_FLOAT_EQ(p.stiffness_global, 0.4f);
    EXPECT_FLOAT_EQ(p.gravity_magnitude, 9.81f);
    EXPECT_FLOAT_EQ(p.gravity_dir.y, -1.0f);
    EXPECT_FLOAT_EQ(p.wind_turbulence, 0.2f);
    EXPECT_EQ(p.local_constraint_iterations, 2);
    EXPECT_EQ(p.length_constraint_iterations, 2);
}

// ============================================================
// HairRenderParams 默认值
// ============================================================

TEST(HairRenderParamsTest, 默认值) {
    HairRenderParams p;
    EXPECT_FLOAT_EQ(p.root_color.r, 0.1f);
    EXPECT_FLOAT_EQ(p.tip_color.a, 1.0f);
    EXPECT_FLOAT_EQ(p.fiber_radius, 0.04f);
    EXPECT_FLOAT_EQ(p.opacity, 0.9f);
    EXPECT_FLOAT_EQ(p.specular_power_primary, 80.0f);
    EXPECT_FLOAT_EQ(p.specular_power_secondary, 20.0f);
    EXPECT_FLOAT_EQ(p.shadow_density, 0.5f);
    EXPECT_TRUE(p.receive_shadow);
    EXPECT_TRUE(p.cast_shadow);
}

// ============================================================
// HairLODParams 默认值
// ============================================================

TEST(HairLODParamsTest, 默认值) {
    HairLODParams p;
    EXPECT_FLOAT_EQ(p.lod0_distance, 20.0f);
    EXPECT_FLOAT_EQ(p.lod1_distance, 40.0f);
    EXPECT_FLOAT_EQ(p.lod2_distance, 80.0f);
    EXPECT_FLOAT_EQ(p.cull_distance, 120.0f);
    EXPECT_FLOAT_EQ(p.lod1_strand_ratio, 0.5f);
    EXPECT_FLOAT_EQ(p.lod2_strand_ratio, 0.25f);
}

// ============================================================
// HairInstance::UpdateLOD
// ============================================================

class HairInstanceLODTest : public ::testing::Test {
protected:
    void SetUp() override {
        asset_.strands.resize(100);
        asset_.vertices.resize(1600);
        asset_.vertices_per_strand = 16;

        instance_.asset = &asset_;
        instance_.total_vertex_count = asset_.num_vertices();
        instance_.active_strand_count = asset_.num_guide_strands();
    }
    HairAsset asset_;
    HairInstance instance_;
};

TEST_F(HairInstanceLODTest, 近距离_LOD0_全精度) {
    instance_.UpdateLOD(10.0f);
    EXPECT_EQ(instance_.current_lod, 0);
    EXPECT_EQ(instance_.active_strand_count, 100u);
}

TEST_F(HairInstanceLODTest, 中距离_LOD1_半精度) {
    instance_.UpdateLOD(50.0f);
    EXPECT_EQ(instance_.current_lod, 1);
    EXPECT_EQ(instance_.active_strand_count, 50u);
}

TEST_F(HairInstanceLODTest, 远距离_LOD2_低精度) {
    instance_.UpdateLOD(90.0f);
    EXPECT_EQ(instance_.current_lod, 2);
    EXPECT_EQ(instance_.active_strand_count, 25u);
}

TEST_F(HairInstanceLODTest, 超远距离_剔除) {
    instance_.UpdateLOD(150.0f);
    EXPECT_EQ(instance_.current_lod, 3);
    EXPECT_EQ(instance_.active_strand_count, 0u);
}

TEST_F(HairInstanceLODTest, 无资产_默认剔除) {
    HairInstance empty;
    empty.UpdateLOD(10.0f);
    EXPECT_EQ(empty.current_lod, 3);
    EXPECT_EQ(empty.active_strand_count, 0u);
}

TEST_F(HairInstanceLODTest, 边界距离_LOD0阈值) {
    instance_.UpdateLOD(20.0f);
    EXPECT_EQ(instance_.current_lod, 0);
    EXPECT_EQ(instance_.active_strand_count, 100u);
}

// ============================================================
// HairInstance 默认状态
// ============================================================

TEST(HairInstanceTest, 默认状态) {
    HairInstance inst;
    EXPECT_EQ(inst.asset, nullptr);
    EXPECT_FALSE(inst.position_ssbo);
    EXPECT_FALSE(inst.position_prev_ssbo);
    EXPECT_FALSE(inst.position_rest_ssbo);
    EXPECT_FALSE(inst.tangent_ssbo);
    EXPECT_FALSE(inst.strand_info_ssbo);
    EXPECT_FALSE(inst.gpu_resources_valid);
    EXPECT_EQ(inst.current_lod, 0);
    EXPECT_EQ(inst.total_vertex_count, 0u);
    EXPECT_EQ(inst.active_strand_count, 0u);
    EXPECT_TRUE(inst.draw_firsts_.empty());
    EXPECT_TRUE(inst.draw_counts_.empty());
}
