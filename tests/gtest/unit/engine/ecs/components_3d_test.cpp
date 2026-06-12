/**
 * @file components_3d_test.cpp
 * @brief 3D 组件数据结构与数学的单元测试
 *
 * 覆盖场景：
 * - BoundingBoxComponent::center() / extents() 数学正确性
 * - 3D 物理组件枚举与默认值
 * - Camera3DComponent / PostProcessComponent / 各光源组件默认值
 */

#include <gtest/gtest.h>
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include <glm/glm.hpp>

using namespace dse;

// ============================================================
// BoundingBoxComponent
// ============================================================

TEST(BoundingBoxComponentTest, Defaultcenterexistpoint) {
    BoundingBoxComponent bb;
    EXPECT_FLOAT_EQ(bb.center().x, 0.0f);
    EXPECT_FLOAT_EQ(bb.center().y, 0.0f);
    EXPECT_FLOAT_EQ(bb.center().z, 0.0f);
}

TEST(BoundingBoxComponentTest, DefaultextentsisZero) {
    BoundingBoxComponent bb;
    EXPECT_FLOAT_EQ(bb.extents().x, 0.0f);
    EXPECT_FLOAT_EQ(bb.extents().y, 0.0f);
    EXPECT_FLOAT_EQ(bb.extents().z, 0.0f);
}

TEST(BoundingBoxComponentTest, Centerexistpoint) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(-1.0f, -2.0f, -3.0f);
    bb.max_extents = glm::vec3(1.0f, 2.0f, 3.0f);

    EXPECT_FLOAT_EQ(bb.center().x, 0.0f);
    EXPECT_FLOAT_EQ(bb.center().y, 0.0f);
    EXPECT_FLOAT_EQ(bb.center().z, 0.0f);
}

TEST(BoundingBoxComponentTest, ExtentsCorrect) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(-1.0f, -2.0f, -3.0f);
    bb.max_extents = glm::vec3(1.0f, 2.0f, 3.0f);

    EXPECT_FLOAT_EQ(bb.extents().x, 1.0f);
    EXPECT_FLOAT_EQ(bb.extents().y, 2.0f);
    EXPECT_FLOAT_EQ(bb.extents().z, 3.0f);
}

TEST(BoundingBoxComponentTest, Noncenteroffset) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(0.0f, 0.0f, 0.0f);
    bb.max_extents = glm::vec3(2.0f, 4.0f, 6.0f);

    EXPECT_FLOAT_EQ(bb.center().x, 1.0f);
    EXPECT_FLOAT_EQ(bb.center().y, 2.0f);
    EXPECT_FLOAT_EQ(bb.center().z, 3.0f);
}

TEST(BoundingBoxComponentTest, NonextentsCorrect) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(0.0f, 0.0f, 0.0f);
    bb.max_extents = glm::vec3(2.0f, 4.0f, 6.0f);

    EXPECT_FLOAT_EQ(bb.extents().x, 1.0f);
    EXPECT_FLOAT_EQ(bb.extents().y, 2.0f);
    EXPECT_FLOAT_EQ(bb.extents().z, 3.0f);
}

TEST(BoundingBoxComponentTest, Burden) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(-10.0f);
    bb.max_extents = glm::vec3(-2.0f);

    EXPECT_FLOAT_EQ(bb.center().x, -6.0f);
    EXPECT_FLOAT_EQ(bb.extents().x, 4.0f);
}

// ============================================================
// 3D 物理组件枚举与默认值
// ============================================================

TEST(Components3DPhysicsTest, RigidBody3DTypeenumerationValue) {
    EXPECT_EQ(static_cast<int>(RigidBody3DType::Static), 0);
    EXPECT_EQ(static_cast<int>(RigidBody3DType::Kinematic), 1);
    EXPECT_EQ(static_cast<int>(RigidBody3DType::Dynamic), 2);
}

TEST(Components3DPhysicsTest, RigidBody3DDefaultValues) {
    RigidBody3DComponent rb;
    EXPECT_EQ(rb.type, RigidBody3DType::Dynamic);
    EXPECT_EQ(rb.velocity, glm::vec3(0.0f));
    EXPECT_EQ(rb.angular_velocity, glm::vec3(0.0f));
    EXPECT_FLOAT_EQ(rb.mass, 1.0f);
    EXPECT_FLOAT_EQ(rb.drag, 0.0f);
    EXPECT_FLOAT_EQ(rb.angular_drag, 0.05f);
    EXPECT_TRUE(rb.use_gravity);
    EXPECT_FLOAT_EQ(rb.gravity_scale, 1.0f);
    EXPECT_FALSE(rb.is_kinematic);
    EXPECT_EQ(rb.runtime_body, nullptr);
}

TEST(Components3DPhysicsTest, BoxCollider3DDefaultValues) {
    BoxCollider3DComponent col;
    EXPECT_EQ(col.size, glm::vec3(1.0f));
    EXPECT_EQ(col.center, glm::vec3(0.0f));
    EXPECT_FALSE(col.is_trigger);
    EXPECT_FLOAT_EQ(col.bounciness, 0.0f);
    EXPECT_FLOAT_EQ(col.friction, 0.5f);
    EXPECT_EQ(col.runtime_shape, nullptr);
}

TEST(Components3DPhysicsTest, SphereCollider3DDefaultValues) {
    SphereCollider3DComponent col;
    EXPECT_FLOAT_EQ(col.radius, 0.5f);
    EXPECT_EQ(col.center, glm::vec3(0.0f));
    EXPECT_FALSE(col.is_trigger);
    EXPECT_FLOAT_EQ(col.bounciness, 0.0f);
    EXPECT_FLOAT_EQ(col.friction, 0.5f);
    EXPECT_EQ(col.runtime_shape, nullptr);
}

TEST(Components3DPhysicsTest, MeshCollider3DDefaultValues) {
    MeshCollider3DComponent col;
    EXPECT_FALSE(col.convex);
    EXPECT_FALSE(col.is_trigger);
    EXPECT_FLOAT_EQ(col.bounciness, 0.0f);
    EXPECT_FLOAT_EQ(col.friction, 0.5f);
    EXPECT_EQ(col.runtime_shape, nullptr);
}

// ============================================================
// 3D 渲染/光照组件默认值
// ============================================================

TEST(Components3DTest, Camera3DDefaultValues) {
    Camera3DComponent cam;
    EXPECT_TRUE(cam.enabled);
    EXPECT_EQ(cam.priority, 0);
    EXPECT_FLOAT_EQ(cam.fov, 60.0f);
    EXPECT_FLOAT_EQ(cam.aspect_ratio, 1.333f);
    EXPECT_FLOAT_EQ(cam.near_clip, 0.1f);
    EXPECT_FLOAT_EQ(cam.far_clip, 1000.0f);
}

TEST(Components3DTest, DirectionalLight3DDefaultValues) {
    DirectionalLight3DComponent light;
    EXPECT_TRUE(light.enabled);
    EXPECT_FLOAT_EQ(light.intensity, 1.0f);
    EXPECT_FLOAT_EQ(light.ambient_intensity, 0.2f);
    EXPECT_TRUE(light.cast_shadow);
}

TEST(Components3DTest, PointLightDefaultValues) {
    PointLightComponent light;
    EXPECT_TRUE(light.enabled);
    EXPECT_FLOAT_EQ(light.radius, 10.0f);
    EXPECT_FLOAT_EQ(light.falloff, 1.0f);
    EXPECT_FALSE(light.cast_shadow);
}

TEST(Components3DTest, SpotLightDefaultValues) {
    SpotLightComponent light;
    EXPECT_TRUE(light.enabled);
    EXPECT_FLOAT_EQ(light.inner_cone_angle, 12.5f);
    EXPECT_FLOAT_EQ(light.outer_cone_angle, 17.5f);
    EXPECT_FLOAT_EQ(light.radius, 20.0f);
}

TEST(Components3DTest, SteeringComponentDefaultValues) {
    SteeringComponent steer;
    EXPECT_TRUE(steer.enabled);
    EXPECT_FLOAT_EQ(steer.max_velocity, 5.0f);
    EXPECT_FLOAT_EQ(steer.max_force, 10.0f);
    EXPECT_FLOAT_EQ(steer.mass, 1.0f);
    EXPECT_FALSE(steer.seek_enabled);
    EXPECT_FALSE(steer.flee_enabled);
    EXPECT_FALSE(steer.arrive_enabled);
    EXPECT_FLOAT_EQ(steer.arrive_deceleration_radius, 5.0f);
}
