/**
 * @file lua_host_smoke_test.cpp
 * @brief Lua 宿主冒烟测试
 *
 * 验证场景：
 * - Lua VM 初始化→脚本执行→关闭完整链路
 * - Lua 脚本读取 C++ 侧数据
 * - Lua 脚本修改 C++ 侧状态
 * - 多次 Lua VM 初始化/关闭不崩溃
 * - Lua 内存统计功能
 * - Startup 脚本路径设置
 * - LuaApiContext 完整注入链路
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

class LuaHostSmokeTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }
};

TEST_F(LuaHostSmokeTest, 完整LuaVM生命周期) {
    LuaTempScript script("smoke_lifecycle.lua", R"(
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

    // 1. Bootstrap
    ASSERT_TRUE(BootstrapLuaRuntime());

    // 2. Tick
    TickLuaRuntime(0.016f);
    TickLuaRuntime(0.016f);

    // 3. Shutdown
    ShutdownLuaRuntime();
    SUCCEED();
}

TEST_F(LuaHostSmokeTest, Lua脚本修改CPlusPlus侧World) {
    LuaTempScript script("smoke_modify.lua", R"(
        function Awake()
            dse.ecs.create_entity()
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    size_t count_before = world.EntityCount();

    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    EXPECT_GT(world.EntityCount(), count_before);

    ShutdownLuaRuntime();
}

TEST_F(LuaHostSmokeTest, 多次LuaVM初始化关闭不崩溃) {
    for (int i = 0; i < 3; ++i) {
        LuaTempScript script("smoke_multi_" + std::to_string(i) + ".lua", R"(
            function Awake() end
            function Update(dt) end
        )");

        SetStartupLuaScriptPath(script.Path());

        World world;
        LuaApiContext ctx;
        ctx.world = &world;
        ConfigureLuaApiContext(ctx);

        ASSERT_TRUE(BootstrapLuaRuntime());
        TickLuaRuntime(0.016f);
        ShutdownLuaRuntime();
    }
}

TEST_F(LuaHostSmokeTest, Lua内存统计) {
    LuaTempScript script("smoke_memory.lua", R"(
        function Awake()
            big_data = {}
            for i = 1, 100 do
                big_data[i] = "string_" .. tostring(i)
            end
        end
        function Update(dt)
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

TEST_F(LuaHostSmokeTest, LuaApiContext完整注入) {
    LuaTempScript script("smoke_ctx.lua", R"(
        function Awake() end
        function Update(dt) end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    int draw_calls = 42;

    LuaApiContext ctx;
    ctx.world = &world;
    ctx.set_window_title = [](const std::string&) {};
    ctx.get_draw_calls = [&draw_calls]() { return draw_calls; };
    ctx.get_max_batch_sprites = []() { return 100; };
    ctx.get_sprite_count = []() { return 50; };
    ctx.asset_manager = nullptr;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();
}

TEST_F(LuaHostSmokeTest, 未配置Context时引导失败) {
    // 不配置任何上下文
    bool result = BootstrapLuaRuntime();
    EXPECT_FALSE(result);
}

TEST_F(LuaHostSmokeTest, Lua脚本访问dse命名空间) {
    LuaTempScript script("smoke_dse.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 1.0, 2.0, 3.0)
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    // 验证 Lua 通过 dse.ecs 创建了实体
    EXPECT_GT(world.EntityCount(), 0u);

    ShutdownLuaRuntime();
}

TEST_F(LuaHostSmokeTest, SetStartupLuaScriptPath路径设置) {
    LuaTempScript script("smoke_path_test.lua", R"(
        function Awake() end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    ShutdownLuaRuntime();
}
