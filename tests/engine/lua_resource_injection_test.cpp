#include "catch/catch.hpp"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"

#include <filesystem>
#include <string>

using dse::runtime::BootstrapLuaRuntime;
using dse::runtime::ConfigureLuaApiContext;
using dse::runtime::GetLuaMemoryUsage;
using dse::runtime::LuaApiContext;
using dse::runtime::SetStartupLuaScriptPath;
using dse::runtime::ShutdownLuaRuntime;

namespace {
std::string MakeTempPath(const char* name) {
    auto base = std::filesystem::temp_directory_path() / "dse_lua_runtime_tests";
    std::filesystem::create_directories(base);
    return (base / name).string();
}

class ScopedLuaApiContextReset {
public:
    ~ScopedLuaApiContextReset() {
        ConfigureLuaApiContext(LuaApiContext{});
        SetStartupLuaScriptPath("");
    }
};
}

TEST_CASE("Given_MissingStartupScript_When_BootstrapFails_Then_LuaMemoryUsageResetsToZero", "[engine][unit][resource_injection][lua_runtime]") {
    ScopedLuaApiContextReset scoped_context_reset;
    dse::debug::SetLogLevel(dse::debug::LogLevel::Off);

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(MakeTempPath("missing_startup.lua"));

    REQUIRE_FALSE(BootstrapLuaRuntime());
    REQUIRE(GetLuaMemoryUsage() == 0);

    ShutdownLuaRuntime();
}
