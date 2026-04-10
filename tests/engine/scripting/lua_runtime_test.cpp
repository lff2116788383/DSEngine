#include "catch/catch.hpp"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"

#include "engine/base/debug.h"

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

class ScopedLogLevel {
public:
    explicit ScopedLogLevel(dse::debug::LogLevel level)
        : previous_level_(dse::debug::GetLogLevel()) {
        dse::debug::SetLogLevel(level);
    }

    ~ScopedLogLevel() {
        dse::debug::SetLogLevel(previous_level_);
    }

    ScopedLogLevel(const ScopedLogLevel&) = delete;
    ScopedLogLevel& operator=(const ScopedLogLevel&) = delete;

private:
    dse::debug::LogLevel previous_level_;
};

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

class ScopedLuaApiContextReset {
public:
    ~ScopedLuaApiContextReset() {
        ConfigureLuaApiContext(LuaApiContext{});
        SetStartupLuaScriptPath("");
    }
};

} // namespace

// 回归用例：启动脚本缺失时 Bootstrap 失败后不应遗留 Lua 内存统计脏值。
TEST_CASE("Given_MissingStartupScript_When_BootstrapFails_Then_LuaMemoryUsageResetsToZero", "[engine][unit][lua_runtime][diagnostic_single]") {
    ScopedLuaApiContextReset scoped_context_reset;
    ScopedLogLevel scoped_logger(dse::debug::LogLevel::Off);

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(MakeTempPath("missing_startup.lua"));

    REQUIRE_FALSE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() == 0);

    ShutdownLuaRuntime();
}

// 回归用例：即便全局 Update 不存在，实体级 ScriptComponent 仍应继续执行 OnUpdate。
TEST_CASE("Given_NoGlobalUpdate_When_TickLuaRuntime_Then_ScriptComponentStillUpdates", "[engine][unit][lua_runtime]") {
    ScopedLuaApiContextReset scoped_context_reset;
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
    AssetManager asset_manager;
    Entity entity = world.CreateEntity();
    auto& script = world.registry().emplace<ScriptComponent>(entity);
    script.script_path = component_script_path;
    script.enabled = true;

    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();

    REQUIRE(std::filesystem::exists(output_path));
}

// 回归用例：正常关闭 Lua 运行时后，Lua 内存统计应复位为 0。
TEST_CASE("Given_BootstrappedRuntime_When_Shutdown_Then_LuaMemoryUsageResetsToZero", "[engine][unit][lua_runtime]") {
    ScopedLuaApiContextReset scoped_context_reset;
    const std::string startup_path = MakeTempPath("startup_memory_reset.lua");
    ScopedTempFile startup_file(startup_path);
    WriteTextFile(startup_path, "Awake = function() end\n");

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() > 0);

    ShutdownLuaRuntime();
    REQUIRE(GetLuaMemoryUsage() == 0);
}

// 回归用例：启动脚本 Awake 抛错后，Bootstrap 应失败且 Lua 内存统计归零。
TEST_CASE("Given_StartupAwakeFailure_When_BootstrapLuaRuntime_Then_StateIsCleanedUp", "[engine][unit][lua_runtime]") {
    ScopedLuaApiContextReset scoped_context_reset;
    const std::string startup_path = MakeTempPath("startup_awake_failure.lua");
    ScopedTempFile startup_file(startup_path);
    WriteTextFile(startup_path, "Awake = function() error('boom') end\n");

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE_FALSE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() == 0);

    ShutdownLuaRuntime();
}

// 回归用例：某个脚本组件加载失败时，不应阻塞其他实体脚本的 OnUpdate。
TEST_CASE("Given_OneBrokenScriptComponent_When_TickLuaRuntime_Then_OtherScriptsStillUpdate", "[engine][unit][lua_runtime]") {
    ScopedLuaApiContextReset scoped_context_reset;
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
    AssetManager asset_manager;
    Entity good_entity = world.CreateEntity();
    auto& good_script = world.registry().emplace<ScriptComponent>(good_entity);
    good_script.script_path = good_script_path;
    good_script.enabled = true;

    Entity bad_entity = world.CreateEntity();
    auto& bad_script = world.registry().emplace<ScriptComponent>(bad_entity);
    bad_script.script_path = bad_script_path;
    bad_script.enabled = true;

    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();

    REQUIRE(std::filesystem::exists(output_path));
}

TEST_CASE("Given_ShutdownRuntime_When_BootstrapAgain_Then_LuaRuntimeCanRestartCleanly", "[engine][unit][lua_runtime]") {
    ScopedLuaApiContextReset scoped_context_reset;
    const std::string startup_path = MakeTempPath("startup_restart_cleanly.lua");
    ScopedTempFile startup_file(startup_path);
    WriteTextFile(startup_path, "Awake = function() end\n");

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() > 0);
    ShutdownLuaRuntime();
    REQUIRE(GetLuaMemoryUsage() == 0);

    REQUIRE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() > 0);
    ShutdownLuaRuntime();
    REQUIRE(GetLuaMemoryUsage() == 0);
}

TEST_CASE("Given_DisabledScriptComponent_When_TickLuaRuntime_Then_OnUpdateIsSkipped", "[engine][unit][lua_runtime]") {
    ScopedLuaApiContextReset scoped_context_reset;
    const std::string startup_path = MakeTempPath("startup_disabled_component.lua");
    const std::string component_script_path = MakeTempPath("disabled_component_update.lua");
    const std::string output_path = MakeTempPath("disabled_component_marker.txt");
    ScopedTempFile startup_file(startup_path);
    ScopedTempFile component_file(component_script_path);
    ScopedTempFile output_file(output_path);

    WriteTextFile(startup_path, "Awake = function() end\n");
    WriteTextFile(
        component_script_path,
        "return {\n"
        "  OnUpdate = function(self, entity, dt)\n"
        "    local f = io.open('" + ToLuaPath(output_path) + "', 'w')\n"
        "    if f then\n"
        "      f:write('should_not_run')\n"
        "      f:close()\n"
        "    end\n"
        "  end\n"
        "}\n");

    World world;
    AssetManager asset_manager;
    Entity entity = world.CreateEntity();
    auto& script = world.registry().emplace<ScriptComponent>(entity);
    script.script_path = component_script_path;
    script.enabled = false;

    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();

    REQUIRE_FALSE(std::filesystem::exists(output_path));
}

TEST_CASE("Given_StandardLuaLibraries_When_BootstrapLuaRuntime_Then_BaseStringTableMathAndPackageApisAreAvailable", "[engine][unit][lua_runtime][regression]") {
    ScopedLuaApiContextReset scoped_context_reset;
    const std::string startup_path = MakeTempPath("startup_stdlibs_regression.lua");
    const std::string output_path = MakeTempPath("startup_stdlibs_regression.txt");
    ScopedTempFile startup_file(startup_path);
    ScopedTempFile output_file(output_path);

    WriteTextFile(
        startup_path,
        "Awake = function()\n"
        "  assert(type(print) == 'function')\n"
        "  assert(string.upper('lua') == 'LUA')\n"
        "  assert(table.concat({'d','s','e'}, '') == 'dse')\n"
        "  assert(math.floor(3.8) == 3)\n"
        "  assert(type(package.path) == 'string')\n"
        "  local f = io.open('" + ToLuaPath(output_path) + "', 'w')\n"
        "  assert(f ~= nil)\n"
        "  f:write(string.format('%s|%s|%d|%d', string.upper('lua'), table.concat({'d','s','e'}, ''), math.floor(3.8), type(package.path) == 'string' and 1 or 0))\n"
        "  f:close()\n"
        "end\n");

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    ShutdownLuaRuntime();

    REQUIRE(std::filesystem::exists(output_path));
    std::ifstream in(output_path);
    REQUIRE(in.is_open());
    std::string content;
    std::getline(in, content);
    REQUIRE(content == "LUA|dse|3|1");
}