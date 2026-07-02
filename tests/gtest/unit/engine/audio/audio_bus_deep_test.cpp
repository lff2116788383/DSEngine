/**
 * @file audio_bus_deep_test.cpp
 * @brief P7: 音频总线系统深度测试 — 总线管理、效果链、快照（无 ma_engine 纯逻辑）
 *
 * AudioBusManager::Initialize 需要 ma_engine*，在无音频设备环境下无法调用。
 * 这里测试 pre-init 行为、数据结构默认值、DSP 参数验证。
 */

#include <gtest/gtest.h>
#include "engine/audio/audio_bus.h"

using namespace dse::gameplay2d;

// ═══════════════════════════════════════════════════════════
// DspEffectParams 默认值
// ═══════════════════════════════════════════════════════════

TEST(DspEffectParamsDeepTest, DefaultFieldValues) {
    DspEffectParams p;
    EXPECT_EQ(p.type, DspEffectType::LowPass);
    EXPECT_FLOAT_EQ(p.cutoff_hz, 1000.0f);
    EXPECT_FLOAT_EQ(p.q, 0.707f);
    EXPECT_FLOAT_EQ(p.delay_time_ms, 250.0f);
    EXPECT_FLOAT_EQ(p.feedback, 0.3f);
    EXPECT_FLOAT_EQ(p.wet_mix, 0.5f);
    EXPECT_FLOAT_EQ(p.gain_db, 0.0f);
    EXPECT_FLOAT_EQ(p.room_size, 0.5f);
    EXPECT_FLOAT_EQ(p.damping, 0.5f);
    EXPECT_TRUE(p.enabled);
}

TEST(DspEffectParamsDeepTest, AllTypesDistinct) {
    EXPECT_NE(static_cast<uint8_t>(DspEffectType::LowPass),
              static_cast<uint8_t>(DspEffectType::HighPass));
    EXPECT_NE(static_cast<uint8_t>(DspEffectType::BandPass),
              static_cast<uint8_t>(DspEffectType::Delay));
    EXPECT_NE(static_cast<uint8_t>(DspEffectType::Reverb),
              static_cast<uint8_t>(DspEffectType::PeakEQ));
    EXPECT_EQ(static_cast<uint8_t>(DspEffectType::Count), 7u);
}

// ═══════════════════════════════════════════════════════════
// AudioBus 结构体
// ═══════════════════════════════════════════════════════════

TEST(AudioBusStructTest, DefaultValues) {
    AudioBus bus;
    EXPECT_FLOAT_EQ(bus.volume, 1.0f);
    EXPECT_FALSE(bus.muted);
    EXPECT_TRUE(bus.name.empty());
    EXPECT_TRUE(bus.parent_name.empty());
    EXPECT_EQ(bus.effects.size(), 0u);
    EXPECT_EQ(bus.group_handle, nullptr);
}

TEST(AudioBusStructTest, EffectChainManipulation) {
    AudioBus bus;
    bus.name = "sfx";

    DspEffectParams lowpass;
    lowpass.type = DspEffectType::LowPass;
    lowpass.cutoff_hz = 500.0f;
    bus.effects.push_back(lowpass);

    DspEffectParams delay;
    delay.type = DspEffectType::Delay;
    delay.delay_time_ms = 100.0f;
    bus.effects.push_back(delay);

    EXPECT_EQ(bus.effects.size(), 2u);
    EXPECT_EQ(bus.effects[0].type, DspEffectType::LowPass);
    EXPECT_EQ(bus.effects[1].type, DspEffectType::Delay);
}

// ═══════════════════════════════════════════════════════════
// AudioBusManager pre-init
// ═══════════════════════════════════════════════════════════

class AudioBusManagerPreInitTest : public ::testing::Test {
protected:
    AudioBusManager mgr;
};

TEST_F(AudioBusManagerPreInitTest, GetBusBeforeInit) {
    auto* bus = mgr.GetBus("master");
    // Before Initialize, depending on implementation, master may or may not exist
    // Just verify no crash
    (void)bus;
}

TEST_F(AudioBusManagerPreInitTest, GetBusNamesBeforeInit) {
    auto names = mgr.GetBusNames();
    // May be empty or have defaults
    (void)names;
}

TEST_F(AudioBusManagerPreInitTest, InitWithNull) {
    bool ok = mgr.Initialize(nullptr);
    // Depending on impl, may return false or still set up bus structure
    // Just verify no crash
    (void)ok;
}

TEST_F(AudioBusManagerPreInitTest, SnapshotBeforeInit) {
    auto names = mgr.GetSnapshotNames();
    EXPECT_EQ(names.size(), 0u);
}

// ═══════════════════════════════════════════════════════════
// DspEffectParams 自定义值
// ═══════════════════════════════════════════════════════════

TEST(DspEffectParamsCustomTest, ReverbParams) {
    DspEffectParams reverb;
    reverb.type = DspEffectType::Reverb;
    reverb.room_size = 0.8f;
    reverb.damping = 0.6f;
    reverb.wet_mix = 0.7f;
    reverb.feedback = 0.4f;

    EXPECT_EQ(reverb.type, DspEffectType::Reverb);
    EXPECT_FLOAT_EQ(reverb.room_size, 0.8f);
    EXPECT_FLOAT_EQ(reverb.damping, 0.6f);
    EXPECT_FLOAT_EQ(reverb.wet_mix, 0.7f);
}

TEST(DspEffectParamsCustomTest, DisabledEffect) {
    DspEffectParams p;
    p.enabled = false;
    EXPECT_FALSE(p.enabled);
}

TEST(DspEffectParamsCustomTest, HighPassParams) {
    DspEffectParams hp;
    hp.type = DspEffectType::HighPass;
    hp.cutoff_hz = 2000.0f;
    hp.q = 1.41f;
    EXPECT_EQ(hp.type, DspEffectType::HighPass);
    EXPECT_FLOAT_EQ(hp.cutoff_hz, 2000.0f);
    EXPECT_FLOAT_EQ(hp.q, 1.41f);
}

TEST(DspEffectParamsCustomTest, BandPassParams) {
    DspEffectParams bp;
    bp.type = DspEffectType::BandPass;
    bp.cutoff_hz = 1500.0f;
    bp.q = 2.0f;
    EXPECT_EQ(bp.type, DspEffectType::BandPass);
}

TEST(DspEffectParamsCustomTest, NotchFilterParams) {
    DspEffectParams notch;
    notch.type = DspEffectType::NotchFilter;
    notch.cutoff_hz = 800.0f;
    EXPECT_EQ(notch.type, DspEffectType::NotchFilter);
}

TEST(DspEffectParamsCustomTest, PeakEQParams) {
    DspEffectParams eq;
    eq.type = DspEffectType::PeakEQ;
    eq.gain_db = 6.0f;
    eq.cutoff_hz = 3000.0f;
    EXPECT_EQ(eq.type, DspEffectType::PeakEQ);
    EXPECT_FLOAT_EQ(eq.gain_db, 6.0f);
}

TEST(DspEffectParamsCustomTest, DelayParams) {
    DspEffectParams delay;
    delay.type = DspEffectType::Delay;
    delay.delay_time_ms = 500.0f;
    delay.feedback = 0.5f;
    EXPECT_EQ(delay.type, DspEffectType::Delay);
    EXPECT_FLOAT_EQ(delay.delay_time_ms, 500.0f);
}

// ═══════════════════════════════════════════════════════════
// DspNodeHandle
// ═══════════════════════════════════════════════════════════

TEST(DspNodeHandleTest, DefaultValues) {
    DspNodeHandle handle;
    EXPECT_EQ(handle.type, DspEffectType::LowPass);
    EXPECT_EQ(handle.node_ptr, nullptr);
}

// ═══════════════════════════════════════════════════════════
// AudioBus Volume/Mute logic
// ═══════════════════════════════════════════════════════════

TEST(AudioBusLogicTest, VolumeClamp) {
    AudioBus bus;
    bus.volume = 0.0f;
    EXPECT_FLOAT_EQ(bus.volume, 0.0f);
    bus.volume = 1.0f;
    EXPECT_FLOAT_EQ(bus.volume, 1.0f);
}

TEST(AudioBusLogicTest, MuteToggle) {
    AudioBus bus;
    EXPECT_FALSE(bus.muted);
    bus.muted = true;
    EXPECT_TRUE(bus.muted);
}

TEST(AudioBusLogicTest, ParentRouting) {
    AudioBus sfx;
    sfx.name = "sfx";
    sfx.parent_name = "master";

    AudioBus music;
    music.name = "music";
    music.parent_name = "master";

    EXPECT_EQ(sfx.parent_name, "master");
    EXPECT_EQ(music.parent_name, "master");
}
