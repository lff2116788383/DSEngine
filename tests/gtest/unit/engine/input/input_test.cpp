/**
 * @file input_test.cpp
 * @brief Input 输入系统单元测试
 *
 * 覆盖场景：
 * - RecordKey / GetKey / GetKeyDown / GetKeyUp 按键状态
 * - Update 帧间状态清理
 * - GetMouseButton 委托到 GetKey
 * - Reset 全局重置
 * - Gamepad 按钮 / 摇杆轴 / 连接状态 / 死区
 * - ActionMapping 动作注册 / 绑定 / 查询 / 运行时 rebind
 * - InputRecorder 录制 / 回放 / JSON 导入导出
 *
 * 注意：mousePosition() / mouseScroll() 等内联访问器引用私有静态成员，
 *       在 DLL 边界上不可用，因此不在单元测试中直接验证。
 */

#include <gtest/gtest.h>
#include "engine/input/input.h"
#include "engine/input/key_code.h"
#include "engine/input/action_mapping.h"
#include "engine/input/input_recorder.h"
#include "engine/base/time.h"

class InputTest : public ::testing::Test {
protected:
    void SetUp() override {
        Time::Init();
        Input::Reset();
    }
    void TearDown() override {
        Input::Reset();
        Time::Reset();
    }
};

// ============================================================
// 键盘输入
// ============================================================

TEST_F(InputTest, 初始状态无按键) {
    EXPECT_FALSE(Input::GetKey(KEY_CODE_A));
    EXPECT_FALSE(Input::GetKeyDown(KEY_CODE_A));
    EXPECT_FALSE(Input::GetKeyUp(KEY_CODE_A));
}

TEST_F(InputTest, RecordKey按下后GetKey为true) {
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_DOWN);
    EXPECT_TRUE(Input::GetKey(KEY_CODE_A));
}

TEST_F(InputTest, RecordKey持续按下GetKey为true) {
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_REPEAT);
    EXPECT_TRUE(Input::GetKey(KEY_CODE_A));
}

TEST_F(InputTest, RecordKey松开后GetKeyUp为true) {
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_DOWN);
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_UP);
    EXPECT_TRUE(Input::GetKeyUp(KEY_CODE_A));
}

TEST_F(InputTest, Update后清空Up状态) {
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_DOWN);
    Input::Update();
    // 持续按下的键仍存在
    EXPECT_TRUE(Input::GetKey(KEY_CODE_A));
}

TEST_F(InputTest, Update清空松开的键) {
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_DOWN);
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_UP);
    Input::Update();
    EXPECT_FALSE(Input::GetKey(KEY_CODE_A));
}

// ============================================================
// 鼠标输入（委托到键盘接口）
// ============================================================

TEST_F(InputTest, GetMouseButton按下为true) {
    Input::RecordKey(MOUSE_BUTTON_LEFT, KEY_ACTION_DOWN);
    EXPECT_TRUE(Input::GetMouseButton(0));
}

// ============================================================
// Reset
// ============================================================

TEST_F(InputTest, Reset清空所有按键状态) {
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_DOWN);
    Input::RecordKey(KEY_CODE_W, KEY_ACTION_DOWN);
    Input::Reset();
    EXPECT_FALSE(Input::GetKey(KEY_CODE_A));
    EXPECT_FALSE(Input::GetKey(KEY_CODE_W));
}

// ============================================================
// Gamepad 按钮
// ============================================================

TEST_F(InputTest, Gamepad按钮按下和松开) {
    Input::RecordKey(GAMEPAD_BUTTON_A, KEY_ACTION_DOWN);
    EXPECT_TRUE(Input::GetKey(GAMEPAD_BUTTON_A));
    Input::RecordKey(GAMEPAD_BUTTON_A, KEY_ACTION_UP);
    EXPECT_TRUE(Input::GetKeyUp(GAMEPAD_BUTTON_A));
}

TEST_F(InputTest, Gamepad多按钮同时按下) {
    Input::RecordKey(GAMEPAD_BUTTON_A, KEY_ACTION_DOWN);
    Input::RecordKey(GAMEPAD_BUTTON_B, KEY_ACTION_DOWN);
    EXPECT_TRUE(Input::GetKey(GAMEPAD_BUTTON_A));
    EXPECT_TRUE(Input::GetKey(GAMEPAD_BUTTON_B));
}

TEST_F(InputTest, Gamepad方向键) {
    Input::RecordKey(GAMEPAD_BUTTON_DPAD_UP, KEY_ACTION_DOWN);
    EXPECT_TRUE(Input::GetKey(GAMEPAD_BUTTON_DPAD_UP));
    EXPECT_FALSE(Input::GetKey(GAMEPAD_BUTTON_DPAD_DOWN));
}

// ============================================================
// Gamepad 摇杆轴
// ============================================================

TEST_F(InputTest, Gamepad轴初始为零) {
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 0.0f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_Y), 0.0f);
}

TEST_F(InputTest, Gamepad轴记录和读取) {
    Input::RecordGamepadAxis(0, GAMEPAD_AXIS_LEFT_X, 0.8f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 0.8f);
}

TEST_F(InputTest, Gamepad轴死区过滤) {
    Input::SetGamepadDeadZone(0.2f);
    Input::RecordGamepadAxis(0, GAMEPAD_AXIS_LEFT_X, 0.1f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 0.0f);
    Input::RecordGamepadAxis(0, GAMEPAD_AXIS_LEFT_X, 0.3f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 0.3f);
}

TEST_F(InputTest, Gamepad多手柄独立) {
    Input::RecordGamepadAxis(0, GAMEPAD_AXIS_LEFT_X, 0.5f);
    Input::RecordGamepadAxis(1, GAMEPAD_AXIS_LEFT_X, -0.5f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 0.5f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(1, GAMEPAD_AXIS_LEFT_X), -0.5f);
}

TEST_F(InputTest, Gamepad越界ID安全) {
    EXPECT_NO_THROW(Input::RecordGamepadAxis(-1, 0, 1.0f));
    EXPECT_NO_THROW(Input::RecordGamepadAxis(99, 0, 1.0f));
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(-1, 0), 0.0f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(99, 0), 0.0f);
}

// ============================================================
// Gamepad 连接状态
// ============================================================

TEST_F(InputTest, Gamepad初始未连接) {
    EXPECT_FALSE(Input::IsGamepadConnected(0));
}

TEST_F(InputTest, Gamepad连接和断开) {
    Input::SetGamepadConnected(0, true);
    EXPECT_TRUE(Input::IsGamepadConnected(0));
    Input::SetGamepadConnected(0, false);
    EXPECT_FALSE(Input::IsGamepadConnected(0));
}

TEST_F(InputTest, Reset清除Gamepad状态) {
    Input::SetGamepadConnected(0, true);
    Input::RecordGamepadAxis(0, GAMEPAD_AXIS_LEFT_X, 1.0f);
    Input::Reset();
    EXPECT_FALSE(Input::IsGamepadConnected(0));
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 0.0f);
}

// ============================================================
// ActionMapping 动作映射
// ============================================================

using dse::input::ActionMapping;

TEST(ActionMappingTest, 注册和查询动作) {
    ActionMapping mapping;
    mapping.RegisterAction("Jump");
    EXPECT_TRUE(mapping.HasAction("Jump"));
    EXPECT_FALSE(mapping.HasAction("Fire"));
}

TEST(ActionMappingTest, 移除动作) {
    ActionMapping mapping;
    mapping.RegisterAction("Jump");
    mapping.RemoveAction("Jump");
    EXPECT_FALSE(mapping.HasAction("Jump"));
}

TEST(ActionMappingTest, 绑定键位) {
    ActionMapping mapping;
    mapping.RegisterAction("Jump");
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    const auto& bindings = mapping.GetBindings("Jump");
    ASSERT_EQ(bindings.size(), 1u);
    EXPECT_EQ(bindings[0], KEY_CODE_SPACE);
}

TEST(ActionMappingTest, 多键绑定同一动作) {
    ActionMapping mapping;
    mapping.RegisterAction("Jump");
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    mapping.BindKey("Jump", GAMEPAD_BUTTON_A);
    EXPECT_EQ(mapping.GetBindings("Jump").size(), 2u);
}

TEST(ActionMappingTest, 重复绑定不重复添加) {
    ActionMapping mapping;
    mapping.RegisterAction("Jump");
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    EXPECT_EQ(mapping.GetBindings("Jump").size(), 1u);
}

class ActionMappingInputTest : public ::testing::Test {
protected:
    void SetUp() override {
        Time::Init();
        Input::Reset();
    }
    void TearDown() override {
        Input::Reset();
        Time::Reset();
    }
};

TEST_F(ActionMappingInputTest, GetAction反映按键状态) {
    ActionMapping mapping;
    mapping.BindKey("Fire", MOUSE_BUTTON_LEFT);
    EXPECT_FALSE(mapping.GetAction("Fire"));
    Input::RecordKey(MOUSE_BUTTON_LEFT, KEY_ACTION_DOWN);
    EXPECT_TRUE(mapping.GetAction("Fire"));
}

TEST_F(ActionMappingInputTest, 多键OR逻辑) {
    ActionMapping mapping;
    mapping.BindKey("Move", KEY_CODE_W);
    mapping.BindKey("Move", GAMEPAD_BUTTON_DPAD_UP);
    Input::RecordKey(GAMEPAD_BUTTON_DPAD_UP, KEY_ACTION_DOWN);
    EXPECT_TRUE(mapping.GetAction("Move"));
}

TEST_F(ActionMappingInputTest, GetActionDown检测按下瞬间) {
    ActionMapping mapping;
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    Input::RecordKey(KEY_CODE_SPACE, KEY_ACTION_DOWN);
    EXPECT_TRUE(mapping.GetActionDown("Jump"));
}

TEST_F(ActionMappingInputTest, GetActionUp检测松开瞬间) {
    ActionMapping mapping;
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    Input::RecordKey(KEY_CODE_SPACE, KEY_ACTION_DOWN);
    Input::RecordKey(KEY_CODE_SPACE, KEY_ACTION_UP);
    EXPECT_TRUE(mapping.GetActionUp("Jump"));
}

TEST_F(ActionMappingInputTest, UnbindKey运行时解绑) {
    ActionMapping mapping;
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    mapping.UnbindKey("Jump", KEY_CODE_SPACE);
    Input::RecordKey(KEY_CODE_SPACE, KEY_ACTION_DOWN);
    EXPECT_FALSE(mapping.GetAction("Jump"));
}

TEST_F(ActionMappingInputTest, 运行时Rebind切换键位) {
    ActionMapping mapping;
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    mapping.UnbindKey("Jump", KEY_CODE_SPACE);
    mapping.BindKey("Jump", KEY_CODE_ENTER);
    Input::RecordKey(KEY_CODE_SPACE, KEY_ACTION_DOWN);
    EXPECT_FALSE(mapping.GetAction("Jump"));
    Input::RecordKey(KEY_CODE_ENTER, KEY_ACTION_DOWN);
    EXPECT_TRUE(mapping.GetAction("Jump"));
}

TEST(ActionMappingTest, GetAllActions列出所有动作) {
    ActionMapping mapping;
    mapping.RegisterAction("Jump");
    mapping.RegisterAction("Fire");
    mapping.RegisterAction("Crouch");
    EXPECT_EQ(mapping.GetActionCount(), 3u);
    auto actions = mapping.GetAllActions();
    EXPECT_EQ(actions.size(), 3u);
}

TEST(ActionMappingTest, Reset清除所有动作) {
    ActionMapping mapping;
    mapping.RegisterAction("Jump");
    mapping.BindKey("Jump", KEY_CODE_SPACE);
    mapping.Reset();
    EXPECT_EQ(mapping.GetActionCount(), 0u);
    EXPECT_FALSE(mapping.HasAction("Jump"));
}

TEST(ActionMappingTest, 未注册动作GetBindings返回空) {
    ActionMapping mapping;
    EXPECT_TRUE(mapping.GetBindings("NonExistent").empty());
}

// ============================================================
// InputRecorder 录制
// ============================================================

using dse::input::InputRecorder;
using dse::input::InputPlayer;
using dse::input::InputEvent;

TEST(InputRecorderTest, 初始状态未录制) {
    InputRecorder recorder;
    EXPECT_FALSE(recorder.IsRecording());
    EXPECT_EQ(recorder.GetEventCount(), 0u);
}

TEST(InputRecorderTest, 开始和停止录制) {
    InputRecorder recorder;
    recorder.StartRecording();
    EXPECT_TRUE(recorder.IsRecording());
    recorder.StopRecording();
    EXPECT_FALSE(recorder.IsRecording());
}

TEST(InputRecorderTest, 录制事件) {
    InputRecorder recorder;
    recorder.StartRecording();
    recorder.RecordEvent(KEY_CODE_A, KEY_ACTION_DOWN, 0.0);
    recorder.RecordEvent(KEY_CODE_A, KEY_ACTION_UP, 0.5);
    EXPECT_EQ(recorder.GetEventCount(), 2u);
    EXPECT_EQ(recorder.GetEvents()[0].key_code, KEY_CODE_A);
    EXPECT_EQ(recorder.GetEvents()[1].key_action, KEY_ACTION_UP);
}

TEST(InputRecorderTest, 未录制时忽略事件) {
    InputRecorder recorder;
    recorder.RecordEvent(KEY_CODE_A, KEY_ACTION_DOWN, 0.0);
    EXPECT_EQ(recorder.GetEventCount(), 0u);
}

TEST(InputRecorderTest, Clear清除事件) {
    InputRecorder recorder;
    recorder.StartRecording();
    recorder.RecordEvent(KEY_CODE_A, KEY_ACTION_DOWN, 0.0);
    recorder.Clear();
    EXPECT_EQ(recorder.GetEventCount(), 0u);
    EXPECT_FALSE(recorder.IsRecording());
}

TEST(InputRecorderTest, ExportJSON格式正确) {
    InputRecorder recorder;
    recorder.StartRecording();
    recorder.RecordEvent(KEY_CODE_A, KEY_ACTION_DOWN, 0.0);
    recorder.RecordEvent(KEY_CODE_A, KEY_ACTION_UP, 1.5);
    std::string json = recorder.ExportJSON();
    EXPECT_EQ(json.front(), '[');
    EXPECT_EQ(json.back(), ']');
    EXPECT_NE(json.find("\"ts\":"), std::string::npos);
    EXPECT_NE(json.find("\"key\":"), std::string::npos);
    EXPECT_NE(json.find("\"action\":"), std::string::npos);
}

TEST(InputRecorderTest, ImportJSON还原事件) {
    InputRecorder recorder;
    recorder.StartRecording();
    recorder.RecordEvent(KEY_CODE_W, KEY_ACTION_DOWN, 0.1);
    recorder.RecordEvent(KEY_CODE_W, KEY_ACTION_UP, 0.5);
    std::string json = recorder.ExportJSON();

    InputRecorder loaded;
    EXPECT_TRUE(loaded.ImportJSON(json));
    EXPECT_EQ(loaded.GetEventCount(), 2u);
    EXPECT_EQ(loaded.GetEvents()[0].key_code, KEY_CODE_W);
    EXPECT_NEAR(loaded.GetEvents()[0].timestamp, 0.1, 0.01);
    EXPECT_EQ(loaded.GetEvents()[1].key_action, KEY_ACTION_UP);
}

// ============================================================
// InputPlayer 回放
// ============================================================

class InputPlayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Time::Init();
        Input::Reset();
    }
    void TearDown() override {
        Input::Reset();
        Time::Reset();
    }
};

TEST_F(InputPlayerTest, 初始状态未播放) {
    InputPlayer player;
    EXPECT_FALSE(player.IsPlaying());
    EXPECT_FALSE(player.IsFinished());
}

TEST_F(InputPlayerTest, 加载并开始播放) {
    std::vector<InputEvent> events = {{0.0, KEY_CODE_A, KEY_ACTION_DOWN}};
    InputPlayer player;
    player.Load(events);
    player.Start(0.0);
    EXPECT_TRUE(player.IsPlaying());
}

TEST_F(InputPlayerTest, Update回放事件到Input) {
    std::vector<InputEvent> events = {
        {0.0, KEY_CODE_A, KEY_ACTION_DOWN},
        {1.0, KEY_CODE_A, KEY_ACTION_UP}
    };
    InputPlayer player;
    player.Load(events);
    player.Start(0.0);
    player.Update(0.0);
    EXPECT_TRUE(Input::GetKey(KEY_CODE_A));
    player.Update(1.0);
    EXPECT_TRUE(Input::GetKeyUp(KEY_CODE_A));
}

TEST_F(InputPlayerTest, 播放完毕IsFinished为true) {
    std::vector<InputEvent> events = {{0.0, KEY_CODE_A, KEY_ACTION_DOWN}};
    InputPlayer player;
    player.Load(events);
    player.Start(0.0);
    player.Update(0.0);
    EXPECT_TRUE(player.IsFinished());
}

TEST_F(InputPlayerTest, Stop中途停止) {
    std::vector<InputEvent> events = {
        {0.0, KEY_CODE_A, KEY_ACTION_DOWN},
        {10.0, KEY_CODE_A, KEY_ACTION_UP}
    };
    InputPlayer player;
    player.Load(events);
    player.Start(0.0);
    player.Update(0.0);
    player.Stop();
    EXPECT_FALSE(player.IsPlaying());
    EXPECT_FALSE(player.IsFinished());
}

TEST_F(InputPlayerTest, 从Recorder加载) {
    InputRecorder recorder;
    recorder.StartRecording();
    recorder.RecordEvent(KEY_CODE_W, KEY_ACTION_DOWN, 0.0);
    recorder.StopRecording();

    InputPlayer player;
    player.Load(recorder);
    player.Start(0.0);
    player.Update(0.0);
    EXPECT_TRUE(Input::GetKey(KEY_CODE_W));
    EXPECT_TRUE(player.IsFinished());
}
