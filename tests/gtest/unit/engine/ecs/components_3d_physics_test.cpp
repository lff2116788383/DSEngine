/**
 * @file components_3d_physics_test.cpp
 * @brief 3D 物理组件默认值与字段修改单元测试
 */

#include <gtest/gtest.h>
#include "engine/ecs/components_3d_physics.h"

using namespace dse;

TEST(RigidBody3DTypeTest, EnumerationValue) {
    EXPECT_EQ(static_cast<int>(RigidBody3DType::Static), 0);
    EXPECT_EQ(static_cast<int>(RigidBody3DType::Kinematic), 1);
    EXPECT_EQ(static_cast<int>(RigidBody3DType::Dynamic), 2);
}

TEST(RigidBody3DComponentTest, DefaultValues) {
    RigidBody3DComponent rb;
    EXPECT_EQ(rb.type, RigidBody3DType::Dynamic);
    EXPECT_FLOAT_EQ(rb.mass, 1.0f);
    EXPECT_FLOAT_EQ(rb.drag, 0.0f);
    EXPECT_FLOAT_EQ(rb.angular_drag, 0.05f);
    EXPECT_TRUE(rb.use_gravity);
    EXPECT_FLOAT_EQ(rb.gravity_scale, 1.0f);
    EXPECT_FALSE(rb.is_kinematic);
    EXPECT_EQ(rb.runtime_body, nullptr);
}

TEST(RigidBody3DComponentTest, FieldModification) {
    RigidBody3DComponent rb;
    rb.type = RigidBody3DType::Static;
    rb.mass = 10.0f;
    rb.velocity = glm::vec3(1.0f, 2.0f, 3.0f);
    rb.use_gravity = false;
    rb.gravity_scale = 0.5f;
    EXPECT_EQ(rb.type, RigidBody3DType::Static);
    EXPECT_FLOAT_EQ(rb.mass, 10.0f);
    EXPECT_FLOAT_EQ(rb.velocity.z, 3.0f);
    EXPECT_FALSE(rb.use_gravity);
}

TEST(BoxCollider3DComponentTest, DefaultValues) {
    BoxCollider3DComponent bc;
    EXPECT_FLOAT_EQ(bc.size.x, 1.0f);
    EXPECT_FLOAT_EQ(bc.size.y, 1.0f);
    EXPECT_FLOAT_EQ(bc.size.z, 1.0f);
    EXPECT_FLOAT_EQ(bc.center.x, 0.0f);
    EXPECT_FALSE(bc.is_trigger);
    EXPECT_FLOAT_EQ(bc.bounciness, 0.0f);
    EXPECT_FLOAT_EQ(bc.friction, 0.5f);
    EXPECT_EQ(bc.runtime_shape, nullptr);
}

TEST(SphereCollider3DComponentTest, DefaultValues) {
    SphereCollider3DComponent sc;
    EXPECT_FLOAT_EQ(sc.radius, 0.5f);
    EXPECT_FLOAT_EQ(sc.center.x, 0.0f);
    EXPECT_FALSE(sc.is_trigger);
    EXPECT_FLOAT_EQ(sc.friction, 0.5f);
    EXPECT_EQ(sc.runtime_shape, nullptr);
}

TEST(MeshCollider3DComponentTest, DefaultValues) {
    MeshCollider3DComponent mc;
    EXPECT_FALSE(mc.convex);
    EXPECT_FALSE(mc.is_trigger);
    EXPECT_FLOAT_EQ(mc.bounciness, 0.0f);
    EXPECT_FLOAT_EQ(mc.friction, 0.5f);
    EXPECT_EQ(mc.runtime_shape, nullptr);
}
