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

TEST_CASE("Smoke Snapshot - Lua runtime bootstrap update and cleanup remain deterministic", "[engine][smoke][snapshot][lua_runtime]") {
    dse::debug::SetLogLevel(dse::debug::LogLevel::Off);

    const std::string startup_path = MakeTempPath("runtime_smoke_startup_single.lua");
    const std::string component_script_path = MakeTempPath("runtime_smoke_component_single.lua");
    const std::string output_path = MakeTempPath("runtime_smoke_output_single.txt");

    WriteTextFile(startup_path, "Awake = function() end\n");
    WriteTextFile(
        component_script_path,
        "return {\n"
        "  OnUpdate = function(self, entity, dt)\n"
        "    local f = io.open('" + ToLuaPath(output_path) + "', 'w')\n"
        "    if f then\n"
        "      f:write('tick:' .. tostring(entity))\n"
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
    REQUIRE(GetLuaMemoryUsage() > 0);
    TickLuaRuntime(0.016f);
    const bool output_exists = std::filesystem::exists(output_path);
    INFO("output_path=" << output_path);
    REQUIRE(output_exists);
    ShutdownLuaRuntime();
    ConfigureLuaApiContext(LuaApiContext{});
    SetStartupLuaScriptPath("");
    REQUIRE(GetLuaMemoryUsage() == 0);
}
