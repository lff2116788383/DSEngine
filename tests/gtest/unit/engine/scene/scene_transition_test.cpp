/**
 * @file scene_transition_test.cpp
 * @brief 场景切换状态机单元测试（Phase 3）
 */

#include <gtest/gtest.h>
#include "engine/scene/scene_manager.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/event_bus.h"
#include <filesystem>
#include <fstream>

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

class SceneTransitionTest : public ::testing::Test {
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

// 测试 场景过渡：初始状态为Idle
TEST_F(SceneTransitionTest, TheInitialStateIsIdle) {
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::Idle);
    EXPECT_TRUE(mgr.GetActiveScenePath().empty());
}

// 测试 场景过渡：即时切换到Replace场景Directly
TEST_F(SceneTransitionTest, InstantSwitchToReplaceSceneDirectly) {
    auto pathA = WriteTempScene("dse_trans_inst_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_trans_inst_b.dscene", kSceneB);

    mgr.TransitionTo(pathA.string(), TransitionMode::Instant);
    EXPECT_EQ(mgr.GetActiveScenePath(), pathA.string());
    EXPECT_EQ(mgr.LoadedCount(), 1u);
    EXPECT_EQ(world.EntityCount(), 1u);

    mgr.TransitionTo(pathB.string(), TransitionMode::Instant);
    EXPECT_EQ(mgr.GetActiveScenePath(), pathB.string());
    EXPECT_EQ(mgr.LoadedCount(), 1u);
    EXPECT_EQ(world.EntityCount(), 2u);

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

// 测试 场景过渡：Additive不卸载Old场景
TEST_F(SceneTransitionTest, AdditiveAdditiveDoesNotUnloadOldScene) {
    auto pathA = WriteTempScene("dse_trans_add_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_trans_add_b.dscene", kSceneB);

    mgr.TransitionTo(pathA.string(), TransitionMode::Additive);
    EXPECT_EQ(world.EntityCount(), 1u);

    mgr.TransitionTo(pathB.string(), TransitionMode::Additive);
    EXPECT_EQ(world.EntityCount(), 3u);
    EXPECT_EQ(mgr.LoadedCount(), 2u);
    EXPECT_EQ(mgr.GetActiveScenePath(), pathB.string());

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

// 测试 场景过渡：淡入淡出过渡Experience淡入淡出输出加载淡入淡出于
TEST_F(SceneTransitionTest, FadeTransitionExperienceFadingOut_Loading_FadingIn) {
    auto pathA = WriteTempScene("dse_trans_fade_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_trans_fade_b.dscene", kSceneB);

    // 先加载初始场景
    mgr.TransitionTo(pathA.string(), TransitionMode::Instant);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::Idle);

    // 开始 Fade 切换
    mgr.TransitionTo(pathB.string(), TransitionMode::Fade, 1.0f);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::FadingOut);

    // FadingOut 半程
    mgr.Update(0.5f);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::FadingOut);
    EXPECT_NEAR(mgr.GetFadeProgress(), 0.5f, 0.01f);

    // FadingOut 完成 → Loading
    mgr.Update(0.5f);
    // 此 Update 内 FadingOut→Loading 转换，但 Loading 立即在同一 Update 周期执行
    // 实际上 UpdateTransition 在 Update 开头被调用一次：先 FadingOut→Loading
    // 下次 Update 才会执行 Loading→FadingIn
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::Loading);

    // Loading → FadingIn（Loading 阶段同步加载后立即进入 FadingIn）
    mgr.Update(0.0f);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::FadingIn);
    EXPECT_TRUE(mgr.IsSubSceneLoaded(pathB.string()));
    EXPECT_FALSE(mgr.IsSubSceneLoaded(pathA.string()));

    // FadingIn 半程
    mgr.Update(0.5f);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::FadingIn);

    // FadingIn 完成 → Idle
    mgr.Update(0.5f);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::Idle);
    EXPECT_EQ(mgr.GetActiveScenePath(), pathB.string());

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

// 测试 场景过渡：淡入淡出完成Immediately带零Duration
TEST_F(SceneTransitionTest, FadeCompleteImmediatelyWithZeroDuration) {
    auto pathA = WriteTempScene("dse_trans_fade0_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_trans_fade0_b.dscene", kSceneB);

    mgr.TransitionTo(pathA.string(), TransitionMode::Instant);
    mgr.TransitionTo(pathB.string(), TransitionMode::Fade, 0.0f);

    // FadingOut with 0 duration → immediately completes
    mgr.Update(0.0f);
    // Should have transitioned to Loading
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::Loading);

    // Loading → FadingIn
    mgr.Update(0.0f);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::FadingIn);

    // FadingIn with 0 duration → immediately completes
    mgr.Update(0.0f);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::Idle);

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

// 测试 场景过渡：于不
TEST_F(SceneTransitionTest, InNot) {
    auto pathA = WriteTempScene("dse_trans_busy_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_trans_busy_b.dscene", kSceneB);

    mgr.TransitionTo(pathA.string(), TransitionMode::Instant);
    mgr.TransitionTo(pathB.string(), TransitionMode::Fade, 1.0f);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::FadingOut);

    // 尝试在过渡中开始新过渡 → 被忽略
    mgr.TransitionTo(pathA.string(), TransitionMode::Instant);
    EXPECT_EQ(mgr.GetTransitionState(), TransitionState::FadingOut);

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

// 测试 场景过渡：即时执行不Uninstall当切换Inactive场景
TEST_F(SceneTransitionTest, InstantDoNotUninstallWhenSwitchingInactiveScenes) {
    auto pathA = WriteTempScene("dse_trans_first.dscene", kSceneA);
    mgr.TransitionTo(pathA.string(), TransitionMode::Instant);
    EXPECT_EQ(mgr.LoadedCount(), 1u);
    EXPECT_EQ(mgr.GetActiveScenePath(), pathA.string());
    std::filesystem::remove(pathA);
}

// 测试 场景过渡：淡入淡出进度为在淡入淡出输出Monotonically递增于
TEST_F(SceneTransitionTest, FadeProgressIsAtFadingOutMonotonicallyIncreasingIn) {
    auto pathA = WriteTempScene("dse_trans_prog_a.dscene", kSceneA);
    auto pathB = WriteTempScene("dse_trans_prog_b.dscene", kSceneB);

    mgr.TransitionTo(pathA.string(), TransitionMode::Instant);
    mgr.TransitionTo(pathB.string(), TransitionMode::Fade, 1.0f);

    float prev = 0.0f;
    for (int i = 0; i < 10; ++i) {
        mgr.Update(0.1f);
        if (mgr.GetTransitionState() == TransitionState::FadingOut) {
            EXPECT_GE(mgr.GetFadeProgress(), prev);
            prev = mgr.GetFadeProgress();
        }
    }

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}
