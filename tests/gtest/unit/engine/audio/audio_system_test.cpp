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

TEST_F(AudioSystemTest, 默认构造不崩溃) {
    // 验证构造/析构正常
}

TEST_F(AudioSystemTest, 未初始化时Shutdown不崩溃) {
    audio.Shutdown();
}

TEST_F(AudioSystemTest, 设置主音量) {
    audio.SetMasterVolume(0.5f);
    // 无公开 getter，仅验证不崩溃
}

TEST_F(AudioSystemTest, 设置背景音乐音量) {
    audio.SetBgmVolume(0.3f);
}

TEST_F(AudioSystemTest, 设置音效音量) {
    audio.SetSfxVolume(0.7f);
}

TEST_F(AudioSystemTest, 设置每片段最大并发音效数) {
    audio.SetMaxConcurrentSfxPerClip(8);
}

TEST_F(AudioSystemTest, 设置音效触发冷却时间) {
    audio.SetSfxTriggerCooldownMs(50);
}

TEST_F(AudioSystemTest, StopAllSfx未初始化不崩溃) {
    audio.StopAllSfx();
}

TEST_F(AudioSystemTest, StopBgm未初始化不崩溃) {
    audio.StopBgm();
}
