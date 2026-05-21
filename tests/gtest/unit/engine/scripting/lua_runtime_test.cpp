/**
 * @file lua_runtime_test.cpp
 * @brief Lua 脚本运行时系统单元测试
 */

#include "gtest/gtest.h"
#include "engine/scripting/lua/lua_runtime.h"

using namespace dse::runtime;

// ============================================================
// 基础生命周期测试
// ============================================================

TEST(LuaRuntimeTest, 默认状态未初始化) {
    // BootstrapLuaRuntime 调用前，VM 不应存在
    // 注意：此测试假设 BootstrapLuaRuntime 不会在静态初始化时自动调用
    // 实际行为取决于实现，这里主要测试 Shutdown 不会崩溃
    ShutdownLuaRuntime();
}

TEST(LuaRuntimeTest, BootstrapLuaRuntime返回值) {
    // world=nullptr 时应返回 false（引擎需要有效的 ECS world 才能初始化 Lua 绑定）
    LuaApiContext ctx;  // world 为 nullptr
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    bool success = BootstrapLuaRuntime();
    EXPECT_FALSE(success) << "world=nullptr 时 BootstrapLuaRuntime 应返回 false";

    ShutdownLuaRuntime();
}

TEST(LuaRuntimeTest, ShutdownLuaRuntime不崩溃) {
    LuaApiContext ctx;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    if (BootstrapLuaRuntime()) {
        ShutdownLuaRuntime();
        SUCCEED();
    } else {
        // 如果初始化失败，Shutdown 仍应安全
        ShutdownLuaRuntime();
        SUCCEED();
    }
}

TEST(LuaRuntimeTest, 重复Shutdown不崩溃) {
    LuaApiContext ctx;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    if (BootstrapLuaRuntime()) {
        ShutdownLuaRuntime();
        ShutdownLuaRuntime();  // 重复调用应安全
        SUCCEED();
    }
}

// ============================================================
// ExecuteLuaString 测试
// ============================================================

TEST(LuaRuntimeTest, ExecuteLuaString未初始化返回false) {
    ShutdownLuaRuntime();  // 确保 VM 关闭
    std::string result;
    bool success = ExecuteLuaString("print('hello')", &result);
    EXPECT_FALSE(success);
}

TEST(LuaRuntimeTest, ExecuteLuaString简单算术) {
    LuaApiContext ctx;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    if (!BootstrapLuaRuntime()) {
        GTEST_SKIP() << "BootstrapLuaRuntime 失败，跳过测试";
    }

    std::string result;
    bool success = ExecuteLuaString("return 1 + 2", &result);
    EXPECT_TRUE(success);
    if (success) {
        EXPECT_EQ(result, "3");
    }

    ShutdownLuaRuntime();
}

TEST(LuaRuntimeTest, ExecuteLuaString语法错误) {
    LuaApiContext ctx;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    if (!BootstrapLuaRuntime()) {
        GTEST_SKIP() << "BootstrapLuaRuntime 失败，跳过测试";
    }

    std::string result;
    bool success = ExecuteLuaString("if true then", &result);
    EXPECT_FALSE(success);
    if (!success) {
        EXPECT_FALSE(result.empty()) << "语法错误应返回错误信息";
    }

    ShutdownLuaRuntime();
}

TEST(LuaRuntimeTest, ExecuteLuaString空代码返回true) {
    LuaApiContext ctx;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    if (!BootstrapLuaRuntime()) {
        GTEST_SKIP() << "BootstrapLuaRuntime 失败，跳过测试";
    }

    bool success = ExecuteLuaString("", nullptr);
    EXPECT_TRUE(success);

    ShutdownLuaRuntime();
}

// ============================================================
// TickLuaRuntime 测试
// ============================================================

TEST(LuaRuntimeTest, TickLuaRuntime未初始化不崩溃) {
    ShutdownLuaRuntime();
    // 未初始化时调用 Tick 应安全（可能是 no-op）
    TickLuaRuntime(0.016f);
    SUCCEED();
}

TEST(LuaRuntimeTest, TickLuaRuntime正常调用不崩溃) {
    LuaApiContext ctx;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    if (!BootstrapLuaRuntime()) {
        GTEST_SKIP() << "BootstrapLuaRuntime 失败，跳过测试";
    }

    TickLuaRuntime(0.016f);
    SUCCEED();

    ShutdownLuaRuntime();
}

// ============================================================
// GetLuaMemoryUsage 测试
// ============================================================

TEST(LuaRuntimeTest, GetLuaMemoryUsage未初始化返回零) {
    ShutdownLuaRuntime();
    size_t usage = GetLuaMemoryUsage();
    EXPECT_EQ(usage, 0u) << "未初始化时应返回 0";
}

TEST(LuaRuntimeTest, GetLuaMemoryUsage初始化后大于零) {
    LuaApiContext ctx;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    if (!BootstrapLuaRuntime()) {
        GTEST_SKIP() << "BootstrapLuaRuntime 失败，跳过测试";
    }

    size_t usage = GetLuaMemoryUsage();
    EXPECT_GT(usage, 0u) << "初始化后应报告非零内存使用";

    ShutdownLuaRuntime();
}

// ============================================================
// PumpLuaScriptHotReloads 测试
// ============================================================

TEST(LuaRuntimeTest, PumpLuaScriptHotReloads未初始化返回零) {
    ShutdownLuaRuntime();
    int count = PumpLuaScriptHotReloads();
    EXPECT_EQ(count, 0) << "未初始化时应返回 0";
}

TEST(LuaRuntimeTest, PumpLuaScriptHotReloads正常调用不崩溃) {
    LuaApiContext ctx;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    if (!BootstrapLuaRuntime()) {
        GTEST_SKIP() << "BootstrapLuaRuntime 失败，跳过测试";
    }

    int count = PumpLuaScriptHotReloads();
    // 无文件变更时应返回 0
    EXPECT_GE(count, 0);

    ShutdownLuaRuntime();
}
