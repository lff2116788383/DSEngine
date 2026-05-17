/**
 * @file lua_lifecycle_smoke_test.cpp
 * @brief Lua 全生命周期冒烟测试 — 验证多帧 Tick + 场景加载 + 安全关闭
 *
 * 覆盖场景：
 *   1. Bootstrap → 多帧 Tick（50帧） → Shutdown 不泄漏
 *   2. 脚本创建多实体 + 多帧 Tick → 实体数稳定增长
 *   3. 错误脚本不崩溃（语法错误）
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/core/service_locator.h"
#include <filesystem>
#include <fstream>

using namespace dse::runtime;

namespace {

class LuaTempFile {
public:
    explicit LuaTempFile(const std::string& name, const std::string& content)
        : path_(name) {
        std::ofstream out(path_);
        out << content;
    }
    ~LuaTempFile() {
        std::filesystem::remove(path_);
    }
    const std::string& Path() const { return path_; }
private:
    std::string path_;
};

} // namespace

class LuaLifecycleSmokeTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }
};

TEST_F(LuaLifecycleSmokeTest, 50帧连续Tick不泄漏) {
    LuaTempFile script("lifecycle_50f.lua", R"(
        local frame = 0
        function Awake() end
        function Update(dt)
            frame = frame + 1
        end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());

    size_t mem_before = GetLuaMemoryUsage();
    for (int i = 0; i < 50; ++i) {
        TickLuaRuntime(0.016f);
    }
    size_t mem_after = GetLuaMemoryUsage();

    // 50帧 tick 后内存不应膨胀超过 2x（检测泄漏）
    EXPECT_LT(mem_after, mem_before * 2 + 4096)
        << "Memory grew from " << mem_before << " to " << mem_after;

    ShutdownLuaRuntime();
    EXPECT_EQ(GetLuaMemoryUsage(), 0u);
}

TEST_F(LuaLifecycleSmokeTest, 动态创建实体多帧稳定) {
    LuaTempFile script("lifecycle_spawn.lua", R"(
        local spawned = 0
        function Awake() end
        function Update(dt)
            if spawned < 10 then
                local e = dse.ecs.create_entity()
                dse.ecs.add_transform(e, spawned, 0, 0)
                spawned = spawned + 1
            end
        end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());

    for (int i = 0; i < 20; ++i) {
        TickLuaRuntime(0.016f);
    }

    // 应创建正好 10 个实体
    int count = 0;
    world.registry().view<TransformComponent>().each(
        [&](auto, const TransformComponent&) { count++; });
    EXPECT_EQ(count, 10);

    ShutdownLuaRuntime();
}

TEST_F(LuaLifecycleSmokeTest, 语法错误脚本不崩溃) {
    LuaTempFile script("lifecycle_error.lua", R"(
        function Awake()
            -- intentional syntax error
            local x = {{{
        end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    // Bootstrap 可能失败（语法错误）但不应崩溃
    bool result = BootstrapLuaRuntime();
    if (result) {
        EXPECT_NO_THROW(TickLuaRuntime(0.016f));
    }
    ShutdownLuaRuntime();
}
