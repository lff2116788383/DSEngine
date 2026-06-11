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

using namespace dse;
using namespace dse::physics3d;

TEST(Physics3DSystemJoltTest, DefaultDoesNotCrash) {
    Physics3DSystem sys;
}

TEST(Physics3DSystemJoltTest, WhenNotInitializedShutdownDoesNotCrash) {
    Physics3DSystem sys;
    sys.Shutdown();
}

TEST(Physics3DSystemJoltTest, WhenNotInitializedFixedUpdateDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

TEST(Physics3DSystemJoltTest, RaycastResultDefaultValues) {
    RaycastResult result;
    EXPECT_FALSE(result.hit);
    EXPECT_FLOAT_EQ(result.distance, 0.0f);
    EXPECT_FLOAT_EQ(result.hit_point.x, 0.0f);
    EXPECT_FLOAT_EQ(result.hit_normal.x, 0.0f);
}

TEST(Physics3DSystemJoltTest, WhenNotInitializedRaycastReturnsNothit) {
    Physics3DSystem sys;
    auto result = sys.Raycast(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 100.0f);
    EXPECT_FALSE(result.hit);
}

TEST(Physics3DSystemJoltTest, JoltInitializationAndShutdownWithoutCrashing) {
    Physics3DSystem sys;
    World world;
    bool ok = sys.Init(world);
    EXPECT_TRUE(ok);
    sys.Shutdown();
}

TEST(Physics3DSystemJoltTest, InitializeAfterFixedUpdateDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));
    sys.FixedUpdate(world, 1.0f / 60.0f);
    sys.Shutdown();
}

TEST(Physics3DSystemJoltTest, InitializeAfterRaycastReturnsNothitEmptyScene) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));
    auto result = sys.Raycast(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 100.0f);
    EXPECT_FALSE(result.hit);
    sys.Shutdown();
}

TEST(Physics3DSystemJoltTest, MultiTimesInitShutdownDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(sys.Init(world));
        sys.FixedUpdate(world, 1.0f / 60.0f);
        sys.Shutdown();
    }
}

TEST(Physics3DSystemJoltTest, PhysXbridgeInterfaceReturnnullptr) {
    Physics3DSystem sys;
    EXPECT_EQ(sys.GetPxPhysics(), nullptr);
    EXPECT_EQ(sys.GetPxScene(), nullptr);
    EXPECT_EQ(sys.GetPxCooking(), nullptr);
}

TEST(Physics3DSystemJoltTest, MeshCollider3DComponentDefaultValue) {
    MeshCollider3DComponent mc;
    EXPECT_FALSE(mc.convex);
    EXPECT_FALSE(mc.is_trigger);
    EXPECT_FLOAT_EQ(mc.bounciness, 0.0f);
    EXPECT_FLOAT_EQ(mc.friction, 0.5f);
    EXPECT_EQ(mc.runtime_shape, nullptr);
    EXPECT_TRUE(mc.prev_mesh_path.empty());
}

#endif // DSE_ENABLE_JOLT
