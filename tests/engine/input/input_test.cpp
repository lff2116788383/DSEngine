#include "catch/catch.hpp"
#include "engine/input/input.h"
#include "engine/input/key_code.h"

// 正向测试：记录按下事件后，应能查询到按下与持续按下状态。
TEST_CASE("Given_KeyDownRecorded_When_QueryKeyState_Then_DownStateIsTrue", "[engine][unit][input]") {
    Input::RecordKey(KEY_CODE_A, KEY_ACTION_DOWN);
    REQUIRE(Input::GetKey(KEY_CODE_A));
    REQUIRE(Input::GetKeyDown(KEY_CODE_A));
    REQUIRE_FALSE(Input::GetKeyUp(KEY_CODE_A));
}

// 边界测试：记录松开事件并调用 Update 后，按键状态应被清理。
TEST_CASE("Given_KeyReleased_When_Update_Then_KeyStateIsCleared", "[engine][unit][input]") {
    Input::RecordKey(KEY_CODE_B, KEY_ACTION_UP);
    REQUIRE(Input::GetKeyUp(KEY_CODE_B));

    Input::Update();
    REQUIRE_FALSE(Input::GetKey(KEY_CODE_B));
    REQUIRE_FALSE(Input::GetKeyDown(KEY_CODE_B));
    REQUIRE_FALSE(Input::GetKeyUp(KEY_CODE_B));
}

// 反向测试：查询未记录的按键时，所有状态查询都应返回 false。
TEST_CASE("Given_UnknownKey_When_Query_Then_AllStatesAreFalse", "[engine][unit][input]") {
    Input::Update();
    REQUIRE_FALSE(Input::GetKey(KEY_CODE_Z));
    REQUIRE_FALSE(Input::GetKeyDown(KEY_CODE_Z));
    REQUIRE_FALSE(Input::GetKeyUp(KEY_CODE_Z));
}

// 正向测试：连续快速按下两次鼠标，应能查询到双击状态。
TEST_CASE("Given_DoubleMouseDown_When_QueryDoubleClick_Then_ReturnTrue", "[engine][unit][input]") {
    Input::RecordKey(MOUSE_BUTTON_LEFT, KEY_ACTION_DOWN);
    Input::RecordKey(MOUSE_BUTTON_LEFT, KEY_ACTION_UP);
    Input::RecordKey(MOUSE_BUTTON_LEFT, KEY_ACTION_DOWN);
    REQUIRE(Input::GetDoubleClick(MOUSE_BUTTON_LEFT));
}

// 反向测试：仅按下一次鼠标，不应触发双击状态。
TEST_CASE("Given_SingleMouseDown_When_QueryDoubleClick_Then_ReturnFalse", "[engine][unit][input]") {
    Input::Update(); // 先清理状态
    Input::RecordKey(MOUSE_BUTTON_RIGHT, KEY_ACTION_DOWN);
    REQUIRE_FALSE(Input::GetDoubleClick(MOUSE_BUTTON_RIGHT));
}

// 正向测试：长按时间阈值为 0 时，按下按键应立即判定为长按。
TEST_CASE("Given_KeyPressed_When_QueryLongPressWithZeroDuration_Then_ReturnTrue", "[engine][unit][input]") {
    Input::RecordKey(KEY_CODE_C, KEY_ACTION_DOWN);
    REQUIRE(Input::GetLongPress(KEY_CODE_C, 0.0f));
}

// 反向测试：按键松开后，不应触发长按状态。
TEST_CASE("Given_KeyReleased_When_QueryLongPress_Then_ReturnFalse", "[engine][unit][input]") {
    Input::RecordKey(KEY_CODE_C, KEY_ACTION_DOWN);
    Input::RecordKey(KEY_CODE_C, KEY_ACTION_UP);
    REQUIRE_FALSE(Input::GetLongPress(KEY_CODE_C, 0.0f));
}

// 正向测试：鼠标发生移动，应能获取到正确的滑动增量。
TEST_CASE("Given_MouseMoved_When_QuerySwipeDelta_Then_ReturnDelta", "[engine][unit][input]") {
    Input::RecordMousePosition(10.0f, 15.0f);
    Input::RecordMousePosition(25.0f, 35.0f);
    glm::vec2 delta = Input::GetSwipeDelta();
    REQUIRE(delta.x == Approx(15.0f));
    REQUIRE(delta.y == Approx(20.0f));
}

// 边界测试：鼠标未移动（同一位置连续记录），滑动增量应为 0。
TEST_CASE("Given_MouseNotMoved_When_QuerySwipeDelta_Then_ReturnZero", "[engine][unit][input]") {
    Input::RecordMousePosition(50.0f, 50.0f);
    Input::RecordMousePosition(50.0f, 50.0f);
    glm::vec2 delta = Input::GetSwipeDelta();
    REQUIRE(delta.x == Approx(0.0f));
    REQUIRE(delta.y == Approx(0.0f));
}
