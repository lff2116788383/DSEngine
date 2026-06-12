/**
 * @file nav_mesh_system_test.cpp
 * @brief NavMesh 寻路系统单元测试
 */

#ifdef DSE_ENABLE_NAVMESH

#include "gtest/gtest.h"
#include "engine/navigation/nav_mesh_system.h"
#include <glm/glm.hpp>
#include <cstdio>
#include <vector>

using namespace dse::navigation;

/// 生成细分测试平面（min_val..max_val, y=0），生成 (n-1)x(n-1)*2 个三角形
static void BuildTestPlane(float min_val, float max_val, int n,
                           std::vector<float>& out_verts,
                           std::vector<int>& out_tris) {
    out_verts.clear();
    out_tris.clear();
    float step = (max_val - min_val) / float(n - 1);
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            out_verts.push_back(min_val + x * step);
            out_verts.push_back(0.0f);
            out_verts.push_back(min_val + z * step);
        }
    }
    for (int z = 0; z < n - 1; ++z) {
        for (int x = 0; x < n - 1; ++x) {
            int i = z * n + x;
            out_tris.push_back(i);
            out_tris.push_back(i + n);
            out_tris.push_back(i + 1);
            out_tris.push_back(i + 1);
            out_tris.push_back(i + n);
            out_tris.push_back(i + n + 1);
        }
    }
}

// ============================================================
// NavMeshBuildConfig 测试
// ============================================================

TEST(NavMeshBuildConfigTest, DefaultValues) {
    NavMeshBuildConfig cfg;
    EXPECT_FLOAT_EQ(cfg.cell_size, 0.3f);
    EXPECT_FLOAT_EQ(cfg.cell_height, 0.2f);
    EXPECT_FLOAT_EQ(cfg.agent_height, 2.0f);
    EXPECT_FLOAT_EQ(cfg.agent_radius, 0.6f);
    EXPECT_FLOAT_EQ(cfg.agent_max_climb, 0.9f);
    EXPECT_FLOAT_EQ(cfg.agent_max_slope, 45.0f);
    EXPECT_EQ(cfg.verts_per_poly, 6);
}

TEST(NavMeshBuildConfigTest, CustomValues) {
    NavMeshBuildConfig cfg;
    cfg.cell_size = 0.5f;
    cfg.cell_height = 0.3f;
    cfg.agent_height = 1.8f;
    cfg.agent_radius = 0.5f;
    cfg.verts_per_poly = 8;

    EXPECT_FLOAT_EQ(cfg.cell_size, 0.5f);
    EXPECT_FLOAT_EQ(cfg.cell_height, 0.3f);
    EXPECT_FLOAT_EQ(cfg.agent_height, 1.8f);
    EXPECT_FLOAT_EQ(cfg.agent_radius, 0.5f);
    EXPECT_EQ(cfg.verts_per_poly, 8);
}

// ============================================================
// NavMeshSystem 生命周期测试
// ============================================================

TEST(NavMeshSystemTest, DefaultDoesNotCrash) {
    NavMeshSystem nav;
    SUCCEED();
}

TEST(NavMeshSystemTest, InitSucceeds) {
    NavMeshSystem nav;
    bool success = nav.Init();
    EXPECT_TRUE(success) << "NavMeshSystem::Init 应返回 true";
    
    if (success) {
        nav.Shutdown();
    }
}

TEST(NavMeshSystemTest, ShutdownDoesNotCrash) {
    NavMeshSystem nav;
    nav.Init();
    nav.Shutdown();
    SUCCEED();
}

TEST(NavMeshSystemTest, ShutdownDoesNotCrash_2) {
    NavMeshSystem nav;
    nav.Init();
    nav.Shutdown();
    nav.Shutdown();  // 重复调用应安全
    SUCCEED();
}

TEST(NavMeshSystemTest, NotInitWhenIsReadyReturnsfalse) {
    NavMeshSystem nav;
    EXPECT_FALSE(nav.IsReady());
}

TEST(NavMeshSystemTest, InitLaterBakeWhenIsReadyReturnsfalse) {
    NavMeshSystem nav;
    if (nav.Init()) {
        EXPECT_FALSE(nav.IsReady());
        nav.Shutdown();
    } else {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }
}

// ============================================================
// BakeFromTriangles 测试
// ============================================================

TEST(NavMeshSystemTest, BakeFromTrianglesEmptyDataReturnedfalse) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    bool success = nav.BakeFromTriangles(nullptr, 0, nullptr, 0);
    EXPECT_FALSE(success) << "空数据应返回 false";

    nav.Shutdown();
}

TEST(NavMeshSystemTest, BakeFromTrianglesSimplePlane) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    bool success = nav.BakeFromTriangles(verts.data(),
                                          static_cast<int>(verts.size() / 3),
                                          tris.data(),
                                          static_cast<int>(tris.size() / 3));
    EXPECT_TRUE(success) << "20x20 细分平面应成功 bake";
    if (success) {
        EXPECT_TRUE(nav.IsReady());
        EXPECT_GT(nav.GetPolyCount(), 0);
    }
    nav.Shutdown();
}

// ============================================================
// FindPath 测试
// ============================================================

TEST(NavMeshSystemTest, FindPathNotBakeReturnsfalse) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<glm::vec3> path;
    bool success = nav.FindPath({0,0,0}, {10,0,10}, path);
    EXPECT_FALSE(success) << "未 bake 时应返回 false";

    nav.Shutdown();
}

TEST(NavMeshSystemTest, FindPathsimplePath) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeFromTriangles(verts.data(),
                                static_cast<int>(verts.size() / 3),
                                tris.data(),
                                static_cast<int>(tris.size() / 3))) {
        GTEST_SKIP() << "Bake 失败，跳过测试";
    }

    std::vector<glm::vec3> path;
    bool success = nav.FindPath({1,0,1}, {9,0,9}, path);
    EXPECT_TRUE(success) << "简单路径应找到";
    if (success) {
        EXPECT_GE(path.size(), 2u) << "路径应至少包含起点和终点";
        EXPECT_NEAR(path.front().x, 1.0f, 1.0f);
        EXPECT_NEAR(path.front().y, 0.0f, 1.0f);
        EXPECT_NEAR(path.front().z, 1.0f, 1.0f);
        EXPECT_NEAR(path.back().x, 9.0f, 1.0f);
        EXPECT_NEAR(path.back().y, 0.0f, 1.0f);
        EXPECT_NEAR(path.back().z, 9.0f, 1.0f);
    }

    nav.Shutdown();
}

TEST(NavMeshSystemTest, FindPathIfTheStartingPointAndEndPointAreTheSameReturnToASinglePoint) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeFromTriangles(verts.data(),
                                static_cast<int>(verts.size() / 3),
                                tris.data(),
                                static_cast<int>(tris.size() / 3))) {
        GTEST_SKIP() << "Bake 失败，跳过测试";
    }

    std::vector<glm::vec3> path;
    bool success = nav.FindPath({5,0,5}, {5,0,5}, path);
    EXPECT_TRUE(success) << "相同点路径应成功";
    if (success) {
        EXPECT_LE(path.size(), 2u) << "相同点路径应包含 1 或 2 个点";
        EXPECT_GE(path.size(), 1u);
    }

    nav.Shutdown();
}

// ============================================================
// FindNearestPoint 测试
// ============================================================

TEST(NavMeshSystemTest, FindNearestPointNotBakeReturnsfalse) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    glm::vec3 nearest;
    bool success = nav.FindNearestPoint({5,0,5}, nearest);
    EXPECT_FALSE(success) << "未 bake 时应返回 false";

    nav.Shutdown();
}

TEST(NavMeshSystemTest, FindNearestPointexistnavmeshReturnToOriginWithin) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeFromTriangles(verts.data(),
                                static_cast<int>(verts.size() / 3),
                                tris.data(),
                                static_cast<int>(tris.size() / 3))) {
        GTEST_SKIP() << "Bake 失败，跳过测试";
    }

    glm::vec3 nearest;
    bool success = nav.FindNearestPoint({5,0,5}, nearest);
    EXPECT_TRUE(success) << "navmesh 内点应找到最近点";
    if (success) {
        EXPECT_NEAR(nearest.x, 5.0f, 1.0f);
        EXPECT_NEAR(nearest.y, 0.0f, 1.0f);
        EXPECT_NEAR(nearest.z, 5.0f, 1.0f);
    }

    nav.Shutdown();
}

// ============================================================
// Raycast 测试
// ============================================================

TEST(NavMeshSystemTest, RaycastNotBakeReturnsfalse) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    glm::vec3 hit_pos;
    bool success = nav.Raycast({0,0,0}, {10,0,10}, hit_pos);
    EXPECT_FALSE(success) << "未 bake 时应返回 false";

    nav.Shutdown();
}

TEST(NavMeshSystemTest, RaycastexistnavmeshinternalMiss) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeFromTriangles(verts.data(),
                                static_cast<int>(verts.size() / 3),
                                tris.data(),
                                static_cast<int>(tris.size() / 3))) {
        GTEST_SKIP() << "Bake 失败，跳过测试";
    }

    glm::vec3 hit_pos;
    bool success = nav.Raycast({3,0,3}, {8,0,8}, hit_pos);
    EXPECT_FALSE(success) << "navmesh 内射线不应命中障碍";

    nav.Shutdown();
}

TEST(NavMeshSystemTest, Raycastejaculationnavmeshhit) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeFromTriangles(verts.data(),
                                static_cast<int>(verts.size() / 3),
                                tris.data(),
                                static_cast<int>(tris.size() / 3))) {
        GTEST_SKIP() << "Bake 失败，跳过测试";
    }

    glm::vec3 hit_pos;
    bool success = nav.Raycast({10,0,10}, {30,0,30}, hit_pos);
    EXPECT_TRUE(success) << "射出 navmesh 应命中边界";

    nav.Shutdown();
}

// ============================================================
// 序列化测试
// ============================================================

TEST(NavMeshSystemTest, SaveNavMeshNotBakeReturnsfalse) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    bool success = nav.SaveNavMesh("test_nav.bin");
    EXPECT_FALSE(success) << "未 bake 时保存应返回 false";

    nav.Shutdown();
}

TEST(NavMeshSystemTest, LoadNavMeshFileDoesNotExistReturnfalse) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    bool success = nav.LoadNavMesh("nonexistent_nav.bin");
    EXPECT_FALSE(success) << "文件不存在时应返回 false";

    nav.Shutdown();
}

TEST(NavMeshSystemTest, SaveLoadNavMeshcycle) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeFromTriangles(verts.data(),
                                static_cast<int>(verts.size() / 3),
                                tris.data(),
                                static_cast<int>(tris.size() / 3))) {
        GTEST_SKIP() << "Bake 失败，跳过测试";
    }

    const char* tmp_file = "test_nav_save_tmp.bin";

    bool save_success = nav.SaveNavMesh(tmp_file);
    EXPECT_TRUE(save_success) << "保存 navmesh 应成功";

    if (save_success) {
        nav.Shutdown();
        nav.Init();

        bool load_success = nav.LoadNavMesh(tmp_file);
        EXPECT_TRUE(load_success) << "加载 navmesh 应成功";

        if (load_success) {
            EXPECT_TRUE(nav.IsReady()) << "加载后 navmesh 应可用";

            std::vector<glm::vec3> path;
            bool path_success = nav.FindPath({1,0,1}, {9,0,9}, path);
            EXPECT_TRUE(path_success) << "加载后路径查找应有效";
        }
    }

    nav.Shutdown();
    std::remove(tmp_file);
}

// ============================================================
// Tiled NavMesh 测试
// ============================================================

TEST(NavMeshSystemTest, BakeTiledFromTrianglesSimplePlane) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    bool success = nav.BakeTiledFromTriangles(verts.data(),
                                               static_cast<int>(verts.size() / 3),
                                               tris.data(),
                                               static_cast<int>(tris.size() / 3),
                                               10.0f);
    EXPECT_TRUE(success);
    if (success) {
        EXPECT_TRUE(nav.IsReady());
        EXPECT_TRUE(nav.IsTiled());
        EXPECT_FLOAT_EQ(nav.tile_size(), 10.0f);
    }
    nav.Shutdown();
}

TEST(NavMeshSystemTest, BakeTiledFromTrianglesAfterFindPath) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeTiledFromTriangles(verts.data(),
                                     static_cast<int>(verts.size() / 3),
                                     tris.data(),
                                     static_cast<int>(tris.size() / 3),
                                     10.0f)) {
        GTEST_SKIP() << "Tiled bake 失败，跳过测试";
    }

    std::vector<glm::vec3> path;
    bool success = nav.FindPath({3,0,3}, {17,0,17}, path);
    EXPECT_TRUE(success);
    if (success) {
        EXPECT_GE(path.size(), 2u);
    }
    nav.Shutdown();
}

TEST(NavMeshSystemTest, RebakeTileAtIncrementalReconstruction) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeTiledFromTriangles(verts.data(),
                                     static_cast<int>(verts.size() / 3),
                                     tris.data(),
                                     static_cast<int>(tris.size() / 3),
                                     10.0f)) {
        GTEST_SKIP() << "Tiled bake 失败，跳过测试";
    }

    bool rebake = nav.RebakeTileAt(5.0f, 5.0f,
                                    verts.data(),
                                    static_cast<int>(verts.size() / 3),
                                    tris.data(),
                                    static_cast<int>(tris.size() / 3));
    EXPECT_TRUE(rebake);
    EXPECT_TRUE(nav.IsReady());
    nav.Shutdown();
}

TEST(NavMeshSystemTest, RemoveTilePathFailedAfterDeletion) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    std::vector<float> verts;
    std::vector<int> tris;
    BuildTestPlane(0.0f, 20.0f, 11, verts, tris);

    if (!nav.BakeTiledFromTriangles(verts.data(),
                                     static_cast<int>(verts.size() / 3),
                                     tris.data(),
                                     static_cast<int>(tris.size() / 3),
                                     20.0f)) {
        GTEST_SKIP() << "Tiled bake 失败，跳过测试";
    }

    nav.RemoveTile(0, 0);

    std::vector<glm::vec3> path;
    bool success = nav.FindPath({3,0,3}, {17,0,17}, path);
    EXPECT_FALSE(success) << "删除所有 tile 后不应找到路径";
    nav.Shutdown();
}

TEST(NavMeshSystemTest, RebakeTileAtReturnUninitializedfalse) {
    NavMeshSystem nav;
    if (!nav.Init()) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }
    float v[] = {0,0,0, 1,0,0, 0,0,1};
    int t[] = {0,1,2};
    bool success = nav.RebakeTileAt(0, 0, v, 3, t, 1);
    EXPECT_FALSE(success) << "非 tiled 模式应返回 false";
    nav.Shutdown();
}

#endif // DSE_ENABLE_NAVMESH
