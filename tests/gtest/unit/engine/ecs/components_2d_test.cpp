/**
 * @file components_2d_test.cpp
 * @brief 2D 物理组件默认值与字段修改单元测试
 */

#include <gtest/gtest.h>
#include "engine/ecs/physics_2d.h"

// RigidBody2DComponent / BoxCollider2DComponent 定义在全局命名空间

TEST(RigidBody2DComponentTest, 默认值) {
    RigidBody2DComponent rb;
    EXPECT_EQ(rb.type, RigidBody2DType::Dynamic);
    EXPECT_EQ(rb.velocity.x, 0.0f);
    EXPECT_EQ(rb.velocity.y, 0.0f);
    EXPECT_FLOAT_EQ(rb.gravity_scale, 1.0f);
    EXPECT_FALSE(rb.fixed_rotation);
    EXPECT_EQ(rb.runtime_body, nullptr);
}

TEST(RigidBody2DComponentTest, 类型修改) {
    RigidBody2DComponent rb;
    rb.type = RigidBody2DType::Static;
    EXPECT_EQ(rb.type, RigidBody2DType::Static);
    rb.type = RigidBody2DType::Kinematic;
    EXPECT_EQ(rb.type, RigidBody2DType::Kinematic);
}

TEST(RigidBody2DComponentTest, 速度修改) {
    RigidBody2DComponent rb;
    rb.velocity = glm::vec2(3.0f, -5.0f);
    EXPECT_FLOAT_EQ(rb.velocity.x, 3.0f);
    EXPECT_FLOAT_EQ(rb.velocity.y, -5.0f);
}

TEST(BoxCollider2DComponentTest, 默认值) {
    BoxCollider2DComponent bc;
    EXPECT_FLOAT_EQ(bc.size.x, 1.0f);
    EXPECT_FLOAT_EQ(bc.size.y, 1.0f);
    EXPECT_FLOAT_EQ(bc.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(bc.offset.y, 0.0f);
    EXPECT_FLOAT_EQ(bc.density, 1.0f);
    EXPECT_FLOAT_EQ(bc.friction, 0.3f);
    EXPECT_FLOAT_EQ(bc.restitution, 0.0f);
    EXPECT_FALSE(bc.is_trigger);
    EXPECT_EQ(bc.runtime_fixture, nullptr);
}

TEST(BoxCollider2DComponentTest, 字段修改) {
    BoxCollider2DComponent bc;
    bc.size = glm::vec2(2.0f, 3.0f);
    bc.offset = glm::vec2(0.5f, -0.5f);
    bc.density = 2.5f;
    bc.friction = 0.8f;
    bc.restitution = 0.5f;
    bc.is_trigger = true;
    EXPECT_FLOAT_EQ(bc.size.x, 2.0f);
    EXPECT_FLOAT_EQ(bc.size.y, 3.0f);
    EXPECT_FLOAT_EQ(bc.density, 2.5f);
    EXPECT_TRUE(bc.is_trigger);
}
