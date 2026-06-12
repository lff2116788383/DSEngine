/**
 * @file lua_runtime_test.cpp
 * @brief Lua 脚本运行时系统单元测试
 */

#include "gtest/gtest.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include <filesystem>
#include <fstream>

using namespace dse::runtime;
using namespace dse;

// ============================================================
// 辅助类 / 辅助函数
// ============================================================

struct LuaTempScript {
    std::string path;
    LuaTempScript(const std::string& filename, const std::string& content) {
        path = (std::filesystem::temp_directory_path() / filename).string();
        std::ofstream ofs(path);
        ofs << content;
    }
    ~LuaTempScript() { std::filesystem::remove(path); }
    const std::string& Path() const { return path; }
};

/// 创建最小 World 并初始化 Lua 运行时
static bool InitLuaWithWorld() {
    static World world;
    static LuaTempScript stub("dse_unit_test_stub.lua", "-- stub\n");

    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath(stub.Path());
    return BootstrapLuaRuntime();
}

// ============================================================
// 基础生命周期测试
// ============================================================

TEST(LuaRuntimeTest, DefaultStateUninitialized) {
    // BootstrapLuaRuntime 调用前，VM 不应存在
    // 注意：此测试假设 BootstrapLuaRuntime 不会在静态初始化时自动调用
    // 实际行为取决于实现，这里主要测试 Shutdown 不会崩溃
    ShutdownLuaRuntime();
}

TEST(LuaRuntimeTest, BootstrapLuaRuntimereturnValue) {
    // world=nullptr 时应返回 false（引擎需要有效的 ECS world 才能初始化 Lua 绑定）
    LuaApiContext ctx;  // world 为 nullptr
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath("");

    bool success = BootstrapLuaRuntime();
    EXPECT_FALSE(success) << "world=nullptr 时 BootstrapLuaRuntime 应返回 false";

    ShutdownLuaRuntime();
}

TEST(LuaRuntimeTest, ShutdownLuaRuntimeDoesNotCrash) {
    if (InitLuaWithWorld()) {
        ShutdownLuaRuntime();
        SUCCEED();
    } else {
        // 如果初始化失败，Shutdown 仍应安全
        ShutdownLuaRuntime();
        SUCCEED();
    }
}

TEST(LuaRuntimeTest, ShutdownDoesNotCrash) {
    if (InitLuaWithWorld()) {
        ShutdownLuaRuntime();
        ShutdownLuaRuntime();  // 重复调用应安全
        SUCCEED();
    }
}

// ============================================================
// ExecuteLuaString 测试
// ============================================================

TEST(LuaRuntimeTest, ExecuteLuaStringReturnUninitializedfalse) {
    ShutdownLuaRuntime();  // 确保 VM 关闭
    std::string result;
    bool success = ExecuteLuaString("print('hello')", &result);
    EXPECT_FALSE(success);
}

TEST(LuaRuntimeTest, ExecuteLuaStringsimpleArithmetic) {
    if (!InitLuaWithWorld()) {
        GTEST_SKIP() << "InitLuaWithWorld 失败，跳过测试";
    }

    std::string result;
    bool success = ExecuteLuaString("return 1 + 2", &result);
    EXPECT_TRUE(success);
    if (success) {
        EXPECT_EQ(result, "3");
    }

    ShutdownLuaRuntime();
}

TEST(LuaRuntimeTest, ExecuteLuaStringSyntaxError) {
    if (!InitLuaWithWorld()) {
        GTEST_SKIP() << "InitLuaWithWorld 失败，跳过测试";
    }

    std::string result;
    bool success = ExecuteLuaString("if true then", &result);
    EXPECT_FALSE(success);
    if (!success) {
        EXPECT_FALSE(result.empty()) << "语法错误应返回错误信息";
    }

    ShutdownLuaRuntime();
}

TEST(LuaRuntimeTest, ExecuteLuaStringEmptyCodeReturntrue) {
    if (!InitLuaWithWorld()) {
        GTEST_SKIP() << "InitLuaWithWorld 失败，跳过测试";
    }

    bool success = ExecuteLuaString("", nullptr);
    EXPECT_TRUE(success);

    ShutdownLuaRuntime();
}

// ============================================================
// TickLuaRuntime 测试
// ============================================================

TEST(LuaRuntimeTest, TickLuaRuntimeUninitializedDoesNotCrash) {
    ShutdownLuaRuntime();
    // 未初始化时调用 Tick 应安全（可能是 no-op）
    TickLuaRuntime(0.016f);
    SUCCEED();
}

TEST(LuaRuntimeTest, TickLuaRuntimeNormalCallsDoNotCrash) {
    if (!InitLuaWithWorld()) {
        GTEST_SKIP() << "InitLuaWithWorld 失败，跳过测试";
    }

    TickLuaRuntime(0.016f);
    SUCCEED();

    ShutdownLuaRuntime();
}

// ============================================================
// GetLuaMemoryUsage 测试
// ============================================================

TEST(LuaRuntimeTest, GetLuaMemoryUsageUninitializedReturnsZero) {
    ShutdownLuaRuntime();
    size_t usage = GetLuaMemoryUsage();
    EXPECT_EQ(usage, 0u) << "未初始化时应返回 0";
}

TEST(LuaRuntimeTest, GetLuaMemoryUsageGreaterThanZeroAfterInitialization) {
    if (!InitLuaWithWorld()) {
        GTEST_SKIP() << "InitLuaWithWorld 失败，跳过测试";
    }

    size_t usage = GetLuaMemoryUsage();
    EXPECT_GT(usage, 0u) << "初始化后应报告非零内存使用";

    ShutdownLuaRuntime();
}

// ============================================================
// PumpLuaScriptHotReloads 测试
// ============================================================

TEST(LuaRuntimeTest, PumpLuaScriptHotReloadsUninitializedReturnsZero) {
    ShutdownLuaRuntime();
    int count = PumpLuaScriptHotReloads();
    EXPECT_EQ(count, 0) << "未初始化时应返回 0";
}

TEST(LuaRuntimeTest, PumpLuaScriptHotReloadsNormalCallsDoNotCrash) {
    if (!InitLuaWithWorld()) {
        GTEST_SKIP() << "InitLuaWithWorld 失败，跳过测试";
    }

    int count = PumpLuaScriptHotReloads();
    // 无文件变更时应返回 0
    EXPECT_GE(count, 0);

    ShutdownLuaRuntime();
}
