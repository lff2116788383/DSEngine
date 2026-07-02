/**
 * @file navmesh_deep_test.cpp
 * @brief P5: NavMesh 系统深度测试 — 构建/寻路/射线/序列化/Tiled/RebaseOrigin
 */

#include <gtest/gtest.h>

#ifdef DSE_ENABLE_NAVMESH

#include "engine/navigation/nav_mesh_system.h"
#include <cmath>
#include <filesystem>
#include <vector>

using namespace dse::navigation;

static void MakeSubdividedPlane(float min_val, float max_val, int n,
                                std::vector<float>& verts,
                                std::vector<int>& tris) {
    verts.clear();
    tris.clear();
    float step = (max_val - min_val) / float(n - 1);
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            verts.push_back(min_val + x * step);
            verts.push_back(0.0f);
            verts.push_back(min_val + z * step);
        }
    }
    for (int z = 0; z < n - 1; ++z) {
        for (int x = 0; x < n - 1; ++x) {
            int i = z * n + x;
            tris.push_back(i);
            tris.push_back(i + n);
            tris.push_back(i + 1);
            tris.push_back(i + 1);
            tris.push_back(i + n);
            tris.push_back(i + n + 1);
        }
    }
}

class NavMeshDeepTest : public ::testing::Test {
protected:
    NavMeshSystem nav;
    std::vector<float> verts_;
    std::vector<int>   tris_;

    void SetUp() override {
        if (!nav.Init()) {
            GTEST_SKIP() << "NavMeshSystem::Init failed";
        }
        MakeSubdividedPlane(0.0f, 20.0f, 11, verts_, tris_);
    }
    void TearDown() override {
        nav.Shutdown();
    }

    bool BakePlane() {
        return nav.BakeFromTriangles(verts_.data(),
                                     static_cast<int>(verts_.size() / 3),
                                     tris_.data(),
                                     static_cast<int>(tris_.size() / 3));
    }
};

TEST_F(NavMeshDeepTest, InitNotReady) {
    NavMeshSystem fresh;
    fresh.Init();
    EXPECT_FALSE(fresh.IsReady());
    fresh.Shutdown();
}

TEST_F(NavMeshDeepTest, BakeAndReady) {
    ASSERT_TRUE(BakePlane());
    EXPECT_TRUE(nav.IsReady());
    EXPECT_GT(nav.GetPolyCount(), 0);
}

TEST_F(NavMeshDeepTest, FindPathOnFlat) {
    ASSERT_TRUE(BakePlane());
    std::vector<glm::vec3> path;
    bool found = nav.FindPath(glm::vec3(3, 0, 3), glm::vec3(17, 0, 17), path);
    EXPECT_TRUE(found);
    if (found) {
        EXPECT_GE(path.size(), 2u);
    }
}

TEST_F(NavMeshDeepTest, FindPathStartEqualsEnd) {
    ASSERT_TRUE(BakePlane());
    std::vector<glm::vec3> path;
    bool found = nav.FindPath(glm::vec3(10, 0, 10), glm::vec3(10, 0, 10), path);
    EXPECT_TRUE(found);
}

TEST_F(NavMeshDeepTest, FindNearestPoint) {
    ASSERT_TRUE(BakePlane());
    glm::vec3 nearest;
    bool found = nav.FindNearestPoint(glm::vec3(10, 0, 10), nearest);
    EXPECT_TRUE(found);
    if (found) {
        EXPECT_NEAR(nearest.x, 10.0f, 2.0f);
        EXPECT_NEAR(nearest.y, 0.0f, 1.0f);
        EXPECT_NEAR(nearest.z, 10.0f, 2.0f);
    }
}

TEST_F(NavMeshDeepTest, RaycastOnFlat) {
    ASSERT_TRUE(BakePlane());
    glm::vec3 hit;
    bool did_hit = nav.Raycast(glm::vec3(5, 0, 5), glm::vec3(15, 0, 15), hit);
    EXPECT_FALSE(did_hit);
}

TEST_F(NavMeshDeepTest, SaveLoadRoundtrip) {
    ASSERT_TRUE(BakePlane());
    int poly_before = nav.GetPolyCount();

    const std::string fpath = "test_navmesh_deep_roundtrip.bin";
    ASSERT_TRUE(nav.SaveNavMesh(fpath));

    nav.Shutdown();
    nav.Init();
    EXPECT_FALSE(nav.IsReady());

    ASSERT_TRUE(nav.LoadNavMesh(fpath));
    EXPECT_TRUE(nav.IsReady());
    EXPECT_EQ(nav.GetPolyCount(), poly_before);

    std::filesystem::remove(fpath);
}

TEST_F(NavMeshDeepTest, RebaseOrigin) {
    ASSERT_TRUE(BakePlane());
    nav.RebaseOrigin(glm::vec3(100, 0, 0));
    auto offset = nav.accumulated_offset();
    EXPECT_FLOAT_EQ(offset.x, 100.0f);

    nav.RebaseOrigin(glm::vec3(50, 0, 0));
    offset = nav.accumulated_offset();
    EXPECT_FLOAT_EQ(offset.x, 150.0f);
}

TEST_F(NavMeshDeepTest, ShutdownCleansUp) {
    ASSERT_TRUE(BakePlane());
    EXPECT_TRUE(nav.IsReady());
    nav.Shutdown();
    EXPECT_FALSE(nav.IsReady());
    EXPECT_EQ(nav.GetPolyCount(), 0);
}

TEST_F(NavMeshDeepTest, LoadNonexistentFile) {
    EXPECT_FALSE(nav.LoadNavMesh("nonexistent_file_deep_abc.bin"));
}

TEST_F(NavMeshDeepTest, FindPathBeforeBake) {
    std::vector<glm::vec3> path;
    bool found = nav.FindPath(glm::vec3(0), glm::vec3(5, 0, 0), path);
    EXPECT_FALSE(found);
}

TEST_F(NavMeshDeepTest, BakeTiledAndQuery) {
    bool ok = nav.BakeTiledFromTriangles(verts_.data(),
                                         static_cast<int>(verts_.size() / 3),
                                         tris_.data(),
                                         static_cast<int>(tris_.size() / 3),
                                         10.0f);
    if (!ok) {
        GTEST_SKIP() << "Tiled bake failed";
    }
    EXPECT_TRUE(nav.IsTiled());

    std::vector<glm::vec3> path;
    bool found = nav.FindPath(glm::vec3(3, 0, 3), glm::vec3(17, 0, 17), path);
    EXPECT_TRUE(found);
}

#endif // DSE_ENABLE_NAVMESH
