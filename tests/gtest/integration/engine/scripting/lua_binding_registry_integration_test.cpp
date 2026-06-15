#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace dse::runtime;

namespace {
class LuaRegistryTempScript {
public:
    LuaRegistryTempScript(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / name) {
        std::ofstream out(path_);
        out << content;
    }

    ~LuaRegistryTempScript() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    std::string Path() const { return path_.string(); }

private:
    std::filesystem::path path_;
};
}

class LuaBindingRegistryIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }

    World world;
};

// 测试 Lua绑定注册表集成：注册基Lua且
TEST_F(LuaBindingRegistryIntegrationTest, RegisterBaseLuaAnd) {
    LuaRegistryTempScript startup("dse_lua_registry_startup.lua", R"(
        function Awake() end
    )");

    LuaApiContext ctx{};
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath(startup.Path());
    ASSERT_TRUE(BootstrapLuaRuntime());

    std::string result;
    ASSERT_TRUE(ExecuteLuaString(R"(
        local checks = {
            type(dse),
            type(dse.ecs),
            type(dse.audio),
            type(dse.spine),
            type(dse.ui),
            type(dse.assets),
            type(dse.app),
            type(dse.metrics),
            type(dssl),
            type(streaming),
            type(dse.get_memory_usage_kb),
            type(dse.ecs.create_entity),
            type(dse.audio.add_source),
            type(dse.ui.add_canvas_scaler),
            type(dssl.load_material),
            type(streaming.create_zone),
        }
        return table.concat(checks, ',')
    )", &result));

    EXPECT_EQ(result,
        "table,table,table,table,table,table,table,table,table,table,"
        "function,function,function,function,function,function");
}

// 测试 Lua绑定注册表集成：缺失能够导航不基API
TEST_F(LuaBindingRegistryIntegrationTest, MissingCanNavNotBaseAPI) {
    LuaRegistryTempScript startup("dse_lua_registry_nav_startup.lua", R"(
        function Awake() end
    )");

    LuaApiContext ctx{};
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    SetStartupLuaScriptPath(startup.Path());
    ASSERT_TRUE(BootstrapLuaRuntime());

    std::string result;
    ASSERT_TRUE(ExecuteLuaString("return type(dse.ecs.create_entity)", &result));
    EXPECT_EQ(result, "function");
}
