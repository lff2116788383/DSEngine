/**
 * @file audio_asset_integration_test.cpp
 * @brief Audio ↔ Assets 集成测试
 *
 * 验证场景：
 * - AudioSystem 与 AssetManager 的协作接口
 * - AudioSourceComponent 与资产引用的关系
 * - 音量参数级联设置
 * - 未初始化 AudioSystem 的安全降级
 *
 * 注意：AudioSystem::Initialize 需要 miniaudio 后端，本测试仅覆盖
 *       无后端初始化的纯逻辑交互和组件数据层集成。
 */

#include <gtest/gtest.h>
#include "engine/audio/audio_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"

using namespace dse::gameplay2d;

class AudioAssetIntegrationTest : public ::testing::Test {
protected:
    World world;
    AudioSystem audio;
};

TEST_F(AudioAssetIntegrationTest, AudioSourceComponent默认值与AssetManager无依赖) {
    AudioSourceComponent audio_comp;
    EXPECT_EQ(audio_comp.clip, nullptr);
    EXPECT_TRUE(audio_comp.play_on_awake);
    EXPECT_FALSE(audio_comp.loop);
    EXPECT_FLOAT_EQ(audio_comp.volume, 1.0f);
    EXPECT_FLOAT_EQ(audio_comp.pitch, 1.0f);
}

TEST_F(AudioAssetIntegrationTest, 未初始化AudioSystem设置音量不崩溃) {
    EXPECT_NO_THROW(audio.SetMasterVolume(0.5f));
    EXPECT_NO_THROW(audio.SetBgmVolume(0.3f));
    EXPECT_NO_THROW(audio.SetSfxVolume(0.7f));
}

TEST_F(AudioAssetIntegrationTest, 未初始化AudioSystem停止播放不崩溃) {
    EXPECT_NO_THROW(audio.StopAllSfx());
    EXPECT_NO_THROW(audio.StopBgm());
}

TEST_F(AudioAssetIntegrationTest, 并发与冷却参数设置) {
    EXPECT_NO_THROW(audio.SetMaxConcurrentSfxPerClip(8));
    EXPECT_NO_THROW(audio.SetSfxTriggerCooldownMs(50));
}

TEST_F(AudioAssetIntegrationTest, 带AudioSourceComponent实体创建销毁不崩溃) {
    auto e = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(e);
    auto& audio_comp = world.registry().emplace<AudioSourceComponent>(e);
    audio_comp.volume = 0.8f;
    audio_comp.pitch = 1.2f;
    audio_comp.loop = true;

    EXPECT_TRUE(world.registry().all_of<AudioSourceComponent>(e));
    EXPECT_FLOAT_EQ(world.registry().get<AudioSourceComponent>(e).volume, 0.8f);

    world.DestroyEntity(e);
    EXPECT_FALSE(world.IsAlive(e));
}

TEST_F(AudioAssetIntegrationTest, 多个AudioSource实体共存) {
    for (int i = 0; i < 5; ++i) {
        auto e = world.CreateEntity();
        world.registry().emplace<TransformComponent>(e);
        auto& audio_comp = world.registry().emplace<AudioSourceComponent>(e);
        audio_comp.volume = static_cast<float>(i) / 5.0f;
    }

    int count = 0;
    auto view = world.registry().view<AudioSourceComponent>();
    for (auto entity : view) {
        (void)entity;
        ++count;
    }
    EXPECT_EQ(count, 5);
}

TEST_F(AudioAssetIntegrationTest, AssetManager与AudioSystem独立生命周期) {
    AssetManager am;
    // AudioSystem 不持有 AssetManager 引用（未初始化状态）
    EXPECT_NO_THROW(audio.SetMasterVolume(0.5f));
    EXPECT_NO_THROW(audio.Shutdown());
}
