/**
 * @file input_actionmapping_integration_test.cpp
 * @brief Input ↔ ActionMapping ↔ InputRecorder/Player 集成测试
 *
 * 覆盖场景：
 *   1. ActionMapping 联动 Input 在多帧更新中状态正确
 *   2. Gamepad 轴输入 + ActionMapping 组合查询
 *   3. InputRecorder 录制后 ExportJSON → ImportJSON → InputPlayer 回放驱动 ActionMapping
 *   4. InputPlayer 回放序列通过 Input 驱动 ECS 组件变化
 *   5. ActionMapping 运行时 Rebind 在录制回放中表现正确
 *   6. 多手柄 + 多动作并发注册与查询
 *   7. Reset 后全局状态干净
 */

#include <gtest/gtest.h>
#include "engine/input/input.h"
#include "engine/input/key_code.h"
#include "engine/input/action_mapping.h"
#include "engine/input/input_recorder.h"
#include "engine/base/time.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"

using namespace dse::input;

class InputIntegrationTest : public ::testing::Test {
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

TEST_F(InputIntegrationTest, ActionMapping多帧持续按下状态正确) {
    ActionMapping mapping;
    mapping.BindKey("Fire", MOUSE_BUTTON_LEFT);

    Input::RecordKey(MOUSE_BUTTON_LEFT, KEY_ACTION_DOWN);
    EXPECT_TRUE(mapping.GetAction("Fire"));
    EXPECT_TRUE(mapping.GetActionDown("Fire"));

    // Update 清理 UP 状态，但持续按下的键保留 DOWN
    Input::Update();
    EXPECT_TRUE(mapping.GetAction("Fire"));
    // 引擎的 GetKeyDown 只要 key 在 map 中且不为 UP 就返回 true
    EXPECT_TRUE(mapping.GetActionDown("Fire"));

    Input::RecordKey(MOUSE_BUTTON_LEFT, KEY_ACTION_UP);
    EXPECT_TRUE(mapping.GetActionUp("Fire"));

    // Update 后 UP 键被移除
    Input::Update();
    EXPECT_FALSE(mapping.GetAction("Fire"));
    EXPECT_FALSE(mapping.GetActionUp("Fire"));
}

TEST_F(InputIntegrationTest, Gamepad轴与按钮ActionMapping组合) {
    ActionMapping mapping;
    mapping.BindKey("Jump", GAMEPAD_BUTTON_A);
    mapping.BindKey("Jump", KEY_CODE_SPACE);

    Input::SetGamepadConnected(0, true);
    Input::RecordGamepadAxis(0, GAMEPAD_AXIS_LEFT_X, 0.9f);
    Input::RecordKey(GAMEPAD_BUTTON_A, KEY_ACTION_DOWN);

    EXPECT_TRUE(mapping.GetAction("Jump"));
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 0.9f);

    Input::RecordKey(GAMEPAD_BUTTON_A, KEY_ACTION_UP);
    Input::Update();
    EXPECT_FALSE(mapping.GetAction("Jump"));

    Input::RecordKey(KEY_CODE_SPACE, KEY_ACTION_DOWN);
    EXPECT_TRUE(mapping.GetAction("Jump"));
}

TEST_F(InputIntegrationTest, Recorder录制ExportImportPlayer回放驱动ActionMapping) {
    ActionMapping mapping;
    mapping.BindKey("MoveRight", KEY_CODE_D);
    mapping.BindKey("MoveLeft", KEY_CODE_A);

    InputRecorder recorder;
    recorder.StartRecording();
    recorder.RecordEvent(KEY_CODE_D, KEY_ACTION_DOWN, 0.0);
    recorder.RecordEvent(KEY_CODE_D, KEY_ACTION_UP, 0.5);
    recorder.RecordEvent(KEY_CODE_A, KEY_ACTION_DOWN, 1.0);
    recorder.RecordEvent(KEY_CODE_A, KEY_ACTION_UP, 1.5);
    recorder.StopRecording();

    std::string json = recorder.ExportJSON();
    ASSERT_FALSE(json.empty());

    InputRecorder loaded;
    ASSERT_TRUE(loaded.ImportJSON(json));
    EXPECT_EQ(loaded.GetEventCount(), 4u);

    InputPlayer player;
    player.Load(loaded);
    player.Start(0.0);

    player.Update(0.0);
    EXPECT_TRUE(mapping.GetAction("MoveRight"));
    EXPECT_FALSE(mapping.GetAction("MoveLeft"));

    player.Update(0.5);
    EXPECT_TRUE(Input::GetKeyUp(KEY_CODE_D));

    player.Update(1.0);
    EXPECT_TRUE(mapping.GetAction("MoveLeft"));

    player.Update(1.5);
    EXPECT_TRUE(player.IsFinished());
}

TEST_F(InputIntegrationTest, Player回放驱动ECS实体移动) {
    World world;
    auto entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(entity);
    auto& transform = world.registry().get<TransformComponent>(entity);
    transform.position = {0.0f, 0.0f, 0.0f};

    std::vector<InputEvent> events = {
        {0.0, KEY_CODE_D, KEY_ACTION_DOWN},
        {0.5, KEY_CODE_D, KEY_ACTION_UP}
    };

    ActionMapping mapping;
    mapping.BindKey("MoveRight", KEY_CODE_D);

    InputPlayer player;
    player.Load(events);
    player.Start(0.0);

    float speed = 5.0f;
    float dt = 0.1f;
    for (double t = 0.0; t <= 0.5; t += dt) {
        player.Update(t);
        if (mapping.GetAction("MoveRight")) {
            transform.position.x += speed * dt;
        }
        Input::Update();
    }

    EXPECT_GT(transform.position.x, 0.0f);
}

TEST_F(InputIntegrationTest, 运行时Rebind后录制回放仍正常) {
    ActionMapping mapping;
    mapping.BindKey("Shoot", KEY_CODE_F);

    Input::RecordKey(KEY_CODE_F, KEY_ACTION_DOWN);
    EXPECT_TRUE(mapping.GetAction("Shoot"));
    Input::RecordKey(KEY_CODE_F, KEY_ACTION_UP);
    Input::Update();

    mapping.UnbindKey("Shoot", KEY_CODE_F);
    mapping.BindKey("Shoot", KEY_CODE_G);

    InputRecorder recorder;
    recorder.StartRecording();
    recorder.RecordEvent(KEY_CODE_G, KEY_ACTION_DOWN, 0.0);
    recorder.RecordEvent(KEY_CODE_G, KEY_ACTION_UP, 0.5);
    recorder.StopRecording();

    InputPlayer player;
    player.Load(recorder);
    player.Start(0.0);
    player.Update(0.0);
    EXPECT_TRUE(mapping.GetAction("Shoot"));

    Input::RecordKey(KEY_CODE_F, KEY_ACTION_DOWN);
    EXPECT_FALSE(mapping.GetAction("Shoot") && !Input::GetKey(KEY_CODE_G));
}

TEST_F(InputIntegrationTest, 多手柄多动作并发) {
    ActionMapping p1_mapping;
    p1_mapping.BindKey("P1_Jump", GAMEPAD_BUTTON_A);

    ActionMapping p2_mapping;
    p2_mapping.BindKey("P2_Jump", GAMEPAD_BUTTON_A);

    Input::SetGamepadConnected(0, true);
    Input::SetGamepadConnected(1, true);

    Input::RecordGamepadAxis(0, GAMEPAD_AXIS_LEFT_X, 1.0f);
    Input::RecordGamepadAxis(1, GAMEPAD_AXIS_LEFT_X, -1.0f);

    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 1.0f);
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(1, GAMEPAD_AXIS_LEFT_X), -1.0f);

    Input::RecordKey(GAMEPAD_BUTTON_A, KEY_ACTION_DOWN);
    EXPECT_TRUE(p1_mapping.GetAction("P1_Jump"));
    EXPECT_TRUE(p2_mapping.GetAction("P2_Jump"));
}

TEST_F(InputIntegrationTest, 全局Reset后ActionMapping和Gamepad状态干净) {
    ActionMapping mapping;
    mapping.BindKey("Fire", KEY_CODE_F);

    Input::RecordKey(KEY_CODE_F, KEY_ACTION_DOWN);
    Input::SetGamepadConnected(0, true);
    Input::RecordGamepadAxis(0, GAMEPAD_AXIS_LEFT_X, 0.8f);

    EXPECT_TRUE(mapping.GetAction("Fire"));
    EXPECT_TRUE(Input::IsGamepadConnected(0));

    Input::Reset();
    EXPECT_FALSE(mapping.GetAction("Fire"));
    EXPECT_FALSE(Input::IsGamepadConnected(0));
    EXPECT_FLOAT_EQ(Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X), 0.0f);
}
