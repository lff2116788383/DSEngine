/**
 * @file scene_manager_test.cpp
 * @brief SceneManager 单元测试
 */

#include <gtest/gtest.h>
#include "engine/scene/scene_manager.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/job_system.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace scene;
using namespace dse::core;

namespace {

std::filesystem::path WriteTempScene(const std::string& name, const std::string& json) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << json;
    out.close();
    return path;
}

const char* kSceneA = R"({ "name": "a", "entities": [
    { "id": 1, "components": { "TransformComponent": { "position": [1,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] } } }
]})";

const char* kSceneB = R"({ "name": "b", "entities": [
    { "id": 1, "components": { "TransformComponent": { "position": [2,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] } } },
    { "id": 2, "components": { "TransformComponent": { "position": [3,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] } } }
]})";

} // namespace

class SceneManagerTest : public ::testing::Test {
protected:
    World world;
    AssetManager asset_manager;
    EventBus event_bus;
    SceneManager mgr;

    void SetUp() override {
        mgr.SetWorld(&world);
        mgr.SetAssetManager(&asset_manager);
        mgr.SetEventBus(&event_bus);
    }

    void TearDown() override {
        mgr.UnloadAll();
    }
};

TEST_F(SceneManagerTest, 初始状态无已加载子场景) {
    EXPECT_EQ(mgr.LoadedCount(), 0u);
    EXPECT_TRUE(mgr.GetLoadedSubScenes().empty());
}

TEST_F(SceneManagerTest, 同步加载子场景) {
    auto path = WriteTempScene("dse_mgr_sync.dscene", kSceneA);
    EXPECT_TRUE(mgr.LoadSubScene(path.string()));
    EXPECT_EQ(mgr.LoadedCount(), 1u);
    EXPECT_TRUE(mgr.IsSubSceneLoaded(path.string()));
    EXPECT_EQ(world.EntityCount(), 1u);
    std::filesystem::remove(path);
}

TEST_F(SceneManagerTest, 重复加载同路径返回false) {
    auto path = WriteTempScene("dse_mgr_dup.dscene", kSceneA);
    EXPECT_TRUE(mgr.LoadSubScene(path.string()));
    EXPECT_FALSE(mgr.LoadSubScene(path.string()));
    EXPECT_EQ(mgr.LoadedCount(), 1u);
    std::filesystem::remove(path);
}

TEST_F(SceneManagerTest, 卸载子场景) {
    auto path = WriteTempScene("dse_mgr_unload.dscene", kSceneA);
    mgr.LoadSubScene(path.string());
    mgr.UnloadSubScene(path.string());
    EXPECT_EQ(mgr.LoadedCount(), 0u);
    EXPECT_FALSE(mgr.IsSubSceneLoaded(path.string()));
    EXPECT_EQ(world.EntityCount(), 0u);
    std::filesystem::remove(path);
}

TEST_F(SceneManagerTest, 卸载不存在的路径无副作用) {
    mgr.UnloadSubScene("nonexistent.dscene");
    EXPECT_EQ(mgr.LoadedCount(), 0u);
}

TEST_F(SceneManagerTest, 多子场景加载卸载) {
    auto pathA = WriteTempScene("dse_mgr_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_mgr_b.dscene", kSceneB);

    mgr.LoadSubScene(pathA.string());
    mgr.LoadSubScene(pathB.string());
    EXPECT_EQ(mgr.LoadedCount(), 2u);
    EXPECT_EQ(world.EntityCount(), 3u);

    mgr.UnloadSubScene(pathA.string());
    EXPECT_EQ(mgr.LoadedCount(), 1u);
    EXPECT_EQ(world.EntityCount(), 2u);

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

TEST_F(SceneManagerTest, UnloadAll清空所有子场景) {
    auto pathA = WriteTempScene("dse_mgr_all_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_mgr_all_b.dscene", kSceneB);

    mgr.LoadSubScene(pathA.string());
    mgr.LoadSubScene(pathB.string());
    mgr.UnloadAll();
    EXPECT_EQ(mgr.LoadedCount(), 0u);
    EXPECT_EQ(world.EntityCount(), 0u);

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

TEST_F(SceneManagerTest, GetLoadedSubScenes返回路径列表) {
    auto pathA = WriteTempScene("dse_mgr_list_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_mgr_list_b.dscene", kSceneB);

    mgr.LoadSubScene(pathA.string());
    mgr.LoadSubScene(pathB.string());
    auto list = mgr.GetLoadedSubScenes();
    EXPECT_EQ(list.size(), 2u);

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

TEST_F(SceneManagerTest, GetSubScene返回对应SubScene) {
    auto path = WriteTempScene("dse_mgr_get.dscene", kSceneA);
    mgr.LoadSubScene(path.string());
    const SubScene* sub = mgr.GetSubScene(path.string());
    ASSERT_NE(sub, nullptr);
    EXPECT_TRUE(sub->IsLoaded());
    EXPECT_EQ(sub->EntityCount(), 1u);
    EXPECT_EQ(mgr.GetSubScene("nonexistent"), nullptr);
    std::filesystem::remove(path);
}

TEST_F(SceneManagerTest, EventBus收到SubSceneLoaded事件) {
    auto path = WriteTempScene("dse_mgr_evt_load.dscene", kSceneA);
    bool received = false;
    std::string received_path;
    auto handle = event_bus.Subscribe<SubSceneLoadedEvent>(
        [&](const SubSceneLoadedEvent& evt) {
            received = true;
            received_path = evt.path;
        });
    mgr.LoadSubScene(path.string());
    EXPECT_TRUE(received);
    EXPECT_EQ(received_path, path.string());
    event_bus.Unsubscribe(handle);
    std::filesystem::remove(path);
}

TEST_F(SceneManagerTest, EventBus收到SubSceneUnloaded事件) {
    auto path = WriteTempScene("dse_mgr_evt_unload.dscene", kSceneA);
    mgr.LoadSubScene(path.string());
    bool received = false;
    auto handle = event_bus.Subscribe<SubSceneUnloadedEvent>(
        [&](const SubSceneUnloadedEvent& evt) { received = true; });
    mgr.UnloadSubScene(path.string());
    EXPECT_TRUE(received);
    event_bus.Unsubscribe(handle);
    std::filesystem::remove(path);
}

TEST_F(SceneManagerTest, 异步加载无JobSystem回退同步) {
    mgr.SetJobSystem(nullptr);
    auto path = WriteTempScene("dse_mgr_async_fallback.dscene", kSceneA);
    mgr.LoadSubSceneAsync(path.string());
    // 无 JobSystem 时立即同步加载
    EXPECT_EQ(mgr.LoadedCount(), 1u);
    EXPECT_EQ(world.EntityCount(), 1u);
    std::filesystem::remove(path);
}

TEST_F(SceneManagerTest, 异步加载有JobSystem) {
    JobSystem js;
    js.Init();
    mgr.SetJobSystem(&js);

    auto path = WriteTempScene("dse_mgr_async_js.dscene", kSceneB);
    mgr.LoadSubSceneAsync(path.string());

    // 等待工作线程完成
    for (int i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        mgr.Update();
        if (mgr.LoadedCount() > 0) break;
    }
    EXPECT_EQ(mgr.LoadedCount(), 1u);
    EXPECT_EQ(world.EntityCount(), 2u);

    js.Shutdown();
    std::filesystem::remove(path);
}

TEST_F(SceneManagerTest, 加载失败文件不影响状态) {
    EXPECT_FALSE(mgr.LoadSubScene("this_file_does_not_exist.dscene"));
    EXPECT_EQ(mgr.LoadedCount(), 0u);
}

TEST_F(SceneManagerTest, 未设置World时加载返回false) {
    SceneManager mgr2;
    mgr2.SetAssetManager(&asset_manager);
    EXPECT_FALSE(mgr2.LoadSubScene("any.dscene"));
}
