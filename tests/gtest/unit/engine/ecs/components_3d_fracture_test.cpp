/**
 * @file components_3d_fracture_test.cpp
 * @brief 物理破碎组件默认值与字段修改单元测试
 */

#include <gtest/gtest.h>
#include "engine/ecs/components_3d_fracture.h"

using namespace dse;

// ─── FragmentDescriptor ────────────────────────────────────────────────

// 测试 片段描述符：默认值
TEST(FragmentDescriptorTest, DefaultValues) {
    FragmentDescriptor desc;
    EXPECT_TRUE(desc.mesh_path.empty());
    EXPECT_FLOAT_EQ(desc.local_offset.x, 0.0f);
    EXPECT_FLOAT_EQ(desc.local_offset.y, 0.0f);
    EXPECT_FLOAT_EQ(desc.local_offset.z, 0.0f);
    EXPECT_FLOAT_EQ(desc.volume, 1.0f);
    EXPECT_TRUE(desc.runtime_vertices.empty());
    EXPECT_TRUE(desc.runtime_indices.empty());
    EXPECT_EQ(desc.runtime_vertex_stride, 0);
}

// 测试 片段描述符：字段修改
TEST(FragmentDescriptorTest, FieldModification) {
    FragmentDescriptor desc;
    desc.mesh_path = "fragments/frag_00.dmesh";
    desc.local_offset = glm::vec3(1.0f, 2.0f, 3.0f);
    desc.volume = 0.5f;
    desc.runtime_vertex_stride = 14;
    EXPECT_EQ(desc.mesh_path, "fragments/frag_00.dmesh");
    EXPECT_FLOAT_EQ(desc.local_offset.y, 2.0f);
    EXPECT_FLOAT_EQ(desc.volume, 0.5f);
    EXPECT_EQ(desc.runtime_vertex_stride, 14);
}

// ─── FractureAsset ─────────────────────────────────────────────────────

// 测试 断裂资源：默认值
TEST(FractureAssetTest, DefaultValues) {
    FractureAsset asset;
    EXPECT_TRUE(asset.source_mesh.empty());
    EXPECT_TRUE(asset.fragments.empty());
}

// 测试 断裂资源：添加到
TEST(FractureAssetTest, AddTo) {
    FractureAsset asset;
    asset.source_mesh = "barrel.dmesh";
    FragmentDescriptor d1;
    d1.mesh_path = "barrel_frag_00.dmesh";
    d1.volume = 0.3f;
    FragmentDescriptor d2;
    d2.mesh_path = "barrel_frag_01.dmesh";
    d2.volume = 0.7f;
    asset.fragments.push_back(d1);
    asset.fragments.push_back(d2);
    ASSERT_EQ(asset.fragments.size(), 2u);
    EXPECT_FLOAT_EQ(asset.fragments[0].volume + asset.fragments[1].volume, 1.0f);
}

// ─── FractureTriggerMode ───────────────────────────────────────────────

// 测试 断裂触发模式：枚举值
TEST(FractureTriggerModeTest, EnumerationValue) {
    EXPECT_EQ(static_cast<int>(FractureTriggerMode::ImpactForce), 0);
    EXPECT_EQ(static_cast<int>(FractureTriggerMode::DamageAccumulation), 1);
}

// ─── FractureSource ────────────────────────────────────────────────────

// 测试 断裂源：枚举值
TEST(FractureSourceTest, EnumerationValue) {
    EXPECT_EQ(static_cast<int>(FractureSource::Prefractured), 0);
    EXPECT_EQ(static_cast<int>(FractureSource::RuntimeVoronoi), 1);
}

// ─── FractureComponent ────────────────────────────────────────────────

// 测试 断裂组件：默认值
TEST(FractureComponentTest, DefaultValues) {
    FractureComponent fc;
    EXPECT_EQ(fc.source, FractureSource::Prefractured);
    EXPECT_TRUE(fc.fracture_asset_path.empty());
    EXPECT_EQ(fc.trigger_mode, FractureTriggerMode::ImpactForce);
    EXPECT_EQ(fc.runtime_fragment_count, 8u);
    EXPECT_EQ(fc.runtime_seed, 0u);
    EXPECT_TRUE(fc.cluster_near_impact);
    EXPECT_FLOAT_EQ(fc.break_force, 1000.0f);
    EXPECT_FLOAT_EQ(fc.health, 100.0f);
    EXPECT_FLOAT_EQ(fc.max_health, 100.0f);
    EXPECT_FLOAT_EQ(fc.fragment_lifetime, 5.0f);
    EXPECT_FLOAT_EQ(fc.fragment_fade_duration, 1.0f);
    EXPECT_FLOAT_EQ(fc.explosion_force, 50.0f);
    EXPECT_FLOAT_EQ(fc.fragment_mass_scale, 1.0f);
    EXPECT_TRUE(fc.inherit_material);
    EXPECT_EQ(fc.fragment_shader_variant, "MESH_LIT");
    EXPECT_FALSE(fc.is_fractured);
    EXPECT_FALSE(fc.fracture_requested);
    EXPECT_EQ(fc.cached_asset, nullptr);
}

// 测试 断裂组件：运行时Voronoi配置
TEST(FractureComponentTest, RuntimeVoronoiConfiguration) {
    FractureComponent fc;
    fc.source = FractureSource::RuntimeVoronoi;
    fc.runtime_fragment_count = 12;
    fc.runtime_seed = 42;
    fc.cluster_near_impact = false;
    EXPECT_EQ(fc.source, FractureSource::RuntimeVoronoi);
    EXPECT_EQ(fc.runtime_fragment_count, 12u);
    EXPECT_EQ(fc.runtime_seed, 42u);
    EXPECT_FALSE(fc.cluster_near_impact);
}

// 测试 断裂组件：模型
TEST(FractureComponentTest, Model) {
    FractureComponent fc;
    fc.trigger_mode = FractureTriggerMode::DamageAccumulation;
    fc.health = 80.0f;
    // 模拟扣血
    fc.health -= 30.0f;
    EXPECT_FLOAT_EQ(fc.health, 50.0f);
    fc.health -= 60.0f;
    EXPECT_LT(fc.health, 0.0f);
}

// ─── FragmentTagComponent ──────────────────────────────────────────────

// 测试 片段标签组件：默认值
TEST(FragmentTagComponentTest, DefaultValues) {
    FragmentTagComponent tag;
    EXPECT_EQ(tag.source_entity_id, UINT32_MAX);
    EXPECT_FLOAT_EQ(tag.elapsed, 0.0f);
    EXPECT_FLOAT_EQ(tag.lifetime, 5.0f);
    EXPECT_FLOAT_EQ(tag.fade_duration, 1.0f);
    EXPECT_FLOAT_EQ(tag.initial_alpha, 1.0f);
}

// 测试 片段标签组件：情形11
TEST(FragmentTagComponentTest, TestCase11) {
    FragmentTagComponent tag;
    tag.lifetime = 3.0f;
    tag.fade_duration = 2.0f;
    tag.initial_alpha = 0.8f;

    // 模拟时间推进到淡出阶段
    tag.elapsed = 4.0f; // 进入 fade 1秒
    float fade_progress = (tag.elapsed - tag.lifetime) / tag.fade_duration;
    float alpha = tag.initial_alpha * (1.0f - fade_progress);
    EXPECT_NEAR(alpha, 0.4f, 0.001f);

    // 完全淡出
    tag.elapsed = 5.0f;
    fade_progress = (tag.elapsed - tag.lifetime) / tag.fade_duration;
    alpha = tag.initial_alpha * (1.0f - fade_progress);
    EXPECT_NEAR(alpha, 0.0f, 0.001f);
}
