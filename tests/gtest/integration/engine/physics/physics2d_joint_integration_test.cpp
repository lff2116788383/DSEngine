/**
 * @file physics2d_joint_integration_test.cpp
 * @brief Physics2D Joint 关节创建/销毁集成测试（使用真实 Box2D 世界）
 *
 * 覆盖场景：
 *   1. RevoluteJoint（铰链 + 马达）创建后 runtime_joint 非空
 *   2. DistanceJoint（弹簧）创建后 runtime_joint 非空
 *   3. PrismaticJoint（滑块 + 马达）创建后 runtime_joint 非空
 *   4. WeldJoint（焊接）创建后 runtime_joint 非空
 *   5. DestroyJoint 后 runtime_joint 置空且不崩溃
 *   6. entity_b 缺 RigidBody2DComponent 时关节不创建
 *   7. 重复 Init 后关节指针重置并重建
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/physics_2d.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include <glm/glm.hpp>

namespace {

Entity MakeBody(World& world, float x, float y,
                RigidBody2DType type = RigidBody2DType::Dynamic) {
    Entity e = world.CreateEntity();
    TransformComponent tc;
    tc.position = glm::vec3(x, y, 0.0f);
    tc.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    tc.scale    = glm::vec3(1.0f);
    world.registry().emplace<TransformComponent>(e, tc);
    RigidBody2DComponent rb;
    rb.type = type;
    world.registry().emplace<RigidBody2DComponent>(e, rb);
    return e;
}

} // namespace

class Physics2DJointIntegrationTest : public ::testing::Test {
protected:
    World world;
    Physics2DSystem sys;

    void TearDown() override { sys.Shutdown(); }
};

// 测试 物理2D关节集成：链创建成功
TEST_F(Physics2DJointIntegrationTest, ChainCreateSucceeds) {
    auto bodyA   = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Static);
    auto bodyB   = MakeBody(world, 1.0f, 0.0f, RigidBody2DType::Dynamic);
    Entity jEnt  = world.CreateEntity();
    Joint2DComponent jc;
    jc.type             = Joint2DType::Revolute;
    jc.entity_a         = bodyA;
    jc.entity_b         = bodyB;
    jc.enable_motor     = true;
    jc.motor_speed      = 90.0f;
    jc.max_motor_torque = 5.0f;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    EXPECT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);
}

// 测试 物理2D关节集成：创建成功
TEST_F(Physics2DJointIntegrationTest, CreateSucceeds) {
    auto bodyA   = MakeBody(world, 0.0f, 5.0f, RigidBody2DType::Static);
    auto bodyB   = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Dynamic);
    Entity jEnt  = world.CreateEntity();
    Joint2DComponent jc;
    jc.type       = Joint2DType::Distance;
    jc.entity_a   = bodyA;
    jc.entity_b   = bodyB;
    jc.min_length = 0.5f;
    jc.max_length = 5.0f;
    jc.stiffness  = 10.0f;
    jc.damping    = 0.5f;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    EXPECT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);
}

// 测试 物理2D关节集成：创建成功2
TEST_F(Physics2DJointIntegrationTest, CreateSucceeds_2) {
    auto bodyA   = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Static);
    auto bodyB   = MakeBody(world, 0.0f, 1.0f, RigidBody2DType::Dynamic);
    Entity jEnt  = world.CreateEntity();
    Joint2DComponent jc;
    jc.type                  = Joint2DType::Prismatic;
    jc.entity_a              = bodyA;
    jc.entity_b              = bodyB;
    jc.prismatic_axis        = {0.0f, 1.0f};
    jc.enable_limit          = true;
    jc.lower_translation     = -2.0f;
    jc.upper_translation     =  2.0f;
    jc.enable_motor          = true;
    jc.prismatic_motor_speed = 1.0f;
    jc.max_motor_force       = 10.0f;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    EXPECT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);
}

// 测试 物理2D关节集成：创建成功3
TEST_F(Physics2DJointIntegrationTest, CreateSucceeds_3) {
    auto bodyA   = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Dynamic);
    auto bodyB   = MakeBody(world, 1.0f, 0.0f, RigidBody2DType::Dynamic);
    Entity jEnt  = world.CreateEntity();
    Joint2DComponent jc;
    jc.type     = Joint2DType::Weld;
    jc.entity_a = bodyA;
    jc.entity_b = bodyB;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    EXPECT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);
}

// 测试 物理2D关节集成：销毁关节返回指针为设置到空
TEST_F(Physics2DJointIntegrationTest, DestroyJointTheBackPointerIsSetToEmpty) {
    auto bodyA   = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Static);
    auto bodyB   = MakeBody(world, 1.0f, 0.0f, RigidBody2DType::Dynamic);
    Entity jEnt  = world.CreateEntity();
    Joint2DComponent jc;
    jc.type     = Joint2DType::Revolute;
    jc.entity_a = bodyA;
    jc.entity_b = bodyB;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    ASSERT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);

    sys.DestroyJoint(world, jEnt);
    EXPECT_EQ(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);
}

// 测试 物理2D关节集成：缺失当不创建
TEST_F(Physics2DJointIntegrationTest, MissingWhenDoesNotCreate) {
    auto bodyA    = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Static);
    Entity emptyE = world.CreateEntity();    // 无 RigidBody2DComponent
    Entity jEnt   = world.CreateEntity();
    Joint2DComponent jc;
    jc.type     = Joint2DType::Revolute;
    jc.entity_a = bodyA;
    jc.entity_b = emptyE;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    EXPECT_EQ(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);
}

// 测试 物理2D关节集成：Reinitposterior关节重建
TEST_F(Physics2DJointIntegrationTest, ReinitposteriorJointReconstruction) {
    auto bodyA   = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Static);
    auto bodyB   = MakeBody(world, 1.0f, 0.0f, RigidBody2DType::Dynamic);
    Entity jEnt  = world.CreateEntity();
    Joint2DComponent jc;
    jc.type     = Joint2DType::Weld;
    jc.entity_a = bodyA;
    jc.entity_b = bodyB;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    EXPECT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);
    sys.Init(world);  // 再次 Init 应重置并重建
    EXPECT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);
}

// ============================================================
// 马达运行时调速 API
// ============================================================

// 测试 物理2D关节集成：链当
TEST_F(Physics2DJointIntegrationTest, ChainWhen) {
    auto bodyA  = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Static);
    auto bodyB  = MakeBody(world, 1.0f, 0.0f, RigidBody2DType::Dynamic);
    Entity jEnt = world.CreateEntity();
    Joint2DComponent jc;
    jc.type             = Joint2DType::Revolute;
    jc.entity_a         = bodyA;
    jc.entity_b         = bodyB;
    jc.enable_motor     = true;
    jc.motor_speed      = 45.0f;
    jc.max_motor_torque = 5.0f;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    ASSERT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);

    sys.SetRevoluteMotorSpeed(world, jEnt, 180.0f);
    EXPECT_FLOAT_EQ(world.registry().get<Joint2DComponent>(jEnt).motor_speed, 180.0f);

    sys.SetRevoluteMotorTorque(world, jEnt, 20.0f);
    EXPECT_FLOAT_EQ(world.registry().get<Joint2DComponent>(jEnt).max_motor_torque, 20.0f);
}

// 测试 物理2D关节集成：当
TEST_F(Physics2DJointIntegrationTest, When) {
    auto bodyA  = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Static);
    auto bodyB  = MakeBody(world, 0.0f, 1.0f, RigidBody2DType::Dynamic);
    Entity jEnt = world.CreateEntity();
    Joint2DComponent jc;
    jc.type                  = Joint2DType::Prismatic;
    jc.entity_a              = bodyA;
    jc.entity_b              = bodyB;
    jc.prismatic_axis        = {0.0f, 1.0f};
    jc.enable_motor          = true;
    jc.prismatic_motor_speed = 1.0f;
    jc.max_motor_force       = 10.0f;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    ASSERT_NE(world.registry().get<Joint2DComponent>(jEnt).runtime_joint, nullptr);

    sys.SetPrismaticMotorSpeed(world, jEnt, 5.0f);
    EXPECT_FLOAT_EQ(world.registry().get<Joint2DComponent>(jEnt).prismatic_motor_speed, 5.0f);

    sys.SetPrismaticMotorForce(world, jEnt, 100.0f);
    EXPECT_FLOAT_EQ(world.registry().get<Joint2DComponent>(jEnt).max_motor_force, 100.0f);
}

// 测试 物理2D关节集成：无效实体不崩溃
TEST_F(Physics2DJointIntegrationTest, InvalidEntityDoesNotCrash) {
    sys.Init(world);
    // 无效实体 — 应静默返回
    sys.SetRevoluteMotorSpeed(world, entt::null, 100.0f);
    sys.SetRevoluteMotorTorque(world, entt::null, 100.0f);
    sys.SetPrismaticMotorSpeed(world, entt::null, 100.0f);
    sys.SetPrismaticMotorForce(world, entt::null, 100.0f);
    SUCCEED();  // 不崩溃即通过
}

// 测试 物理2D关节集成：类型不崩溃
TEST_F(Physics2DJointIntegrationTest, TypeNotDoesNotCrash) {
    auto bodyA  = MakeBody(world, 0.0f, 0.0f, RigidBody2DType::Static);
    auto bodyB  = MakeBody(world, 1.0f, 0.0f, RigidBody2DType::Dynamic);
    Entity jEnt = world.CreateEntity();
    Joint2DComponent jc;
    jc.type     = Joint2DType::Weld;  // Weld 不是 Revolute/Prismatic
    jc.entity_a = bodyA;
    jc.entity_b = bodyB;
    world.registry().emplace<Joint2DComponent>(jEnt, jc);

    sys.Init(world);
    // 在 Weld 上调 Revolute/Prismatic API — 应静默返回
    sys.SetRevoluteMotorSpeed(world, jEnt, 100.0f);
    sys.SetPrismaticMotorSpeed(world, jEnt, 100.0f);
    SUCCEED();
}
