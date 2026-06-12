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

TEST(DebugFormatTest, WithoutReturns) {
    EXPECT_EQ(Format("hello world"), "hello world");
    EXPECT_EQ(Format(""), "");
}

TEST(DebugFormatTest, Single) {
    EXPECT_EQ(Format("value: {}", 42), "value: 42");
    EXPECT_EQ(Format("name: {}", std::string("test")), "name: test");
}

TEST(DebugFormatTest, MultiBy) {
    EXPECT_EQ(Format("{} + {} = {}", 1, 2, 3), "1 + 2 = 3");
}

TEST(DebugFormatTest, MultiParametersWhen) {
    // 只提供一个参数，第二个 {} 不会被替换
    std::string result = Format("a={} b={}", 1);
    EXPECT_EQ(result, "a=1 b={}");
}

TEST(DebugFormatTest, Empty) {
    EXPECT_EQ(Format(""), "");
}

TEST(DebugFormatTest, Andpoint) {
    std::string result = Format("int={} float={}", 10, 3.14);
    EXPECT_EQ(result, "int=10 float=3.14");
}

// ============================================================
// LogLevel 与 SetLogLevel/GetLogLevel
// ============================================================

TEST(DebugLogLevelTest, DefaultIsInfo) {
    // SetLogLevel/GetLogLevel 操作的是全局静态变量，测试后需恢复
    LogLevel original = GetLogLevel();
    // Debug::Init 会设为 Info
    EXPECT_EQ(original, LogLevel::Info);
    // 恢复（以防其他测试依赖）
    SetLogLevel(original);
}

TEST(DebugLogLevelTest, SetLogLevelModifiableLevel) {
    LogLevel original = GetLogLevel();
    SetLogLevel(LogLevel::Warn);
    EXPECT_EQ(GetLogLevel(), LogLevel::Warn);
    SetLogLevel(LogLevel::Error);
    EXPECT_EQ(GetLogLevel(), LogLevel::Error);
    // 恢复
    SetLogLevel(original);
}

TEST(DebugLogLevelTest, SetLogLevelIsOff) {
    LogLevel original = GetLogLevel();
    SetLogLevel(LogLevel::Off);
    EXPECT_EQ(GetLogLevel(), LogLevel::Off);
    SetLogLevel(original);
}

// ============================================================
// Debug 生命周期
// ============================================================

TEST(DebugLifecycleTest, InitAfterCanLogReturnstrue) {
    Debug::Init();
    EXPECT_TRUE(Debug::CanLog());
    Debug::ShutDown();
}

TEST(DebugLifecycleTest, ShutDownAfterCanLogReturnsfalse) {
    Debug::Init();
    Debug::ShutDown();
    EXPECT_FALSE(Debug::CanLog());
}

TEST(DebugLifecycleTest, WhenNotInitializedCanLogReturnsfalse) {
    // 先确保关闭状态
    Debug::ShutDown();
    EXPECT_FALSE(Debug::CanLog());
}

// ============================================================
// LogMessage 安全性
// ============================================================

TEST(DebugLogTest, LogMessageDoesNotCrash) {
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

TEST(DebugLogTest, WhenNotInitializedLogMessageDoesNotCrash) {
    Debug::ShutDown();
    EXPECT_NO_THROW(LogMessage(LogLevel::Info, "should be suppressed"));
}
