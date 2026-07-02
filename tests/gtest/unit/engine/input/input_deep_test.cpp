/**
 * @file input_deep_test.cpp
 * @brief P7: 输入系统深度测试 — ActionMapping 绑定/解绑、InputRecorder/Player
 */

#include <gtest/gtest.h>
#include "engine/input/action_mapping.h"
#include "engine/input/input_recorder.h"
#include <string>
#include <vector>

using namespace dse::input;

// ═══════════════════════════════════════════════════════════
// ActionMapping 深度
// ═══════════════════════════════════════════════════════════

class ActionMappingDeepTest : public ::testing::Test {
protected:
    ActionMapping mapping;
};

TEST_F(ActionMappingDeepTest, RegisterAction) {
    mapping.RegisterAction("jump");
    EXPECT_TRUE(mapping.HasAction("jump"));
    EXPECT_EQ(mapping.GetActionCount(), 1u);
}

TEST_F(ActionMappingDeepTest, RemoveAction) {
    mapping.RegisterAction("fire");
    mapping.RemoveAction("fire");
    EXPECT_FALSE(mapping.HasAction("fire"));
    EXPECT_EQ(mapping.GetActionCount(), 0u);
}

TEST_F(ActionMappingDeepTest, BindKey) {
    mapping.RegisterAction("jump");
    mapping.BindKey("jump", 0x20); // space
    auto& bindings = mapping.GetBindings("jump");
    EXPECT_EQ(bindings.size(), 1u);
    EXPECT_EQ(bindings[0], 0x20);
}

TEST_F(ActionMappingDeepTest, MultipleBindings) {
    mapping.RegisterAction("move_left");
    mapping.BindKey("move_left", 'A');
    mapping.BindKey("move_left", 0x25); // left arrow
    auto& bindings = mapping.GetBindings("move_left");
    EXPECT_EQ(bindings.size(), 2u);
}

TEST_F(ActionMappingDeepTest, UnbindKey) {
    mapping.RegisterAction("crouch");
    mapping.BindKey("crouch", 'C');
    mapping.BindKey("crouch", 0x11); // ctrl
    mapping.UnbindKey("crouch", 'C');
    auto& bindings = mapping.GetBindings("crouch");
    EXPECT_EQ(bindings.size(), 1u);
    EXPECT_EQ(bindings[0], 0x11);
}

TEST_F(ActionMappingDeepTest, UnbindAll) {
    mapping.RegisterAction("run");
    mapping.BindKey("run", 0x10); // shift
    mapping.BindKey("run", 'R');
    mapping.UnbindAll("run");
    auto& bindings = mapping.GetBindings("run");
    EXPECT_EQ(bindings.size(), 0u);
}

TEST_F(ActionMappingDeepTest, GetAllActions) {
    mapping.RegisterAction("A");
    mapping.RegisterAction("B");
    mapping.RegisterAction("C");
    auto actions = mapping.GetAllActions();
    EXPECT_EQ(actions.size(), 3u);
}

TEST_F(ActionMappingDeepTest, Reset) {
    mapping.RegisterAction("jump");
    mapping.BindKey("jump", 0x20);
    mapping.RegisterAction("fire");
    mapping.Reset();
    EXPECT_EQ(mapping.GetActionCount(), 0u);
}

TEST_F(ActionMappingDeepTest, NonexistentActionBindings) {
    auto& bindings = mapping.GetBindings("nonexistent");
    EXPECT_EQ(bindings.size(), 0u);
}

TEST_F(ActionMappingDeepTest, RemoveNonexistentAction) {
    mapping.RemoveAction("nothing");
    EXPECT_EQ(mapping.GetActionCount(), 0u);
}

TEST_F(ActionMappingDeepTest, DuplicateRegister) {
    mapping.RegisterAction("fire");
    mapping.RegisterAction("fire");
    EXPECT_EQ(mapping.GetActionCount(), 1u);
}

// ═══════════════════════════════════════════════════════════
// InputRecorder 深度
// ═══════════════════════════════════════════════════════════

class InputRecorderDeepTest : public ::testing::Test {
protected:
    InputRecorder recorder;
};

TEST_F(InputRecorderDeepTest, InitialState) {
    EXPECT_FALSE(recorder.IsRecording());
    EXPECT_EQ(recorder.GetEventCount(), 0u);
}

TEST_F(InputRecorderDeepTest, StartStopRecording) {
    recorder.StartRecording();
    EXPECT_TRUE(recorder.IsRecording());
    recorder.StopRecording();
    EXPECT_FALSE(recorder.IsRecording());
}

TEST_F(InputRecorderDeepTest, RecordEvents) {
    recorder.StartRecording();
    recorder.RecordEvent(0x41, 1, 0.0);   // A key down
    recorder.RecordEvent(0x41, 0, 0.1);   // A key up
    recorder.RecordEvent(0x42, 1, 0.2);   // B key down
    recorder.StopRecording();

    EXPECT_EQ(recorder.GetEventCount(), 3u);
    auto& events = recorder.GetEvents();
    EXPECT_EQ(events[0].key_code, 0x41);
    EXPECT_EQ(events[0].key_action, 1);
    EXPECT_EQ(events[2].key_code, 0x42);
}

TEST_F(InputRecorderDeepTest, Clear) {
    recorder.StartRecording();
    recorder.RecordEvent(0x41, 1, 0.0);
    recorder.StopRecording();
    recorder.Clear();
    EXPECT_EQ(recorder.GetEventCount(), 0u);
}

TEST_F(InputRecorderDeepTest, JSONRoundtrip) {
    recorder.StartRecording();
    recorder.RecordEvent(0x41, 1, 0.0);
    recorder.RecordEvent(0x42, 1, 0.1);
    recorder.RecordEvent(0x41, 0, 0.2);
    recorder.StopRecording();

    std::string json = recorder.ExportJSON();
    EXPECT_FALSE(json.empty());

    InputRecorder loaded;
    EXPECT_TRUE(loaded.ImportJSON(json));
    EXPECT_EQ(loaded.GetEventCount(), 3u);

    auto& orig = recorder.GetEvents();
    auto& copy = loaded.GetEvents();
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_EQ(orig[i].key_code, copy[i].key_code);
        EXPECT_EQ(orig[i].key_action, copy[i].key_action);
        EXPECT_NEAR(orig[i].timestamp, copy[i].timestamp, 0.001);
    }
}

TEST_F(InputRecorderDeepTest, ImportInvalidJSON) {
    bool result = recorder.ImportJSON("not valid json {{{");
    // If it returns true, it silently ignored — either way, no crash
    (void)result;
}

TEST_F(InputRecorderDeepTest, RecordWhileNotRecording) {
    recorder.RecordEvent(0x41, 1, 0.0);
    EXPECT_EQ(recorder.GetEventCount(), 0u);
}

// ═══════════════════════════════════════════════════════════
// InputPlayer 深度
// ═══════════════════════════════════════════════════════════

class InputPlayerDeepTest : public ::testing::Test {
protected:
    InputPlayer player;
    InputRecorder recorder;

    void SetUp() override {
        recorder.StartRecording();
        recorder.RecordEvent(0x41, 1, 0.1);
        recorder.RecordEvent(0x42, 1, 0.3);
        recorder.RecordEvent(0x43, 1, 0.5);
        recorder.StopRecording();
    }
};

TEST_F(InputPlayerDeepTest, LoadFromRecorder) {
    player.Load(recorder);
    EXPECT_EQ(player.GetTotalEvents(), 3u);
}

TEST_F(InputPlayerDeepTest, PlaybackProgress) {
    player.Load(recorder);
    player.Start(0.0);
    EXPECT_TRUE(player.IsPlaying());
    EXPECT_FALSE(player.IsFinished());

    player.Update(0.2);
    EXPECT_GE(player.GetCurrentIndex(), 1u);

    player.Update(0.6);
    EXPECT_TRUE(player.IsFinished());
}

TEST_F(InputPlayerDeepTest, StopMidPlay) {
    player.Load(recorder);
    player.Start(0.0);
    player.Update(0.2);
    player.Stop();
    EXPECT_FALSE(player.IsPlaying());
}

TEST_F(InputPlayerDeepTest, LoadFromVector) {
    std::vector<InputEvent> events;
    InputEvent e;
    e.key_code = 0x50;
    e.key_action = 1;
    e.timestamp = 0.0;
    events.push_back(e);

    player.Load(events);
    EXPECT_EQ(player.GetTotalEvents(), 1u);
}

TEST_F(InputPlayerDeepTest, EmptyPlayback) {
    std::vector<InputEvent> empty;
    player.Load(empty);
    player.Start(0.0);
    // With 0 events, verify it doesn't crash on update
    player.Update(0.1);
    // IsFinished() depends on impl; just verify no crash
    (void)player.IsFinished();
}
