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

// 测试 调试格式：无返回
TEST(DebugFormatTest, WithoutReturns) {
    EXPECT_EQ(Format("hello world"), "hello world");
    EXPECT_EQ(Format(""), "");
}

// 测试 调试格式：单一
TEST(DebugFormatTest, Single) {
    EXPECT_EQ(Format("value: {}", 42), "value: 42");
    EXPECT_EQ(Format("name: {}", std::string("test")), "name: test");
}

// 测试 调试格式：多按
TEST(DebugFormatTest, MultiBy) {
    EXPECT_EQ(Format("{} + {} = {}", 1, 2, 3), "1 + 2 = 3");
}

// 测试 调试格式：多参数当
TEST(DebugFormatTest, MultiParametersWhen) {
    // 只提供一个参数，第二个 {} 不会被替换
    std::string result = Format("a={} b={}", 1);
    EXPECT_EQ(result, "a=1 b={}");
}

// 测试 调试格式：空
TEST(DebugFormatTest, Empty) {
    EXPECT_EQ(Format(""), "");
}

// 测试 调试格式：Andpoint
TEST(DebugFormatTest, Andpoint) {
    std::string result = Format("int={} float={}", 10, 3.14);
    EXPECT_EQ(result, "int=10 float=3.14");
}

// ============================================================
// LogLevel 与 SetLogLevel/GetLogLevel
// ============================================================

// 测试 调试日志级别：默认为信息
TEST(DebugLogLevelTest, DefaultIsInfo) {
    // SetLogLevel/GetLogLevel 操作的是全局静态变量，测试后需恢复
    LogLevel original = GetLogLevel();
    // Debug::Init 会设为 Info
    EXPECT_EQ(original, LogLevel::Info);
    // 恢复（以防其他测试依赖）
    SetLogLevel(original);
}

// 测试 调试日志级别：设置日志级别Modifiable级别
TEST(DebugLogLevelTest, SetLogLevelModifiableLevel) {
    LogLevel original = GetLogLevel();
    SetLogLevel(LogLevel::Warn);
    EXPECT_EQ(GetLogLevel(), LogLevel::Warn);
    SetLogLevel(LogLevel::Error);
    EXPECT_EQ(GetLogLevel(), LogLevel::Error);
    // 恢复
    SetLogLevel(original);
}

// 测试 调试日志级别：设置日志级别为关闭
TEST(DebugLogLevelTest, SetLogLevelIsOff) {
    LogLevel original = GetLogLevel();
    SetLogLevel(LogLevel::Off);
    EXPECT_EQ(GetLogLevel(), LogLevel::Off);
    SetLogLevel(original);
}

// ============================================================
// Debug 生命周期
// ============================================================

// 测试 调试生命周期：初始化之后能够日志返回true
TEST(DebugLifecycleTest, InitAfterCanLogReturnstrue) {
    Debug::Init();
    EXPECT_TRUE(Debug::CanLog());
    Debug::ShutDown();
}

// 测试 调试生命周期：Shut下之后能够日志返回false
TEST(DebugLifecycleTest, ShutDownAfterCanLogReturnsfalse) {
    Debug::Init();
    Debug::ShutDown();
    EXPECT_FALSE(Debug::CanLog());
}

// 测试 调试生命周期：当不已初始化能够日志返回false
TEST(DebugLifecycleTest, WhenNotInitializedCanLogReturnsfalse) {
    // 先确保关闭状态
    Debug::ShutDown();
    EXPECT_FALSE(Debug::CanLog());
}

// ============================================================
// LogMessage 安全性
// ============================================================

// 测试 调试日志：日志消息不崩溃
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

// 测试 调试日志：当不已初始化日志消息不崩溃
TEST(DebugLogTest, WhenNotInitializedLogMessageDoesNotCrash) {
    Debug::ShutDown();
    EXPECT_NO_THROW(LogMessage(LogLevel::Info, "should be suppressed"));
}
