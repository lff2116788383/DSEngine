/**
 * @file streaming_manager_test.cpp
 * @brief StreamingManager zone 管理 + 距离触发逻辑单元测试（无 AssetManager 异步依赖）
 */

#include <gtest/gtest.h>
#include "engine/assets/streaming_manager.h"

using namespace dse::streaming;

class StreamingManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr_.Init(nullptr);
    }

    void TearDown() override {
        mgr_.Shutdown();
    }

    StreamingManager mgr_;
};

// 测试 流式管理器：初始状态
TEST_F(StreamingManagerTest, InitialState) {
    EXPECT_EQ(mgr_.GetZoneCount(), 0u);
    EXPECT_EQ(mgr_.GetActiveLoadCount(), 0);
}

// 测试 流式管理器：创建且销毁区域
TEST_F(StreamingManagerTest, CreateAndDestroyZone) {
    uint32_t id = mgr_.CreateZone("zone_a", glm::vec3(0.0f), 100.0f, 200.0f);
    EXPECT_GT(id, 0u);
    EXPECT_EQ(mgr_.GetZoneCount(), 1u);
    EXPECT_EQ(mgr_.GetZoneState(id), ZoneState::Unloaded);

    mgr_.DestroyZone(id);
    EXPECT_EQ(mgr_.GetZoneCount(), 0u);
}

// 测试 流式管理器：多个Zones
TEST_F(StreamingManagerTest, MultipleZones) {
    uint32_t id1 = mgr_.CreateZone("z1", glm::vec3(0.0f), 50.0f, 100.0f);
    uint32_t id2 = mgr_.CreateZone("z2", glm::vec3(100.0f, 0.0f, 0.0f), 50.0f, 100.0f);
    uint32_t id3 = mgr_.CreateZone("z3", glm::vec3(200.0f, 0.0f, 0.0f), 50.0f, 100.0f);
    EXPECT_EQ(mgr_.GetZoneCount(), 3u);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);

    mgr_.DestroyZone(id2);
    EXPECT_EQ(mgr_.GetZoneCount(), 2u);
}

// 测试 流式管理器：销毁不存在区域
TEST_F(StreamingManagerTest, DestroyNonexistentZone) {
    mgr_.DestroyZone(9999);
    EXPECT_EQ(mgr_.GetZoneCount(), 0u);
}

// 测试 流式管理器：添加资源
TEST_F(StreamingManagerTest, AddAssets) {
    uint32_t id = mgr_.CreateZone("zone", glm::vec3(0.0f), 100.0f, 200.0f);
    mgr_.AddAsset(id, "textures/diffuse.png", AssetType::Texture);
    mgr_.AddAssets(id, {"meshes/a.dmesh", "meshes/b.dmesh"}, AssetType::Mesh);
    EXPECT_EQ(mgr_.GetZoneState(id), ZoneState::Unloaded);
}

// 测试 流式管理器：添加资源到无效区域
TEST_F(StreamingManagerTest, AddAssetToInvalidZone) {
    mgr_.AddAsset(999, "path", AssetType::Texture);
    EXPECT_EQ(mgr_.GetZoneCount(), 0u);
}

// 测试 流式管理器：设置区域中心
TEST_F(StreamingManagerTest, SetZoneCenter) {
    uint32_t id = mgr_.CreateZone("zone", glm::vec3(0.0f), 100.0f, 200.0f);
    mgr_.SetZoneCenter(id, glm::vec3(500.0f, 0.0f, 0.0f));
    EXPECT_EQ(mgr_.GetZoneState(id), ZoneState::Unloaded);
}

// 测试 流式管理器：进度的空区域
TEST_F(StreamingManagerTest, ProgressOfEmptyZone) {
    uint32_t id = mgr_.CreateZone("empty", glm::vec3(0.0f), 100.0f, 200.0f);
    EXPECT_FLOAT_EQ(mgr_.GetZoneProgress(id), 1.0f);
}

// 测试 流式管理器：进度的不存在区域
TEST_F(StreamingManagerTest, ProgressOfNonexistentZone) {
    EXPECT_FLOAT_EQ(mgr_.GetZoneProgress(999), 0.0f);
}

// 测试 流式管理器：状态的不存在区域
TEST_F(StreamingManagerTest, StateOfNonexistentZone) {
    EXPECT_EQ(mgr_.GetZoneState(999), ZoneState::Unloaded);
}

// 测试 流式管理器：强制加载空区域
TEST_F(StreamingManagerTest, ForceLoadEmptyZone) {
    uint32_t id = mgr_.CreateZone("empty_force", glm::vec3(0.0f), 100.0f, 200.0f);
    mgr_.ForceLoadZone(id);
    EXPECT_EQ(mgr_.GetZoneState(id), ZoneState::Loaded);
}

// 测试 流式管理器：强制卸载区域
TEST_F(StreamingManagerTest, ForceUnloadZone) {
    uint32_t id = mgr_.CreateZone("force_unload", glm::vec3(0.0f), 100.0f, 200.0f);
    mgr_.ForceLoadZone(id);
    EXPECT_EQ(mgr_.GetZoneState(id), ZoneState::Loaded);

    mgr_.ForceUnloadZone(id);
    EXPECT_EQ(mgr_.GetZoneState(id), ZoneState::Unloaded);
}

// 测试 流式管理器：滴答带无Zones
TEST_F(StreamingManagerTest, TickWithNoZones) {
    mgr_.Tick(glm::vec3(0.0f));
    EXPECT_EQ(mgr_.GetZoneCount(), 0u);
}

// 测试 流式管理器：设置预算且Concurrency
TEST_F(StreamingManagerTest, SetBudgetAndConcurrency) {
    mgr_.SetLoadBudgetPerFrame(16);
    mgr_.SetMaxConcurrentLoads(64);
    EXPECT_EQ(mgr_.GetActiveLoadCount(), 0);
}

// 测试 流式管理器：关闭Idempotent
TEST_F(StreamingManagerTest, ShutdownIdempotent) {
    mgr_.Shutdown();
    mgr_.Shutdown();
    EXPECT_EQ(mgr_.GetZoneCount(), 0u);
}
