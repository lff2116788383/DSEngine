/**
 * @file screen_test.cpp
 * @brief Screen 全局尺寸/宽高比/Reset 测试
 */

#include <gtest/gtest.h>
#include "engine/platform/screen.h"
#include <cmath>

class ScreenTest : public ::testing::Test {
protected:
    void SetUp() override { Screen::Reset(); }
    void TearDown() override { Screen::Reset(); }
};

// 测试 屏幕：默认值
TEST_F(ScreenTest, DefaultValues) {
    EXPECT_EQ(Screen::width(), 0);
    EXPECT_EQ(Screen::height(), 0);
    EXPECT_FLOAT_EQ(Screen::aspect_ratio(), 0.0f);
}

// 测试 屏幕：设置宽度高度
TEST_F(ScreenTest, SetWidthHeight) {
    Screen::set_width_height(1920, 1080);
    EXPECT_EQ(Screen::width(), 1920);
    EXPECT_EQ(Screen::height(), 1080);
    EXPECT_NEAR(Screen::aspect_ratio(), 16.0f / 9.0f, 0.01f);
}

// 测试 屏幕：设置宽度然后高度
TEST_F(ScreenTest, SetWidthThenHeight) {
    Screen::set_width_height(800, 600);
    EXPECT_EQ(Screen::width(), 800);
    EXPECT_EQ(Screen::height(), 600);
    EXPECT_NEAR(Screen::aspect_ratio(), 800.0f / 600.0f, 0.01f);

    Screen::set_width(1024);
    EXPECT_EQ(Screen::width(), 1024);
    EXPECT_NEAR(Screen::aspect_ratio(), 1024.0f / 600.0f, 0.01f);

    Screen::set_height(768);
    EXPECT_EQ(Screen::height(), 768);
    EXPECT_NEAR(Screen::aspect_ratio(), 1024.0f / 768.0f, 0.01f);
}

// 测试 屏幕：方形宽高比
TEST_F(ScreenTest, SquareAspect) {
    Screen::set_width_height(512, 512);
    EXPECT_FLOAT_EQ(Screen::aspect_ratio(), 1.0f);
}

// 测试 屏幕：重置
TEST_F(ScreenTest, Reset) {
    Screen::set_width_height(2560, 1440);
    Screen::Reset();
    EXPECT_EQ(Screen::width(), 0);
    EXPECT_EQ(Screen::height(), 0);
    EXPECT_FLOAT_EQ(Screen::aspect_ratio(), 0.0f);
}

// 测试 屏幕：宽度变更更新宽高比
TEST_F(ScreenTest, WidthChangeUpdatesAspect) {
    Screen::set_width_height(1920, 1080);
    float old_ratio = Screen::aspect_ratio();

    Screen::set_width(3840);
    EXPECT_GT(Screen::aspect_ratio(), old_ratio);
    EXPECT_NEAR(Screen::aspect_ratio(), 3840.0f / 1080.0f, 0.01f);
}

// 测试 屏幕：高度变更更新宽高比
TEST_F(ScreenTest, HeightChangeUpdatesAspect) {
    Screen::set_width_height(1920, 1080);
    float old_ratio = Screen::aspect_ratio();

    Screen::set_height(2160);
    EXPECT_LT(Screen::aspect_ratio(), old_ratio);
    EXPECT_NEAR(Screen::aspect_ratio(), 1920.0f / 2160.0f, 0.01f);
}
