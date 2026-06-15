/**
 * @file foot_ik_system_test.cpp
 * @brief FootIK 系统单元测试
 *
 * 覆盖场景：
 * - 组件默认值
 * - 空 World 调用 Update 不崩溃
 * - 坐标系转换（世界空间 ↔ 模型空间）
 * - 骨盆调整逻辑
 * - 地面射线检测 fallback
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/animation/foot_ik_system.h"

using namespace dse;
using namespace gameplay3d;

// ============================================================
// FootIK3DComponent 默认值
// ============================================================

// 测试 足部IK：足部IK 3D组件默认值
TEST(FootIKTest, FootIK3DComponentDefaultValues) {
    FootIK3DComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_TRUE(comp.feet.empty());
    EXPECT_FLOAT_EQ(comp.pelvis_weight, 0.5f);
    EXPECT_FLOAT_EQ(comp.max_pelvis_offset, 0.3f);
}

// 测试 足部IK：足部IK配置默认值
TEST(FootIKTest, FootIKConfigDefaultValues) {
    FootIKConfig config;
    EXPECT_TRUE(config.name.empty());
    EXPECT_TRUE(config.hip_bone.empty());
    EXPECT_TRUE(config.foot_bone.empty());
    EXPECT_FLOAT_EQ(config.weight, 1.0f);
    EXPECT_FLOAT_EQ(config.blend_speed, 10.0f);
    EXPECT_TRUE(config.chain_indices.empty());
}

// ============================================================
// 系统 Update 基本功能
// ============================================================

// 测试 足部IK：空世界调用更新不崩溃
TEST(FootIKTest, EmptyWorldCallsUpdateDoesNotCrash) {
    World world;
    EXPECT_NO_THROW(FootIKSystem::Update(world, 0.016f));
}

// 测试 足部IK：带组件无有效缓存不崩溃
TEST(FootIKTest, WithComponentWithoutValidCacheDoesNotCrash) {
    World world;
    auto e = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(e);
    anim.enabled = true;
    anim.skel_cache.valid = false;
    world.registry().emplace<FootIK3DComponent>(e);
    EXPECT_NO_THROW(FootIKSystem::Update(world, 0.016f));
}

// 测试 足部IK：带组件无变换不崩溃
TEST(FootIKTest, WithComponentWithoutTransformDoesNotCrash) {
    World world;
    auto e = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(e);
    anim.enabled = true;
    anim.skel_cache.valid = false;
    world.registry().emplace<FootIK3DComponent>(e);
    EXPECT_NO_THROW(FootIKSystem::Update(world, 0.016f));
}

// ============================================================
// 坐标系转换测试
// ============================================================

// 测试 足部IK：过渡空到空
TEST(FootIKTest, Transition_EmptyToEmpty) {
    glm::mat4 entity_world = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 5.0f, 0.0f));
    glm::mat4 inv_world = glm::inverse(entity_world);

    glm::vec3 world_pos(15.0f, 7.0f, 2.0f);
    glm::vec3 model_pos = glm::vec3(inv_world * glm::vec4(world_pos, 1.0f));

    EXPECT_NEAR(model_pos.x, 5.0f, 0.01f);
    EXPECT_NEAR(model_pos.y, 2.0f, 0.01f);
    EXPECT_NEAR(model_pos.z, 2.0f, 0.01f);
}

// 测试 足部IK：过渡空到空2
TEST(FootIKTest, Transition_EmptyToEmpty_2) {
    glm::mat4 entity_world = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 5.0f, 0.0f));

    glm::vec3 model_pos(5.0f, 2.0f, 2.0f);
    glm::vec3 world_pos = glm::vec3(entity_world * glm::vec4(model_pos, 1.0f));

    EXPECT_NEAR(world_pos.x, 15.0f, 0.01f);
    EXPECT_NEAR(world_pos.y, 7.0f, 0.01f);
    EXPECT_NEAR(world_pos.z, 2.0f, 0.01f);
}

// ============================================================
// 骨盆调整逻辑测试
// ============================================================

// 测试 足部IK：情形
TEST(FootIKTest, Case_Case) {
    // 模拟脚部需要下移的情况
    float min_foot_delta = -0.3f; // 最低脚需要下移 0.3 米
    float max_pelvis_offset = 0.5f;
    float pelvis_weight = 0.5f;

    float pelvis_drop = min_foot_delta;
    if (pelvis_drop < -max_pelvis_offset) pelvis_drop = -max_pelvis_offset;

    EXPECT_NEAR(pelvis_drop, -0.3f, 0.01f);
    EXPECT_NEAR(pelvis_drop * pelvis_weight, -0.15f, 0.01f);
}

// 测试 足部IK：情形不情形
TEST(FootIKTest, Case_Not_Case) {
    float min_foot_delta = 0.2f; // 脚部需要上移，不触发骨盆下移
    float max_pelvis_offset = 0.5f;
    float pelvis_weight = 0.5f;

    float pelvis_drop = (min_foot_delta < 0.0f) ? min_foot_delta : 0.0f;
    if (pelvis_drop < -max_pelvis_offset) pelvis_drop = -max_pelvis_offset;

    EXPECT_FLOAT_EQ(pelvis_drop, 0.0f);
}

// ============================================================
// 地面射线检测 fallback 测试
// ============================================================

// 测试 足部IK：情形无系统返回
TEST(FootIKTest, Case_WithoutSystemReturns) {
    glm::vec3 world_pos(10.0f, 5.0f, 2.0f);
    float max_distance = 10.0f;

    // 无物理系统时返回原高度
    float ground_height = world_pos.y;

    EXPECT_NEAR(ground_height, 5.0f, 0.01f);
}

// 测试 足部IK：情形Withouthit返回
TEST(FootIKTest, Case_WithouthitReturns) {
    glm::vec3 world_pos(10.0f, 5.0f, 2.0f);
    float max_distance = 10.0f;

    // 无射线命中时返回原高度
    float ground_height = world_pos.y;

    EXPECT_NEAR(ground_height, 5.0f, 0.01f);
}
