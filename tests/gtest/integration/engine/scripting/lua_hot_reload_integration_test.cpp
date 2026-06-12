#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/script.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

using namespace dse::runtime;

namespace {
class LuaHotReloadTempScript {
public:
    LuaHotReloadTempScript(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / name) {
        Write(content);
    }

    ~LuaHotReloadTempScript() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    void Write(const std::string& content) const {
        std::ofstream out(path_, std::ios::trunc);
        out << content;
        out.close();
        std::filesystem::last_write_time(path_, std::filesystem::file_time_type::clock::now());
    }

    std::string Path() const { return path_.string(); }

private:
    std::filesystem::path path_;
};
}

class LuaHotReloadIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }

    World world;
};

// 测试 Lua热重新加载集成：之后状态加载
TEST_F(LuaHotReloadIntegrationTest, AfterStateLoad) {
    LuaHotReloadTempScript startup("dse_hot_reload_startup.lua", R"(
        function Awake() end
        function Update(dt) end
    )");
    LuaHotReloadTempScript script("dse_hot_reload_entity.lua", R"(
        local M = { value = 1, version = 'v1' }
        function M:OnAwake(entity_id) self.value = self.value + 1 end
        function M:OnUpdate(entity_id, dt) _G.hot_reload_value = self.value; _G.hot_reload_version = self.version end
        function M:OnSerializeState(entity_id) return { value = self.value } end
        function M:OnDeserializeState(entity_id, state) self.value = state.value + 10 end
        return M
    )");

    LuaApiContext ctx{};
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath(startup.Path());
    ASSERT_TRUE(BootstrapLuaRuntime());

    auto e = world.CreateEntity();
    auto& sc = world.registry().emplace<ScriptComponent>(e);
    sc.script_path = script.Path();
    sc.enabled = true;

    TickLuaRuntime(0.016f);

    std::string before;
    ASSERT_TRUE(ExecuteLuaString("return hot_reload_version .. ':' .. tostring(hot_reload_value)", &before));
    EXPECT_EQ(before, "v1:2");

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    script.Write(R"(
        local M = { value = 100, version = 'v2' }
        function M:OnAwake(entity_id) self.value = self.value + 1000 end
        function M:OnUpdate(entity_id, dt) _G.hot_reload_value = self.value; _G.hot_reload_version = self.version end
        function M:OnSerializeState(entity_id) return { value = self.value } end
        function M:OnDeserializeState(entity_id, state) self.value = state.value + 10 end
        return M
    )");

    EXPECT_EQ(PumpLuaScriptHotReloads(), 1);
    TickLuaRuntime(0.016f);

    std::string after;
    ASSERT_TRUE(ExecuteLuaString("return hot_reload_version .. ':' .. tostring(hot_reload_value)", &after));
    EXPECT_EQ(after, "v2:12");
}

// 测试 Lua热重新加载集成：禁用脚本组件销毁示例不再次
TEST_F(LuaHotReloadIntegrationTest, DisabledScriptComponentDestroyExampleNotAgain) {
    LuaHotReloadTempScript startup("dse_hot_reload_disabled_startup.lua", R"(
        function Awake() end
    )");
    LuaHotReloadTempScript script("dse_hot_reload_disabled_entity.lua", R"(
        local M = {}
        function M:OnAwake(entity_id) _G.disabled_awake = (_G.disabled_awake or 0) + 1 end
        function M:OnDestroy(entity_id) _G.disabled_destroy = (_G.disabled_destroy or 0) + 1 end
        return M
    )");

    LuaApiContext ctx{};
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath(startup.Path());
    ASSERT_TRUE(BootstrapLuaRuntime());

    auto e = world.CreateEntity();
    auto& sc = world.registry().emplace<ScriptComponent>(e);
    sc.script_path = script.Path();
    sc.enabled = true;
    TickLuaRuntime(0.016f);

    sc.enabled = false;
    TickLuaRuntime(0.016f);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    script.Write("return {}\n");
    EXPECT_EQ(PumpLuaScriptHotReloads(), 0);

    std::string result;
    ASSERT_TRUE(ExecuteLuaString("return tostring(disabled_awake) .. ':' .. tostring(disabled_destroy)", &result));
    EXPECT_EQ(result, "1:1");
}
