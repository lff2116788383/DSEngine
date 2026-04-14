#include "catch/catch.hpp"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"

#include <filesystem>
#include <fstream>
#include <sstream>
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
    auto base = std::filesystem::temp_directory_path() / "dse_runtime_smoke_tests";
    std::filesystem::create_directories(base);
    return (base / name).string();
}

std::string ToLuaPath(const std::string& path) {
    return std::filesystem::path(path).generic_string();
}

void WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    REQUIRE(out.is_open());
    out << content;
}

}


TEST_CASE("Smoke Snapshot - Lua demo 15.8 setup loads reference scene without known missing resources", "[engine][smoke][snapshot][lua_runtime][lua_demo]") {
    dse::debug::SetLogLevel(dse::debug::LogLevel::Off);

    const std::string startup_path = MakeTempPath("runtime_smoke_demo15_8_startup.lua");
    WriteTextFile(
        startup_path,
        "package.path = package.path .. ';samples/lua/?.lua;samples/lua/?/init.lua;samples/lua/?/?.lua'\n"
        "local demo = require('vse_demo.demo15_8')\n"
        "Awake = function() demo.Setup({ title = 'Smoke 15.8' }) end\n"
        "Update = function(dt) end\n");

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    std::size_t transform_count_158 = 0;
    for (auto entity : world.registry().view<TransformComponent>()) {
        (void)entity;
        ++transform_count_158;
    }
    REQUIRE(transform_count_158 == 6u);
    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();
    ConfigureLuaApiContext(LuaApiContext{});
    SetStartupLuaScriptPath("");
}

TEST_CASE("Smoke Snapshot - Lua demo 15.9 setup loads reference scene and keeps material showcase entities bound", "[engine][smoke][snapshot][lua_runtime][lua_demo]") {
    dse::debug::SetLogLevel(dse::debug::LogLevel::Off);

    const std::string startup_path = MakeTempPath("runtime_smoke_demo15_9_startup.lua");
    WriteTextFile(
        startup_path,
        "package.path = package.path .. ';samples/lua/?.lua;samples/lua/?/init.lua;samples/lua/?/?.lua'\n"
        "local demo = require('vse_demo.demo15_9')\n"
        "Awake = function() demo.Setup({ title = 'Smoke 15.9' }) end\n"
        "Update = function(dt) demo.Update(dt) end\n");

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    std::size_t transform_count_159 = 0;
    for (auto entity : world.registry().view<TransformComponent>()) {
        (void)entity;
        ++transform_count_159;
    }
    REQUIRE(transform_count_159 == 7u);
    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();
    ConfigureLuaApiContext(LuaApiContext{});
    SetStartupLuaScriptPath("");
}

