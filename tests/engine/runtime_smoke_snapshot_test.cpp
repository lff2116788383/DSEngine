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
    auto base = std::filesystem::temp_directory_path() / "dse_runtime_smoke_tests";
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

struct LuaRuntimeSmokeSnapshot {
    bool bootstrap_ok = false;
    bool update_marker_exists = false;
    bool memory_used_after_bootstrap = false;
    bool memory_reset_after_shutdown = false;

    std::string ToDebugString() const {
        std::ostringstream oss;
        oss << "LuaRuntimeSmokeSnapshot{";
        oss << "bootstrap_ok=" << bootstrap_ok;
        oss << ", update_marker_exists=" << update_marker_exists;
        oss << ", memory_used_after_bootstrap=" << memory_used_after_bootstrap;
        oss << ", memory_reset_after_shutdown=" << memory_reset_after_shutdown;
        oss << "}";
        return oss.str();
    }
};

} // namespace

TEST_CASE("Smoke Snapshot - Lua runtime bootstrap update and cleanup remain deterministic", "[engine][smoke][snapshot][lua_runtime]") {
    ScopedLuaApiContextReset scoped_context_reset;
    ScopedLogLevel scoped_logger(dse::debug::LogLevel::Off);

    const std::string startup_path = MakeTempPath("runtime_smoke_startup.lua");
    const std::string component_script_path = MakeTempPath("runtime_smoke_component.lua");
    const std::string output_path = MakeTempPath("runtime_smoke_output.txt");
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

    LuaRuntimeSmokeSnapshot snapshot;
    snapshot.bootstrap_ok = BootstrapLuaRuntime();
    snapshot.memory_used_after_bootstrap = GetLuaMemoryUsage() > 0;

    if (snapshot.bootstrap_ok) {
        TickLuaRuntime(0.016f);
    }

    snapshot.update_marker_exists = std::filesystem::exists(output_path);
    ShutdownLuaRuntime();
    snapshot.memory_reset_after_shutdown = (GetLuaMemoryUsage() == 0);

    INFO(snapshot.ToDebugString());
    REQUIRE(snapshot.bootstrap_ok);
    REQUIRE(snapshot.memory_used_after_bootstrap);
    REQUIRE(snapshot.update_marker_exists);
    REQUIRE(snapshot.memory_reset_after_shutdown);
}
