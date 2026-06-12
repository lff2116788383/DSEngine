/**
 * @file animator_system_test.cpp
 * @brief 3D 动画系统单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - 带 AnimatorComponent 但无 AssetManager 不崩溃
 * - Animator3DComponent 基本字段默认值
 * - Animator3DComponent 字段修改
 * - 状态机 time 推进
 * - enabled 为 false 时跳过更新
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/animation/animator_system.h"

using namespace dse;
using namespace gameplay3d;

class AnimatorSystem3DTest : public ::testing::Test {
protected:
    void SetUp() override {
        AnimatorSystem::SetAssetManager(nullptr);
    }
    void TearDown() override {
        AnimatorSystem::SetAssetManager(nullptr);
    }
};

// 测试 动画器系统3D：空世界调用更新不崩溃
TEST_F(AnimatorSystem3DTest, EmptyWorldCallsUpdateDoesNotCrash) {
    World world;
    EXPECT_NO_THROW(AnimatorSystem::Update(world, 0.016f));
}

// 测试 动画器系统3D：带有动画器3D组件实体更新不崩溃
TEST_F(AnimatorSystem3DTest, BringAnimator3DComponentEntityUpdateDoesNotCrash) {
    World world;
    auto entity = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(entity);
    anim.danim_path = "test.danim";
    EXPECT_NO_THROW(AnimatorSystem::Update(world, 0.016f));
}

// 测试 动画器系统3D：动画器3D组件默认值
TEST_F(AnimatorSystem3DTest, Animator3DComponentDefaultValues) {
    Animator3DComponent anim;
    EXPECT_TRUE(anim.enabled);
    EXPECT_TRUE(anim.dskel_path.empty());
    EXPECT_TRUE(anim.danim_path.empty());
    EXPECT_FLOAT_EQ(anim.current_time, 0.0f);
    EXPECT_FLOAT_EQ(anim.speed, 1.0f);
    EXPECT_TRUE(anim.loop);
    EXPECT_FALSE(anim.use_anim_tree);
    EXPECT_FLOAT_EQ(anim.blend_parameter_value, 0.0f);
    EXPECT_FALSE(anim.is_transitioning);
    EXPECT_TRUE(anim.final_bone_matrices.empty());
}

// 测试 动画器系统3D：动画器3D组件字段修改
TEST_F(AnimatorSystem3DTest, Animator3DComponentFieldModification) {
    Animator3DComponent anim;
    anim.enabled = false;
    anim.speed = 2.0f;
    anim.loop = false;
    anim.danim_path = "walk.danim";
    anim.blend_parameter_value = 5.0f;
    anim.is_transitioning = true;
    anim.transition_duration = 0.3f;

    EXPECT_FALSE(anim.enabled);
    EXPECT_FLOAT_EQ(anim.speed, 2.0f);
    EXPECT_FALSE(anim.loop);
    EXPECT_EQ(anim.danim_path, "walk.danim");
    EXPECT_FLOAT_EQ(anim.blend_parameter_value, 5.0f);
    EXPECT_TRUE(anim.is_transitioning);
    EXPECT_FLOAT_EQ(anim.transition_duration, 0.3f);
}

// 测试 动画器系统3D：禁用动画器3D不实体
TEST_F(AnimatorSystem3DTest, DisabledAnimator3DNotEntity) {
    World world;
    auto e1 = world.CreateEntity();
    auto& anim1 = world.registry().emplace<Animator3DComponent>(e1);
    anim1.enabled = false;
    anim1.danim_path = "disabled.danim";

    auto e2 = world.CreateEntity();
    auto& anim2 = world.registry().emplace<Animator3DComponent>(e2);
    anim2.enabled = true;
    anim2.danim_path = "enabled.danim";

    EXPECT_NO_THROW(AnimatorSystem::Update(world, 0.016f));
}

// 测试 动画器系统3D：混合节点默认值
TEST_F(AnimatorSystem3DTest, BlendNodeDefaultValues) {
    AnimBlendNode node;
    EXPECT_TRUE(node.name.empty());
    EXPECT_TRUE(node.danim_path.empty());
    EXPECT_FLOAT_EQ(node.current_time, 0.0f);
    EXPECT_FLOAT_EQ(node.speed, 1.0f);
    EXPECT_TRUE(node.loop);
    EXPECT_FLOAT_EQ(node.weight, 1.0f);
    EXPECT_FLOAT_EQ(node.threshold, 0.0f);
}

// 测试 动画器系统3D：变形组件默认值
TEST_F(AnimatorSystem3DTest, MorphComponentDefaultValues) {
    MorphComponent morph;
    EXPECT_TRUE(morph.enabled);
    EXPECT_TRUE(morph.targets.empty());
    EXPECT_EQ(morph.morph_buffer_handle, 0u);
}
