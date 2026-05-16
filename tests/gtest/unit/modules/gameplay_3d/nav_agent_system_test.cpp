/**
 * @file nav_agent_system_test.cpp
 * @brief NavAgentSystem / NavMeshAgentComponent 单元测试
 *
 * 测试策略：
 * - NavMeshAgentComponent 默认值（条件编译 DSE_ENABLE_NAVMESH）
 * - NavAgentSystem 构造安全
 */

#include <gtest/gtest.h>
#include "engine/ecs/components_3d.h"
#include <glm/glm.hpp>

#ifdef DSE_ENABLE_NAVMESH
#include "modules/gameplay_3d/ai/nav_agent_system.h"
#endif

using namespace dse;

// ============================================================
// NavMeshAgentComponent 默认值
// ============================================================

#ifdef DSE_ENABLE_NAVMESH

TEST(NavMeshAgentComponentTest, 默认值) {
    NavMeshAgentComponent nac;
    EXPECT_FLOAT_EQ(nac.speed, 3.5f);
    EXPECT_FLOAT_EQ(nac.acceleration, 8.0f);
    EXPECT_FLOAT_EQ(nac.stopping_dist, 0.1f);
    EXPECT_FLOAT_EQ(nac.agent_radius, 0.6f);
    EXPECT_FLOAT_EQ(nac.agent_height, 2.0f);
    EXPECT_FLOAT_EQ(nac.destination.x, 0.0f);
    EXPECT_FALSE(nac.has_path);
    EXPECT_FALSE(nac.path_pending);
    EXPECT_TRUE(nac.arrived);
    EXPECT_TRUE(nac.path_points.empty());
    EXPECT_EQ(nac.current_waypoint, 0);
}

TEST(NavMeshAgentComponentTest, 设置目标) {
    NavMeshAgentComponent nac;
    nac.destination = glm::vec3(10.0f, 0.0f, 20.0f);
    nac.path_pending = true;
    nac.arrived = false;
    EXPECT_TRUE(nac.path_pending);
    EXPECT_FALSE(nac.arrived);
    EXPECT_FLOAT_EQ(nac.destination.x, 10.0f);
}

TEST(NavAgentSystemTest, 默认构造) {
    gameplay3d::NavAgentSystem sys;
    (void)sys;
    SUCCEED();
}

#else

TEST(NavAgentSystemTest, NavMesh未启用_编译验证) {
    GTEST_SKIP() << "DSE_ENABLE_NAVMESH not defined";
}

#endif // DSE_ENABLE_NAVMESH
