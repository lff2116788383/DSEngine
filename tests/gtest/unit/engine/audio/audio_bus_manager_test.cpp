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

// 测试 DSP特效参数：默认值
TEST(DspEffectParamsTest, DefaultValues) {
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

// 测试 DSP特效参数：枚举值连续计数正确
TEST(DspEffectParamsTest, EnumerationValueContinuousCountCorrect) {
    EXPECT_EQ(static_cast<int>(DspEffectType::LowPass),     0);
    EXPECT_EQ(static_cast<int>(DspEffectType::HighPass),    1);
    EXPECT_EQ(static_cast<int>(DspEffectType::BandPass),    2);
    EXPECT_EQ(static_cast<int>(DspEffectType::NotchFilter), 3);
    EXPECT_EQ(static_cast<int>(DspEffectType::PeakEQ),      4);
    EXPECT_EQ(static_cast<int>(DspEffectType::Delay),       5);
    EXPECT_EQ(static_cast<int>(DspEffectType::Reverb),      6);
    EXPECT_EQ(static_cast<int>(DspEffectType::Count),       7);
}

// 测试 DSP特效参数：带能够修订
TEST(DspEffectParamsTest, WithCanRevise) {
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

// 测试 DSP特效参数：禁用不
TEST(DspEffectParamsTest, DisabledNot) {
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

// 测试 音频总线管理器未初始化：不崩溃
TEST_F(AudioBusManagerUninitTest, DoesNotCrash) {}

// 测试 音频总线管理器未初始化：未初始化获取总线返回空指针
TEST_F(AudioBusManagerUninitTest, UninitializedGetBusReturnsnullptr) {
    EXPECT_EQ(mgr.GetBus("master"), nullptr);
    EXPECT_EQ(mgr.GetBus("music"),  nullptr);
    EXPECT_EQ(mgr.GetBus("sfx"),    nullptr);
    EXPECT_EQ(mgr.GetBus("voice"),  nullptr);
}

// 测试 音频总线管理器未初始化：未初始化创建总线返回false
TEST_F(AudioBusManagerUninitTest, UninitializedCreateBusReturnsfalse) {
    EXPECT_FALSE(mgr.CreateBus("custom"));
}

// 测试 音频总线管理器未初始化：未初始化移除总线返回false
TEST_F(AudioBusManagerUninitTest, UninitializedRemoveBusReturnsfalse) {
    EXPECT_FALSE(mgr.RemoveBus("custom"));
}

// 测试 音频总线管理器未初始化：未初始化添加特效返回false
TEST_F(AudioBusManagerUninitTest, UninitializedAddEffectReturnsfalse) {
    EXPECT_FALSE(mgr.AddEffect("master", DspEffectParams{}));
}

// 测试 音频总线管理器未初始化：未初始化移除特效返回false
TEST_F(AudioBusManagerUninitTest, UninitializedRemoveEffectReturnsfalse) {
    EXPECT_FALSE(mgr.RemoveEffect("master", 0));
}

// 测试 音频总线管理器未初始化：未初始化设置特效参数返回false
TEST_F(AudioBusManagerUninitTest, UninitializedSetEffectParamsReturnsfalse) {
    EXPECT_FALSE(mgr.SetEffectParams("master", 0, DspEffectParams{}));
}

// 测试 音频总线管理器未初始化：未初始化获取特效计数返回0
TEST_F(AudioBusManagerUninitTest, UninitializedGetEffectCountReturns0) {
    EXPECT_EQ(mgr.GetEffectCount("master"),  0u);
    EXPECT_EQ(mgr.GetEffectCount("music"),   0u);
    EXPECT_EQ(mgr.GetEffectCount("nonexist"),0u);
}

// 测试 音频总线管理器未初始化：未初始化获取分组句柄返回空指针
TEST_F(AudioBusManagerUninitTest, UninitializedGetGroupHandleReturnsnullptr) {
    EXPECT_EQ(mgr.GetGroupHandle("master"), nullptr);
}

// 测试 音频总线管理器未初始化：未初始化获取总线名称返回空
TEST_F(AudioBusManagerUninitTest, UninitializedGetBusNamesReturnsEmpty) {
    EXPECT_TRUE(mgr.GetBusNames().empty());
}

// 测试 音频总线管理器未初始化：未初始化设置总线体积不崩溃
TEST_F(AudioBusManagerUninitTest, UninitializedSetBusVolumeDoesNotCrash) {
    EXPECT_NO_THROW(mgr.SetBusVolume("master", 0.5f));
    EXPECT_NO_THROW(mgr.SetBusVolume("nonexist", 0.3f));
}

// 测试 音频总线管理器未初始化：未初始化设置总线静音不崩溃
TEST_F(AudioBusManagerUninitTest, UninitializedSetBusMutedDoesNotCrash) {
    EXPECT_NO_THROW(mgr.SetBusMuted("master", true));
    EXPECT_NO_THROW(mgr.SetBusMuted("nonexist", false));
}

// 测试 音频总线管理器未初始化：未初始化关闭不崩溃
TEST_F(AudioBusManagerUninitTest, UninitializedShutdownDoesNotCrash) {
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

// 测试 音频总线管理器引擎：初始化成功
TEST_F(AudioBusManagerEngineTest, InitSucceeds) {
    EXPECT_TRUE(engine_ok_);
}

// 测试 音频总线管理器引擎：总线管理器初始化成功
TEST_F(AudioBusManagerEngineTest, BusManagerInitSucceeds) {
    EXPECT_TRUE(mgr_ok_);
}

// 测试 音频总线管理器引擎：默认Busexist
TEST_F(AudioBusManagerEngineTest, DefaultBusexist) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_NE(mgr_.GetBus("master"), nullptr);
    EXPECT_NE(mgr_.GetBus("music"),  nullptr);
    EXPECT_NE(mgr_.GetBus("sfx"),    nullptr);
    EXPECT_NE(mgr_.GetBus("voice"),  nullptr);
}

// 测试 音频总线管理器引擎：默认总线分组句柄非空
TEST_F(AudioBusManagerEngineTest, DefaultBusGroupHandleNonEmpty) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_NE(mgr_.GetGroupHandle("master"), nullptr);
    EXPECT_NE(mgr_.GetGroupHandle("music"),  nullptr);
    EXPECT_NE(mgr_.GetGroupHandle("sfx"),    nullptr);
    EXPECT_NE(mgr_.GetGroupHandle("voice"),  nullptr);
}

// 测试 音频总线管理器引擎：获取总线名称包含Four默认Buses
TEST_F(AudioBusManagerEngineTest, GetBusNamesContainsFourDefaultBuses) {
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

// 测试 音频总线管理器引擎：存在为无总线返回空指针
TEST_F(AudioBusManagerEngineTest, ThereIsNoBusReturnnullptr) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_EQ(mgr_.GetBus("nonexist"), nullptr);
    EXPECT_EQ(mgr_.GetGroupHandle("nonexist"), nullptr);
}

// 测试 音频总线管理器引擎：初始化返回false
TEST_F(AudioBusManagerEngineTest, InitializeReturnsfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.Initialize(&engine_));
}

// 测试 音频总线管理器引擎：关闭不崩溃
TEST_F(AudioBusManagerEngineTest, ShutdownDoesNotCrash) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    mgr_.Shutdown();
    EXPECT_NO_THROW(mgr_.Shutdown());
}

// --- CreateBus / RemoveBus ---

// 测试 音频总线管理器引擎：创建Busmount Tomaster成功
TEST_F(AudioBusManagerEngineTest, CreateBusmountTomasterSucceeds) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_TRUE(mgr_.CreateBus("ambient", "master", 0.8f));
    EXPECT_NE(mgr_.GetBus("ambient"), nullptr);
}

// 测试 音频总线管理器引擎：创建总线重复名称返回false
TEST_F(AudioBusManagerEngineTest, CreateBusDuplicateNamesReturnedfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_TRUE(mgr_.CreateBus("fx2", "master"));
    EXPECT_FALSE(mgr_.CreateBus("fx2", "master"));
}

// 测试 音频总线管理器引擎：创建总线空名称返回false
TEST_F(AudioBusManagerEngineTest, CreateBusEmptyNameReturnedfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.CreateBus("", "master"));
}

// 测试 音频总线管理器引擎：创建总线返回若父总线不Existfalse
TEST_F(AudioBusManagerEngineTest, CreateBusReturnIfTheParentBusDoesNotExistfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.CreateBus("orphan", "nonexist_parent"));
}

// 测试 音频总线管理器引擎：移除总线自定义总线成功
TEST_F(AudioBusManagerEngineTest, RemoveBusCustomBusSuccessful) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_TRUE(mgr_.CreateBus("temp", "master"));
    EXPECT_TRUE(mgr_.RemoveBus("temp"));
    EXPECT_EQ(mgr_.GetBus("temp"), nullptr);
}

// 测试 音频总线管理器引擎：移除总线Deletion的已构建于Buses为不Allowed
TEST_F(AudioBusManagerEngineTest, RemoveBusDeletionOfBuiltInBusesIsNotAllowed) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.RemoveBus("master"));
    EXPECT_FALSE(mgr_.RemoveBus("music"));
    EXPECT_FALSE(mgr_.RemoveBus("sfx"));
    EXPECT_FALSE(mgr_.RemoveBus("voice"));
}

// 测试 音频总线管理器引擎：移除总线存在为无总线返回false
TEST_F(AudioBusManagerEngineTest, RemoveBusThereIsNoBusReturnfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.RemoveBus("nonexist"));
}

// --- Volume / Mute ---

// 测试 音频总线管理器引擎：设置总线体积有效范围
TEST_F(AudioBusManagerEngineTest, SetBusVolumeValidRange) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_NO_THROW(mgr_.SetBusVolume("master", 0.0f));
    EXPECT_NO_THROW(mgr_.SetBusVolume("master", 0.5f));
    EXPECT_NO_THROW(mgr_.SetBusVolume("master", 1.0f));
}

// 测试 音频总线管理器引擎：设置总线体积总线不崩溃若不存在
TEST_F(AudioBusManagerEngineTest, SetBusVolumeTheBusDoesNotCrashIfItDoesNotExist) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_NO_THROW(mgr_.SetBusVolume("nonexist", 0.5f));
}

// 测试 音频总线管理器引擎：设置总线静音体积字段Retained之后Muting
TEST_F(AudioBusManagerEngineTest, SetBusMutedVolumeFieldRetainedAfterMuting) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    mgr_.SetBusVolume("sfx", 0.7f);
    mgr_.SetBusMuted("sfx", true);
    auto* bus = mgr_.GetBus("sfx");
    ASSERT_NE(bus, nullptr);
    EXPECT_TRUE(bus->muted);
    EXPECT_FLOAT_EQ(bus->volume, 0.7f);
}

// 测试 音频总线管理器引擎：设置总线静音Unmute
TEST_F(AudioBusManagerEngineTest, SetBusMutedUnmute) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    mgr_.SetBusMuted("music", true);
    mgr_.SetBusMuted("music", false);
    auto* bus = mgr_.GetBus("music");
    ASSERT_NE(bus, nullptr);
    EXPECT_FALSE(bus->muted);
}

// --- AddEffect / GetEffectCount ---

// 测试 音频总线管理器引擎：添加特效Low通道成功
TEST_F(AudioBusManagerEngineTest, AddEffect_LowPassSucceeds) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::LowPass;
    p.cutoff_hz = 800.0f;
    EXPECT_TRUE(mgr_.AddEffect("master", p));
    EXPECT_EQ(mgr_.GetEffectCount("master"), 1u);
}

// 测试 音频总线管理器引擎：添加特效High通道成功
TEST_F(AudioBusManagerEngineTest, AddEffect_HighPassSucceeds) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::HighPass;
    p.cutoff_hz = 400.0f;
    EXPECT_TRUE(mgr_.AddEffect("sfx", p));
    EXPECT_EQ(mgr_.GetEffectCount("sfx"), 1u);
}

// 测试 音频总线管理器引擎：添加特效Band通道成功
TEST_F(AudioBusManagerEngineTest, AddEffect_BandPassSucceeds) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::BandPass;
    p.cutoff_hz = 2000.0f;
    EXPECT_TRUE(mgr_.AddEffect("music", p));
    EXPECT_EQ(mgr_.GetEffectCount("music"), 1u);
}

// 测试 音频总线管理器引擎：添加特效Delay成功
TEST_F(AudioBusManagerEngineTest, AddEffect_DelaySucceeds) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::Delay;
    p.delay_time_ms = 300.0f;
    p.feedback = 0.4f;
    EXPECT_TRUE(mgr_.AddEffect("voice", p));
    EXPECT_EQ(mgr_.GetEffectCount("voice"), 1u);
}

// 测试 音频总线管理器引擎：链多DSP
TEST_F(AudioBusManagerEngineTest, ChainMultiDSP) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams lpf; lpf.type = DspEffectType::LowPass;  lpf.cutoff_hz = 2000.0f;
    DspEffectParams hpf; hpf.type = DspEffectType::HighPass; hpf.cutoff_hz = 200.0f;
    DspEffectParams dly; dly.type = DspEffectType::Delay;    dly.delay_time_ms = 200.0f;
    EXPECT_TRUE(mgr_.AddEffect("master", lpf));
    EXPECT_TRUE(mgr_.AddEffect("master", hpf));
    EXPECT_TRUE(mgr_.AddEffect("master", dly));
    EXPECT_EQ(mgr_.GetEffectCount("master"), 3u);
}

// 测试 音频总线管理器引擎：添加特效存在为无总线返回false
TEST_F(AudioBusManagerEngineTest, AddEffectThereIsNoBusReturnfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.AddEffect("nonexist", DspEffectParams{}));
}

// --- RemoveEffect ---

// 测试 音频总线管理器引擎：移除特效成功Reduced数量的Effects
TEST_F(AudioBusManagerEngineTest, RemoveEffectSuccessfullyReducedTheNumberOfEffects) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p; p.type = DspEffectType::LowPass;
    mgr_.AddEffect("sfx", p);
    EXPECT_EQ(mgr_.GetEffectCount("sfx"), 1u);
    EXPECT_TRUE(mgr_.RemoveEffect("sfx", 0));
    EXPECT_EQ(mgr_.GetEffectCount("sfx"), 0u);
}

// 测试 音频总线管理器引擎：移除特效输出的Bounds索引返回false
TEST_F(AudioBusManagerEngineTest, RemoveEffectOutOfBoundsIndexReturnfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.RemoveEffect("master", 0));
    DspEffectParams p;
    mgr_.AddEffect("master", p);
    EXPECT_FALSE(mgr_.RemoveEffect("master", 5));
}

// 测试 音频总线管理器引擎：移除特效存在为无总线返回false
TEST_F(AudioBusManagerEngineTest, RemoveEffectThereIsNoBusReturnfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.RemoveEffect("nonexist", 0));
}

// 测试 音频总线管理器引擎：移除特效Intermediate Effects链重建正确
TEST_F(AudioBusManagerEngineTest, RemoveEffectIntermediateEffectsChainRebuiltCorrectly) {
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

// 测试 音频总线管理器引擎：设置特效参数更新截止Frequency
TEST_F(AudioBusManagerEngineTest, SetEffectParamsUpdateCutoffFrequency) {
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

// 测试 音频总线管理器引擎：设置特效参数输出的Bounds索引返回false
TEST_F(AudioBusManagerEngineTest, SetEffectParamsOutOfBoundsIndexReturnfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.SetEffectParams("master", 99, DspEffectParams{}));
}

// 测试 音频总线管理器引擎：设置特效参数存在为无总线返回false
TEST_F(AudioBusManagerEngineTest, SetEffectParamsThereIsNoBusReturnfalse) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_FALSE(mgr_.SetEffectParams("nonexist", 0, DspEffectParams{}));
}

// 测试 音频总线管理器引擎：设置特效参数之后Disabling特效获取特效Countconstant
TEST_F(AudioBusManagerEngineTest, SetEffectParamsAfterDisablingTheEffectGetEffectCountconstant) {
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

// 测试 音频总线管理器引擎：获取特效计数存在为无总线返回0
TEST_F(AudioBusManagerEngineTest, GetEffectCountThereIsNoBusReturn0) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    EXPECT_EQ(mgr_.GetEffectCount("nonexist"), 0u);
}

// 测试 音频总线管理器引擎：添加移除特效计数为正确
TEST_F(AudioBusManagerEngineTest, AddRemoveEffectEffectCountIsCorrect) {
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

// ============================================================
// Part 4: NotchFilter / PeakEQ 新增效果
// ============================================================

// 测试 DSP特效参数：陷波过滤参数构造
TEST(DspEffectParamsTest, NotchFilterParameterConstruction) {
    DspEffectParams p;
    p.type = DspEffectType::NotchFilter;
    p.cutoff_hz = 1500.0f;
    p.q = 2.0f;
    EXPECT_EQ(p.type, DspEffectType::NotchFilter);
    EXPECT_FLOAT_EQ(p.cutoff_hz, 1500.0f);
    EXPECT_FLOAT_EQ(p.q, 2.0f);
    EXPECT_FLOAT_EQ(p.gain_db, 0.0f);
}

// 测试 DSP特效参数：峰值均衡器参数构造
TEST(DspEffectParamsTest, PeakEQParameterConstruction) {
    DspEffectParams p;
    p.type = DspEffectType::PeakEQ;
    p.cutoff_hz = 3000.0f;
    p.q = 1.5f;
    p.gain_db = 6.0f;
    EXPECT_EQ(p.type, DspEffectType::PeakEQ);
    EXPECT_FLOAT_EQ(p.cutoff_hz, 3000.0f);
    EXPECT_FLOAT_EQ(p.q, 1.5f);
    EXPECT_FLOAT_EQ(p.gain_db, 6.0f);
}

// 测试 DSP特效参数：峰值均衡器Gain默认值到零
TEST(DspEffectParamsTest, PeakEQGainDefaultsToZero) {
    DspEffectParams p;
    p.type = DspEffectType::PeakEQ;
    EXPECT_FLOAT_EQ(p.gain_db, 0.0f);
}

// 测试 音频总线管理器引擎：添加特效陷波过滤成功
TEST_F(AudioBusManagerEngineTest, AddEffect_NotchFilterSucceeds) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::NotchFilter;
    p.cutoff_hz = 1000.0f;
    p.q = 1.0f;
    EXPECT_TRUE(mgr_.AddEffect("master", p));
    EXPECT_EQ(mgr_.GetEffectCount("master"), 1u);
}

// 测试 音频总线管理器引擎：添加特效峰值均衡器成功
TEST_F(AudioBusManagerEngineTest, AddEffect_PeakEQSucceeds) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams p;
    p.type = DspEffectType::PeakEQ;
    p.cutoff_hz = 2000.0f;
    p.q = 1.5f;
    p.gain_db = 3.0f;
    EXPECT_TRUE(mgr_.AddEffect("sfx", p));
    EXPECT_EQ(mgr_.GetEffectCount("sfx"), 1u);
}

// 测试 音频总线管理器引擎：链陷波过滤且峰值均衡器
TEST_F(AudioBusManagerEngineTest, ChainNotchFilterAndPeakEQ) {
    if (!mgr_ok_) GTEST_SKIP() << "AudioBusManager init failed";
    DspEffectParams notch;
    notch.type = DspEffectType::NotchFilter;
    notch.cutoff_hz = 500.0f;
    notch.q = 2.0f;

    DspEffectParams peak;
    peak.type = DspEffectType::PeakEQ;
    peak.cutoff_hz = 4000.0f;
    peak.q = 1.0f;
    peak.gain_db = -3.0f;

    DspEffectParams lpf;
    lpf.type = DspEffectType::LowPass;
    lpf.cutoff_hz = 8000.0f;

    EXPECT_TRUE(mgr_.AddEffect("music", notch));
    EXPECT_TRUE(mgr_.AddEffect("music", peak));
    EXPECT_TRUE(mgr_.AddEffect("music", lpf));
    EXPECT_EQ(mgr_.GetEffectCount("music"), 3u);

    auto* bus = mgr_.GetBus("music");
    ASSERT_NE(bus, nullptr);
    EXPECT_EQ(bus->effects[0].type, DspEffectType::NotchFilter);
    EXPECT_EQ(bus->effects[1].type, DspEffectType::PeakEQ);
    EXPECT_EQ(bus->effects[2].type, DspEffectType::LowPass);
}
