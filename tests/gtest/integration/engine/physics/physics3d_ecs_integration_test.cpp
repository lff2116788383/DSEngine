/**
 * @file physics3d_ecs_integration_test.cpp
 * @brief Physics3D ↔ ECS 集成测试
 *
 * 验证场景：
 * - RigidBody3DComponent 默认值
 * - Physics3D 相关组件字段可修改
 * - 带 RigidBody3DComponent 实体创建不崩溃
 * - Collider3DComponent 默认值
 *
 * 注意：Physics3D 系统（PhysX）需要完整的运行时初始化，
 *       此处仅验证组件数据层面的集成，物理模拟验证归入冒烟测试。
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_physics.h"

class Physics3dEcsIntegrationTest : public ::testing::Test {
protected:
    World world;
};

TEST_F(Physics3dEcsIntegrationTest, RigidBody3DComponent默认值) {
    dse::RigidBody3DComponent rb;
    EXPECT_EQ(rb.type, dse::RigidBody3DType::Dynamic);
    EXPECT_FLOAT_EQ(rb.mass, 1.0f);
    EXPECT_FLOAT_EQ(rb.drag, 0.0f);
    EXPECT_FLOAT_EQ(rb.angular_drag, 0.05f);
    EXPECT_TRUE(rb.use_gravity);
    EXPECT_FALSE(rb.is_kinematic);
    EXPECT_EQ(rb.runtime_body, nullptr);
}

TEST_F(Physics3dEcsIntegrationTest, RigidBody3DComponent字段修改) {
    dse::RigidBody3DComponent rb;
    rb.type = dse::RigidBody3DType::Kinematic;
    rb.mass = 5.0f;
    rb.drag = 0.1f;
    rb.is_kinematic = true;
    rb.use_gravity = false;
    rb.gravity_scale = 2.0f;

    EXPECT_EQ(rb.type, dse::RigidBody3DType::Kinematic);
    EXPECT_FLOAT_EQ(rb.mass, 5.0f);
    EXPECT_FLOAT_EQ(rb.drag, 0.1f);
    EXPECT_TRUE(rb.is_kinematic);
    EXPECT_FALSE(rb.use_gravity);
    EXPECT_FLOAT_EQ(rb.gravity_scale, 2.0f);
}

TEST_F(Physics3dEcsIntegrationTest, 带RigidBody3D实体创建不崩溃) {
    auto e = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(e);
    auto& rb = world.registry().emplace<dse::RigidBody3DComponent>(e);
    rb.mass = 2.0f;

    EXPECT_TRUE(world.IsAlive(e));
    EXPECT_TRUE(world.registry().all_of<dse::RigidBody3DComponent>(e));
}

TEST_F(Physics3dEcsIntegrationTest, 多个物理实体共存不崩溃) {
    for (int i = 0; i < 10; ++i) {
        auto e = world.CreateEntity();
        world.registry().emplace<TransformComponent>(e);
        world.registry().emplace<dse::RigidBody3DComponent>(e);
    }
    EXPECT_EQ(world.EntityCount(), 10u);
}

TEST_F(Physics3dEcsIntegrationTest, 碰撞体组件默认值) {
    dse::BoxCollider3DComponent box;
    EXPECT_FLOAT_EQ(box.size.x, 1.0f);
    EXPECT_FLOAT_EQ(box.size.y, 1.0f);
    EXPECT_FLOAT_EQ(box.size.z, 1.0f);
    EXPECT_FALSE(box.is_trigger);
    EXPECT_FLOAT_EQ(box.friction, 0.5f);

    dse::SphereCollider3DComponent sphere;
    EXPECT_FLOAT_EQ(sphere.radius, 0.5f);
    EXPECT_FALSE(sphere.is_trigger);

    dse::MeshCollider3DComponent mesh_collider;
    EXPECT_FALSE(mesh_collider.convex);
    EXPECT_FALSE(mesh_collider.is_trigger);
}

TEST_F(Physics3dEcsIntegrationTest, 物理实体销毁后World一致) {
    std::vector<Entity> entities;
    for (int i = 0; i < 5; ++i) {
        auto e = world.CreateEntity();
        world.registry().emplace<TransformComponent>(e);
        world.registry().emplace<dse::RigidBody3DComponent>(e);
        entities.push_back(e);
    }
    for (auto e : entities) {
        world.DestroyEntity(e);
    }
    EXPECT_EQ(world.EntityCount(), 0u);
}
