/**
 * @file input_test.cpp
 * @brief Input 输入系统单元测试
 *
 * 覆盖场景：
 * - RecordKey / GetKey / GetKeyDown / GetKeyUp 按键状态
 * - Update 帧间状态清理
 * - GetMouseButton 委托到 GetKey
 * - Reset 全局重置
 *
 * 注意：mousePosition() / mouseScroll() 等内联访问器引用私有静态成员，
 *       在 DLL 边界上不可用，因此不在单元测试中直接验证。
 */

#include <gtest/gtest.h>
#include "engine/input/input.h"
#include "engine/input/key_code.h"
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
