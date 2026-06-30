/**
 * @file mesh_streaming_test.cpp
 * @brief GTest for P2: Mesh LOD Streaming System
 */

#include <gtest/gtest.h>
#include "engine/render/mesh_streaming.h"

using namespace dse::render;

class MeshStreamingTest : public ::testing::Test {
protected:
    void SetUp() override {
        MeshStreamingConfig cfg;
        cfg.hysteresis_factor = 1.2f;
        cfg.min_switch_interval = 0.0f; // No delay for tests
        system_.Init(cfg);
    }
    void TearDown() override { system_.Shutdown(); }
    MeshStreamingSystem system_;
};

TEST_F(MeshStreamingTest, RegisterAndUnregister) {
    uint32_t id = system_.RegisterMesh("test_mesh", glm::vec3(0, 0, 0), 5.0f);
    EXPECT_GT(id, 0u);
    EXPECT_EQ(system_.GetMeshCount(), 1u);
    system_.UnregisterMesh(id);
    EXPECT_EQ(system_.GetMeshCount(), 0u);
}

TEST_F(MeshStreamingTest, AddLODLevels) {
    uint32_t id = system_.RegisterMesh("mesh1", glm::vec3(0, 0, 0), 5.0f);
    system_.AddLODLevel(id, 0, "mesh1_lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id, 1, "mesh1_lod1.dmesh", 50.0f, 5000);
    system_.AddLODLevel(id, 2, "mesh1_lod2.dmesh", 100.0f, 1000);
    EXPECT_EQ(system_.GetLODCount(id), 3u);
}

TEST_F(MeshStreamingTest, LODSelection_NearCamera) {
    uint32_t id = system_.RegisterMesh("mesh1", glm::vec3(10, 0, 0), 5.0f);
    system_.AddLODLevel(id, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id, 1, "lod1.dmesh", 50.0f, 5000);
    system_.AddLODLevel(id, 2, "lod2.dmesh", 100.0f, 1000);

    // Camera at origin, mesh at (10,0,0) → distance=10 → LOD 0
    system_.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(system_.GetCurrentLOD(id), 0u);
}

TEST_F(MeshStreamingTest, LODSelection_FarCamera) {
    uint32_t id = system_.RegisterMesh("mesh1", glm::vec3(200, 0, 0), 5.0f);
    system_.AddLODLevel(id, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id, 1, "lod1.dmesh", 50.0f, 5000);
    system_.AddLODLevel(id, 2, "lod2.dmesh", 100.0f, 1000);

    // Camera at origin, mesh at (200,0,0) → distance=200 → LOD 2
    system_.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(system_.GetCurrentLOD(id), 2u);
}

TEST_F(MeshStreamingTest, LODSelection_MediumDistance) {
    uint32_t id = system_.RegisterMesh("mesh1", glm::vec3(70, 0, 0), 5.0f);
    system_.AddLODLevel(id, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id, 1, "lod1.dmesh", 50.0f, 5000);
    system_.AddLODLevel(id, 2, "lod2.dmesh", 100.0f, 1000);

    // distance=70 → > 50 threshold → LOD 1
    system_.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(system_.GetCurrentLOD(id), 1u);
}

TEST_F(MeshStreamingTest, Hysteresis_PreventsThrashing) {
    MeshStreamingConfig cfg;
    cfg.hysteresis_factor = 1.5f;
    cfg.min_switch_interval = 0.0f;
    MeshStreamingSystem sys;
    sys.Init(cfg);

    uint32_t id = sys.RegisterMesh("mesh1", glm::vec3(55, 0, 0), 5.0f);
    sys.AddLODLevel(id, 0, "lod0.dmesh", 0.0f, 10000);
    sys.AddLODLevel(id, 1, "lod1.dmesh", 50.0f, 5000);

    // First tick: distance=55 → LOD 1
    sys.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(sys.GetCurrentLOD(id), 1u);

    // Move closer to 40 (within hysteresis zone: 50/1.5 = 33.3, so 40 > 33.3 → stay at LOD 1)
    sys.UpdatePosition(id, glm::vec3(40, 0, 0));
    sys.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(sys.GetCurrentLOD(id), 1u);

    // Move much closer to 20 (below hysteresis threshold: 50/1.5=33.3 → 20 < 33.3)
    sys.UpdatePosition(id, glm::vec3(20, 0, 0));
    sys.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(sys.GetCurrentLOD(id), 0u);

    sys.Shutdown();
}

TEST_F(MeshStreamingTest, ForceLoadUnload) {
    uint32_t id = system_.RegisterMesh("mesh1", glm::vec3(200, 0, 0), 5.0f);
    system_.AddLODLevel(id, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id, 1, "lod1.dmesh", 50.0f, 5000);

    system_.ForceLoadLOD(id, 0);
    EXPECT_EQ(system_.GetCurrentLOD(id), 0u);
}

TEST_F(MeshStreamingTest, TriangleCountTracking) {
    uint32_t id = system_.RegisterMesh("mesh1", glm::vec3(0, 0, 0), 5.0f);
    system_.AddLODLevel(id, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id, 1, "lod1.dmesh", 50.0f, 5000);

    system_.ForceLoadLOD(id, 0);
    system_.ForceLoadLOD(id, 1);
    EXPECT_EQ(system_.GetLoadedTriangleCount(), 15000u);
}

TEST_F(MeshStreamingTest, RebaseOrigin) {
    uint32_t id = system_.RegisterMesh("mesh1", glm::vec3(100, 0, 0), 5.0f);
    system_.AddLODLevel(id, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id, 1, "lod1.dmesh", 50.0f, 5000);

    // Before rebase: camera at (0,0,0), mesh at (100,0,0) → distance=100 → LOD 1
    system_.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(system_.GetCurrentLOD(id), 1u);

    // Rebase by (90,0,0): mesh moves to (10,0,0) → distance=10 → LOD 0
    system_.RebaseOrigin(glm::vec3(90, 0, 0));
    system_.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(system_.GetCurrentLOD(id), 0u);
}

TEST_F(MeshStreamingTest, MultipleMeshes) {
    uint32_t id1 = system_.RegisterMesh("near", glm::vec3(10, 0, 0), 5.0f);
    uint32_t id2 = system_.RegisterMesh("far", glm::vec3(200, 0, 0), 5.0f);

    system_.AddLODLevel(id1, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id1, 1, "lod1.dmesh", 50.0f, 5000);
    system_.AddLODLevel(id2, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id2, 1, "lod1.dmesh", 50.0f, 5000);

    system_.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(system_.GetCurrentLOD(id1), 0u);
    EXPECT_EQ(system_.GetCurrentLOD(id2), 1u);
}

TEST_F(MeshStreamingTest, LoadCallback) {
    bool called = false;
    uint32_t cb_mesh_id = 0;
    uint32_t cb_lod = 0;
    system_.SetLoadCallback([&](uint32_t mid, uint32_t lod, bool success) {
        called = true;
        cb_mesh_id = mid;
        cb_lod = lod;
        EXPECT_TRUE(success);
    });

    // Mesh close to camera → wants LOD 0, but only LOD 1 is resident initially
    uint32_t id = system_.RegisterMesh("mesh1", glm::vec3(10, 0, 0), 5.0f);
    system_.AddLODLevel(id, 0, "lod0.dmesh", 0.0f, 10000);
    system_.AddLODLevel(id, 1, "lod1.dmesh", 50.0f, 5000);

    system_.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_TRUE(called);
    EXPECT_EQ(cb_mesh_id, id);
    EXPECT_EQ(cb_lod, 0u); // LOD 0 loaded because mesh is near camera
}

TEST_F(MeshStreamingTest, EmptyMeshNoLODs) {
    uint32_t id = system_.RegisterMesh("empty", glm::vec3(0, 0, 0), 1.0f);
    system_.Tick(glm::vec3(0, 0, 0), 0.1f);
    EXPECT_EQ(system_.GetCurrentLOD(id), 0u);
    EXPECT_EQ(system_.GetLODCount(id), 0u);
}
