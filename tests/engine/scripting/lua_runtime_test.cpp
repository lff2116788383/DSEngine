#include "catch/catch.hpp"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/null_sink.h"

#include <filesystem>
#include <fstream>
#include <string>

using dse::runtime::BootstrapLuaRuntime;
using dse::runtime::ConfigureLuaApiContext;
using dse::runtime::GetLuaMemoryUsage;
using dse::runtime::LuaApiContext;
using dse::runtime::SetStartupLuaScriptPath;
using dse::runtime::ShutdownLuaRuntime;
using dse::runtime::TickLuaRuntime;

namespace {

std::string MakeTempPath(const char* name) {
    auto base = std::filesystem::temp_directory_path() / "dse_lua_runtime_tests";
    std::filesystem::create_directories(base);
    return (base / name).string();
}

std::string ToLuaPath(const std::string& path) {
    return std::filesystem::path(path).generic_string();
}

struct ScopedTempFile {
    explicit ScopedTempFile(std::string p) : path(std::move(p)) {}
    ~ScopedTempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    std::string path;
};

void WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    REQUIRE(out.is_open());
    out << content;
}

} // namespace

// 回归用例：启动脚本缺失时 Bootstrap 失败后不应遗留 Lua 内存统计脏值。
TEST_CASE("Given_MissingStartupScript_When_BootstrapFails_Then_LuaMemoryUsageResetsToZero", "[engine][unit][lua_runtime]") {
    auto previous_logger = spdlog::default_logger();
    auto null_logger = std::make_shared<spdlog::logger>("lua_runtime_test_null", std::make_shared<spdlog::sinks::null_sink_mt>());
    spdlog::set_default_logger(null_logger);

    World world;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, nullptr});
    SetStartupLuaScriptPath(MakeTempPath("missing_startup.lua"));

    REQUIRE_FALSE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() == 0);

    ShutdownLuaRuntime();
    if (previous_logger) {
        spdlog::set_default_logger(previous_logger);
    }
}

// 回归用例：即便全局 Update 不存在，实体级 ScriptComponent 仍应继续执行 OnUpdate。
TEST_CASE("Given_NoGlobalUpdate_When_TickLuaRuntime_Then_ScriptComponentStillUpdates", "[engine][unit][lua_runtime]") {
    const std::string startup_path = MakeTempPath("startup_no_global_update.lua");
    const std::string component_script_path = MakeTempPath("component_update.lua");
    const std::string output_path = MakeTempPath("component_update_marker.txt");
    ScopedTempFile startup_file(startup_path);
    ScopedTempFile component_file(component_script_path);
    ScopedTempFile output_file(output_path);

    WriteTextFile(startup_path, "-- intentionally no global Update()\n");
    WriteTextFile(
        component_script_path,
        "return {\n"
        "  OnUpdate = function(self, entity, dt)\n"
        "    local f = io.open('" + ToLuaPath(output_path) + "', 'w')\n"
        "    if f then\n"
        "      f:write('updated:' .. tostring(entity))\n"
        "      f:close()\n"
        "    end\n"
        "  end\n"
        "}\n");

    World world;
    Entity entity = world.CreateEntity();
    auto& script = world.registry().emplace<ScriptComponent>(entity);
    script.script_path = component_script_path;
    script.enabled = true;

    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, nullptr});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();

    REQUIRE(std::filesystem::exists(output_path));
}

// 回归用例：正常关闭 Lua 运行时后，Lua 内存统计应复位为 0。
TEST_CASE("Given_BootstrappedRuntime_When_Shutdown_Then_LuaMemoryUsageResetsToZero", "[engine][unit][lua_runtime]") {
    const std::string startup_path = MakeTempPath("startup_memory_reset.lua");
    ScopedTempFile startup_file(startup_path);
    WriteTextFile(startup_path, "Awake = function() end\n");

    World world;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, nullptr});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() > 0);

    ShutdownLuaRuntime();
    REQUIRE(GetLuaMemoryUsage() == 0);
}

// 回归用例：启动脚本 Awake 抛错后，Bootstrap 应失败且 Lua 内存统计归零。
TEST_CASE("Given_StartupAwakeFailure_When_BootstrapLuaRuntime_Then_StateIsCleanedUp", "[engine][unit][lua_runtime]") {
    const std::string startup_path = MakeTempPath("startup_awake_failure.lua");
    ScopedTempFile startup_file(startup_path);
    WriteTextFile(startup_path, "Awake = function() error('boom') end\n");

    World world;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, nullptr});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE_FALSE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() == 0);

    ShutdownLuaRuntime();
}

// 回归用例：某个脚本组件加载失败时，不应阻塞其他实体脚本的 OnUpdate。
TEST_CASE("Given_OneBrokenScriptComponent_When_TickLuaRuntime_Then_OtherScriptsStillUpdate", "[engine][unit][lua_runtime]") {
    const std::string startup_path = MakeTempPath("startup_multi_entity.lua");
    const std::string good_script_path = MakeTempPath("good_component_update.lua");
    const std::string bad_script_path = MakeTempPath("bad_component_update.lua");
    const std::string output_path = MakeTempPath("good_component_marker.txt");
    ScopedTempFile startup_file(startup_path);
    ScopedTempFile good_script_file(good_script_path);
    ScopedTempFile bad_script_file(bad_script_path);
    ScopedTempFile output_file(output_path);

    WriteTextFile(startup_path, "Awake = function() end\n");
    WriteTextFile(
        good_script_path,
        "return {\n"
        "  OnUpdate = function(self, entity, dt)\n"
        "    local f = io.open('" + ToLuaPath(output_path) + "', 'w')\n"
        "    if f then\n"
        "      f:write('ok')\n"
        "      f:close()\n"
        "    end\n"
        "  end\n"
        "}\n");
    WriteTextFile(bad_script_path, "return 'invalid'\n");

    World world;
    Entity good_entity = world.CreateEntity();
    auto& good_script = world.registry().emplace<ScriptComponent>(good_entity);
    good_script.script_path = good_script_path;
    good_script.enabled = true;

    Entity bad_entity = world.CreateEntity();
    auto& bad_script = world.registry().emplace<ScriptComponent>(bad_entity);
    bad_script.script_path = bad_script_path;
    bad_script.enabled = true;

    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, nullptr});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();

    REQUIRE(std::filesystem::exists(output_path));
}