/**
 * @file hair_instance_test.cpp
 * @brief HairInstance LOD 选择 + 参数默认值测试（无 GPU 依赖）
 */

#include <gtest/gtest.h>
#include "engine/render/hair/hair_instance.h"
#include "engine/render/hair/hair_asset.h"

using namespace dse::render;

class HairInstanceLODTest : public ::testing::Test {
protected:
    void SetUp() override {
        GenerateTestHairAsset(100, 16, 5.0f, 2.0f, asset_);
        inst_.asset = &asset_;
        inst_.active_strand_count = asset_.num_guide_strands();
        inst_.total_vertex_count = asset_.num_vertices();
    }

    HairAsset asset_;
    HairInstance inst_;
};

// 测试 毛发实例LOD：资源为有效
TEST_F(HairInstanceLODTest, AssetIsValid) {
    EXPECT_TRUE(asset_.IsValid());
    EXPECT_EQ(asset_.num_guide_strands(), 100u);
    EXPECT_EQ(asset_.num_vertices(), 1600u);
}

// 测试 毛发实例LOD：默认Sim参数
TEST_F(HairInstanceLODTest, DefaultSimParams) {
    HairSimParams params;
    EXPECT_FLOAT_EQ(params.damping, 0.04f);
    EXPECT_FLOAT_EQ(params.stiffness_local, 0.8f);
    EXPECT_FLOAT_EQ(params.stiffness_global, 0.4f);
    EXPECT_FLOAT_EQ(params.gravity_magnitude, 9.81f);
    EXPECT_EQ(params.local_constraint_iterations, 2);
    EXPECT_EQ(params.length_constraint_iterations, 2);
}

// 测试 毛发实例LOD：默认渲染参数
TEST_F(HairInstanceLODTest, DefaultRenderParams) {
    HairRenderParams params;
    EXPECT_GT(params.fiber_radius, 0.0f);
    EXPECT_GT(params.opacity, 0.0f);
    EXPECT_TRUE(params.receive_shadow);
    EXPECT_TRUE(params.cast_shadow);
}

// 测试 毛发实例LOD：默认LOD参数
TEST_F(HairInstanceLODTest, DefaultLODParams) {
    HairLODParams params;
    EXPECT_LT(params.lod0_distance, params.lod1_distance);
    EXPECT_LT(params.lod1_distance, params.lod2_distance);
    EXPECT_LT(params.lod2_distance, params.cull_distance);
}

// 测试 毛发实例LOD：LOD 0完整Quality
TEST_F(HairInstanceLODTest, LOD0FullQuality) {
    inst_.UpdateLOD(10.0f);
    EXPECT_EQ(inst_.current_lod, 0);
    EXPECT_EQ(inst_.active_strand_count, asset_.num_guide_strands());
}

// 测试 毛发实例LOD：LOD 1 Mid距离
TEST_F(HairInstanceLODTest, LOD1MidDistance) {
    inst_.UpdateLOD(50.0f);
    EXPECT_EQ(inst_.current_lod, 1);
    EXPECT_LT(inst_.active_strand_count, asset_.num_guide_strands());
    EXPECT_GT(inst_.active_strand_count, 0u);
}

// 测试 毛发实例LOD：LOD 2远距离
TEST_F(HairInstanceLODTest, LOD2FarDistance) {
    inst_.UpdateLOD(100.0f);
    EXPECT_EQ(inst_.current_lod, 2);
    EXPECT_LT(inst_.active_strand_count, asset_.num_guide_strands());
    EXPECT_GT(inst_.active_strand_count, 0u);
}

// 测试 毛发实例LOD：LOD 3剔除
TEST_F(HairInstanceLODTest, LOD3Culled) {
    inst_.UpdateLOD(200.0f);
    EXPECT_EQ(inst_.current_lod, 3);
    EXPECT_EQ(inst_.active_strand_count, 0u);
}

// 测试 毛发实例LOD：LOD边界
TEST_F(HairInstanceLODTest, LODBoundary) {
    inst_.UpdateLOD(inst_.lod_params.lod0_distance - 0.01f);
    EXPECT_EQ(inst_.current_lod, 0);

    inst_.UpdateLOD(inst_.lod_params.cull_distance + 1.0f);
    EXPECT_EQ(inst_.current_lod, 3);
    EXPECT_EQ(inst_.active_strand_count, 0u);
}

// 测试 毛发实例LOD：空资源剔除
TEST_F(HairInstanceLODTest, NullAssetCulled) {
    HairInstance empty;
    empty.asset = nullptr;
    empty.UpdateLOD(5.0f);
    EXPECT_EQ(empty.current_lod, 3);
    EXPECT_EQ(empty.active_strand_count, 0u);
}

// 测试 毛发实例LOD：初始GPU资源无效
TEST_F(HairInstanceLODTest, InitialGPUResourcesInvalid) {
    HairInstance fresh;
    EXPECT_FALSE(fresh.gpu_resources_valid);
}
