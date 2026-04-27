/**
 * @file debug_test.cpp
 * @brief Debug / dse::debug 调试系统单元测试
 *
 * 覆盖场景：
 * - Format 格式化字符串
 * - AppendFormatted 无占位符 / 单占位符 / 多占位符
 * - LogLevel 枚举与 SetLogLevel/GetLogLevel
 * - Debug::Init / ShutDown / CanLog 生命周期
 * - LogMessage 不同级别日志不崩溃
 */

#include <gtest/gtest.h>
#include "engine/base/debug.h"
#include <string>

using namespace dse::debug;

// ============================================================
// Format 格式化工具
// ============================================================

TEST(DebugFormatTest, 无占位符原样返回) {
    EXPECT_EQ(Format("hello world"), "hello world");
    EXPECT_EQ(Format(""), "");
}

TEST(DebugFormatTest, 单占位符替换) {
    EXPECT_EQ(Format("value: {}", 42), "value: 42");
    EXPECT_EQ(Format("name: {}", std::string("test")), "name: test");
}

TEST(DebugFormatTest, 多占位符按序替换) {
    EXPECT_EQ(Format("{} + {} = {}", 1, 2, 3), "1 + 2 = 3");
}

TEST(DebugFormatTest, 占位符多于参数时剩余保留) {
    // 只提供一个参数，第二个 {} 不会被替换
    std::string result = Format("a={} b={}", 1);
    EXPECT_EQ(result, "a=1 b={}");
}

TEST(DebugFormatTest, 空格式字符串) {
    EXPECT_EQ(Format(""), "");
}

TEST(DebugFormatTest, 整数和浮点数格式化) {
    std::string result = Format("int={} float={}", 10, 3.14);
    EXPECT_EQ(result, "int=10 float=3.14");
}

// ============================================================
// LogLevel 与 SetLogLevel/GetLogLevel
// ============================================================

TEST(DebugLogLevelTest, 默认级别为Info) {
    // SetLogLevel/GetLogLevel 操作的是全局静态变量，测试后需恢复
    LogLevel original = GetLogLevel();
    // Debug::Init 会设为 Info
    EXPECT_EQ(original, LogLevel::Info);
    // 恢复（以防其他测试依赖）
    SetLogLevel(original);
}

TEST(DebugLogLevelTest, SetLogLevel可修改级别) {
    LogLevel original = GetLogLevel();
    SetLogLevel(LogLevel::Warn);
    EXPECT_EQ(GetLogLevel(), LogLevel::Warn);
    SetLogLevel(LogLevel::Error);
    EXPECT_EQ(GetLogLevel(), LogLevel::Error);
    // 恢复
    SetLogLevel(original);
}

TEST(DebugLogLevelTest, SetLogLevel为Off) {
    LogLevel original = GetLogLevel();
    SetLogLevel(LogLevel::Off);
    EXPECT_EQ(GetLogLevel(), LogLevel::Off);
    SetLogLevel(original);
}

// ============================================================
// Debug 生命周期
// ============================================================

TEST(DebugLifecycleTest, Init后CanLog返回true) {
    Debug::Init();
    EXPECT_TRUE(Debug::CanLog());
    Debug::ShutDown();
}

TEST(DebugLifecycleTest, ShutDown后CanLog返回false) {
    Debug::Init();
    Debug::ShutDown();
    EXPECT_FALSE(Debug::CanLog());
}

TEST(DebugLifecycleTest, 未初始化时CanLog返回false) {
    // 先确保关闭状态
    Debug::ShutDown();
    EXPECT_FALSE(Debug::CanLog());
}

// ============================================================
// LogMessage 安全性
// ============================================================

TEST(DebugLogTest, 各级别LogMessage不崩溃) {
    Debug::Init();
    LogLevel original = GetLogLevel();
    SetLogLevel(LogLevel::Trace);

    EXPECT_NO_THROW(LogMessage(LogLevel::Trace, "trace msg"));
    EXPECT_NO_THROW(LogMessage(LogLevel::Info, "info msg"));
    EXPECT_NO_THROW(LogMessage(LogLevel::Warn, "warn msg"));
    EXPECT_NO_THROW(LogMessage(LogLevel::Error, "error msg"));

    SetLogLevel(original);
    Debug::ShutDown();
}

TEST(DebugLogTest, 未初始化时LogMessage不崩溃) {
    Debug::ShutDown();
    EXPECT_NO_THROW(LogMessage(LogLevel::Info, "should be suppressed"));
}
