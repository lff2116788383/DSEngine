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

// 测试 毛发发丝顶点：默认值全部零
TEST(HairStrandVertexTest, DefaultValuesAllZero) {
    HairStrandVertex v{};
    EXPECT_FLOAT_EQ(v.position.x, 0.0f);
    EXPECT_FLOAT_EQ(v.position.w, 0.0f);
    EXPECT_FLOAT_EQ(v.tangent.x, 0.0f);
    EXPECT_FLOAT_EQ(v.tangent.w, 0.0f);
}

// ============================================================
// HairStrand
// ============================================================

// 测试 毛发发丝：默认值
TEST(HairStrandTest, DefaultValues) {
    HairStrand s;
    EXPECT_EQ(s.vertex_offset, 0u);
    EXPECT_EQ(s.vertex_count, 0u);
}

// ============================================================
// HairAsset::IsValid
// ============================================================

// 测试 毛发资源：空无效
TEST(HairAssetTest, EmptyInvalid) {
    HairAsset asset;
    EXPECT_FALSE(asset.IsValid());
}

// 测试 毛发资源：点无效若空
TEST(HairAssetTest, PointInvalidIfEmpty) {
    HairAsset asset;
    asset.strands.resize(2);
    asset.vertices_per_strand = 4;
    EXPECT_FALSE(asset.IsValid());
}

// 测试 毛发资源：发丝无效若空
TEST(HairAssetTest, StrandInvalidIfEmpty) {
    HairAsset asset;
    asset.vertices.resize(8);
    asset.vertices_per_strand = 4;
    EXPECT_FALSE(asset.IsValid());
}

// 测试 毛发资源：点不无效
TEST(HairAssetTest, PointNotInvalid) {
    HairAsset asset;
    asset.strands.resize(2);
    asset.vertices.resize(7);  // 应该是 2*4=8
    asset.vertices_per_strand = 4;
    EXPECT_FALSE(asset.IsValid());
}

// 测试 毛发资源：Vertices每发丝无效若零
TEST(HairAssetTest, VerticesPerStrandInvalidIfZero) {
    HairAsset asset;
    asset.strands.resize(2);
    asset.vertices.resize(0);
    asset.vertices_per_strand = 0;
    EXPECT_FALSE(asset.IsValid());
}

// 测试 毛发资源：有效
TEST(HairAssetTest, Valid) {
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

// 测试 毛发资源：默认参数
TEST(HairAssetTest, DefaultParameters) {
    HairAsset asset;
    EXPECT_EQ(asset.vertices_per_strand, 16u);
    EXPECT_EQ(asset.num_follow_per_guide, 4u);
    EXPECT_FLOAT_EQ(asset.follow_root_offset_range, 1.5f);
    EXPECT_TRUE(asset.name.empty());
}

// ============================================================
// GenerateTestHairAsset
// ============================================================

// 测试 毛发资源：程序化代次数据Integrity
TEST(HairAssetTest, ProceduralGeneration_DataIntegrity) {
    HairAsset asset;
    GenerateTestHairAsset(10, 8, 2.0f, 1.0f, asset);

    EXPECT_TRUE(asset.IsValid());
    EXPECT_EQ(asset.num_guide_strands(), 10u);
    EXPECT_EQ(asset.num_vertices(), 80u);
    EXPECT_EQ(asset.vertices_per_strand, 8u);
    EXPECT_EQ(asset.name, "procedural_sphere_hair");
}

// 测试 毛发资源：程序化代次存在
TEST(HairAssetTest, ProceduralGeneration_Exist) {
    HairAsset asset;
    GenerateTestHairAsset(20, 16, 3.0f, 2.0f, asset);

    for (uint32_t s = 0; s < asset.num_guide_strands(); ++s) {
        glm::vec3 root(asset.vertices[s * 16].position);
        float dist = glm::length(root);
        EXPECT_NEAR(dist, 2.0f, 0.1f) << "strand " << s << " root not on sphere";
    }
}

// 测试 毛发资源：程序化代次已经单个
TEST(HairAssetTest, ProceduralGeneration_AlreadyOne) {
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

// 测试 毛发Sim参数：默认值
TEST(HairSimParamsTest, DefaultValues) {
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

// 测试 毛发渲染参数：默认值
TEST(HairRenderParamsTest, DefaultValues) {
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

// 测试 毛发LOD参数：默认值
TEST(HairLODParamsTest, DefaultValues) {
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

// 测试 毛发实例LOD：Near距离LOD 0完整精度
TEST_F(HairInstanceLODTest, NearDistance_LOD0_FullPrecision) {
    instance_.UpdateLOD(10.0f);
    EXPECT_EQ(instance_.current_lod, 0);
    EXPECT_EQ(instance_.active_strand_count, 100u);
}

// 测试 毛发实例LOD：Mid距离LOD 1半精度
TEST_F(HairInstanceLODTest, MidDistance_LOD1_HalfPrecision) {
    instance_.UpdateLOD(50.0f);
    EXPECT_EQ(instance_.current_lod, 1);
    EXPECT_EQ(instance_.active_strand_count, 50u);
}

// 测试 毛发实例LOD：远距离LOD 2 Low精度
TEST_F(HairInstanceLODTest, FarDistance_LOD2_LowPrecision) {
    instance_.UpdateLOD(90.0f);
    EXPECT_EQ(instance_.current_lod, 2);
    EXPECT_EQ(instance_.active_strand_count, 25u);
}

// 测试 毛发实例LOD：远距离情形
TEST_F(HairInstanceLODTest, FarDistance_Case) {
    instance_.UpdateLOD(150.0f);
    EXPECT_EQ(instance_.current_lod, 3);
    EXPECT_EQ(instance_.active_strand_count, 0u);
}

// 测试 毛发实例LOD：无默认
TEST_F(HairInstanceLODTest, Without_Default) {
    HairInstance empty;
    empty.UpdateLOD(10.0f);
    EXPECT_EQ(empty.current_lod, 3);
    EXPECT_EQ(empty.active_strand_count, 0u);
}

// 测试 毛发实例LOD：边界距离LOD 0
TEST_F(HairInstanceLODTest, BoundaryDistance_LOD0) {
    instance_.UpdateLOD(20.0f);
    EXPECT_EQ(instance_.current_lod, 0);
    EXPECT_EQ(instance_.active_strand_count, 100u);
}

// ============================================================
// HairInstance 默认状态
// ============================================================

// 测试 毛发实例：默认状态
TEST(HairInstanceTest, DefaultState) {
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
