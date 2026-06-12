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

// 测试 物理3D ECS集成：刚体3D组件默认值
TEST_F(Physics3dEcsIntegrationTest, RigidBody3DComponentDefaultValues) {
    dse::RigidBody3DComponent rb;
    EXPECT_EQ(rb.type, dse::RigidBody3DType::Dynamic);
    EXPECT_FLOAT_EQ(rb.mass, 1.0f);
    EXPECT_FLOAT_EQ(rb.drag, 0.0f);
    EXPECT_FLOAT_EQ(rb.angular_drag, 0.05f);
    EXPECT_TRUE(rb.use_gravity);
    EXPECT_FALSE(rb.is_kinematic);
    EXPECT_EQ(rb.runtime_body, nullptr);
}

// 测试 物理3D ECS集成：刚体3D组件字段修改
TEST_F(Physics3dEcsIntegrationTest, RigidBody3DComponentFieldModification) {
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

// 测试 物理3D ECS集成：带有刚体3D实体创建不崩溃
TEST_F(Physics3dEcsIntegrationTest, BringRigidBody3DEntityCreateDoesNotCrash) {
    auto e = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(e);
    auto& rb = world.registry().emplace<dse::RigidBody3DComponent>(e);
    rb.mass = 2.0f;

    EXPECT_TRUE(world.IsAlive(e));
    EXPECT_TRUE(world.registry().all_of<dse::RigidBody3DComponent>(e));
}

// 测试 物理3D ECS集成：多实体不崩溃
TEST_F(Physics3dEcsIntegrationTest, MultiEntityDoesNotCrash) {
    for (int i = 0; i < 10; ++i) {
        auto e = world.CreateEntity();
        world.registry().emplace<TransformComponent>(e);
        world.registry().emplace<dse::RigidBody3DComponent>(e);
    }
    EXPECT_EQ(world.EntityCount(), 10u);
}

// 测试 物理3D ECS集成：组件默认值
TEST_F(Physics3dEcsIntegrationTest, ComponentDefaultValue) {
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

// 测试 物理3D ECS集成：实体销毁之后Worldconsistent
TEST_F(Physics3dEcsIntegrationTest, EntityDestroyAfterWorldconsistent) {
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
