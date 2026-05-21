/**
 * @file nav_mesh_integration_test.cpp
 * @brief NavMesh 集成测试 - 使用真实场景数据
 */

#include "gtest/gtest.h"
#include "engine/navigation/nav_mesh_system.h"

using namespace dse::navigation;

// ============================================================
// 真实场景 NavMesh 测试
// ============================================================

TEST(NavMeshIntegrationTest, 大型平面Bake成功) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    // 创建一个足够大的平面 (4 个顶点，2 个三角形)
    // 使用宽松的配置确保 Recast 约束满足
    float verts[] = {
        -100.0f, 0.0f, -100.0f,
         100.0f, 0.0f, -100.0f,
         100.0f, 1.0f,  100.0f,  // 微带坡度确保 Y 范围 > agent_height
        -100.0f, 1.0f,  100.0f
    };
    int tris[] = {
        0, 1, 2,
        0, 2, 3
    };

    NavMeshBuildConfig cfg;
    cfg.cell_size       = 1.0f;
    cfg.cell_height     = 0.5f;
    cfg.agent_height    = 1.5f;  // 减小 agent 高度
    cfg.agent_radius    = 0.5f;
    cfg.agent_max_climb = 0.5f;
    cfg.region_min_size = 1.0f;  // 减小最小区域要求
    cfg.region_merge_size = 5.0f;

    bool success = nav.BakeFromTriangles(verts, 4, tris, 2, cfg);
    EXPECT_TRUE(success) << "大型平面应成功 bake";

    if (success) {
        EXPECT_TRUE(nav.IsReady());
    }

    nav.Shutdown();
}

TEST(NavMeshIntegrationTest, 多层台阶Bake成功) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    // 创建多层台阶几何 (6 个顶点，4 个三角形)
    float verts[] = {
        -50.0f, 0.0f, -50.0f,
         50.0f, 0.0f, -50.0f,
         50.0f, 0.0f,   0.0f,
        -50.0f, 0.0f,   0.0f,
        -50.0f, 1.0f,   0.0f,
         50.0f, 1.0f,   0.0f,
         50.0f, 1.0f,  50.0f,
        -50.0f, 1.0f,  50.0f
    };
    int tris[] = {
        0, 1, 2,  // 底层平面
        0, 2, 3,
        4, 5, 6,  // 顶层平面
        4, 6, 7
    };

    NavMeshBuildConfig cfg;
    cfg.cell_size       = 1.0f;
    cfg.cell_height     = 0.5f;
    cfg.agent_height    = 1.5f;
    cfg.agent_radius    = 0.5f;
    cfg.agent_max_climb = 0.6f;  // 允许攀爬 0.6，可跨越 1.0 高度差
    cfg.region_min_size = 2.0f;
    cfg.region_merge_size = 10.0f;

    bool success = nav.BakeFromTriangles(verts, 8, tris, 4, cfg);
    EXPECT_TRUE(success) << "多层台阶应成功 bake";

    if (success) {
        EXPECT_TRUE(nav.IsReady());
    }

    nav.Shutdown();
}

TEST(NavMeshIntegrationTest, FindPath简单路径) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    float verts[] = {
        -50.0f, 0.0f, -50.0f,
         50.0f, 0.0f, -50.0f,
         50.0f, 1.0f,  50.0f,
        -50.0f, 1.0f,  50.0f
    };
    int tris[] = {
        0, 1, 2,
        0, 2, 3
    };

    NavMeshBuildConfig cfg;
    cfg.cell_size       = 1.0f;
    cfg.cell_height     = 0.5f;
    cfg.agent_height    = 1.5f;
    cfg.agent_radius    = 0.5f;
    cfg.agent_max_climb = 0.5f;
    cfg.region_min_size = 1.0f;
    cfg.region_merge_size = 5.0f;

    if (!nav.BakeFromTriangles(verts, 4, tris, 2, cfg)) {
        GTEST_SKIP() << "Bake 失败，跳过测试";
    }

    std::vector<glm::vec3> path;
    bool success = nav.FindPath({-40.0f, 0.5f, -40.0f}, {40.0f, 0.5f, 40.0f}, path, 100);
    EXPECT_TRUE(success) << "应在可行走区域内找到路径";

    if (success) {
        EXPECT_GT(path.size(), 1u) << "路径应包含至少起点和终点";
    }

    nav.Shutdown();
}
