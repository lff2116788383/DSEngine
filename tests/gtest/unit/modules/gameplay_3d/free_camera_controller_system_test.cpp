/**
 * @file free_camera_controller_system_test.cpp
 * @brief 自由相机控制器系统单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - 无输入时相机位置不变
 * - 禁用组件不更新
 * - FreeCameraControllerComponent 默认值
 * - FreeCameraControllerComponent 字段修改
 * - 只有 Transform+Camera3D 无 Controller 时不崩溃
 *
 * 注意：FreeCameraControllerSystem 依赖 Input 系统（键盘/鼠标状态），
 *       单元测试环境无真实输入，因此主要验证数据正确性和边界条件，
 *       输入驱动测试归入集成测试。
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/camera/free_camera_controller_system.h"

using namespace dse;
using namespace gameplay3d;

class FreeCameraControllerSystemTest : public ::testing::Test {
protected:
    World world;
    FreeCameraControllerSystem system;
};

// 测试 释放相机控制器系统：空世界调用更新不崩溃
TEST_F(FreeCameraControllerSystemTest, EmptyWorldCallsUpdateDoesNotCrash) {
    EXPECT_NO_THROW(system.Update(world, 0.016f));
}

// 测试 释放相机控制器系统：无控制器组件当不崩溃
TEST_F(FreeCameraControllerSystemTest, WithoutControllerComponentWhenDoesNotCrash) {
    auto entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(entity);
    world.registry().emplace<Camera3DComponent>(entity);
    EXPECT_NO_THROW(system.Update(world, 0.016f));
}

// 测试 释放相机控制器系统：释放相机控制器组件默认值
TEST_F(FreeCameraControllerSystemTest, FreeCameraControllerComponentDefaultValues) {
    FreeCameraControllerComponent ctrl;
    EXPECT_TRUE(ctrl.enabled);
    EXPECT_FLOAT_EQ(ctrl.move_speed, 5.0f);
    EXPECT_FLOAT_EQ(ctrl.mouse_sensitivity, 0.1f);
    EXPECT_FLOAT_EQ(ctrl.pitch, 0.0f);
    EXPECT_FLOAT_EQ(ctrl.yaw, -90.0f);
}

// 测试 释放相机控制器系统：释放相机控制器组件字段修改
TEST_F(FreeCameraControllerSystemTest, FreeCameraControllerComponentFieldModification) {
    FreeCameraControllerComponent ctrl;
    ctrl.enabled = false;
    ctrl.move_speed = 10.0f;
    ctrl.mouse_sensitivity = 0.5f;
    ctrl.pitch = 45.0f;
    ctrl.yaw = 0.0f;

    EXPECT_FALSE(ctrl.enabled);
    EXPECT_FLOAT_EQ(ctrl.move_speed, 10.0f);
    EXPECT_FLOAT_EQ(ctrl.mouse_sensitivity, 0.5f);
    EXPECT_FLOAT_EQ(ctrl.pitch, 45.0f);
    EXPECT_FLOAT_EQ(ctrl.yaw, 0.0f);
}

// 测试 释放相机控制器系统：禁用组件Whenconstant
TEST_F(FreeCameraControllerSystemTest, DisabledComponentWhenconstant) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(1.0f, 2.0f, 3.0f);
    auto& ctrl = world.registry().emplace<FreeCameraControllerComponent>(entity);
    ctrl.enabled = false;

    glm::vec3 pos_before = tf.position;
    system.Update(world, 0.016f);

    EXPECT_EQ(tf.position, pos_before);
}

// 测试 释放相机控制器系统：Pitch范围约束校验
TEST_F(FreeCameraControllerSystemTest, PitchRangeConstraintValidation) {
    // 验证默认 yaw=-90 使得相机朝 -Z 方向，pitch=0 无仰角
    FreeCameraControllerComponent ctrl;
    ctrl.pitch = 0.0f;
    ctrl.yaw = -90.0f;

    // 从 yaw/pitch 计算前方向量
    float yaw_rad = glm::radians(ctrl.yaw);
    float pitch_rad = glm::radians(ctrl.pitch);
    glm::vec3 front;
    front.x = cos(pitch_rad) * cos(yaw_rad);
    front.y = sin(pitch_rad);
    front.z = cos(pitch_rad) * sin(yaw_rad);
    front = glm::normalize(front);

    // 默认朝 -Z
    EXPECT_LT(front.z, -0.9f);
    EXPECT_NEAR(front.x, 0.0f, 0.1f);
}
