/**
 * @file input_recorder_test.cpp
 * @brief InputRecorder 录制/回放 + JSON round-trip 测试
 */

#include <gtest/gtest.h>
#include "engine/input/input_recorder.h"

using namespace dse::input;

class InputRecorderExtTest : public ::testing::Test {};

// 测试 输入录制器扩展：初始状态
TEST_F(InputRecorderExtTest, InitialState) {
    InputRecorder rec;
    EXPECT_FALSE(rec.IsRecording());
    EXPECT_EQ(rec.GetEventCount(), 0u);
}

// 测试 输入录制器扩展：启动停止录制
TEST_F(InputRecorderExtTest, StartStopRecording) {
    InputRecorder rec;
    rec.StartRecording();
    EXPECT_TRUE(rec.IsRecording());
    rec.StopRecording();
    EXPECT_FALSE(rec.IsRecording());
}

// 测试 输入录制器扩展：记录While录制
TEST_F(InputRecorderExtTest, RecordWhileRecording) {
    InputRecorder rec;
    rec.StartRecording();
    rec.RecordEvent(65, 1, 0.0);   // key A press
    rec.RecordEvent(65, 0, 0.1);   // key A release
    EXPECT_EQ(rec.GetEventCount(), 2u);

    const auto& events = rec.GetEvents();
    EXPECT_EQ(events[0].key_code, 65);
    EXPECT_EQ(events[0].key_action, 1);
    EXPECT_DOUBLE_EQ(events[0].timestamp, 0.0);
    EXPECT_EQ(events[1].key_action, 0);
}

// 测试 输入录制器扩展：忽略事件当不录制
TEST_F(InputRecorderExtTest, IgnoreEventsWhenNotRecording) {
    InputRecorder rec;
    rec.RecordEvent(65, 1, 0.0);
    EXPECT_EQ(rec.GetEventCount(), 0u);
}

// 测试 输入录制器扩展：清空
TEST_F(InputRecorderExtTest, Clear) {
    InputRecorder rec;
    rec.StartRecording();
    rec.RecordEvent(65, 1, 0.0);
    rec.Clear();
    EXPECT_FALSE(rec.IsRecording());
    EXPECT_EQ(rec.GetEventCount(), 0u);
}

// 测试 输入录制器扩展：导出导入JSON往返
TEST_F(InputRecorderExtTest, ExportImportJSONRoundTrip) {
    InputRecorder rec;
    rec.StartRecording();
    rec.RecordEvent(87, 1, 1.000);    // W press
    rec.RecordEvent(87, 0, 1.500);    // W release
    rec.RecordEvent(68, 1, 2.000);    // D press
    rec.StopRecording();

    std::string json = rec.ExportJSON();
    EXPECT_FALSE(json.empty());

    InputRecorder rec2;
    EXPECT_TRUE(rec2.ImportJSON(json));
    EXPECT_EQ(rec2.GetEventCount(), 3u);

    const auto& events = rec2.GetEvents();
    EXPECT_EQ(events[0].key_code, 87);
    EXPECT_EQ(events[0].key_action, 1);
    EXPECT_NEAR(events[0].timestamp, 1.0, 0.01);
    EXPECT_EQ(events[2].key_code, 68);
}

// 测试 输入录制器扩展：导入空JSON
TEST_F(InputRecorderExtTest, ImportEmptyJSON) {
    InputRecorder rec;
    EXPECT_TRUE(rec.ImportJSON("[]"));
    EXPECT_EQ(rec.GetEventCount(), 0u);
}

// ============================================================
// InputPlayer
// ============================================================

class InputPlayerExtTest : public ::testing::Test {};

// 测试 输入玩家扩展：初始状态
TEST_F(InputPlayerExtTest, InitialState) {
    InputPlayer player;
    EXPECT_FALSE(player.IsPlaying());
    EXPECT_FALSE(player.IsFinished());
    EXPECT_EQ(player.GetCurrentIndex(), 0u);
    EXPECT_EQ(player.GetTotalEvents(), 0u);
}

// 测试 输入玩家扩展：加载从录制器
TEST_F(InputPlayerExtTest, LoadFromRecorder) {
    InputRecorder rec;
    rec.StartRecording();
    rec.RecordEvent(65, 1, 0.0);
    rec.RecordEvent(65, 0, 0.5);
    rec.StopRecording();

    InputPlayer player;
    player.Load(rec);
    EXPECT_EQ(player.GetTotalEvents(), 2u);
    EXPECT_FALSE(player.IsPlaying());
}

// 测试 输入玩家扩展：加载从向量
TEST_F(InputPlayerExtTest, LoadFromVector) {
    std::vector<InputEvent> events;
    events.push_back({0.1, 65, 1});
    events.push_back({0.2, 65, 0});

    InputPlayer player;
    player.Load(events);
    EXPECT_EQ(player.GetTotalEvents(), 2u);
}

// 测试 输入玩家扩展：启动且停止
TEST_F(InputPlayerExtTest, StartAndStop) {
    std::vector<InputEvent> events;
    events.push_back({0.1, 65, 1});

    InputPlayer player;
    player.Load(events);
    player.Start(10.0);
    EXPECT_TRUE(player.IsPlaying());

    player.Stop();
    EXPECT_FALSE(player.IsPlaying());
}
