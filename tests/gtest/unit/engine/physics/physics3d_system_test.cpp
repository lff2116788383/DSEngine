/**
 * @file physics3d_system_test.cpp
 * @brief Physics3DSystem 三维物理系统单元测试
 *
 * 覆盖场景：
 * - 默认构造与析构不崩溃
 * - 未初始化时 Shutdown / Raycast 安全
 * - RaycastResult 默认值
 *
 * 注意：Physics3DSystem 的实现依赖 PhysX 后端，在未启用时
 *       编译期排除。本测试仅在 PhysX 可用时编译。
 */

#include <gtest/gtest.h>

#if defined(DSE_HAS_PHYSX_LIBS) && DSE_HAS_PHYSX_LIBS

#include <entt/entt.hpp>
#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/ecs/world.h"

using namespace dse::physics3d;

TEST(Physics3DSystemTest, 默认构造不崩溃) {
    Physics3DSystem sys;
}

TEST(Physics3DSystemTest, 未初始化时Shutdown不崩溃) {
    Physics3DSystem sys;
    sys.Shutdown();
}

TEST(Physics3DSystemTest, 未初始化时FixedUpdate不崩溃) {
    Physics3DSystem sys;
    World world;
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

TEST(Physics3DSystemTest, RaycastResult默认值) {
    RaycastResult result;
    EXPECT_FALSE(result.hit);
    EXPECT_FLOAT_EQ(result.distance, 0.0f);
    EXPECT_FLOAT_EQ(result.hit_point.x, 0.0f);
    EXPECT_FLOAT_EQ(result.hit_normal.x, 0.0f);
}

TEST(Physics3DSystemTest, 未初始化时Raycast返回未命中) {
    Physics3DSystem sys;
    auto result = sys.Raycast(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 100.0f);
    EXPECT_FALSE(result.hit);
}

#endif // DSE_HAS_PHYSX_LIBS
