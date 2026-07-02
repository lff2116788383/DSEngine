/**
 * @file camera_arm_3d_system_test.cpp
 * @brief CameraArm3DSystem 单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - delta_time <= 0 时跳过更新
 * - 禁用时不更新
 * - 弹簧臂跟随目标实体
 * - Pitch 角度夹紧
 * - 第一人称模式臂长为零
 * - 第三人称模式使用配置臂长
 * - 碰撞缩短（无物理时走正常臂长）
 * - 非对称插值：缩短即时 / 恢复缓慢
 * - 屏幕震动添加偏移
 * - 震动衰减到零后重置
 * - 相机朝向 pivot
 * - 无目标实体时 pivot 在原点
 */

#include <gtest/gtest.h>
#include <glm/gtx/norm.hpp>
#include "modules/gameplay_3d/camera/camera_arm_3d_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_character.h"

using namespace dse;
using namespace gameplay3d;

class CameraArm3DSystemTest : public ::testing::Test {
protected:
    World world;
    CameraArm3DSystem sys;
    static constexpr float kDt = 1.0f / 60.0f;

    struct CameraBundle {
        Entity cam_entity;
        SpringArm3DComponent* arm;
        TransformComponent* cam_tf;
        Entity target_entity;
        TransformComponent* target_tf;
    };

    CameraBundle CreateCameraWithTarget() {
        auto target = world.CreateEntity();
        auto& target_tf = world.registry().emplace<TransformComponent>(target);
        target_tf.position = glm::vec3(0.0f, 0.0f, 0.0f);

        auto cam = world.CreateEntity();
        auto& arm = world.registry().emplace<SpringArm3DComponent>(cam);
        auto& cam_tf = world.registry().emplace<TransformComponent>(cam);
        arm.target_entity = target;
        arm.arm_length = 4.0f;
        arm.current_arm_length_ = 4.0f;

        return {cam, &arm, &cam_tf, target, &target_tf};
    }
};

TEST_F(CameraArm3DSystemTest, EmptyWorldCallsUpdateDoesNotCrash) {
    EXPECT_NO_THROW(sys.Update(world, kDt));
}

TEST_F(CameraArm3DSystemTest, DeltaTimeZeroSkipsUpdate) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    glm::vec3 pos_before = cam_tf->position;
    sys.Update(world, 0.0f);
    EXPECT_EQ(cam_tf->position, pos_before);
}

TEST_F(CameraArm3DSystemTest, DisabledArmSkipsUpdate) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->enabled = false;
    glm::vec3 pos_before = cam_tf->position;
    sys.Update(world, kDt);
    EXPECT_EQ(cam_tf->position, pos_before);
}

TEST_F(CameraArm3DSystemTest, CameraFollowsTargetPosition) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    target_tf->position = glm::vec3(10.0f, 0.0f, 0.0f);

    for (int i = 0; i < 120; ++i) {
        sys.Update(world, kDt);
    }

    float dist_to_pivot = glm::length(cam_tf->position -
                                       (target_tf->position + arm->target_offset));
    EXPECT_NEAR(dist_to_pivot, arm->arm_length, 1.0f);
}

TEST_F(CameraArm3DSystemTest, PitchClampedToMinMax) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->min_pitch = -80.0f;
    arm->max_pitch = 80.0f;
    arm->pitch = 100.0f;

    sys.Update(world, kDt);
    EXPECT_LE(arm->pitch, arm->max_pitch);
}

TEST_F(CameraArm3DSystemTest, PitchClampedToMinBound) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->min_pitch = -80.0f;
    arm->pitch = -100.0f;

    sys.Update(world, kDt);
    EXPECT_GE(arm->pitch, arm->min_pitch);
}

TEST_F(CameraArm3DSystemTest, FirstPersonModeZeroArmLength) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->view_mode = SpringArm3DComponent::ViewMode::FirstPerson;
    arm->current_arm_length_ = 0.0f;

    sys.Update(world, kDt);

    float dist = glm::length(cam_tf->position - arm->current_pivot_);
    EXPECT_NEAR(dist, 0.0f, 0.5f);
}

TEST_F(CameraArm3DSystemTest, ThirdPersonModeUsesConfiguredArmLength) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->view_mode = SpringArm3DComponent::ViewMode::ThirdPerson;
    arm->arm_length = 5.0f;
    arm->current_arm_length_ = 5.0f;

    for (int i = 0; i < 60; ++i) {
        sys.Update(world, kDt);
    }

    EXPECT_NEAR(arm->current_arm_length_, 5.0f, 0.5f);
}

TEST_F(CameraArm3DSystemTest, AsymmetricLerpShortenImmediate) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->current_arm_length_ = 4.0f;

    // Simulate collision by making arm respond to shorter desired length
    // Without physics, collision_test=true but no raycast hit → desired = arm_length
    // Force test by setting collision_test=false and arm_length smaller
    arm->collision_test = false;
    arm->arm_length = 2.0f;

    sys.Update(world, kDt);
    // current_arm_length should shrink immediately (not interpolated)
    EXPECT_LE(arm->current_arm_length_, 2.01f);
}

TEST_F(CameraArm3DSystemTest, AsymmetricLerpRestoreSlow) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->current_arm_length_ = 1.0f;
    arm->arm_length = 4.0f;
    arm->collision_test = false;

    sys.Update(world, kDt);
    // Should restore slowly, not jump to 4.0
    EXPECT_GT(arm->current_arm_length_, 1.0f);
    EXPECT_LT(arm->current_arm_length_, 4.0f);
}

TEST_F(CameraArm3DSystemTest, ShakeTraumaAddsOffset) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->shake_trauma = 1.0f;
    arm->shake_max_offset = 0.5f;

    // Record position without shake
    arm->shake_trauma = 0.0f;
    sys.Update(world, kDt);
    glm::vec3 no_shake_pos = cam_tf->position;

    // Now with shake
    arm->shake_trauma = 1.0f;
    sys.Update(world, kDt);
    glm::vec3 shake_pos = cam_tf->position;

    // Positions should differ due to shake offset
    EXPECT_NE(shake_pos, no_shake_pos);
}

TEST_F(CameraArm3DSystemTest, ShakeTraumaDecays) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->shake_trauma = 1.0f;
    arm->shake_decay_rate = 2.0f;

    sys.Update(world, kDt);
    EXPECT_LT(arm->shake_trauma, 1.0f);
}

TEST_F(CameraArm3DSystemTest, ShakeTraumaDecaysToZero) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->shake_trauma = 0.01f;
    arm->shake_decay_rate = 100.0f;

    sys.Update(world, kDt);
    EXPECT_FLOAT_EQ(arm->shake_trauma, 0.0f);
    EXPECT_FLOAT_EQ(arm->shake_time_acc_, 0.0f);
}

TEST_F(CameraArm3DSystemTest, CameraTransformDirtyAfterUpdate) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    cam_tf->dirty = false;
    sys.Update(world, kDt);
    EXPECT_TRUE(cam_tf->dirty);
}

TEST_F(CameraArm3DSystemTest, NoTargetEntityPivotAtOriginPlusOffset) {
    auto cam = world.CreateEntity();
    auto& arm = world.registry().emplace<SpringArm3DComponent>(cam);
    auto& cam_tf = world.registry().emplace<TransformComponent>(cam);
    arm.target_entity = entt::null;
    arm.target_offset = glm::vec3(0.0f, 1.6f, 0.0f);

    for (int i = 0; i < 60; ++i) {
        sys.Update(world, kDt);
    }

    glm::vec3 expected_pivot = arm.target_offset;
    EXPECT_NEAR(arm.current_pivot_.x, expected_pivot.x, 0.1f);
    EXPECT_NEAR(arm.current_pivot_.y, expected_pivot.y, 0.1f);
    EXPECT_NEAR(arm.current_pivot_.z, expected_pivot.z, 0.1f);
}

TEST_F(CameraArm3DSystemTest, YawChangesOrbit) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->yaw = 0.0f;
    arm->pitch = 0.0f;
    arm->current_arm_length_ = arm->arm_length;

    sys.Update(world, kDt);
    glm::vec3 pos_yaw0 = cam_tf->position;

    arm->yaw = 90.0f;
    sys.Update(world, kDt);
    glm::vec3 pos_yaw90 = cam_tf->position;

    EXPECT_NE(pos_yaw0, pos_yaw90);
}

TEST_F(CameraArm3DSystemTest, CollisionTestDisabledUsesFullArmLength) {
    auto [cam, arm, cam_tf, target, target_tf] = CreateCameraWithTarget();
    arm->collision_test = false;
    arm->arm_length = 8.0f;
    arm->current_arm_length_ = 8.0f;

    sys.Update(world, kDt);
    EXPECT_NEAR(arm->current_arm_length_, 8.0f, 0.1f);
}
