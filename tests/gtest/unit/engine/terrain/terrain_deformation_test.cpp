/**
 * @file terrain_deformation_test.cpp
 * @brief GTest for P4: Runtime Terrain Deformation System
 */

#include <gtest/gtest.h>
#include "engine/terrain/terrain_deformation.h"
#include <cmath>
#include <cstring>

using namespace dse::terrain;

class TerrainDeformTest : public ::testing::Test {
protected:
    void SetUp() override {
        system_.Init();
        // Create a 32x32 flat heightmap at height 0
        heightmap_.resize(32 * 32, 0.0f);
        system_.SetHeightmap(heightmap_.data(), 32, 32, 1.0f, glm::vec3(0, 0, 0));
    }
    void TearDown() override { system_.Shutdown(); }
    TerrainDeformationSystem system_;
    std::vector<float> heightmap_;
};

TEST_F(TerrainDeformTest, ApplyLower_CreatesHole) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(16, 0, 16);
    op.radius = 5.0f;
    op.strength = 10.0f;

    uint32_t id = system_.ApplyDeformation(op);
    EXPECT_GT(id, 0u);

    // Center should be lowered
    float center_h = system_.SampleHeight(16.0f, 16.0f);
    EXPECT_LT(center_h, 0.0f);
}

TEST_F(TerrainDeformTest, ApplyRaise_CreatesMound) {
    DeformationOp op;
    op.type = DeformationType::Raise;
    op.center = glm::vec3(16, 0, 16);
    op.radius = 5.0f;
    op.strength = 10.0f;

    system_.ApplyDeformation(op);
    float center_h = system_.SampleHeight(16.0f, 16.0f);
    EXPECT_GT(center_h, 0.0f);
}

TEST_F(TerrainDeformTest, ApplyFlatten) {
    // First raise some area
    DeformationOp raise_op;
    raise_op.type = DeformationType::Raise;
    raise_op.center = glm::vec3(16, 0, 16);
    raise_op.radius = 5.0f;
    raise_op.strength = 20.0f;
    system_.ApplyDeformation(raise_op);

    // Then flatten to 5.0
    DeformationOp flatten_op;
    flatten_op.type = DeformationType::Flatten;
    flatten_op.center = glm::vec3(16, 0, 16);
    flatten_op.radius = 5.0f;
    flatten_op.strength = 1.0f;
    flatten_op.target_height = 5.0f;
    system_.ApplyDeformation(flatten_op);

    float center_h = system_.SampleHeight(16.0f, 16.0f);
    EXPECT_NEAR(center_h, 5.0f, 0.5f);
}

TEST_F(TerrainDeformTest, BrushFalloff) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(16, 0, 16);
    op.radius = 8.0f;
    op.strength = 10.0f;
    op.falloff = 1.0f; // Linear

    system_.ApplyDeformation(op);

    float center_h = system_.SampleHeight(16.0f, 16.0f);
    float edge_h = system_.SampleHeight(16.0f + 6.0f, 16.0f);

    // Center should be deeper than edge
    EXPECT_LT(center_h, edge_h);
}

TEST_F(TerrainDeformTest, UndoRestoresHeight) {
    float orig = system_.SampleHeight(16.0f, 16.0f);

    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(16, 0, 16);
    op.radius = 5.0f;
    op.strength = 10.0f;
    system_.ApplyDeformation(op);

    float modified = system_.SampleHeight(16.0f, 16.0f);
    EXPECT_NE(modified, orig);

    bool undone = system_.Undo();
    EXPECT_TRUE(undone);

    float restored = system_.SampleHeight(16.0f, 16.0f);
    EXPECT_FLOAT_EQ(restored, orig);
}

TEST_F(TerrainDeformTest, RedoReapplies) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(16, 0, 16);
    op.radius = 5.0f;
    op.strength = 10.0f;
    system_.ApplyDeformation(op);

    float after_deform = system_.SampleHeight(16.0f, 16.0f);
    system_.Undo();
    system_.Redo();

    float after_redo = system_.SampleHeight(16.0f, 16.0f);
    EXPECT_FLOAT_EQ(after_deform, after_redo);
}

TEST_F(TerrainDeformTest, Undo_EmptyHistory_ReturnsFalse) {
    EXPECT_FALSE(system_.Undo());
}

TEST_F(TerrainDeformTest, Redo_EmptyStack_ReturnsFalse) {
    EXPECT_FALSE(system_.Redo());
}

TEST_F(TerrainDeformTest, MultipleDeformations) {
    DeformationOp op1;
    op1.type = DeformationType::Lower;
    op1.center = glm::vec3(10, 0, 10);
    op1.radius = 3.0f;
    op1.strength = 5.0f;
    system_.ApplyDeformation(op1);

    DeformationOp op2;
    op2.type = DeformationType::Raise;
    op2.center = glm::vec3(20, 0, 20);
    op2.radius = 3.0f;
    op2.strength = 5.0f;
    system_.ApplyDeformation(op2);

    EXPECT_EQ(system_.GetHistoryCount(), 2u);
    EXPECT_LT(system_.SampleHeight(10.0f, 10.0f), 0.0f);
    EXPECT_GT(system_.SampleHeight(20.0f, 20.0f), 0.0f);
}

TEST_F(TerrainDeformTest, DeformationCallback) {
    bool called = false;
    glm::vec3 cb_center;
    float cb_radius = 0;
    system_.SetDeformationCallback([&](const glm::vec3& c, float r) {
        called = true;
        cb_center = c;
        cb_radius = r;
    });

    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(16, 0, 16);
    op.radius = 5.0f;
    op.strength = 1.0f;
    system_.ApplyDeformation(op);

    EXPECT_TRUE(called);
    EXPECT_EQ(cb_center, glm::vec3(16, 0, 16));
    EXPECT_FLOAT_EQ(cb_radius, 5.0f);
}

TEST_F(TerrainDeformTest, ClampHeight) {
    TerrainDeformConfig cfg;
    cfg.max_height = 20.0f;
    cfg.min_height = -20.0f;
    TerrainDeformationSystem sys;
    sys.Init(cfg);
    std::vector<float> hm(32 * 32, 0.0f);
    sys.SetHeightmap(hm.data(), 32, 32, 1.0f, glm::vec3(0, 0, 0));

    DeformationOp op;
    op.type = DeformationType::Raise;
    op.center = glm::vec3(16, 0, 16);
    op.radius = 5.0f;
    op.strength = 100.0f; // Way over max
    sys.ApplyDeformation(op);

    float h = sys.SampleHeight(16.0f, 16.0f);
    EXPECT_LE(h, 20.0f);
    sys.Shutdown();
}

TEST_F(TerrainDeformTest, NoHeightmap_ReturnsZero) {
    TerrainDeformationSystem sys;
    sys.Init();
    EXPECT_FLOAT_EQ(sys.SampleHeight(10.0f, 10.0f), 0.0f);
    sys.Shutdown();
}

TEST_F(TerrainDeformTest, Serialize_Deserialize) {
    DeformationOp op;
    op.type = DeformationType::Lower;
    op.center = glm::vec3(16, 0, 16);
    op.radius = 5.0f;
    op.strength = 10.0f;
    system_.ApplyDeformation(op);

    std::string path = "test_deform.bin";
    EXPECT_TRUE(system_.Serialize(path));

    // Create fresh system, apply serialized deformations
    TerrainDeformationSystem sys2;
    sys2.Init();
    std::vector<float> hm2(32 * 32, 0.0f);
    sys2.SetHeightmap(hm2.data(), 32, 32, 1.0f, glm::vec3(0, 0, 0));
    EXPECT_TRUE(sys2.Deserialize(path));

    float h1 = system_.SampleHeight(16.0f, 16.0f);
    float h2 = sys2.SampleHeight(16.0f, 16.0f);
    EXPECT_FLOAT_EQ(h1, h2);
    sys2.Shutdown();

    // Cleanup
    std::remove(path.c_str());
}
