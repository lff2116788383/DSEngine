/**
 * @file audio_system_test.cpp
 * @brief AudioSystem 音频系统单元测试
 *
 * 覆盖场景：
 * - 默认构造与析构不崩溃
 * - 未初始化状态下调 Shutdown 不崩溃
 * - 音量设置与读取
 * - 并发限制与冷却参数设置
 *
 * 注意：AudioSystem::Initialize 需要 AssetManager 和 miniaudio 后端，
 *       本测试仅覆盖无需后端初始化的纯逻辑接口。
 */

#include <gtest/gtest.h>
#include "engine/audio/audio_system.h"

using namespace dse::gameplay2d;

class AudioSystemTest : public ::testing::Test {
protected:
    AudioSystem audio;
};

// 测试 音频系统：默认不崩溃
TEST_F(AudioSystemTest, DefaultDoesNotCrash) {
    // 验证构造/析构正常
}

// 测试 音频系统：当不已初始化关闭不崩溃
TEST_F(AudioSystemTest, WhenNotInitializedShutdownDoesNotCrash) {
    audio.Shutdown();
}

// 测试 音频系统：设置上
TEST_F(AudioSystemTest, SetUp) {
    audio.SetMasterVolume(0.5f);
    // 无公开 getter，仅验证不崩溃
}

// 测试 音频系统：设置上2
TEST_F(AudioSystemTest, SetUp_2) {
    audio.SetBgmVolume(0.3f);
}

// 测试 音频系统：设置上3
TEST_F(AudioSystemTest, SetUp_3) {
    audio.SetSfxVolume(0.7f);
}

// 测试 音频系统：设置上最大
TEST_F(AudioSystemTest, SetUpMax) {
    audio.SetMaxConcurrentSfxPerClip(8);
}

// 测试 音频系统：设置上触发时间
TEST_F(AudioSystemTest, SetUpTriggersTime) {
    audio.SetSfxTriggerCooldownMs(50);
}

// 测试 音频系统：停止全部音效未初始化不崩溃
TEST_F(AudioSystemTest, StopAllSfxUninitializedDoesNotCrash) {
    audio.StopAllSfx();
}

// 测试 音频系统：停止Bgm未初始化不崩溃
TEST_F(AudioSystemTest, StopBgmUninitializedDoesNotCrash) {
    audio.StopBgm();
}

// 测试 音频系统：淡入淡出输出全部音效未初始化不崩溃
TEST_F(AudioSystemTest, FadeOutAllSfxUninitializedDoesNotCrash) {
    audio.FadeOutAllSfx(0.5f);
}

// 测试 音频系统：淡入淡出输出全部音效默认参数执行不崩溃
TEST_F(AudioSystemTest, FadeOutAllSfxDefaultParametersDoNotCrash) {
    audio.FadeOutAllSfx();
}

// 测试 音频系统：淡入淡出输出全部音效无崩溃零时间
TEST_F(AudioSystemTest, FadeOutAllSfxNoCrashForZeroTime) {
    audio.FadeOutAllSfx(0.0f);
}
