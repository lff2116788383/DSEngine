/**
 * @file physics3d_system_jolt_test.cpp
 * @brief Physics3DSystem (Jolt 后端) 单元测试
 *
 * 覆盖场景：
 * - 默认构造与析构不崩溃
 * - 未初始化时 Shutdown / FixedUpdate / Raycast 安全
 * - RaycastResult 默认值
 * - Jolt Init→Shutdown 完整生命周期
 * - Init 后 FixedUpdate 不崩溃
 *
 * 注意：Physics3DSystem 的 Jolt 后端实现，在未启用时
 *       编译期排除。本测试仅在 Jolt 可用时编译。
 */

#include <gtest/gtest.h>

#if defined(DSE_ENABLE_JOLT)

#include <entt/entt.hpp>
#include "engine/physics/physics3d/physics3d_system_jolt.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"

using namespace dse;
using namespace dse::physics3d;

// 测试 物理3D系统Jolt：默认不崩溃
TEST(Physics3DSystemJoltTest, DefaultDoesNotCrash) {
    Physics3DSystem sys;
}

// 测试 物理3D系统Jolt：当不已初始化关闭不崩溃
TEST(Physics3DSystemJoltTest, WhenNotInitializedShutdownDoesNotCrash) {
    Physics3DSystem sys;
    sys.Shutdown();
}

// 测试 物理3D系统Jolt：当不已初始化固定更新不崩溃
TEST(Physics3DSystemJoltTest, WhenNotInitializedFixedUpdateDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

// 测试 物理3D系统Jolt：射线检测结果默认值
TEST(Physics3DSystemJoltTest, RaycastResultDefaultValues) {
    RaycastResult result;
    EXPECT_FALSE(result.hit);
    EXPECT_FLOAT_EQ(result.distance, 0.0f);
    EXPECT_FLOAT_EQ(result.hit_point.x, 0.0f);
    EXPECT_FLOAT_EQ(result.hit_normal.x, 0.0f);
}

// 测试 物理3D系统Jolt：当不已初始化射线检测返回未命中
TEST(Physics3DSystemJoltTest, WhenNotInitializedRaycastReturnsNothit) {
    Physics3DSystem sys;
    auto result = sys.Raycast(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 100.0f);
    EXPECT_FALSE(result.hit);
}

// 测试 物理3D系统Jolt：Jolt初始化且关闭无崩溃
TEST(Physics3DSystemJoltTest, JoltInitializationAndShutdownWithoutCrashing) {
    Physics3DSystem sys;
    World world;
    bool ok = sys.Init(world);
    EXPECT_TRUE(ok);
    sys.Shutdown();
}

// 测试 物理3D系统Jolt：初始化之后固定更新不崩溃
TEST(Physics3DSystemJoltTest, InitializeAfterFixedUpdateDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));
    sys.FixedUpdate(world, 1.0f / 60.0f);
    sys.Shutdown();
}

// 测试 物理3D系统Jolt：初始化之后射线检测返回未命中空场景
TEST(Physics3DSystemJoltTest, InitializeAfterRaycastReturnsNothitEmptyScene) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));
    auto result = sys.Raycast(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 100.0f);
    EXPECT_FALSE(result.hit);
    sys.Shutdown();
}

// 测试 物理3D系统Jolt：多次数初始化关闭不崩溃
TEST(Physics3DSystemJoltTest, MultiTimesInitShutdownDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(sys.Init(world));
        sys.FixedUpdate(world, 1.0f / 60.0f);
        sys.Shutdown();
    }
}

// 测试 物理3D系统Jolt：物理Xbridge接口返回空指针
TEST(Physics3DSystemJoltTest, PhysXbridgeInterfaceReturnnullptr) {
    Physics3DSystem sys;
    EXPECT_EQ(sys.GetPxPhysics(), nullptr);
    EXPECT_EQ(sys.GetPxScene(), nullptr);
    EXPECT_EQ(sys.GetPxCooking(), nullptr);
}

// 测试 物理3D系统Jolt：网格碰撞体3D组件默认值
TEST(Physics3DSystemJoltTest, MeshCollider3DComponentDefaultValue) {
    MeshCollider3DComponent mc;
    EXPECT_FALSE(mc.convex);
    EXPECT_FALSE(mc.is_trigger);
    EXPECT_FLOAT_EQ(mc.bounciness, 0.0f);
    EXPECT_FLOAT_EQ(mc.friction, 0.5f);
    EXPECT_EQ(mc.runtime_shape, nullptr);
    EXPECT_TRUE(mc.prev_mesh_path.empty());
}

// 测试 物理3D系统Jolt：LOD 休眠冻结刚体，唤醒后恢复模拟
// 前置：Init 后 FixedUpdate 创建动态刚体并开始下落；
// 预期：SetBodySleepState(true) 后多帧位置不变（不参与模拟），
//       SetBodySleepState(false) 后恢复重力下落。
TEST(Physics3DSystemJoltTest, SetBodySleepStateFreezesThenResumesBody) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));

    auto e = world.registry().create();
    auto& t = world.registry().emplace<TransformComponent>(e);
    t.position = glm::vec3(0.0f, 10.0f, 0.0f);
    t.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    auto& rb = world.registry().emplace<RigidBody3DComponent>(e);
    rb.type = RigidBody3DType::Dynamic;
    rb.mass = 1.0f;
    auto& box = world.registry().emplace<BoxCollider3DComponent>(e);
    box.size = glm::vec3(1.0f);

    // 创建 body 并模拟数帧（开始下落）
    for (int i = 0; i < 5; ++i) sys.FixedUpdate(world, 1.0f / 60.0f);
    const float y_before = world.registry().get<TransformComponent>(e).position.y;
    EXPECT_LT(y_before, 10.0f);  // 确认确实在下落

    // 休眠：后续帧位置必须冻结
    sys.SetBodySleepState(e, true);
    for (int i = 0; i < 10; ++i) sys.FixedUpdate(world, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(world.registry().get<TransformComponent>(e).position.y, y_before);

    // 唤醒：恢复重力下落
    sys.SetBodySleepState(e, false);
    for (int i = 0; i < 5; ++i) sys.FixedUpdate(world, 1.0f / 60.0f);
    EXPECT_LT(world.registry().get<TransformComponent>(e).position.y, y_before);

    sys.Shutdown();
}

#endif // DSE_ENABLE_JOLT
