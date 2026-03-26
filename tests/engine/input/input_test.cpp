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
