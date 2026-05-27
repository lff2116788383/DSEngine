/**
 * @file audio_bus_manager_test.cpp
 * @brief AudioBusManager + DSP 效果链单元测试
 *
 * 覆盖场景：
 * - DspEffectParams 默认值与所有枚举
 * - AudioBusManager 未初始化时的安全行为（全路径不崩溃）
 * - 带真实 miniaudio 引擎（noDevice 无硬件模式）：
 *     * Initialize / Shutdown / 重复初始化
 *     * 默认总线（master / music / sfx / voice）
 *     * CreateBus / RemoveBus（保护内置总线）
 *     * SetBusVolume / SetBusMuted
 *     * AddEffect / RemoveEffect / SetEffectParams（LPF/HPF/BPF/Delay）
 *     * GetEffectCount / GetGroupHandle / GetBusNames
 *     * 越界效果索引
 *     * 链式 DSP 效果 + 参数更新
 */

#include <gtest/gtest.h>
#include "engine/audio/audio_bus.h"
#include <miniaudio/miniaudio.h>

using namespace dse::gameplay2d;

// ============================================================
// Part 1: DspEffectParams 纯数据
// ============================================================

TEST(DspEffectParamsTest, 默认值) {
    DspEffectParams p;
    EXPECT_EQ(p.type, DspEffectType::LowPass);
    EXPECT_FLOAT_EQ(p.cutoff_hz, 1000.0f);
    EXPECT_FLOAT_EQ(p.q, 0.707f);
    EXPECT_FLOAT_EQ(p.delay_time_ms, 250.0f);
    EXPECT_FLOAT_EQ(p.feedback, 0.3f);
    EXPECT_FLOAT_EQ(p.wet_mix, 0.5f);
    EXPECT_FLOAT_EQ(p.room_size, 0.5f);
    EXPECT_FLOAT_EQ(p.damping, 0.5f);
    EXPECT_TRUE(p.enabled);
}

TEST(DspEffectParamsTest, 枚举值连续且Count正确) {
    EXPECT_EQ(static_cast<int>(DspEffectType::LowPass),  0);
    EXPECT_EQ(static_cast<int>(DspEffectType::HighPass), 1);
    EXPECT_EQ(static_cast<int>(DspEffectType::BandPass), 2);
    EXPECT_EQ(static_cast<int>(DspEffectType::Delay),    3);
    EXPECT_EQ(static_cast<int>(DspEffectType::Reverb),   4);
    EXPECT_EQ(static_cast<int>(DspEffectType::Count),    5);
}

TEST(DspEffectParamsTest, 所有字段可修改) {
    DspEffectParams p;
    p.type         = DspEffectType::Delay;
    p.delay_time_ms = 500.0f;
    p.feedback     = 0.6f;
    p.wet_mix      = 0.8f;
    p.enabled      = false;
    p.cutoff_hz    = 800.0f;
    p.q            = 1.0f;
    EXPECT_EQ(p.type, DspEffectType::Delay);
    EXPECT_FLOAT_EQ(p.delay_time_ms, 500.0f);
    EXPECT_FLOAT_EQ(p.feedback, 0.6f);
    EXPECT_FLOAT_EQ(p.wet_mix, 0.8f);
    EXPECT_FALSE(p.enabled);
    EXPECT_FLOAT_EQ(p.cutoff_hz, 800.0f);
    EXPECT_FLOAT_EQ(p.q, 1.0f);
}

TEST(DspEffectParamsTest, 禁用效果不影响其他字段) {
    DspEffectParams p;
    p.type    = DspEffectType::HighPass;
    p.enabled = false;
    EXPECT_EQ(p.type, DspEffectType::HighPass);
    EXPECT_FALSE(p.enabled);
    EXPECT_FLOAT_EQ(p.cutoff_hz, 1000.0f);
}

// ============================================================
// Part 2: AudioBusManager 未初始化安全行为
// ============================================================

class AudioBusManagerUninitTest : public ::testing::Test {
protected:
    AudioBusManager mgr;
};

TEST_F(AudioBusManagerUninitTest, 析构不崩溃) {}

TEST_F(AudioBusManagerUninitTest, 未初始化GetBus返回nullptr) {
    EXPECT_EQ(mgr.GetBus("master"), nullptr);
    EXPECT_EQ(mgr.GetBus("music"),  nullptr);
    EXPECT_EQ(mgr.GetBus("sfx"),    nullptr);
    EXPECT_EQ(mgr.GetBus("voice"),  nullptr);
}

TEST_F(AudioBusManagerUninitTest, 未初始化CreateBus返回false) {
    EXPECT_FALSE(mgr.CreateBus("custom"));
}

TEST_F(AudioBusManagerUninitTest, 未初始化RemoveBus返回false) {
    EXPECT_FALSE(mgr.RemoveBus("custom"));
}

TEST_F(AudioBusManagerUninitTest, 未初始化AddEffect返回false) {
    EXPECT_FALSE(mgr.AddEffect("master", DspEffectParams{}));
}

TEST_F(AudioBusManagerUninitTest, 未初始化RemoveEffect返回false) {
    EXPECT_FALSE(mgr.RemoveEffect("master", 0));
}

TEST_F(AudioBusManagerUninitTest, 未初始化SetEffectParams返回false) {
    EXPECT_FALSE(mgr.SetEffectParams("master", 0, DspEffectParams{}));
}

TEST_F(AudioBusManagerUninitTest, 未初始化GetEffectCount返回0) {
    EXPECT_EQ(mgr.GetEffectCount("master"),  0u);
    EXPECT_EQ(mgr.GetEffectCount("music"),   0u);
    EXPECT_EQ(mgr.GetEffectCount("nonexist"),0u);
}

TEST_F(AudioBusManagerUninitTest, 未初始化GetGroupHandle返回nullptr) {
    EXPECT_EQ(mgr.GetGroupHandle("master"), nullptr);
}

TEST_F(AudioBusManagerUninitTest, 未初始化GetBusNames返回空列表) {
    EXPECT_TRUE(mgr.GetBusNames().empty());
}

TEST_F(AudioBusManagerUninitTest, 未初始化SetBusVolume不崩溃) {
    EXPECT_NO_THROW(mgr.SetBusVolume("master", 0.5f));
    EXPECT_NO_THROW(mgr.SetBusVolume("nonexist", 0.3f));
}

TEST_F(AudioBusManagerUninitTest, 未初始化SetBusMuted不崩溃) {
    EXPECT_NO_THROW(mgr.SetBusMuted("master", true));
    EXPECT_NO_THROW(mgr.SetBusMuted("nonexist", false));
}

TEST_F(AudioBusManagerUninitTest, 未初始化Shutdown不崩溃) {
    EXPECT_NO_THROW(mgr.Shutdown());
}

// ============================================================
// Part 3: 带真实 miniaudio 引擎（noDevice 无硬件模式）
// ============================================================

static bool TryInitEngine(ma_engine& engine) {
    ma_engine_config cfg = ma_engine_config_init();
    cfg.noDevice   = MA_TRUE;
    cfg.channels   = 2;
    cfg.sampleRate = 44100;
    return ma_engine_init(&cfg, &engine) == MA_SUCCESS;
}

class AudioBusManagerEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ok_ = TryInitEngine(engine_);
        if (engine_ok_) {
            mgr_ok_ = mgr_.Initialize(&engine_);
        }
    }
    void TearDown() override {
        mgr_.Shutdown();
        if (engine_ok_) {
            ma_engine_uninit(&engine_);
        }
    }
    bool engine_ok_ = false;
    bool mgr_ok_    = false;
    ma_engine engine_{};
    AudioBusManager mgr_;
};

TEST_F(AudioBusManagerEngineTest, 引擎初始化成功) {
    EXPECT_TRUE(engine_ok_);
}

TEST_F(AudioBusManagerEngineTest, 总线管理器初始化成功) {
    EXPECT_TRUE(mgr_ok_);
}

TEST_F(AudioBusManagerEngineTest, 默认四条总线均存在) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_NE(mgr_.GetBus("master"), nullptr);
    EXPECT_NE(mgr_.GetBus("music"),  nullptr);
    EXPECT_NE(mgr_.GetBus("sfx"),    nullptr);
    EXPECT_NE(mgr_.GetBus("voice"),  nullptr);
}

TEST_F(AudioBusManagerEngineTest, 默认总线GroupHandle非空) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_NE(mgr_.GetGroupHandle("master"), nullptr);
    EXPECT_NE(mgr_.GetGroupHandle("music"),  nullptr);
    EXPECT_NE(mgr_.GetGroupHandle("sfx"),    nullptr);
    EXPECT_NE(mgr_.GetGroupHandle("voice"),  nullptr);
}

TEST_F(AudioBusManagerEngineTest, GetBusNames包含四条默认总线) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    auto names = mgr_.GetBusNames();
    EXPECT_GE(names.size(), 4u);
    auto has = [&](const std::string& n) {
        return std::find(names.begin(), names.end(), n) != names.end();
    };
    EXPECT_TRUE(has("master"));
    EXPECT_TRUE(has("music"));
    EXPECT_TRUE(has("sfx"));
    EXPECT_TRUE(has("voice"));
}

TEST_F(AudioBusManagerEngineTest, 不存在总线返回nullptr) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_EQ(mgr_.GetBus("nonexist"), nullptr);
    EXPECT_EQ(mgr_.GetGroupHandle("nonexist"), nullptr);
}

TEST_F(AudioBusManagerEngineTest, 重复初始化返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.Initialize(&engine_));
}

TEST_F(AudioBusManagerEngineTest, 重复Shutdown不崩溃) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    mgr_.Shutdown();
    EXPECT_NO_THROW(mgr_.Shutdown());
}

// --- CreateBus / RemoveBus ---

TEST_F(AudioBusManagerEngineTest, CreateBus挂载到master成功) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_TRUE(mgr_.CreateBus("ambient", "master", 0.8f));
    EXPECT_NE(mgr_.GetBus("ambient"), nullptr);
}

TEST_F(AudioBusManagerEngineTest, CreateBus重复名称返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_TRUE(mgr_.CreateBus("fx2", "master"));
    EXPECT_FALSE(mgr_.CreateBus("fx2", "master"));
}

TEST_F(AudioBusManagerEngineTest, CreateBus空名称返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.CreateBus("", "master"));
}

TEST_F(AudioBusManagerEngineTest, CreateBus父总线不存在返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.CreateBus("orphan", "nonexist_parent"));
}

TEST_F(AudioBusManagerEngineTest, RemoveBus自定义总线成功) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_TRUE(mgr_.CreateBus("temp", "master"));
    EXPECT_TRUE(mgr_.RemoveBus("temp"));
    EXPECT_EQ(mgr_.GetBus("temp"), nullptr);
}

TEST_F(AudioBusManagerEngineTest, RemoveBus不允许删除内置总线) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.RemoveBus("master"));
    EXPECT_FALSE(mgr_.RemoveBus("music"));
    EXPECT_FALSE(mgr_.RemoveBus("sfx"));
    EXPECT_FALSE(mgr_.RemoveBus("voice"));
}

TEST_F(AudioBusManagerEngineTest, RemoveBus不存在总线返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.RemoveBus("nonexist"));
}

// --- Volume / Mute ---

TEST_F(AudioBusManagerEngineTest, SetBusVolume有效范围) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_NO_THROW(mgr_.SetBusVolume("master", 0.0f));
    EXPECT_NO_THROW(mgr_.SetBusVolume("master", 0.5f));
    EXPECT_NO_THROW(mgr_.SetBusVolume("master", 1.0f));
}

TEST_F(AudioBusManagerEngineTest, SetBusVolume不存在总线不崩溃) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_NO_THROW(mgr_.SetBusVolume("nonexist", 0.5f));
}

TEST_F(AudioBusManagerEngineTest, SetBusMuted静音后音量字段保留) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    mgr_.SetBusVolume("sfx", 0.7f);
    mgr_.SetBusMuted("sfx", true);
    auto* bus = mgr_.GetBus("sfx");
    ASSERT_NE(bus, nullptr);
    EXPECT_TRUE(bus->muted);
    EXPECT_FLOAT_EQ(bus->volume, 0.7f);
}

TEST_F(AudioBusManagerEngineTest, SetBusMuted取消静音) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    mgr_.SetBusMuted("music", true);
    mgr_.SetBusMuted("music", false);
    auto* bus = mgr_.GetBus("music");
    ASSERT_NE(bus, nullptr);
    EXPECT_FALSE(bus->muted);
}

// --- AddEffect / GetEffectCount ---

TEST_F(AudioBusManagerEngineTest, AddEffect_LowPass成功) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::LowPass;
    p.cutoff_hz = 800.0f;
    EXPECT_TRUE(mgr_.AddEffect("master", p));
    EXPECT_EQ(mgr_.GetEffectCount("master"), 1u);
}

TEST_F(AudioBusManagerEngineTest, AddEffect_HighPass成功) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::HighPass;
    p.cutoff_hz = 400.0f;
    EXPECT_TRUE(mgr_.AddEffect("sfx", p));
    EXPECT_EQ(mgr_.GetEffectCount("sfx"), 1u);
}

TEST_F(AudioBusManagerEngineTest, AddEffect_BandPass成功) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::BandPass;
    p.cutoff_hz = 2000.0f;
    EXPECT_TRUE(mgr_.AddEffect("music", p));
    EXPECT_EQ(mgr_.GetEffectCount("music"), 1u);
}

TEST_F(AudioBusManagerEngineTest, AddEffect_Delay成功) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::Delay;
    p.delay_time_ms = 300.0f;
    p.feedback = 0.4f;
    EXPECT_TRUE(mgr_.AddEffect("voice", p));
    EXPECT_EQ(mgr_.GetEffectCount("voice"), 1u);
}

TEST_F(AudioBusManagerEngineTest, 链式多个DSP效果) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams lpf; lpf.type = DspEffectType::LowPass;  lpf.cutoff_hz = 2000.0f;
    DspEffectParams hpf; hpf.type = DspEffectType::HighPass; hpf.cutoff_hz = 200.0f;
    DspEffectParams dly; dly.type = DspEffectType::Delay;    dly.delay_time_ms = 200.0f;
    EXPECT_TRUE(mgr_.AddEffect("master", lpf));
    EXPECT_TRUE(mgr_.AddEffect("master", hpf));
    EXPECT_TRUE(mgr_.AddEffect("master", dly));
    EXPECT_EQ(mgr_.GetEffectCount("master"), 3u);
}

TEST_F(AudioBusManagerEngineTest, AddEffect不存在总线返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.AddEffect("nonexist", DspEffectParams{}));
}

// --- RemoveEffect ---

TEST_F(AudioBusManagerEngineTest, RemoveEffect成功减少效果数) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p; p.type = DspEffectType::LowPass;
    mgr_.AddEffect("sfx", p);
    EXPECT_EQ(mgr_.GetEffectCount("sfx"), 1u);
    EXPECT_TRUE(mgr_.RemoveEffect("sfx", 0));
    EXPECT_EQ(mgr_.GetEffectCount("sfx"), 0u);
}

TEST_F(AudioBusManagerEngineTest, RemoveEffect越界索引返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.RemoveEffect("master", 0));
    DspEffectParams p;
    mgr_.AddEffect("master", p);
    EXPECT_FALSE(mgr_.RemoveEffect("master", 5));
}

TEST_F(AudioBusManagerEngineTest, RemoveEffect不存在总线返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.RemoveEffect("nonexist", 0));
}

TEST_F(AudioBusManagerEngineTest, RemoveEffect中间效果链重建正确) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams lpf; lpf.type = DspEffectType::LowPass;
    DspEffectParams hpf; hpf.type = DspEffectType::HighPass;
    DspEffectParams bpf; bpf.type = DspEffectType::BandPass;
    mgr_.AddEffect("music", lpf);
    mgr_.AddEffect("music", hpf);
    mgr_.AddEffect("music", bpf);
    EXPECT_TRUE(mgr_.RemoveEffect("music", 1));
    EXPECT_EQ(mgr_.GetEffectCount("music"), 2u);
    auto* bus = mgr_.GetBus("music");
    ASSERT_NE(bus, nullptr);
    EXPECT_EQ(bus->effects[0].type, DspEffectType::LowPass);
    EXPECT_EQ(bus->effects[1].type, DspEffectType::BandPass);
}

// --- SetEffectParams ---

TEST_F(AudioBusManagerEngineTest, SetEffectParams更新截止频率) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p; p.type = DspEffectType::LowPass; p.cutoff_hz = 500.0f;
    mgr_.AddEffect("sfx", p);
    DspEffectParams updated = p;
    updated.cutoff_hz = 1500.0f;
    EXPECT_TRUE(mgr_.SetEffectParams("sfx", 0, updated));
    auto* bus = mgr_.GetBus("sfx");
    ASSERT_NE(bus, nullptr);
    EXPECT_FLOAT_EQ(bus->effects[0].cutoff_hz, 1500.0f);
}

TEST_F(AudioBusManagerEngineTest, SetEffectParams越界索引返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.SetEffectParams("master", 99, DspEffectParams{}));
}

TEST_F(AudioBusManagerEngineTest, SetEffectParams不存在总线返回false) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.SetEffectParams("nonexist", 0, DspEffectParams{}));
}

TEST_F(AudioBusManagerEngineTest, SetEffectParams禁用效果后GetEffectCount不变) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p; p.type = DspEffectType::HighPass;
    mgr_.AddEffect("voice", p);
    DspEffectParams off = p; off.enabled = false;
    EXPECT_TRUE(mgr_.SetEffectParams("voice", 0, off));
    EXPECT_EQ(mgr_.GetEffectCount("voice"), 1u);
    auto* bus = mgr_.GetBus("voice");
    ASSERT_NE(bus, nullptr);
    EXPECT_FALSE(bus->effects[0].enabled);
}

// --- GetEffectCount 边界 ---

TEST_F(AudioBusManagerEngineTest, GetEffectCount不存在总线返回0) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_EQ(mgr_.GetEffectCount("nonexist"), 0u);
}

TEST_F(AudioBusManagerEngineTest, AddRemoveEffect效果数正确) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_EQ(mgr_.GetEffectCount("master"), 0u);
    DspEffectParams p;
    mgr_.AddEffect("master", p);
    EXPECT_EQ(mgr_.GetEffectCount("master"), 1u);
    mgr_.AddEffect("master", p);
    EXPECT_EQ(mgr_.GetEffectCount("master"), 2u);
    mgr_.RemoveEffect("master", 0);
    EXPECT_EQ(mgr_.GetEffectCount("master"), 1u);
    mgr_.RemoveEffect("master", 0);
    EXPECT_EQ(mgr_.GetEffectCount("master"), 0u);
}
