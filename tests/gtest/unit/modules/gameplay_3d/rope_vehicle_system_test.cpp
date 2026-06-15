/**
 * @file rope_vehicle_system_test.cpp
 * @brief RopeSystem 与 VehicleSystem 的 3D Gameplay 行为单元测试
 */

#include <gtest/gtest.h>
#include <glm/gtx/norm.hpp>
#include "modules/gameplay_3d/rope/rope_system.h"
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
#include "modules/gameplay_3d/vehicle/vehicle_system.h"
#endif
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_physics.h"

using namespace dse;
using namespace dse::gameplay3d;

namespace {
constexpr float kDt = 1.0f / 60.0f;

Entity CreateTransformEntity(World& world, const glm::vec3& position) {
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);
    transform.position = position;
    return e;
}
} // namespace

class RopeSystemTest : public ::testing::Test {
protected:
    World world;
    RopeSystem system;
};

// 测试 绳索系统：空世界固定更新不崩溃
TEST_F(RopeSystemTest, EmptyWorldFixedUpdateDoesNotCrash) {
    EXPECT_NO_THROW(system.FixedUpdate(world, kDt));
}

// 测试 绳索系统：Initializegenerate单个
TEST_F(RopeSystemTest, InitializegenerateOne) {
    Entity e = CreateTransformEntity(world, glm::vec3(1.0f, 5.0f, -2.0f));
    auto& rope = world.registry().emplace<RopeComponent>(e);
    rope.segment_count = 4;
    rope.segment_length = 0.5f;
    rope.use_gravity = false;

    system.FixedUpdate(world, kDt);

    EXPECT_TRUE(rope.initialized);
    ASSERT_EQ(rope.positions.size(), 5u);
    ASSERT_EQ(rope.prev_positions.size(), 5u);
    EXPECT_NEAR(rope.positions.front().x, 1.0f, 0.001f);
    EXPECT_NEAR(rope.positions.front().y, 5.0f, 0.001f);
    EXPECT_NEAR(rope.positions.front().z, -2.0f, 0.001f);
    EXPECT_NEAR(rope.positions.back().y, 3.0f, 0.001f);
}

// 测试 绳索系统：禁用绳索组件不初始化
TEST_F(RopeSystemTest, DisabledRopeComponentNotInitialize) {
    Entity e = CreateTransformEntity(world, glm::vec3(0.0f));
    auto& rope = world.registry().emplace<RopeComponent>(e);
    rope.enabled = false;

    system.FixedUpdate(world, kDt);

    EXPECT_FALSE(rope.initialized);
    EXPECT_TRUE(rope.positions.empty());
}

// 测试 绳索系统：增量时间当为零仅Initializes且不模拟重力
TEST_F(RopeSystemTest, DeltaTimeWhenItIsZeroItOnlyInitializesAndDoesNotSimulateGravity) {
    Entity e = CreateTransformEntity(world, glm::vec3(0.0f, 3.0f, 0.0f));
    auto& rope = world.registry().emplace<RopeComponent>(e);
    rope.segment_count = 2;
    rope.segment_length = 1.0f;
    rope.use_gravity = true;

    system.FixedUpdate(world, 0.0f);

    ASSERT_EQ(rope.positions.size(), 3u);
    EXPECT_NEAR(rope.positions[1].y, 2.0f, 0.001f);
    EXPECT_NEAR(rope.positions[2].y, 1.0f, 0.001f);
}

// 测试 绳索系统：点实体
TEST_F(RopeSystemTest, PointEntity) {
    // RopeComponent 使用 0 表示“无锚点”，因此先创建一个占位实体，
    // 避免第一个有效锚点实体 ID 恰好为 0 时被当作未设置。
    (void)world.CreateEntity();
    Entity anchor_a = CreateTransformEntity(world, glm::vec3(0.0f, 4.0f, 0.0f));
    Entity anchor_b = CreateTransformEntity(world, glm::vec3(2.0f, 4.0f, 0.0f));
    Entity rope_entity = CreateTransformEntity(world, glm::vec3(0.0f));
    auto& rope = world.registry().emplace<RopeComponent>(rope_entity);
    rope.segment_count = 4;
    rope.segment_length = 0.5f;
    rope.use_gravity = false;
    rope.solver_iterations = 0;
    rope.radius = 0.0f;
    rope.anchor_entity_a = static_cast<uint32_t>(anchor_a);
    rope.anchor_entity_b = static_cast<uint32_t>(anchor_b);
    rope.anchor_offset_a = glm::vec3(0.1f, 0.2f, 0.3f);
    rope.anchor_offset_b = glm::vec3(-0.1f, -0.2f, -0.3f);

    system.FixedUpdate(world, kDt);

    ASSERT_EQ(rope.positions.size(), 5u);
    EXPECT_NEAR(rope.positions.front().x, 0.1f, 0.001f);
    EXPECT_NEAR(rope.positions.front().y, 4.2f, 0.001f);
    EXPECT_NEAR(rope.positions.front().z, 0.3f, 0.001f);
    EXPECT_NEAR(rope.positions.back().x, 1.9f, 0.001f);
    EXPECT_NEAR(rope.positions.back().y, 3.8f, 0.001f);
    EXPECT_NEAR(rope.positions.back().z, -0.3f, 0.001f);
}

// 测试 绳索系统：半
TEST_F(RopeSystemTest, Half) {
    Entity terrain = world.CreateEntity();
    auto& hm = world.registry().emplace<TerrainHeightmapComponent>(terrain);
    hm.origin_x = 0.0f;
    hm.origin_z = 0.0f;
    hm.block_size = 1.0f;
    hm.cols = 2;
    hm.rows = 2;
    hm.heights = {1.0f, 1.0f, 1.0f, 1.0f};

    Entity e = CreateTransformEntity(world, glm::vec3(0.0f, 0.0f, 0.0f));
    auto& rope = world.registry().emplace<RopeComponent>(e);
    rope.segment_count = 2;
    rope.segment_length = 1.0f;
    rope.radius = 0.25f;
    rope.use_gravity = false;

    system.FixedUpdate(world, kDt);

    ASSERT_EQ(rope.positions.size(), 3u);
    for (const auto& p : rope.positions) {
        EXPECT_GE(p.y, 1.25f);
    }
}

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
class VehicleSystemTest : public ::testing::Test {
protected:
    World world;
    VehicleSystem system;
};

// 测试 载具系统：空世界固定更新不崩溃
TEST_F(VehicleSystemTest, EmptyWorldFixedUpdateDoesNotCrash) {
    EXPECT_NO_THROW(system.FixedUpdate(world, kDt));
}

// 测试 载具系统：无物理注入当仍Initializedefault
TEST_F(VehicleSystemTest, WithoutPhysicsInjectsWhenStillInitializedefault) {
    Entity e = CreateTransformEntity(world, glm::vec3(0.0f));
    auto& vehicle = world.registry().emplace<VehicleComponent>(e);
    world.registry().emplace<RigidBody3DComponent>(e);

    system.FixedUpdate(world, kDt);

    EXPECT_TRUE(vehicle.initialized);
    ASSERT_EQ(vehicle.wheels.size(), 4u);
    ASSERT_EQ(vehicle.wheel_states.size(), 4u);
    EXPECT_TRUE(vehicle.wheels[0].is_steer_wheel);
    EXPECT_TRUE(vehicle.wheels[1].is_steer_wheel);
    EXPECT_TRUE(vehicle.wheels[2].is_drive_wheel);
    EXPECT_TRUE(vehicle.wheels[3].is_drive_wheel);
}

// 测试 载具系统：初始化配置创建状态
TEST_F(VehicleSystemTest, InitializeConfigurationCreateState) {
    Entity e = CreateTransformEntity(world, glm::vec3(0.0f));
    auto& vehicle = world.registry().emplace<VehicleComponent>(e);
    world.registry().emplace<RigidBody3DComponent>(e);
    VehicleWheelConfig wheel;
    wheel.position = glm::vec3(1.0f, -0.5f, 1.2f);
    wheel.radius = 0.42f;
    wheel.is_drive_wheel = true;
    wheel.is_steer_wheel = true;
    vehicle.wheels.push_back(wheel);

    system.FixedUpdate(world, kDt);

    EXPECT_TRUE(vehicle.initialized);
    ASSERT_EQ(vehicle.wheels.size(), 1u);
    ASSERT_EQ(vehicle.wheel_states.size(), 1u);
    EXPECT_NEAR(vehicle.wheels[0].radius, 0.42f, 0.001f);
    EXPECT_TRUE(vehicle.wheels[0].is_drive_wheel);
    EXPECT_TRUE(vehicle.wheels[0].is_steer_wheel);
}

// 测试 载具系统：禁用载具组件不初始化
TEST_F(VehicleSystemTest, DisabledVehicleComponentNotInitialize) {
    Entity e = CreateTransformEntity(world, glm::vec3(0.0f));
    auto& vehicle = world.registry().emplace<VehicleComponent>(e);
    world.registry().emplace<RigidBody3DComponent>(e);
    vehicle.enabled = false;

    system.FixedUpdate(world, kDt);

    EXPECT_FALSE(vehicle.initialized);
    EXPECT_TRUE(vehicle.wheels.empty());
    EXPECT_TRUE(vehicle.wheel_states.empty());
}

// 测试 载具系统：缺失或变换当不按
TEST_F(VehicleSystemTest, MissingOrTransformWhenNotBy) {
    Entity only_vehicle = world.CreateEntity();
    auto& vehicle = world.registry().emplace<VehicleComponent>(only_vehicle);

    system.FixedUpdate(world, kDt);

    EXPECT_FALSE(vehicle.initialized);
}
#endif // DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT
