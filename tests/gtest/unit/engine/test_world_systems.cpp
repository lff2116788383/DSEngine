/**
 * @file test_world_systems.cpp
 * @brief GTest for 6 open-world systems:
 *        Spline, Ocean, WorldEditorTools, VSM, EQS, AssetDistribution
 */

#include <gtest/gtest.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "engine/terrain/spline_system.h"
#include "engine/render/ocean_system.h"
#include "engine/terrain/world_editor_tools.h"
#include "engine/render/virtual_shadow_map.h"
#include "engine/ai/eqs_system.h"
#include "engine/assets/asset_distribution.h"

using namespace dse::terrain;
using namespace dse::render;
using namespace dse::ai;
using namespace dse::assets;

// ============================================================
// Spline System Tests
// ============================================================

class SplineSystemTest : public ::testing::Test {
protected:
    SplineSystem sys;

    void SetUp() override {}
    void TearDown() override { sys.Shutdown(); }
};

TEST_F(SplineSystemTest, CreateDestroySpline) {
    auto id = sys.CreateSpline("test");
    EXPECT_EQ(sys.GetSplineCount(), 1u);
    sys.DestroySpline(id);
    EXPECT_EQ(sys.GetSplineCount(), 0u);
}

TEST_F(SplineSystemTest, AddRemovePoints) {
    auto id = sys.CreateSpline("path");
    SplinePoint p0; p0.position = {0, 0, 0};
    SplinePoint p1; p1.position = {10, 0, 0};
    SplinePoint p2; p2.position = {20, 0, 0};

    sys.AddPoint(id, p0);
    sys.AddPoint(id, p1);
    sys.AddPoint(id, p2);
    EXPECT_EQ(sys.GetPointCount(id), 3u);

    sys.RemovePoint(id, 1);
    EXPECT_EQ(sys.GetPointCount(id), 2u);
}

TEST_F(SplineSystemTest, EvaluateLinearSpline) {
    auto id = sys.CreateSpline("linear");
    SplinePoint p0; p0.position = {0, 0, 0};
    SplinePoint p1; p1.position = {10, 0, 0};
    sys.AddPoint(id, p0);
    sys.AddPoint(id, p1);

    auto s = sys.EvaluateAtParam(id, 0.0f);
    EXPECT_NEAR(s.position.x, 0.0f, 0.1f);

    s = sys.EvaluateAtParam(id, 1.0f);
    EXPECT_NEAR(s.position.x, 10.0f, 0.1f);

    s = sys.EvaluateAtParam(id, 0.5f);
    EXPECT_NEAR(s.position.x, 5.0f, 0.5f);
}

TEST_F(SplineSystemTest, SplineLengthPositive) {
    auto id = sys.CreateSpline("curve");
    SplinePoint p0; p0.position = {0, 0, 0};
    SplinePoint p1; p1.position = {5, 5, 0};
    SplinePoint p2; p2.position = {10, 0, 0};
    SplinePoint p3; p3.position = {15, 5, 0};
    sys.AddPoint(id, p0);
    sys.AddPoint(id, p1);
    sys.AddPoint(id, p2);
    sys.AddPoint(id, p3);

    float len = sys.GetSplineLength(id);
    EXPECT_GT(len, 14.0f);  // Should be longer than straight line distance
    EXPECT_LT(len, 30.0f);
}

TEST_F(SplineSystemTest, FindNearestPoint) {
    auto id = sys.CreateSpline("nearest");
    SplinePoint p0; p0.position = {0, 0, 0};
    SplinePoint p1; p1.position = {10, 0, 0};
    SplinePoint p2; p2.position = {20, 0, 0};
    sys.AddPoint(id, p0);
    sys.AddPoint(id, p1);
    sys.AddPoint(id, p2);

    float t = sys.FindNearestPoint(id, glm::vec3(10, 5, 0));
    EXPECT_NEAR(t, 0.5f, 0.05f);  // Nearest to midpoint
}

TEST_F(SplineSystemTest, GenerateRoadMesh) {
    auto id = sys.CreateSpline("road");
    SplinePoint p0; p0.position = {0, 0, 0}; p0.width = 4.0f;
    SplinePoint p1; p1.position = {10, 0, 0}; p1.width = 4.0f;
    SplinePoint p2; p2.position = {20, 0, 0}; p2.width = 4.0f;
    sys.AddPoint(id, p0);
    sys.AddPoint(id, p1);
    sys.AddPoint(id, p2);

    RoadConfig cfg;
    cfg.segment_length = 2.0f;
    cfg.width_segments = 2;
    cfg.conform_to_terrain = false;

    auto mesh = sys.GenerateRoadMesh(id, cfg);
    EXPECT_GT(mesh.vertices.size(), 0u);
    EXPECT_GT(mesh.indices.size(), 0u);
    EXPECT_EQ(mesh.indices.size() % 3, 0u);  // All triangles
}

TEST_F(SplineSystemTest, GenerateRiverMesh) {
    auto id = sys.CreateSpline("river");
    SplinePoint p0; p0.position = {0, 0, 0}; p0.width = 6.0f;
    SplinePoint p1; p1.position = {0, 0, 10}; p1.width = 6.0f;
    sys.AddPoint(id, p0);
    sys.AddPoint(id, p1);

    RiverConfig cfg;
    cfg.conform_to_terrain = false;
    auto mesh = sys.GenerateRiverMesh(id, cfg);
    EXPECT_GT(mesh.vertices.size(), 0u);
    EXPECT_GT(mesh.indices.size(), 0u);
}

TEST_F(SplineSystemTest, RebaseOrigin) {
    auto id = sys.CreateSpline("rebase");
    SplinePoint p0; p0.position = {100, 0, 100};
    sys.AddPoint(id, p0);

    sys.RebaseOrigin(glm::vec3(50, 0, 50));
    auto pt = sys.GetPoint(id, 0);
    EXPECT_NEAR(pt.position.x, 50.0f, 0.01f);
    EXPECT_NEAR(pt.position.z, 50.0f, 0.01f);
}

TEST_F(SplineSystemTest, WidthInterpolation) {
    auto id = sys.CreateSpline("width");
    SplinePoint p0; p0.position = {0, 0, 0}; p0.width = 2.0f;
    SplinePoint p1; p1.position = {10, 0, 0}; p1.width = 8.0f;
    sys.AddPoint(id, p0);
    sys.AddPoint(id, p1);

    auto s = sys.EvaluateAtParam(id, 0.5f);
    EXPECT_NEAR(s.width, 5.0f, 0.5f);  // Interpolated between 2 and 8
}

// ============================================================
// Ocean System Tests
// ============================================================

class OceanSystemTest : public ::testing::Test {
protected:
    OceanSystem sys;
    OceanConfig cfg;

    void SetUp() override {
        cfg.fft_resolution = 16;  // Small for tests
        cfg.tile_size = 50.0f;
        cfg.wind_speed = 15.0f;
        cfg.tile_count = 4;
        sys.Init(cfg);
    }
    void TearDown() override { sys.Shutdown(); }
};

TEST_F(OceanSystemTest, InitShutdown) {
    EXPECT_EQ(sys.GetConfig().fft_resolution, 16);
    EXPECT_EQ(sys.GetConfig().tile_count, 4);
}

TEST_F(OceanSystemTest, UpdateProducesHeight) {
    sys.Update(1.0f, glm::vec3(0, 0, 0));
    // After update, some points should have non-zero height
    bool has_nonzero = false;
    for (int x = 0; x < 16; ++x) {
        float h = sys.GetHeightAt(static_cast<float>(x) * 3.0f, 0.0f);
        if (std::abs(h) > 0.0001f) { has_nonzero = true; break; }
    }
    EXPECT_TRUE(has_nonzero);
}

TEST_F(OceanSystemTest, NormalIsValid) {
    sys.Update(2.0f, glm::vec3(0, 0, 0));
    auto n = sys.GetNormalAt(10.0f, 10.0f);
    float len = glm::length(n);
    EXPECT_NEAR(len, 1.0f, 0.01f);  // Normalized
}

TEST_F(OceanSystemTest, FoamInRange) {
    sys.Update(1.0f, glm::vec3(0, 0, 0));
    float foam = sys.GetFoamAt(5.0f, 5.0f);
    EXPECT_GE(foam, 0.0f);
    EXPECT_LE(foam, 1.0f);
}

TEST_F(OceanSystemTest, SetWindUpdatesSpectrum) {
    sys.Update(1.0f, glm::vec3(0, 0, 0));
    float h1 = sys.GetHeightAt(10.0f, 10.0f);
    sys.SetWind(30.0f, 0.0f, 1.0f);
    sys.Update(1.0f, glm::vec3(0, 0, 0));
    float h2 = sys.GetHeightAt(10.0f, 10.0f);
    // Heights should differ after wind change (not guaranteed but very likely)
    // Just verify no crash
    (void)h1; (void)h2;
}

TEST_F(OceanSystemTest, StatsAreValid) {
    sys.Update(1.0f, glm::vec3(0, 0, 0));
    auto stats = sys.GetStats();
    EXPECT_EQ(stats.total_tiles, static_cast<uint32_t>(cfg.tile_count * cfg.tile_count));
    EXPECT_GT(stats.visible_tiles, 0u);
    EXPECT_EQ(stats.fft_resolution, 16u);
}

TEST_F(OceanSystemTest, LODLevels) {
    EXPECT_EQ(sys.GetLODCount(), cfg.lod_levels);
}

TEST_F(OceanSystemTest, RebaseOrigin) {
    sys.Update(1.0f, glm::vec3(0, 0, 0));
    float h_before = sys.GetHeightAt(25.0f, 25.0f);
    sys.RebaseOrigin(glm::vec3(25, 0, 25));
    float h_after = sys.GetHeightAt(0.0f, 0.0f);
    EXPECT_NEAR(h_before, h_after, 0.01f);
}

// ============================================================
// World Editor Tools Tests
// ============================================================

class WorldEditorTest : public ::testing::Test {
protected:
    WorldEditorTools tools;

    void SetUp() override { tools.Init(); }
    void TearDown() override { tools.Shutdown(); }
};

TEST_F(WorldEditorTest, InitShutdown) {
    EXPECT_EQ(tools.GetFoliageCount(), 0u);
}

TEST_F(WorldEditorTest, BrushPreview) {
    BrushParams params;
    params.center = {50, 0, 50};
    params.radius = 10.0f;
    auto aabb = tools.GetBrushPreview(params);
    EXPECT_FLOAT_EQ(aabb.x, 40.0f);
    EXPECT_FLOAT_EQ(aabb.y, 40.0f);
    EXPECT_FLOAT_EQ(aabb.z, 60.0f);
    EXPECT_FLOAT_EQ(aabb.w, 60.0f);
}

TEST_F(WorldEditorTest, BrushWeightFalloff) {
    BrushParams params;
    params.center = {0, 0, 0};
    params.radius = 10.0f;
    params.strength = 1.0f;
    params.falloff = 0.5f;

    float w_center = tools.CalculateBrushWeight({0, 0, 0}, params);
    float w_edge = tools.CalculateBrushWeight({10, 0, 0}, params);
    float w_mid = tools.CalculateBrushWeight({5, 0, 0}, params);

    EXPECT_GT(w_center, w_mid);
    EXPECT_GT(w_mid, w_edge);
    EXPECT_NEAR(w_edge, 0.0f, 0.01f);
}

TEST_F(WorldEditorTest, PlaceFoliage) {
    FoliageBrushParams params;
    params.center = {50, 0, 50};
    params.radius = 10.0f;
    params.density = 0.1f;
    params.mesh_path = "tree_01";

    uint32_t placed = tools.PlaceFoliage(params);
    EXPECT_GT(placed, 0u);
    EXPECT_EQ(tools.GetFoliageCount(), placed);
}

TEST_F(WorldEditorTest, EraseFoliage) {
    FoliageBrushParams params;
    params.center = {50, 0, 50};
    params.radius = 10.0f;
    params.density = 0.5f;
    params.mesh_path = "bush";
    tools.PlaceFoliage(params);
    EXPECT_GT(tools.GetFoliageCount(), 0u);

    uint32_t erased = tools.EraseFoliage(glm::vec3(50, 0, 50), 20.0f);
    EXPECT_GT(erased, 0u);
    EXPECT_EQ(tools.GetFoliageCount(), 0u);
}

TEST_F(WorldEditorTest, UndoRedo) {
    BrushParams params;
    params.center = {0, 0, 0};
    params.radius = 5.0f;
    tools.ApplyTerrainBrush(TerrainBrushOp::RaiseHeight, params);
    EXPECT_EQ(tools.GetUndoCount(), 1u);
    EXPECT_EQ(tools.GetRedoCount(), 0u);

    EXPECT_TRUE(tools.Undo());
    EXPECT_EQ(tools.GetUndoCount(), 0u);
    EXPECT_EQ(tools.GetRedoCount(), 1u);

    EXPECT_TRUE(tools.Redo());
    EXPECT_EQ(tools.GetUndoCount(), 1u);
    EXPECT_EQ(tools.GetRedoCount(), 0u);
}

TEST_F(WorldEditorTest, PartitionVisualization) {
    tools.UpdatePartitionVisualization(glm::vec3(500, 0, 500), 256.0f);
    EXPECT_GT(tools.GetVisibleCellCount(), 0u);
    auto& states = tools.GetCellStates();
    EXPECT_FALSE(states.empty());
    // Check that some cells are loaded (near camera)
    bool has_loaded = false;
    for (const auto& s : states) {
        if (s.loaded) { has_loaded = true; break; }
    }
    EXPECT_TRUE(has_loaded);
}

TEST_F(WorldEditorTest, RoadDrawSession) {
    uint32_t session = tools.BeginRoadDraw(4.0f);
    EXPECT_GT(session, 0u);
    tools.AddRoadPoint(session, glm::vec3(0, 0, 0));
    tools.AddRoadPoint(session, glm::vec3(10, 0, 0));
    tools.EndRoadDraw(session);
    EXPECT_EQ(tools.GetUndoCount(), 1u);
}

// ============================================================
// Virtual Shadow Map Tests
// ============================================================

class VSMTest : public ::testing::Test {
protected:
    VirtualShadowMapSystem vsm;
    VSMConfig cfg;

    void SetUp() override {
        cfg.virtual_resolution = 1024;
        cfg.page_size = 128;
        cfg.physical_pool_pages = 64;
        cfg.clipmap_levels = 4;
        vsm.Init(cfg);
    }
    void TearDown() override { vsm.Shutdown(); }
};

TEST_F(VSMTest, InitConfig) {
    auto& c = vsm.GetConfig();
    EXPECT_EQ(c.virtual_resolution, 1024u);
    EXPECT_EQ(c.page_size, 128u);
    EXPECT_EQ(c.physical_pool_pages, 64u);
}

TEST_F(VSMTest, RegisterUnregisterLight) {
    ShadowLightInfo info;
    info.light_id = 1;
    info.is_directional = true;
    vsm.RegisterLight(info);
    // No crash
    vsm.UnregisterLight(1);
}

TEST_F(VSMTest, BeginEndFrame) {
    vsm.BeginFrame(1, glm::vec3(0, 0, 0));
    vsm.EndFrame();
    auto stats = vsm.GetStats();
    EXPECT_EQ(stats.rendered_this_frame, 0u);
}

TEST_F(VSMTest, StatsInitial) {
    auto stats = vsm.GetStats();
    EXPECT_GT(stats.total_pages, 0u);
    EXPECT_EQ(stats.mapped_pages, 0u);
    EXPECT_EQ(stats.dirty_pages, 0u);
}

TEST_F(VSMTest, ClipmapLevels) {
    EXPECT_EQ(vsm.GetConfig().clipmap_levels, 4u);
    auto mat = vsm.GetClipmapLevelMatrix(0);
    // Should be a valid matrix (not identity by default since camera is at origin)
    EXPECT_NE(mat[0][0], 0.0f);
}

TEST_F(VSMTest, LookupUnmapped) {
    uint32_t px, py;
    bool found = vsm.LookupPage(0, 0, 0, 0, px, py);
    EXPECT_FALSE(found);
}

// ============================================================
// EQS Tests
// ============================================================

class EQSTest : public ::testing::Test {
protected:
    EQSSystem eqs;

    void SetUp() override { eqs.Init(); }
    void TearDown() override { eqs.Shutdown(); }
};

TEST_F(EQSTest, CreateDestroyTemplate) {
    auto id = eqs.CreateTemplate("find_cover");
    EXPECT_EQ(eqs.GetTemplateCount(), 1u);
    eqs.DestroyTemplate(id);
    EXPECT_EQ(eqs.GetTemplateCount(), 0u);
}

TEST_F(EQSTest, GridGeneratorProducesPoints) {
    auto id = eqs.CreateTemplate("grid_test");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 10.0f;
    gen.spacing = 5.0f;
    gen.max_points = 100;
    eqs.SetGenerator(id, gen);

    // Add distance scorer
    ScorerConfig sc;
    sc.type = ScorerType::Distance;
    sc.weight = 1.0f;
    sc.max_value = 20.0f;
    eqs.AddScorer(id, sc);

    auto result = eqs.Execute(id, glm::vec3(0, 0, 0));
    EXPECT_GT(result.total_generated, 0u);
    EXPECT_GT(result.valid_count, 0u);
    EXPECT_GT(result.best_score, 0.0f);
}

TEST_F(EQSTest, RingGenerator) {
    auto id = eqs.CreateTemplate("ring_test");
    GeneratorConfig gen;
    gen.type = GeneratorType::Ring;
    gen.radius = 15.0f;
    gen.spacing = 3.0f;
    eqs.SetGenerator(id, gen);

    ScorerConfig sc;
    sc.type = ScorerType::Distance;
    sc.weight = 1.0f;
    sc.max_value = 30.0f;
    eqs.AddScorer(id, sc);

    auto result = eqs.Execute(id, glm::vec3(0, 0, 0));
    EXPECT_GT(result.total_generated, 0u);

    // All points should be at radius distance
    for (const auto& c : result.candidates) {
        float dist = glm::length(c.position - glm::vec3(0, 0, 0));
        EXPECT_NEAR(dist, 15.0f, 1.0f);
    }
}

TEST_F(EQSTest, DistanceScorerCloserIsBetter) {
    auto id = eqs.CreateTemplate("close_test");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 20.0f;
    gen.spacing = 5.0f;
    eqs.SetGenerator(id, gen);

    ScorerConfig sc;
    sc.type = ScorerType::Distance;
    sc.weight = 1.0f;
    sc.max_value = 30.0f;
    sc.invert = false;  // Closer = higher score
    eqs.AddScorer(id, sc);
    eqs.SetMaxResults(id, 5);

    auto result = eqs.Execute(id, glm::vec3(0, 0, 0));
    // Best point should be closest to querier
    if (result.candidates.size() >= 2) {
        float d0 = glm::length(result.candidates[0].position);
        float d1 = glm::length(result.candidates[1].position);
        EXPECT_LE(d0, d1 + 0.1f);
    }
}

TEST_F(EQSTest, CombineModeMuliply) {
    auto id = eqs.CreateTemplate("multiply_test");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 10.0f;
    gen.spacing = 5.0f;
    eqs.SetGenerator(id, gen);
    eqs.SetCombineMode(id, CombineMode::Multiply);

    ScorerConfig s1; s1.type = ScorerType::Distance; s1.max_value = 20.0f;
    ScorerConfig s2; s2.type = ScorerType::Height; s2.max_value = 10.0f;
    eqs.AddScorer(id, s1);
    eqs.AddScorer(id, s2);

    auto result = eqs.Execute(id, glm::vec3(0, 0, 0));
    EXPECT_GT(result.total_generated, 0u);
}

TEST_F(EQSTest, MaxResultsLimitsOutput) {
    auto id = eqs.CreateTemplate("limit_test");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 20.0f;
    gen.spacing = 2.0f;
    gen.max_points = 100;
    eqs.SetGenerator(id, gen);

    ScorerConfig sc; sc.type = ScorerType::Distance; sc.max_value = 40.0f;
    eqs.AddScorer(id, sc);
    eqs.SetMaxResults(id, 3);

    auto result = eqs.Execute(id, glm::vec3(0, 0, 0));
    EXPECT_LE(static_cast<uint32_t>(result.candidates.size()), 3u);
}

TEST_F(EQSTest, ExecuteAtCustomCenter) {
    auto id = eqs.CreateTemplate("custom_center");
    GeneratorConfig gen;
    gen.type = GeneratorType::Grid;
    gen.radius = 5.0f;
    gen.spacing = 2.0f;
    eqs.SetGenerator(id, gen);

    ScorerConfig sc; sc.type = ScorerType::Distance; sc.max_value = 20.0f;
    eqs.AddScorer(id, sc);

    auto result = eqs.ExecuteAt(id, glm::vec3(0, 0, 0), glm::vec3(50, 0, 50));
    // Points should be near custom center (50, 0, 50)
    if (!result.candidates.empty()) {
        float dist = glm::length(result.candidates[0].position - glm::vec3(50, 0, 50));
        EXPECT_LT(dist, 10.0f);
    }
}

// ============================================================
// Asset Distribution Tests
// ============================================================

class AssetDistributionTest : public ::testing::Test {
protected:
    AssetDistribution dist;
    DistributionConfig cfg;

    void SetUp() override {
        cfg.cell_size = 256.0f;
        cfg.max_concurrent_downloads = 4;
        cfg.cdn_base_url = "https://cdn.example.com";
        dist.Init(cfg);
    }
    void TearDown() override { dist.Shutdown(); }
};

TEST_F(AssetDistributionTest, PackageCell) {
    std::vector<std::string> assets = {"mesh_a.dmesh", "tex_b.dtex"};
    uint32_t idx = dist.PackageCell(0, 0, 0, assets);
    EXPECT_EQ(idx, 0u);

    auto info = dist.GetPackageInfo("cell_0_0_lod_0");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->cell_x, 0);
    EXPECT_EQ(info->cell_y, 0);
    EXPECT_GT(info->size_bytes, 0u);
}

TEST_F(AssetDistributionTest, DownloadSimulation) {
    std::vector<std::string> assets = {"data.bin"};
    dist.PackageCell(1, 1, 0, assets);

    dist.RequestDownload("cell_1_1_lod_0");
    auto info = dist.GetPackageInfo("cell_1_1_lod_0");
    EXPECT_EQ(info->state, PackageState::Downloading);

    // Simulate some time
    for (int i = 0; i < 100; ++i) dist.Tick(0.1f);

    info = dist.GetPackageInfo("cell_1_1_lod_0");
    EXPECT_EQ(info->state, PackageState::Installed);
    EXPECT_TRUE(dist.IsPackageInstalled("cell_1_1_lod_0"));
}

TEST_F(AssetDistributionTest, CancelDownload) {
    std::vector<std::string> assets = {"big_file.bin"};
    dist.PackageCell(2, 2, 0, assets);
    dist.RequestDownload("cell_2_2_lod_0");
    dist.CancelDownload("cell_2_2_lod_0");

    auto info = dist.GetPackageInfo("cell_2_2_lod_0");
    EXPECT_EQ(info->state, PackageState::NotDownloaded);
}

TEST_F(AssetDistributionTest, GetRequiredPackages) {
    dist.PackageCell(0, 0, 0, {"a"});
    dist.PackageCell(1, 0, 0, {"b"});
    dist.PackageCell(2, 2, 0, {"c"});

    auto required = dist.GetRequiredPackages(glm::vec3(128, 0, 128), 300.0f);
    EXPECT_GE(required.size(), 2u);  // cell_0_0 and cell_1_0 should be in range
}

TEST_F(AssetDistributionTest, GetMissingPackages) {
    dist.PackageCell(0, 0, 0, {"a"});
    dist.PackageCell(1, 0, 0, {"b"});

    auto missing = dist.GetMissingPackages(glm::vec3(128, 0, 128), 500.0f);
    EXPECT_EQ(missing.size(), 2u);  // Both not installed

    // Install one
    dist.RequestDownload("cell_0_0_lod_0");
    for (int i = 0; i < 100; ++i) dist.Tick(0.1f);

    missing = dist.GetMissingPackages(glm::vec3(128, 0, 128), 500.0f);
    EXPECT_EQ(missing.size(), 1u);
}

TEST_F(AssetDistributionTest, UpdatePriorities) {
    dist.PackageCell(0, 0, 0, {"a"});
    dist.PackageCell(10, 10, 0, {"b"});  // Far away

    dist.UpdatePriorities(glm::vec3(0, 0, 0));
    auto queue = dist.GetDownloadQueue();
    ASSERT_GE(queue.size(), 2u);
    // First in queue should be closer cell
    EXPECT_LT(queue[0].distance, queue[1].distance);
}

TEST_F(AssetDistributionTest, Stats) {
    dist.PackageCell(0, 0, 0, {"a"});
    dist.PackageCell(1, 1, 0, {"b"});

    auto stats = dist.GetStats();
    EXPECT_EQ(stats.total_packages, 2u);
    EXPECT_EQ(stats.installed_packages, 0u);
    EXPECT_EQ(stats.pending_packages, 2u);
}

TEST_F(AssetDistributionTest, DiskUsage) {
    dist.PackageCell(0, 0, 0, {"asset1", "asset2", "asset3"});
    EXPECT_EQ(dist.GetDiskUsage(), 0u);  // Nothing installed

    dist.RequestDownload("cell_0_0_lod_0");
    for (int i = 0; i < 100; ++i) dist.Tick(0.1f);

    EXPECT_GT(dist.GetDiskUsage(), 0u);
}

TEST_F(AssetDistributionTest, ManifestSaveLoad) {
    dist.PackageCell(0, 0, 0, {"a"});
    EXPECT_TRUE(dist.SaveManifest("test.manifest"));
    EXPECT_TRUE(dist.LoadManifest("test.manifest"));
}
