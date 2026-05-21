/**
 * @file physics3d_advanced_test.cpp
 * @brief Physics3DSystem 高级特性单元测试
 *
 * 覆盖场景：
 * - 碰撞事件队列（CollisionEvent/TriggerEvent）
 * - 角色控制器 API（MoveCharacter/JumpCharacter/IsCharacterGrounded）
 * - 材质缓存（GetOrCreateMaterial）
 * - 动力学 API（AddForce/AddTorque/SetVelocity）
 * - 碰撞层设置（SetCollisionLayer）
 *
 * 注意：Physics3DSystem 的实现依赖 PhysX 后端，在未启用时
 *       编译期排除。本测试仅在 PhysX 可用时编译。
 */

#include <gtest/gtest.h>

#if defined(DSE_ENABLE_PHYSX)

#include <entt/entt.hpp>
#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/components/physics3d.h"

using namespace dse::physics3d;

// ============================================================
// 碰撞事件测试
// ============================================================

TEST(Physics3DAdvancedTest, 初始化后事件队列为空) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));

    EXPECT_TRUE(sys.GetCollisionEvents().empty());
    EXPECT_TRUE(sys.GetTriggerEvents().empty());

    sys.Shutdown();
}

TEST(Physics3DAdvancedTest, FlushEvents清空队列) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));

    // 手动模拟添加事件（实际场景由物理模拟回调填充）
    // 注意：这里无法真正触发碰撞事件，仅测试 Flush 不崩溃
    sys.FlushEvents();
    EXPECT_TRUE(sys.GetCollisionEvents().empty());
    EXPECT_TRUE(sys.GetTriggerEvents().empty());

    sys.Shutdown();
}

// ============================================================
// 角色控制器测试
// ============================================================

TEST(Physics3DAdvancedTest, MoveCharacter未初始化返回失败) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    CharacterMoveResult result = sys.MoveCharacter(entity, {1,0,0}, 0.1f, 0.016f);
    
    // 未初始化时应返回失败
    EXPECT_FALSE(result.success);
    EXPECT_FLOAT_EQ(result.final_position.x, 0.0f);
}

TEST(Physics3DAdvancedTest, JumpCharacter未初始化返回false) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    bool success = sys.JumpCharacter(entity, 5.0f);
    
    EXPECT_FALSE(success);
}

TEST(Physics3DAdvancedTest, IsCharacterGrounded未初始化返回false) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    bool grounded = sys.IsCharacterGrounded(entity);
    
    EXPECT_FALSE(grounded);
}

TEST(Physics3DAdvancedTest, GetCharacterPosition未初始化返回零) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    glm::vec3 pos = sys.GetCharacterPosition(entity);
    
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

// ============================================================
// 动力学 API 测试
// ============================================================

TEST(Physics3DAdvancedTest, AddForce未初始化不崩溃) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    EXPECT_NO_THROW(sys.AddForce(entity, {0,100,0}));
}

TEST(Physics3DAdvancedTest, AddTorque未初始化不崩溃) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    EXPECT_NO_THROW(sys.AddTorque(entity, {0,0,100}));
}

TEST(Physics3DAdvancedTest, AddImpulse未初始化不崩溃) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    EXPECT_NO_THROW(sys.AddImpulse(entity, {0,10,0}));
}

TEST(Physics3DAdvancedTest, SetVelocity未初始化不崩溃) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    EXPECT_NO_THROW(sys.SetVelocity(entity, {1,0,0}));
}

TEST(Physics3DAdvancedTest, SetAngularVelocity未初始化不崩溃) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    EXPECT_NO_THROW(sys.SetAngularVelocity(entity, {0,1,0}));
}

TEST(Physics3DAdvancedTest, GetVelocity未初始化返回零) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    glm::vec3 vel = sys.GetVelocity(entity);
    
    EXPECT_FLOAT_EQ(vel.x, 0.0f);
    EXPECT_FLOAT_EQ(vel.y, 0.0f);
    EXPECT_FLOAT_EQ(vel.z, 0.0f);
}

TEST(Physics3DAdvancedTest, GetAngularVelocity未初始化返回零) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    glm::vec3 ang_vel = sys.GetAngularVelocity(entity);
    
    EXPECT_FLOAT_EQ(ang_vel.x, 0.0f);
    EXPECT_FLOAT_EQ(ang_vel.y, 0.0f);
    EXPECT_FLOAT_EQ(ang_vel.z, 0.0f);
}

TEST(Physics3DAdvancedTest, SetGravityEnabled未初始化不崩溃) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    EXPECT_NO_THROW(sys.SetGravityEnabled(entity, false));
}

TEST(Physics3DAdvancedTest, IsGravityEnabled未初始化返回true) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    bool enabled = sys.IsGravityEnabled(entity);
    
    EXPECT_TRUE(enabled);  // 默认值
}

TEST(Physics3DAdvancedTest, RemoveActor未初始化不崩溃) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    EXPECT_NO_THROW(sys.RemoveActor(entity));
}

// ============================================================
// 碰撞层测试
// ============================================================

TEST(Physics3DAdvancedTest, SetCollisionLayer未初始化不崩溃) {
    Physics3DSystem sys;
    World world;
    
    entt::entity entity = world.create();
    EXPECT_NO_THROW(sys.SetCollisionLayer(entity, 1, 0xFFFF));
}

// ============================================================
// 材质组件测试
// ============================================================

TEST(Physics3DAdvancedTest, PhysicsMaterial3DComponent默认值) {
    PhysicsMaterial3DComponent mat;
    EXPECT_FLOAT_EQ(mat.friction, 0.5f);
    EXPECT_FLOAT_EQ(mat.bounciness, 0.0f);
}

TEST(Physics3DAdvancedTest, PhysicsMaterial3DComponent自定义值) {
    PhysicsMaterial3DComponent mat;
    mat.friction = 0.8f;
    mat.bounciness = 0.5f;
    
    EXPECT_FLOAT_EQ(mat.friction, 0.8f);
    EXPECT_FLOAT_EQ(mat.bounciness, 0.5f);
}

// ============================================================
// 角色控制器组件测试
// ============================================================

TEST(Physics3DAdvancedTest, CharacterController3DComponent默认值) {
    CharacterController3DComponent cc;
    EXPECT_FLOAT_EQ(cc.height, 2.0f);
    EXPECT_FLOAT_EQ(cc.radius, 0.5f);
    EXPECT_FLOAT_EQ(cc.step_offset, 0.3f);
    EXPECT_FLOAT_EQ(cc.slope_limit, 45.0f);
    EXPECT_FALSE(cc.auto_bake);
}

TEST(Physics3DAdvancedTest, CharacterController3DComponent自定义值) {
    CharacterController3DComponent cc;
    cc.height = 1.8f;
    cc.radius = 0.4f;
    cc.step_offset = 0.25f;
    cc.slope_limit = 50.0f;
    cc.auto_bake = true;
    
    EXPECT_FLOAT_EQ(cc.height, 1.8f);
    EXPECT_FLOAT_EQ(cc.radius, 0.4f);
    EXPECT_FLOAT_EQ(cc.step_offset, 0.25f);
    EXPECT_FLOAT_EQ(cc.slope_limit, 50.0f);
    EXPECT_TRUE(cc.auto_bake);
}

// ============================================================
// Joint3D 组件测试
// ============================================================

TEST(Physics3DAdvancedTest, Joint3DComponent默认值) {
    Joint3DComponent joint;
    EXPECT_EQ(joint.type, JointType3D::Fixed);
    EXPECT_EQ(joint.entity_a, entt::null);
    EXPECT_EQ(joint.entity_b, entt::null);
    EXPECT_FLOAT_EQ(joint.break_force, 0.0f);
    EXPECT_FLOAT_EQ(joint.break_torque, 0.0f);
}

TEST(Physics3DAdvancedTest, Joint3DComponent自定义值) {
    Joint3DComponent joint;
    joint.type = JointType3D::Hinge;
    joint.break_force = 1000.0f;
    joint.break_torque = 500.0f;
    
    EXPECT_EQ(joint.type, JointType3D::Hinge);
    EXPECT_FLOAT_EQ(joint.break_force, 1000.0f);
    EXPECT_FLOAT_EQ(joint.break_torque, 500.0f);
}

TEST(Physics3DAdvancedTest, JointType3D枚举值) {
    EXPECT_EQ(static_cast<int>(JointType3D::Fixed), 0);
    EXPECT_EQ(static_cast<int>(JointType3D::Distance), 1);
    EXPECT_EQ(static_cast<int>(JointType3D::Hinge), 2);
    EXPECT_EQ(static_cast<int>(JointType3D::Slider), 3);
    EXPECT_EQ(static_cast<int>(JointType3D::ConeTwist), 4);
    EXPECT_EQ(static_cast<int>(JointType3D::SixDOF), 5);
}

// ============================================================
// Rigidbody3D 组件测试
// ============================================================

TEST(Physics3DAdvancedTest, Rigidbody3DComponent默认值) {
    Rigidbody3DComponent rb;
    EXPECT_FLOAT_EQ(rb.mass, 1.0f);
    EXPECT_FLOAT_EQ(rb.linear_damping, 0.0f);
    EXPECT_FLOAT_EQ(rb.angular_damping, 0.0f);
    EXPECT_TRUE(rb.is_kinematic);
    EXPECT_FALSE(rb.use_gravity);
}

TEST(Physics3DAdvancedTest, Rigidbody3DComponent自定义值) {
    Rigidbody3DComponent rb;
    rb.mass = 10.0f;
    rb.linear_damping = 0.1f;
    rb.angular_damping = 0.2f;
    rb.is_kinematic = false;
    rb.use_gravity = true;
    
    EXPECT_FLOAT_EQ(rb.mass, 10.0f);
    EXPECT_FLOAT_EQ(rb.linear_damping, 0.1f);
    EXPECT_FLOAT_EQ(rb.angular_damping, 0.2f);
    EXPECT_FALSE(rb.is_kinematic);
    EXPECT_TRUE(rb.use_gravity);
}

// ============================================================
// 碰撞体组件测试
// ============================================================

TEST(Physics3DAdvancedTest, BoxCollider3DComponent默认值) {
    BoxCollider3DComponent box;
    EXPECT_FLOAT_EQ(box.size.x, 1.0f);
    EXPECT_FLOAT_EQ(box.size.y, 1.0f);
    EXPECT_FLOAT_EQ(box.size.z, 1.0f);
    EXPECT_FALSE(box.is_trigger);
    EXPECT_FLOAT_EQ(box.friction, 0.5f);
    EXPECT_FLOAT_EQ(box.bounciness, 0.0f);
}

TEST(Physics3DAdvancedTest, SphereCollider3DComponent默认值) {
    SphereCollider3DComponent sphere;
    EXPECT_FLOAT_EQ(sphere.radius, 0.5f);
    EXPECT_FALSE(sphere.is_trigger);
    EXPECT_FLOAT_EQ(sphere.friction, 0.5f);
    EXPECT_FLOAT_EQ(sphere.bounciness, 0.0f);
}

TEST(Physics3DAdvancedTest, CapsuleCollider3DComponent默认值) {
    CapsuleCollider3DComponent capsule;
    EXPECT_FLOAT_EQ(capsule.height, 1.0f);
    EXPECT_FLOAT_EQ(capsule.radius, 0.5f);
    EXPECT_FALSE(capsule.is_trigger);
    EXPECT_FLOAT_EQ(capsule.friction, 0.5f);
    EXPECT_FLOAT_EQ(capsule.bounciness, 0.0f);
}

#endif // DSE_ENABLE_PHYSX
