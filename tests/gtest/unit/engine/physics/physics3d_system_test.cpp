/**
 * @file physics3d_system_test.cpp
 * @brief Physics3DSystem 三维物理系统单元测试
 *
 * 覆盖场景：
 * - 默认构造与析构不崩溃
 * - 未初始化时 Shutdown / FixedUpdate / Raycast 安全
 * - RaycastResult 默认值
 * - PhysX Init→Shutdown 完整生命周期
 * - Init 后 FixedUpdate 不崩溃
 *
 * 注意：Physics3DSystem 的实现依赖 PhysX 后端，在未启用时
 *       编译期排除。本测试仅在 PhysX 可用时编译。
 */

#include <gtest/gtest.h>

#if defined(DSE_ENABLE_PHYSX)

#include <entt/entt.hpp>
#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/ecs/world.h"

using namespace dse::physics3d;

// 测试 物理3D系统：默认不崩溃
TEST(Physics3DSystemTest, DefaultDoesNotCrash) {
    Physics3DSystem sys;
}

// 测试 物理3D系统：当不已初始化关闭不崩溃
TEST(Physics3DSystemTest, WhenNotInitializedShutdownDoesNotCrash) {
    Physics3DSystem sys;
    sys.Shutdown();
}

// 测试 物理3D系统：当不已初始化固定更新不崩溃
TEST(Physics3DSystemTest, WhenNotInitializedFixedUpdateDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

// 测试 物理3D系统：射线检测结果默认值
TEST(Physics3DSystemTest, RaycastResultDefaultValues) {
    RaycastResult result;
    EXPECT_FALSE(result.hit);
    EXPECT_FLOAT_EQ(result.distance, 0.0f);
    EXPECT_FLOAT_EQ(result.hit_point.x, 0.0f);
    EXPECT_FLOAT_EQ(result.hit_normal.x, 0.0f);
}

// 测试 物理3D系统：当不已初始化射线检测返回未命中
TEST(Physics3DSystemTest, WhenNotInitializedRaycastReturnsNothit) {
    Physics3DSystem sys;
    auto result = sys.Raycast(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 100.0f);
    EXPECT_FALSE(result.hit);
}

// 测试 物理3D系统：物理X初始化且关闭无崩溃
TEST(Physics3DSystemTest, PhysXInitializationAndShutdownWithoutCrashing) {
    Physics3DSystem sys;
    World world;
    bool ok = sys.Init(world);
    EXPECT_TRUE(ok);
    sys.Shutdown();
}

// 测试 物理3D系统：初始化之后固定更新不崩溃
TEST(Physics3DSystemTest, InitializeAfterFixedUpdateDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));
    sys.FixedUpdate(world, 1.0f / 60.0f);
    sys.Shutdown();
}

// 测试 物理3D系统：初始化之后射线检测返回未命中空场景
TEST(Physics3DSystemTest, InitializeAfterRaycastReturnsNothitEmptyScene) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));
    auto result = sys.Raycast(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 100.0f);
    EXPECT_FALSE(result.hit);
    sys.Shutdown();
}

// 测试 物理3D系统：多次数初始化关闭不崩溃
TEST(Physics3DSystemTest, MultiTimesInitShutdownDoesNotCrash) {
    Physics3DSystem sys;
    World world;
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(sys.Init(world));
        sys.FixedUpdate(world, 1.0f / 60.0f);
        sys.Shutdown();
    }
}

// 测试 物理3D系统：物理X桥接接口返回有效指针
TEST(Physics3DSystemTest, PhysXBridgeInterfaceReturnsValidPointer) {
    Physics3DSystem sys;
    World world;
    ASSERT_TRUE(sys.Init(world));
    EXPECT_NE(sys.GetPxPhysics(), nullptr);
    EXPECT_NE(sys.GetPxScene(), nullptr);
    EXPECT_NE(sys.GetPxCooking(), nullptr);
    sys.Shutdown();
}

// 测试 物理3D系统：网格碰撞体3D组件默认值
TEST(Physics3DSystemTest, MeshCollider3DComponentDefaultValue) {
    MeshCollider3DComponent mc;
    EXPECT_FALSE(mc.convex);
    EXPECT_FALSE(mc.is_trigger);
    EXPECT_FLOAT_EQ(mc.bounciness, 0.0f);
    EXPECT_FLOAT_EQ(mc.friction, 0.5f);
    EXPECT_EQ(mc.runtime_shape, nullptr);
    EXPECT_TRUE(mc.prev_mesh_path.empty());
}

#endif // DSE_ENABLE_PHYSX
