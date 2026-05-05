/**
 * @file physics2d_joint_test.cpp
 * @brief Physics2D Joint2DComponent 字段默认值单元测试（无 GPU / 无 Box2D 世界）
 *
 * 覆盖场景：
 *   1. Joint2DComponent 各类型默认值校验
 *   2. Joint2DType 枚举值校验
 *
 * 物理世界层面的关节创建 / 销毁测试见：
 *   tests/gtest/integration/engine/physics/physics2d_joint_integration_test.cpp
 */

#include <gtest/gtest.h>
#include "engine/ecs/physics_2d.h"

// ============================================================
// Joint2DType 枚举
// ============================================================

TEST(Joint2DTypeTest, 枚举值可区分) {
    EXPECT_NE(Joint2DType::Revolute,  Joint2DType::Distance);
    EXPECT_NE(Joint2DType::Distance,  Joint2DType::Prismatic);
    EXPECT_NE(Joint2DType::Prismatic, Joint2DType::Weld);
}

// ============================================================
// Joint2DComponent 默认值
// ============================================================

TEST(Joint2DComponentTest, 默认类型为Revolute) {
    Joint2DComponent jc;
    EXPECT_EQ(jc.type, Joint2DType::Revolute);
}

TEST(Joint2DComponentTest, 默认运行时指针为nullptr) {
    Joint2DComponent jc;
    EXPECT_EQ(jc.runtime_joint, nullptr);
}

TEST(Joint2DComponentTest, 默认锚点为零向量) {
    Joint2DComponent jc;
    EXPECT_FLOAT_EQ(jc.anchor_a.x, 0.0f);
    EXPECT_FLOAT_EQ(jc.anchor_a.y, 0.0f);
    EXPECT_FLOAT_EQ(jc.anchor_b.x, 0.0f);
    EXPECT_FLOAT_EQ(jc.anchor_b.y, 0.0f);
}

TEST(Joint2DComponentTest, 铰链默认禁用限制和马达) {
    Joint2DComponent jc;
    EXPECT_FALSE(jc.enable_limit);
    EXPECT_FALSE(jc.enable_motor);
    EXPECT_FLOAT_EQ(jc.lower_angle,      0.0f);
    EXPECT_FLOAT_EQ(jc.upper_angle,      0.0f);
    EXPECT_FLOAT_EQ(jc.motor_speed,      0.0f);
    EXPECT_FLOAT_EQ(jc.max_motor_torque, 0.0f);
}

TEST(Joint2DComponentTest, 距离关节默认参数) {
    Joint2DComponent jc;
    EXPECT_FLOAT_EQ(jc.min_length, 0.0f);
    EXPECT_FLOAT_EQ(jc.max_length, 1.0f);
    EXPECT_FLOAT_EQ(jc.stiffness,  0.0f);
    EXPECT_FLOAT_EQ(jc.damping,    0.0f);
}

TEST(Joint2DComponentTest, 棱柱关节默认轴为X轴) {
    Joint2DComponent jc;
    EXPECT_FLOAT_EQ(jc.prismatic_axis.x, 1.0f);
    EXPECT_FLOAT_EQ(jc.prismatic_axis.y, 0.0f);
    EXPECT_FLOAT_EQ(jc.lower_translation,     0.0f);
    EXPECT_FLOAT_EQ(jc.upper_translation,     0.0f);
    EXPECT_FLOAT_EQ(jc.prismatic_motor_speed, 0.0f);
    EXPECT_FLOAT_EQ(jc.max_motor_force,       0.0f);
}

TEST(Joint2DComponentTest, 默认不碰撞已连接刚体) {
    Joint2DComponent jc;
    EXPECT_FALSE(jc.collide_connected);
}
