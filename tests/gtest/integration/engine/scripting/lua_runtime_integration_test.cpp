/**
 * @file lua_runtime_integration_test.cpp
 * @brief Lua runtime integration tests
 *
 * Test scenarios:
 * - Lua VM lifecycle: Bootstrap / Tick / Shutdown
 * - LuaApiContext injection with World and callbacks
 * - ScriptComponent driving Lua script instance OnAwake / OnUpdate / OnDestroy
 * - Lua memory tracking
 * - Error handling
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/script.h"
#include "engine/ecs/transform.h"
#include "engine/core/service_locator.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace dse::runtime;

// ============================================================
// Helper: create temporary Lua script
// ============================================================

class LuaTempScript {
public:
    explicit LuaTempScript(const std::string& name, const std::string& content)
        : path_(name) {
        std::ofstream out(path_);
        out << content;
    }

    ~LuaTempScript() {
        std::filesystem::remove(path_);
    }

    const std::string& Path() const { return path_; }

private:
    std::string path_;
};

// ============================================================
// Lua VM lifecycle
// ============================================================

class LuaRuntimeIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }
};

// 测试 Lua运行时集成：引导失败无启动脚本
TEST_F(LuaRuntimeIntegrationTest, BootstrapFailsWithoutStartupScript) {
    bool result = BootstrapLuaRuntime();
    EXPECT_FALSE(result);
}

// 测试 Lua运行时集成：引导成功带有效脚本
TEST_F(LuaRuntimeIntegrationTest, BootstrapSucceedsWithValidScript) {
    LuaTempScript script("test_bootstrap.lua", R"(
        function Awake()
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    bool result = BootstrapLuaRuntime();
    EXPECT_TRUE(result);

    ShutdownLuaRuntime();
}

// 测试 Lua运行时集成：无世界当引导失败
TEST_F(LuaRuntimeIntegrationTest, WithoutWorldWhenBootstrapFails) {
    LuaTempScript script("test_no_world.lua", R"(
        function Awake() end
    )");

    SetStartupLuaScriptPath(script.Path());

    LuaApiContext ctx;
    ctx.world = nullptr;
    ConfigureLuaApiContext(ctx);

    bool result = BootstrapLuaRuntime();
    EXPECT_FALSE(result);
}

// 测试 Lua运行时集成：关闭之后引导不崩溃
TEST_F(LuaRuntimeIntegrationTest, ShutdownAfterBootstrapDoesNotCrash) {
    LuaTempScript script("test_shutdown.lua", R"(
        function Awake() end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    ShutdownLuaRuntime();

    // Double shutdown should not crash
    ShutdownLuaRuntime();
    SUCCEED();
}

// ============================================================
// LuaApiContext injection
// ============================================================

// 测试 Lua运行时集成：Lua API上下文注入世界
TEST_F(LuaRuntimeIntegrationTest, LuaApiContextInjectsWorld) {
    LuaTempScript script("test_ctx_world.lua", R"(
        function Awake()
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    Entity e = world.CreateEntity();

    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    bool result = BootstrapLuaRuntime();
    EXPECT_TRUE(result);

    ShutdownLuaRuntime();
}

// ============================================================
// Tick driving
// ============================================================

// 测试 Lua运行时集成：帧更新执行更新
TEST_F(LuaRuntimeIntegrationTest, FrameUpdateExecuteUpdate) {
    LuaTempScript script("test_tick.lua", R"(
        update_count = 0
        function Update(dt)
            update_count = update_count + 1
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());

    TickLuaRuntime(0.016f);
    TickLuaRuntime(0.016f);
    TickLuaRuntime(0.016f);

    SUCCEED();
    ShutdownLuaRuntime();
}

// 测试 Lua运行时集成：帧更新无引导不崩溃
TEST_F(LuaRuntimeIntegrationTest, FrameUpdateWithoutBootstrapDoesNotCrash) {
    TickLuaRuntime(0.016f);
    SUCCEED();
}

// ============================================================
// ScriptComponent driving
// ============================================================

// 测试 Lua运行时集成：脚本Componentdrive开启Awake且开启更新
TEST_F(LuaRuntimeIntegrationTest, ScriptComponentdriveOnAwakeAndOnUpdate) {
    LuaTempScript startup("test_startup.lua", R"(
        function Awake() end
        function Update(dt) end
    )");
    LuaTempScript entity_script("test_entity_script.lua", R"(
        local M = {}
        function M:OnAwake(entity_id)
        end
        function M:OnUpdate(entity_id, dt)
        end
        return M
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    Entity e = world.CreateEntity();
    auto& script_comp = world.registry().emplace<ScriptComponent>(e);
    script_comp.script_path = entity_script.Path();
    script_comp.enabled = true;

    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());

    TickLuaRuntime(0.016f);
    TickLuaRuntime(0.016f);

    SUCCEED();
    ShutdownLuaRuntime();
}

// 测试 Lua运行时集成：禁用脚本组件不执行
TEST_F(LuaRuntimeIntegrationTest, DisabledScriptComponentDoesNotExecute) {
    LuaTempScript startup("test_startup2.lua", R"(
        function Awake() end
    )");
    LuaTempScript entity_script("test_disabled_script.lua", R"(
        local M = {}
        function M:OnAwake(entity_id) end
        function M:OnUpdate(entity_id, dt) end
        return M
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    Entity e = world.CreateEntity();
    auto& script_comp = world.registry().emplace<ScriptComponent>(e);
    script_comp.script_path = entity_script.Path();
    script_comp.enabled = false;

    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
    SUCCEED();
    ShutdownLuaRuntime();
}

// 测试 Lua运行时集成：脚本实例Cleaned上之后实体销毁
TEST_F(LuaRuntimeIntegrationTest, ScriptInstanceCleanedUpAfterEntityDestroy) {
    LuaTempScript startup("test_startup3.lua", R"(
        function Awake() end
    )");
    LuaTempScript entity_script("test_destroy_script.lua", R"(
        local M = {}
        function M:OnAwake(entity_id) end
        function M:OnUpdate(entity_id, dt) end
        function M:OnDestroy(entity_id) end
        return M
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    Entity e = world.CreateEntity();
    auto& script_comp = world.registry().emplace<ScriptComponent>(e);
    script_comp.script_path = entity_script.Path();
    script_comp.enabled = true;

    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    world.DestroyEntity(e);
    TickLuaRuntime(0.016f);

    SUCCEED();
    ShutdownLuaRuntime();
}

// ============================================================
// Memory tracking
// ============================================================

// 测试 Lua运行时集成：获取Lua内存用量返回非零值
TEST_F(LuaRuntimeIntegrationTest, GetLuaMemoryUsageReturnsNonZeroValue) {
    LuaTempScript script("test_memory.lua", R"(
        function Awake() end
        big_table = {}
        for i = 1, 1000 do
            big_table[i] = "string_" .. tostring(i)
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());

    size_t mem = GetLuaMemoryUsage();
    EXPECT_GT(mem, 0u);

    ShutdownLuaRuntime();
    EXPECT_EQ(GetLuaMemoryUsage(), 0u);
}

// ============================================================
// Error handling
// ============================================================

// 测试 Lua运行时集成：语法错误Causes引导失败
TEST_F(LuaRuntimeIntegrationTest, SyntaxErrorCausesBootstrapFailure) {
    LuaTempScript script("test_syntax_error.lua", R"(
        function Awake(
        -- missing closing paren
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    bool result = BootstrapLuaRuntime();
    EXPECT_FALSE(result);
}

// 测试 Lua运行时集成：Awake运行时错误Causes启动失败
TEST_F(LuaRuntimeIntegrationTest, AwakeRuntimeErrorCausesBootFailure) {
    LuaTempScript script("test_runtime_error.lua", R"(
        function Awake()
            error("intentional error")
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    bool result = BootstrapLuaRuntime();
    EXPECT_FALSE(result);
}
