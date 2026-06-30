/**
 * @file world_state_persistence_test.cpp
 * @brief 持久化世界状态系统单元测试
 */

#include <gtest/gtest.h>
#include "engine/scene/world_state_persistence.h"
#include <filesystem>

using namespace dse;

namespace {
const std::string kTestDir = "test_world_state_temp";
}

class WorldStatePersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(kTestDir);
        persistence_.Init(kTestDir);
    }

    void TearDown() override {
        persistence_.Shutdown();
        std::filesystem::remove_all(kTestDir);
    }

    WorldStatePersistence persistence_;
};

TEST_F(WorldStatePersistenceTest, RecordModification_IncreasesCount) {
    EntityModRecord rec;
    rec.entity_id = 100;
    rec.type = EntityModType::Modified;
    rec.component_name = "TransformComponent";
    rec.field_name = "position";
    rec.new_value = {1, 2, 3, 4};

    persistence_.RecordModification(0, 0, rec);

    EXPECT_EQ(persistence_.TotalModificationCount(), 1u);
    EXPECT_EQ(persistence_.DirtyCellCount(), 1u);
}

TEST_F(WorldStatePersistenceTest, RecordSpawn_CreatesSpawnRecord) {
    std::vector<uint8_t> data = {10, 20, 30};
    persistence_.RecordSpawn(1, 2, 42, data);

    const auto* state = persistence_.GetCellState(1, 2);
    ASSERT_NE(state, nullptr);
    ASSERT_EQ(state->modifications.size(), 1u);
    EXPECT_EQ(state->modifications[0].entity_id, 42u);
    EXPECT_EQ(state->modifications[0].type, EntityModType::Spawned);
    EXPECT_EQ(state->modifications[0].new_value, data);
}

TEST_F(WorldStatePersistenceTest, RecordDestruction_CreatesDestroyRecord) {
    persistence_.RecordDestruction(3, 4, 99);

    const auto* state = persistence_.GetCellState(3, 4);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->modifications[0].type, EntityModType::Destroyed);
    EXPECT_EQ(state->modifications[0].entity_id, 99u);
}

TEST_F(WorldStatePersistenceTest, SaveAndLoad_RoundTrip) {
    EntityModRecord rec;
    rec.entity_id = 200;
    rec.type = EntityModType::Modified;
    rec.component_name = "Health";
    rec.field_name = "current_hp";
    rec.new_value = {0x64, 0x00, 0x00, 0x00}; // 100 as int32

    persistence_.RecordModification(5, 6, rec);
    EXPECT_TRUE(persistence_.SaveCell(5, 6));

    // Create a new instance and load
    WorldStatePersistence loader;
    loader.Init(kTestDir);
    EXPECT_TRUE(loader.LoadCell(5, 6));

    const auto* state = loader.GetCellState(5, 6);
    ASSERT_NE(state, nullptr);
    ASSERT_EQ(state->modifications.size(), 1u);
    EXPECT_EQ(state->modifications[0].entity_id, 200u);
    EXPECT_EQ(state->modifications[0].type, EntityModType::Modified);
    EXPECT_EQ(state->modifications[0].component_name, "Health");
    EXPECT_EQ(state->modifications[0].field_name, "current_hp");
    EXPECT_EQ(state->modifications[0].new_value.size(), 4u);
    loader.Shutdown();
}

TEST_F(WorldStatePersistenceTest, ResetCell_RemovesState) {
    persistence_.RecordSpawn(0, 0, 1, {1, 2, 3});
    EXPECT_NE(persistence_.GetCellState(0, 0), nullptr);

    persistence_.SaveCell(0, 0);
    persistence_.ResetCell(0, 0);

    EXPECT_EQ(persistence_.GetCellState(0, 0), nullptr);
    // File should be deleted
    std::string path = kTestDir + "/cell_0_0.dcell_state";
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_F(WorldStatePersistenceTest, GetCellState_NonExistent_ReturnsNull) {
    EXPECT_EQ(persistence_.GetCellState(99, 99), nullptr);
}

TEST_F(WorldStatePersistenceTest, SaveAll_SavesAllDirtyCells) {
    persistence_.RecordSpawn(0, 0, 1, {1});
    persistence_.RecordSpawn(1, 1, 2, {2});
    persistence_.RecordSpawn(2, 2, 3, {3});

    EXPECT_EQ(persistence_.DirtyCellCount(), 3u);

    persistence_.SaveAll();

    EXPECT_EQ(persistence_.DirtyCellCount(), 0u);
}
