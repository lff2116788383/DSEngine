/**
 * @file touch_test.cpp
 * @brief Touch 触摸抽象层单元测试
 *
 * 覆盖场景：
 * - RecordTouch 新建 / 移动 / 结束触点
 * - 多点触控独立追踪
 * - delta 位移计算
 * - Update 帧间相位推进与触点清理
 * - 查询接口（计数 / 索引 / ID / IsAnyTouchDown）
 * - 越界 / 未知 ID / 容量上限保护
 */

#include <gtest/gtest.h>
#include "engine/input/touch.h"

using dse::input::Touch;
using dse::input::TouchPhase;
using dse::input::TouchPoint;

class TouchTest : public ::testing::Test {
protected:
    void SetUp() override { Touch::Reset(); }
    void TearDown() override { Touch::Reset(); }
};

// 测试 触摸：初始无触点
TEST_F(TouchTest, InitiallyEmpty) {
    EXPECT_EQ(Touch::GetTouchCount(), 0);
    EXPECT_FALSE(Touch::IsAnyTouchActive());
    EXPECT_FALSE(Touch::IsAnyTouchDown());
}

// 测试 触摸：按下新建触点
TEST_F(TouchTest, BeganCreatesTouch) {
    Touch::RecordTouch(0, 10.0f, 20.0f, TouchPhase::Began);
    EXPECT_EQ(Touch::GetTouchCount(), 1);
    EXPECT_TRUE(Touch::IsAnyTouchDown());

    TouchPoint tp;
    ASSERT_TRUE(Touch::TryGetTouch(0, tp));
    EXPECT_EQ(tp.finger_id, 0);
    EXPECT_FLOAT_EQ(tp.position.x, 10.0f);
    EXPECT_FLOAT_EQ(tp.position.y, 20.0f);
    EXPECT_EQ(tp.phase, TouchPhase::Began);
    EXPECT_FLOAT_EQ(tp.delta.x, 0.0f);
    EXPECT_FLOAT_EQ(tp.delta.y, 0.0f);
}

// 测试 触摸：移动计算 delta
TEST_F(TouchTest, MoveComputesDelta) {
    Touch::RecordTouch(0, 10.0f, 20.0f, TouchPhase::Began);
    Touch::RecordTouch(0, 15.0f, 28.0f, TouchPhase::Moved);

    TouchPoint tp;
    ASSERT_TRUE(Touch::GetTouchById(0, tp));
    EXPECT_FLOAT_EQ(tp.position.x, 15.0f);
    EXPECT_FLOAT_EQ(tp.position.y, 28.0f);
    EXPECT_FLOAT_EQ(tp.delta.x, 5.0f);
    EXPECT_FLOAT_EQ(tp.delta.y, 8.0f);
    EXPECT_EQ(tp.phase, TouchPhase::Moved);
}

// 测试 触摸：多点独立追踪
TEST_F(TouchTest, MultiTouchIndependent) {
    Touch::RecordTouch(0, 0.0f, 0.0f, TouchPhase::Began);
    Touch::RecordTouch(1, 100.0f, 200.0f, TouchPhase::Began);
    EXPECT_EQ(Touch::GetTouchCount(), 2);

    TouchPoint a, b;
    ASSERT_TRUE(Touch::GetTouchById(0, a));
    ASSERT_TRUE(Touch::GetTouchById(1, b));
    EXPECT_FLOAT_EQ(a.position.x, 0.0f);
    EXPECT_FLOAT_EQ(b.position.x, 100.0f);
    EXPECT_FLOAT_EQ(b.position.y, 200.0f);
}

// 测试 触摸：Update 推进相位到 Stationary 并归零 delta
TEST_F(TouchTest, UpdateAdvancesToStationary) {
    Touch::RecordTouch(0, 10.0f, 20.0f, TouchPhase::Began);
    Touch::RecordTouch(0, 30.0f, 40.0f, TouchPhase::Moved);
    Touch::Update();

    TouchPoint tp;
    ASSERT_TRUE(Touch::GetTouchById(0, tp));
    EXPECT_EQ(tp.phase, TouchPhase::Stationary);
    EXPECT_FLOAT_EQ(tp.delta.x, 0.0f);
    EXPECT_FLOAT_EQ(tp.delta.y, 0.0f);
    EXPECT_FLOAT_EQ(tp.position.x, 30.0f);  // 位置保持
    EXPECT_FALSE(Touch::IsAnyTouchDown());  // Began 已过
}

// 测试 触摸：结束触点在 Update 后被清除
TEST_F(TouchTest, EndedTouchRemovedAfterUpdate) {
    Touch::RecordTouch(0, 10.0f, 20.0f, TouchPhase::Began);
    Touch::RecordTouch(0, 10.0f, 20.0f, TouchPhase::Ended);
    // Update 之前仍可查询到结束帧
    TouchPoint tp;
    ASSERT_TRUE(Touch::GetTouchById(0, tp));
    EXPECT_EQ(tp.phase, TouchPhase::Ended);

    Touch::Update();
    EXPECT_EQ(Touch::GetTouchCount(), 0);
    EXPECT_FALSE(Touch::GetTouchById(0, tp));
}

// 测试 触摸：取消触点在 Update 后被清除
TEST_F(TouchTest, CancelledTouchRemovedAfterUpdate) {
    Touch::RecordTouch(0, 1.0f, 1.0f, TouchPhase::Began);
    Touch::RecordTouch(0, 1.0f, 1.0f, TouchPhase::Cancelled);
    Touch::Update();
    EXPECT_EQ(Touch::GetTouchCount(), 0);
}

// 测试 触摸：结束一个触点保留其它触点
TEST_F(TouchTest, EndingOneKeepsOthers) {
    Touch::RecordTouch(0, 0.0f, 0.0f, TouchPhase::Began);
    Touch::RecordTouch(1, 5.0f, 5.0f, TouchPhase::Began);
    Touch::RecordTouch(0, 0.0f, 0.0f, TouchPhase::Ended);
    Touch::Update();

    EXPECT_EQ(Touch::GetTouchCount(), 1);
    TouchPoint tp;
    EXPECT_FALSE(Touch::GetTouchById(0, tp));
    ASSERT_TRUE(Touch::GetTouchById(1, tp));
    EXPECT_EQ(tp.phase, TouchPhase::Stationary);
}

// 测试 触摸：未知 ID 的移动/抬起被忽略（无对应 Began）
TEST_F(TouchTest, UnknownPointerMoveIgnored) {
    Touch::RecordTouch(7, 10.0f, 10.0f, TouchPhase::Moved);
    EXPECT_EQ(Touch::GetTouchCount(), 0);
    Touch::RecordTouch(7, 10.0f, 10.0f, TouchPhase::Ended);
    EXPECT_EQ(Touch::GetTouchCount(), 0);
}

// 测试 触摸：None 相位被忽略
TEST_F(TouchTest, NonePhaseIgnored) {
    Touch::RecordTouch(0, 1.0f, 1.0f, TouchPhase::None);
    EXPECT_EQ(Touch::GetTouchCount(), 0);
}

// 测试 触摸：索引越界安全
TEST_F(TouchTest, IndexOutOfRangeSafe) {
    TouchPoint tp;
    EXPECT_FALSE(Touch::TryGetTouch(-1, tp));
    EXPECT_FALSE(Touch::TryGetTouch(0, tp));
    Touch::RecordTouch(0, 0.0f, 0.0f, TouchPhase::Began);
    EXPECT_FALSE(Touch::TryGetTouch(1, tp));
    EXPECT_TRUE(Touch::TryGetTouch(0, tp));
}

// 测试 触摸：超过容量上限丢弃新触点
TEST_F(TouchTest, CapacityLimitDropsExtra) {
    for (int i = 0; i < Touch::kMaxTouchPoints + 5; ++i) {
        Touch::RecordTouch(i, static_cast<float>(i), 0.0f, TouchPhase::Began);
    }
    EXPECT_EQ(Touch::GetTouchCount(), Touch::kMaxTouchPoints);
}

// 测试 触摸：Reset 清空全部触点
TEST_F(TouchTest, ResetClearsAll) {
    Touch::RecordTouch(0, 0.0f, 0.0f, TouchPhase::Began);
    Touch::RecordTouch(1, 0.0f, 0.0f, TouchPhase::Began);
    Touch::Reset();
    EXPECT_EQ(Touch::GetTouchCount(), 0);
    EXPECT_FALSE(Touch::IsAnyTouchActive());
}
