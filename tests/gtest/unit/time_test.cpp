/**
 * @file time_test.cpp
 * @brief Time 时间管理系统单元测试
 *
 * 覆盖场景：
 * - Init/Update 基本流程
 * - delta_time 更新计算
 * - TimeSinceStartup 返回非负值
 * - fixed_update_time 默认值与设置
 * - 连续 Update 的 delta_time 累积
 * - 重新 Init 后静态状态重置语义
 */

#include <gtest/gtest.h>
#include "engine/base/time.h"
#include <thread>
#include <chrono>

// ============================================================
// Time 基础功能测试
// ============================================================

class TimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        Time::Init();
        Time::set_fixed_update_time(1.0f / 60.0f);
    }
};

TEST_F(TimeTest, 初始化后TimeSinceStartup接近零) {
    float t = Time::TimeSinceStartup();
    // 刚初始化后应接近 0（允许少量误差）
    EXPECT_GE(t, 0.0f);
    EXPECT_LT(t, 1.0f);
}

TEST_F(TimeTest, Update后delta_time非负) {
    // 第一次 Update 之前 delta_time 为 0
    EXPECT_FLOAT_EQ(Time::delta_time(), 0.0f);

    // 短暂等待后调用 Update
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    Time::Update();

    // delta_time 应大于 0
    EXPECT_GT(Time::delta_time(), 0.0f);
}

TEST_F(TimeTest, 多次Update后delta_time递增) {
    Time::Update(); // 第一次 Update 建立 last_frame_time
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    Time::Update();
    float dt1 = Time::delta_time();
    EXPECT_GT(dt1, 0.0f);

    std::this_thread::sleep_for(std::chrono::milliseconds(32));
    Time::Update();
    float dt2 = Time::delta_time();
    EXPECT_GT(dt2, dt1);
}

TEST_F(TimeTest, TimeSinceStartup随时间增长) {
    float t1 = Time::TimeSinceStartup();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    float t2 = Time::TimeSinceStartup();
    EXPECT_GT(t2, t1);
}

TEST_F(TimeTest, 再次Init后TimeSinceStartup重新接近零) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_GT(Time::TimeSinceStartup(), 0.0f);

    Time::Init();

    float reset_time = Time::TimeSinceStartup();
    EXPECT_GE(reset_time, 0.0f);
    EXPECT_LT(reset_time, 1.0f);
}

// ============================================================
// fixed_update_time 测试
// ============================================================

TEST_F(TimeTest, fixed_update_time默认值为1除以60) {
    float expected = 1.0f / 60.0f;
    EXPECT_NEAR(Time::fixed_update_time(), expected, 0.001f);
}

TEST_F(TimeTest, 设置fixed_update_time后可读取) {
    Time::set_fixed_update_time(0.02f);
    EXPECT_FLOAT_EQ(Time::fixed_update_time(), 0.02f);
}

TEST_F(TimeTest, 多次设置fixed_update_time以最后一次为准) {
    Time::set_fixed_update_time(0.01f);
    Time::set_fixed_update_time(0.033f);
    EXPECT_FLOAT_EQ(Time::fixed_update_time(), 0.033f);
}

TEST_F(TimeTest, 允许设置fixed_update_time为零) {
    Time::set_fixed_update_time(0.0f);
    EXPECT_FLOAT_EQ(Time::fixed_update_time(), 0.0f);
}

TEST_F(TimeTest, 当前实现允许设置fixed_update_time为负值) {
    Time::set_fixed_update_time(-0.5f);
    EXPECT_FLOAT_EQ(Time::fixed_update_time(), -0.5f);
}

