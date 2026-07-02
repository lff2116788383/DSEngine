/**
 * @file terrain_deep_test.cpp
 * @brief P5: 地形系统深度测试 — Clipmap 层级/采样、Deformation 操作/撤销
 */

#include <gtest/gtest.h>
#include "engine/terrain/geometry_clipmap.h"
#include "engine/terrain/terrain_deformation.h"
#include <cmath>
#include <vector>

using namespace dse::terrain;

// ═══════════════════════════════════════════════════════════
// GeometryClipmap 深度
// ═══════════════════════════════════════════════════════════

class ClipmapDeepTest : public ::testing::Test {
protected:
    GeometryClipmapSystem clipmap;
};

TEST_F(ClipmapDeepTest, InitCreatesLevels) {
    GeometryClipmapConfig config;
    config.num_levels = 4;
    config.grid_size = 32;
    clipmap.Init(config);
    EXPECT_EQ(clipmap.LevelCount(), 4);
}

TEST_F(ClipmapDeepTest, LevelExtentDoubles) {
    GeometryClipmapConfig config;
    config.num_levels = 4;
    config.grid_size = 64;
    config.base_cell_size = 1.0f;
    clipmap.Init(config);

    for (int i = 1; i < clipmap.LevelCount(); ++i) {
        float prev_cell = clipmap.GetLevel(i - 1).cell_size;
        float curr_cell = clipmap.GetLevel(i).cell_size;
        EXPECT_NEAR(curr_cell, prev_cell * 2.0f, 0.01f) << "level=" << i;
    }
}

TEST_F(ClipmapDeepTest, ConfigPreserved) {
    GeometryClipmapConfig config;
    config.num_levels = 5;
    config.grid_size = 128;
    config.base_cell_size = 2.0f;
    config.height_scale = 200.0f;
    config.blend_width = 8.0f;
    clipmap.Init(config);

    auto& c = clipmap.GetConfig();
    EXPECT_EQ(c.num_levels, 5);
    EXPECT_EQ(c.grid_size, 128);
    EXPECT_FLOAT_EQ(c.base_cell_size, 2.0f);
    EXPECT_FLOAT_EQ(c.height_scale, 200.0f);
    EXPECT_FLOAT_EQ(c.blend_width, 8.0f);
}

static float flat_sampler(float, float, void*) { return 0.5f; }

TEST_F(ClipmapDeepTest, UpdateWithFlatSampler) {
    GeometryClipmapConfig config;
    config.num_levels = 3;
    config.grid_size = 16;
    clipmap.Init(config);
    clipmap.SetHeightSampler(flat_sampler);
    clipmap.Update(glm::vec3(0.0f));

    for (int i = 0; i < clipmap.LevelCount(); ++i) {
        auto& lvl = clipmap.GetLevel(i);
        EXPECT_EQ(static_cast<int>(lvl.height_data.size()),
                  lvl.grid_size * lvl.grid_size) << "level=" << i;
    }
}

TEST_F(ClipmapDeepTest, DirtyAfterUpdate) {
    GeometryClipmapConfig config;
    config.num_levels = 3;
    config.grid_size = 16;
    clipmap.Init(config);
    clipmap.SetHeightSampler(flat_sampler);
    clipmap.Update(glm::vec3(0.0f));

    auto dirty = clipmap.GetDirtyLevels();
    EXPECT_GT(dirty.size(), 0u);

    for (int d : dirty) {
        clipmap.ClearDirty(d);
    }
    dirty = clipmap.GetDirtyLevels();
    EXPECT_EQ(dirty.size(), 0u);
}

TEST_F(ClipmapDeepTest, SampleHeight_FlatTerrain) {
    GeometryClipmapConfig config;
    config.num_levels = 3;
    config.grid_size = 16;
    clipmap.Init(config);
    clipmap.SetHeightSampler(flat_sampler);
    clipmap.Update(glm::vec3(0.0f));

    float h = clipmap.SampleHeight(0.0f, 0.0f);
    EXPECT_GE(h, 0.0f);
}

TEST_F(ClipmapDeepTest, ShutdownAndReinit) {
    GeometryClipmapConfig config;
    config.num_levels = 4;
    config.grid_size = 16;
    clipmap.Init(config);
    clipmap.Shutdown();
    EXPECT_EQ(clipmap.LevelCount(), 0);

    config.num_levels = 2;
    clipmap.Init(config);
    EXPECT_EQ(clipmap.LevelCount(), 2);
}

// ═══════════════════════════════════════════════════════════
// TerrainDeformation 深度
// ═══════════════════════════════════════════════════════════

class DeformDeepTest : public ::testing::Test {
protected:
    TerrainDeformationSystem deform;
    std::vector<float> heightmap;
    static constexpr uint32_t W = 64;
    static constexpr uint32_t H = 64;

    void SetUp() override {
        heightmap.assign(W * H, 0.0f);
        deform.Init();
        deform.SetHeightmap(heightmap.data(), W, H, 1.0f, glm::vec3(0.0f));
    }

    void TearDown() override {
        deform.Shutdown();
    }
};

TEST_F(DeformDeepTest, LowerCreatesDepression) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(32.0f, 0.0f, 32.0f);
    op.radius = 5.0f;
    op.strength = 1.0f;

    uint32_t id = deform.ApplyDeformation(op);
    EXPECT_GT(id, 0u);

    float h = deform.SampleHeight(32.0f, 32.0f);
    EXPECT_LT(h, 0.0f);
}

TEST_F(DeformDeepTest, RaiseCreatesElevation) {
    DeformationOp op;
    op.type = DeformationType::Raise;
    op.center = glm::vec3(32.0f, 0.0f, 32.0f);
    op.radius = 5.0f;
    op.strength = 1.0f;

    deform.ApplyDeformation(op);
    float h = deform.SampleHeight(32.0f, 32.0f);
    EXPECT_GT(h, 0.0f);
}

TEST_F(DeformDeepTest, UndoRestoresOriginal) {
    float before = deform.SampleHeight(32.0f, 32.0f);

    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(32.0f, 0.0f, 32.0f);
    op.radius = 5.0f;
    op.strength = 1.0f;
    deform.ApplyDeformation(op);

    EXPECT_TRUE(deform.Undo());
    float after_undo = deform.SampleHeight(32.0f, 32.0f);
    EXPECT_FLOAT_EQ(after_undo, before);
}

TEST_F(DeformDeepTest, RedoReapplies) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(32.0f, 0.0f, 32.0f);
    op.radius = 5.0f;
    op.strength = 1.0f;
    deform.ApplyDeformation(op);

    float after_apply = deform.SampleHeight(32.0f, 32.0f);
    deform.Undo();
    deform.Redo();
    float after_redo = deform.SampleHeight(32.0f, 32.0f);
    EXPECT_FLOAT_EQ(after_redo, after_apply);
}

TEST_F(DeformDeepTest, MultipleOpsAccumulate) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(32.0f, 0.0f, 32.0f);
    op.radius = 5.0f;
    op.strength = 1.0f;

    deform.ApplyDeformation(op);
    float h1 = deform.SampleHeight(32.0f, 32.0f);
    deform.ApplyDeformation(op);
    float h2 = deform.SampleHeight(32.0f, 32.0f);
    EXPECT_LT(h2, h1);
}

TEST_F(DeformDeepTest, HistoryCount) {
    EXPECT_EQ(deform.GetHistoryCount(), 0u);

    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(32.0f, 0.0f, 32.0f);
    op.radius = 3.0f;
    op.strength = 0.5f;

    deform.ApplyDeformation(op);
    EXPECT_EQ(deform.GetHistoryCount(), 1u);
    deform.ApplyDeformation(op);
    EXPECT_EQ(deform.GetHistoryCount(), 2u);
}

TEST_F(DeformDeepTest, ClearHistoryKeepsHeights) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(32.0f, 0.0f, 32.0f);
    op.radius = 5.0f;
    op.strength = 1.0f;
    deform.ApplyDeformation(op);

    float h_before_clear = deform.SampleHeight(32.0f, 32.0f);
    deform.ClearHistory();
    EXPECT_EQ(deform.GetHistoryCount(), 0u);

    float h_after_clear = deform.SampleHeight(32.0f, 32.0f);
    EXPECT_FLOAT_EQ(h_after_clear, h_before_clear);
}

TEST_F(DeformDeepTest, UndoOnEmptyReturnsFalse) {
    EXPECT_FALSE(deform.Undo());
}

TEST_F(DeformDeepTest, RedoOnEmptyReturnsFalse) {
    EXPECT_FALSE(deform.Redo());
}

TEST_F(DeformDeepTest, CallbackFired) {
    bool called = false;
    deform.SetDeformationCallback([&](const glm::vec3&, float) {
        called = true;
    });

    DeformationOp op;
    op.type = DeformationType::Raise;
    op.center = glm::vec3(32.0f, 0.0f, 32.0f);
    op.radius = 3.0f;
    op.strength = 0.5f;
    deform.ApplyDeformation(op);
    EXPECT_TRUE(called);
}

TEST_F(DeformDeepTest, ConfigValues) {
    auto& cfg = deform.GetConfig();
    EXPECT_TRUE(cfg.enable_history);
    EXPECT_GT(cfg.max_history, 0u);
}

TEST_F(DeformDeepTest, HasHeightmap) {
    EXPECT_TRUE(deform.HasHeightmap());
    EXPECT_EQ(deform.GetHeightmapWidth(), W);
    EXPECT_EQ(deform.GetHeightmapHeight(), H);
}

TEST_F(DeformDeepTest, DeformOutsideBoundsNoEffect) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(1000.0f, 0.0f, 1000.0f);
    op.radius = 2.0f;
    op.strength = 1.0f;
    deform.ApplyDeformation(op);

    float h = deform.SampleHeight(32.0f, 32.0f);
    EXPECT_FLOAT_EQ(h, 0.0f);
}
