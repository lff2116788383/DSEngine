/**
 * @file anim_layer_ik_test.cpp
 * @brief 动画层混合 + IK 系统单元测试
 *
 * 覆盖场景：
 * - 组件默认值
 * - 空 World 调用 Update 不崩溃
 * - AnimLayerComponent + IKChain3DComponent 字段设置
 * - IK 链索引缓存 dirty 标记
 * - anim_clip_eval.h 工具函数
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/animation/anim_layer_blend_system.h"
#include "modules/gameplay_3d/animation/ik_solver_system.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"

using namespace dse;
using namespace gameplay3d;

// ============================================================
// AnimLayerComponent 默认值
// ============================================================

TEST(AnimLayerTest, AnimLayerComponent默认值) {
    AnimLayerComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_TRUE(comp.layers.empty());
}

TEST(AnimLayerTest, AnimLayerConfig默认值) {
    AnimLayerConfig layer;
    EXPECT_TRUE(layer.name.empty());
    EXPECT_FLOAT_EQ(layer.weight, 1.0f);
    EXPECT_EQ(layer.blend_mode, AnimLayerBlendMode::Override);
    EXPECT_EQ(layer.source_type, AnimSourceType::SingleClip);
    EXPECT_TRUE(layer.bone_mask_include.empty());
    EXPECT_TRUE(layer.bone_mask_dirty);
    EXPECT_TRUE(layer.danim_path.empty());
    EXPECT_FLOAT_EQ(layer.current_time, 0.0f);
    EXPECT_FLOAT_EQ(layer.speed, 1.0f);
    EXPECT_TRUE(layer.loop);
}

TEST(AnimLayerTest, AnimBlendNode2D默认值) {
    AnimBlendNode2D node;
    EXPECT_TRUE(node.name.empty());
    EXPECT_TRUE(node.danim_path.empty());
    EXPECT_FLOAT_EQ(node.x, 0.0f);
    EXPECT_FLOAT_EQ(node.y, 0.0f);
    EXPECT_FLOAT_EQ(node.speed, 1.0f);
    EXPECT_TRUE(node.loop);
}

TEST(AnimLayerTest, 空World调用LayerBlendUpdate不崩溃) {
    World world;
    AnimLayerBlendSystem::SetAssetManager(nullptr);
    EXPECT_NO_THROW(AnimLayerBlendSystem::Update(world, 0.016f));
}

TEST(AnimLayerTest, 有组件但无AssetManager不崩溃) {
    World world;
    AnimLayerBlendSystem::SetAssetManager(nullptr);
    auto e = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(e);
    anim.enabled = true;
    anim.skel_cache.valid = false;
    world.registry().emplace<AnimLayerComponent>(e);
    EXPECT_NO_THROW(AnimLayerBlendSystem::Update(world, 0.016f));
}

// ============================================================
// IKChain3DComponent 默认值
// ============================================================

TEST(IKTest, IKChain3DComponent默认值) {
    IKChain3DComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_TRUE(comp.chains.empty());
}

TEST(IKTest, IKChainConfig默认值) {
    IKChainConfig chain;
    EXPECT_TRUE(chain.name.empty());
    EXPECT_EQ(chain.type, IKChainType::FABRIK);
    EXPECT_TRUE(chain.root_bone.empty());
    EXPECT_TRUE(chain.tip_bone.empty());
    EXPECT_FLOAT_EQ(chain.weight, 1.0f);
    EXPECT_EQ(chain.target_entity, UINT32_MAX);
    EXPECT_FLOAT_EQ(chain.target_position.x, 0.0f);
    EXPECT_FLOAT_EQ(chain.tolerance, 0.01f);
    EXPECT_EQ(chain.iterations, 10);
    EXPECT_TRUE(chain.indices_dirty);
    EXPECT_EQ(chain.root_bone_index, -1);
    EXPECT_EQ(chain.tip_bone_index, -1);
}

TEST(IKTest, 空World调用IKUpdate不崩溃) {
    World world;
    EXPECT_NO_THROW(IKSolverSystem::Update(world, 0.016f));
}

TEST(IKTest, 有组件但无有效缓存不崩溃) {
    World world;
    auto e = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(e);
    anim.enabled = true;
    anim.skel_cache.valid = false;
    auto& ik = world.registry().emplace<IKChain3DComponent>(e);
    IKChainConfig chain;
    chain.name = "test_chain";
    chain.type = IKChainType::FABRIK;
    chain.root_bone = "Hips";
    chain.tip_bone = "LeftFoot";
    ik.chains.push_back(chain);
    EXPECT_NO_THROW(IKSolverSystem::Update(world, 0.016f));
}

// ============================================================
// anim_clip_eval.h 工具函数测试
// ============================================================

TEST(AnimClipEvalTest, AdvanceClipTime_Loop) {
    float t = anim_util::AdvanceClipTime(0.9f, 0.016f, 10.0f, 1.0f, true);
    EXPECT_GE(t, 0.0f);
    EXPECT_LT(t, 1.0f);
}

TEST(AnimClipEvalTest, AdvanceClipTime_NoLoop_Clamp) {
    float t = anim_util::AdvanceClipTime(0.9f, 0.016f, 10.0f, 1.0f, false);
    EXPECT_FLOAT_EQ(t, 1.0f);
}

TEST(AnimClipEvalTest, AdvanceClipTime_ZeroDuration) {
    float t = anim_util::AdvanceClipTime(0.5f, 0.016f, 1.0f, 0.0f, true);
    EXPECT_FLOAT_EQ(t, 0.0f);
}

TEST(AnimClipEvalTest, Interpolate_SingleKey) {
    std::vector<float> times = {0.0f};
    std::vector<glm::vec3> values = {glm::vec3(1.0f, 2.0f, 3.0f)};
    auto r = anim_util::Interpolate<glm::vec3>(times, values, 0.5f);
    EXPECT_FLOAT_EQ(r.x, 1.0f);
    EXPECT_FLOAT_EQ(r.y, 2.0f);
    EXPECT_FLOAT_EQ(r.z, 3.0f);
}

TEST(AnimClipEvalTest, Interpolate_TwoKeys_Midpoint) {
    std::vector<float> times = {0.0f, 1.0f};
    std::vector<glm::vec3> values = {glm::vec3(0.0f), glm::vec3(10.0f)};
    auto r = anim_util::Interpolate<glm::vec3>(times, values, 0.5f);
    EXPECT_NEAR(r.x, 5.0f, 0.01f);
    EXPECT_NEAR(r.y, 5.0f, 0.01f);
    EXPECT_NEAR(r.z, 5.0f, 0.01f);
}

TEST(AnimClipEvalTest, Interpolate_QuatSlerp) {
    std::vector<float> times = {0.0f, 1.0f};
    glm::quat q0 = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat q1 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    std::vector<glm::quat> values = {q0, q1};
    auto r = anim_util::Interpolate<glm::quat>(times, values, 0.5f);
    float angle = glm::degrees(glm::angle(r));
    EXPECT_NEAR(angle, 45.0f, 1.0f);
}

TEST(AnimClipEvalTest, AnimSampleBuffer_Init) {
    anim_util::AnimSampleBuffer buf(4);
    EXPECT_EQ(buf.positions.size(), 4u);
    EXPECT_EQ(buf.rotations.size(), 4u);
    EXPECT_EQ(buf.scales.size(), 4u);
    EXPECT_EQ(buf.touched.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_FALSE(buf.touched[i]);
        EXPECT_FLOAT_EQ(buf.scales[i].x, 1.0f);
    }
}
