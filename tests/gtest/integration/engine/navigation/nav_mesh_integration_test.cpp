/**
 * @file nav_mesh_integration_test.cpp
 * @brief NavMesh 集成测试 - 使用真实场景数据
 */

#include "gtest/gtest.h"
#include "engine/navigation/nav_mesh_system.h"

#include <glm/glm.hpp>
#include <vector>

using namespace dse::navigation;

namespace {

NavMeshBuildConfig TestNavConfig() {
    // 使用 unit 测试中已验证的默认配置
    NavMeshBuildConfig cfg;  // 使用默认值
    return cfg;
}

bool BakeTestScene(NavMeshSystem& nav) {
    float verts[] = {
        -20.0f, 0.0f, -20.0f,
         20.0f, 0.0f, -20.0f,
         20.0f, 0.2f,  20.0f,
        -20.0f, 0.2f,  20.0f
    };
    int tris[] = {
        0, 2, 1,
        0, 3, 2
    };
    return nav.BakeFromTriangles(verts, 4, tris, 2, TestNavConfig());
}

} // namespace

TEST(NavMeshIntegrationTest, BakeEffectiveScenarioSuccessful) {
    NavMeshSystem nav;
    ASSERT_TRUE(nav.Init());

    EXPECT_TRUE(BakeTestScene(nav));
    EXPECT_TRUE(nav.IsReady());

    nav.Shutdown();
}

TEST(NavMeshIntegrationTest, FindNearestPointEffectiveScenarioSuccessful) {
    NavMeshSystem nav;
    ASSERT_TRUE(nav.Init());
    ASSERT_TRUE(BakeTestScene(nav));

    glm::vec3 nearest;
    EXPECT_TRUE(nav.FindNearestPoint({0.0f, 0.1f, 0.0f}, nearest));
    EXPECT_NEAR(nearest.x, 0.0f, 1.0f);
    EXPECT_NEAR(nearest.z, 0.0f, 1.0f);

    nav.Shutdown();
}

TEST(NavMeshIntegrationTest, FindPathEffectiveScenarioSuccessful) {
    NavMeshSystem nav;
    ASSERT_TRUE(nav.Init());
    ASSERT_TRUE(BakeTestScene(nav));

    std::vector<glm::vec3> path;
    EXPECT_TRUE(nav.FindPath({-10.0f, 0.1f, -10.0f}, {10.0f, 0.1f, 10.0f}, path, 64));
    EXPECT_GE(path.size(), 2u);

    nav.Shutdown();
}

TEST(NavMeshIntegrationTest, RaycaststayNavMeshinternalMiss) {
    NavMeshSystem nav;
    ASSERT_TRUE(nav.Init());
    ASSERT_TRUE(BakeTestScene(nav));

    glm::vec3 hit;
    EXPECT_FALSE(nav.Raycast({-5.0f, 0.1f, -5.0f}, {5.0f, 0.1f, 5.0f}, hit));
    EXPECT_NEAR(hit.x, 5.0f, 0.01f);
    EXPECT_NEAR(hit.z, 5.0f, 0.01f);

    nav.Shutdown();
}
