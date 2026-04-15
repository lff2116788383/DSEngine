#include "catch/catch.hpp"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
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


TEST_CASE("Smoke Snapshot - Lua demo 15.7 setup boots material showcase preview without runtime failure", "[engine][smoke][snapshot][lua_runtime][lua_demo]") {
    ScopedLuaApiContextReset scoped_context_reset;
    dse::debug::SetLogLevel(dse::debug::LogLevel::Off);


    const std::string startup_path = MakeTempPath("runtime_smoke_demo15_7_startup.lua");
    WriteTextFile(
        startup_path,
        "package.path = package.path .. ';samples/lua/?.lua;samples/lua/?/init.lua;samples/lua/?/?.lua'\n"
        "local demo = require('vse_demo.demo15_7')\n"
        "Awake = function() demo.Setup({ title = 'Smoke 15.7' }) end\n"
        "Update = function(dt) demo.Update(dt) end\n");

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    std::size_t transform_count_157 = 0;
    for (auto entity : world.registry().view<TransformComponent>()) {
        (void)entity;
        ++transform_count_157;
    }
    REQUIRE(transform_count_157 == 10u);



    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();
    ConfigureLuaApiContext(LuaApiContext{});
    SetStartupLuaScriptPath("");
}

TEST_CASE("Smoke Snapshot - Lua set_mesh_material supports dmat material index binding", "[engine][smoke][snapshot][lua_runtime][lua_demo]") {
    dse::debug::SetLogLevel(dse::debug::LogLevel::Off);

    const std::string startup_path = MakeTempPath("runtime_smoke_dmat_index_startup.lua");
    WriteTextFile(
        startup_path,
        "local entity0 = dse.ecs.create_entity()\n"
        "local entity1 = dse.ecs.create_entity()\n"
        "Awake = function()\n"
        "  dse.ecs.add_transform(entity0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)\n"
        "  dse.ecs.add_mesh_renderer(entity0, 1.0, 1.0, 1.0, 1.0, { -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5 }, { 0, 1, 2 })\n"
        "  dse.ecs.set_mesh_material(entity0, 'assets/cooked/reference_demo/shared/monster/Monster.dmat', 0)\n"
        "  dse.ecs.add_transform(entity1, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0)\n"
        "  dse.ecs.add_mesh_renderer(entity1, 1.0, 1.0, 1.0, 1.0, { -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5 }, { 0, 1, 2 })\n"
        "  dse.ecs.set_mesh_material(entity1, 'assets/cooked/reference_demo/shared/monster/Monster.dmat', 1)\n"
        "end\n"
        "Update = function(dt) end\n");

    World world;
    AssetManager asset_manager;
    ConfigureLuaApiContext(LuaApiContext{&world, {}, {}, {}, {}, &asset_manager});
    SetStartupLuaScriptPath(startup_path);

    REQUIRE(BootstrapLuaRuntime());
    auto mesh_view = world.registry().view<dse::MeshRendererComponent>();
    REQUIRE(mesh_view.begin() != mesh_view.end());
    std::vector<unsigned int> material_ids;
    for (auto entity : mesh_view) {
        const auto& mesh = mesh_view.get<dse::MeshRendererComponent>(entity);
        REQUIRE(mesh.material_instance_id != 0u);
        REQUIRE(mesh.material_data_source == dse::MeshRendererComponent::MaterialDataSource::MaterialInstance);
        REQUIRE(mesh.albedo_texture_handle != 0u);
        material_ids.push_back(mesh.material_instance_id);
    }
    REQUIRE(material_ids.size() == 2u);
    REQUIRE(material_ids[0] != material_ids[1]);
    ShutdownLuaRuntime();
    ConfigureLuaApiContext(LuaApiContext{});
    SetStartupLuaScriptPath("");
}


TEST_CASE("Smoke Snapshot - Lua demo 15.8 setup loads reference scene without known missing resources", "[engine][smoke][snapshot][lua_runtime][lua_demo][vse_demo_15_8_15_9]") {
    ScopedLuaApiContextReset scoped_context_reset;
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

    auto camera_view = world.registry().view<dse::Camera3DComponent, TransformComponent>();
    REQUIRE(camera_view.begin() != camera_view.end());
    const auto camera_entity = *camera_view.begin();
    const auto& camera = camera_view.get<dse::Camera3DComponent>(camera_entity);
    const auto& camera_transform = camera_view.get<TransformComponent>(camera_entity);
    REQUIRE(camera_transform.position.y == Approx(5.4f));
    REQUIRE(camera_transform.position.z == Approx(15.5f));
    REQUIRE(camera.fov == Approx(52.0f));

    auto light_view = world.registry().view<dse::DirectionalLight3DComponent>();
    REQUIRE(light_view.begin() != light_view.end());
    const auto light_entity = *light_view.begin();
    const auto& light = light_view.get<dse::DirectionalLight3DComponent>(light_entity);
    REQUIRE(light.intensity == Approx(2.15f));
    REQUIRE(light.shadow_strength == Approx(0.52f));

    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();
    ConfigureLuaApiContext(LuaApiContext{});
    SetStartupLuaScriptPath("");
}


TEST_CASE("Smoke Snapshot - Lua demo 15.9 setup loads reference scene and keeps material showcase entities bound", "[engine][smoke][snapshot][lua_runtime][lua_demo][vse_demo_15_8_15_9]") {
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

    auto camera_view = world.registry().view<dse::Camera3DComponent, TransformComponent>();
    REQUIRE(camera_view.begin() != camera_view.end());
    const auto camera_entity = *camera_view.begin();
    const auto& camera = camera_view.get<dse::Camera3DComponent>(camera_entity);
    const auto& camera_transform = camera_view.get<TransformComponent>(camera_entity);
    REQUIRE(camera_transform.position.y == Approx(6.2f));
    REQUIRE(camera_transform.position.z == Approx(17.8f));
    REQUIRE(camera.fov == Approx(50.0f));

    auto mesh_view = world.registry().view<dse::MeshRendererComponent, TransformComponent>();
    bool left_found = false;
    bool right_found = false;
    for (auto entity : mesh_view) {
        const auto& mesh = mesh_view.get<dse::MeshRendererComponent>(entity);
        const auto& transform = mesh_view.get<TransformComponent>(entity);
        if (transform.position.y > -1.0f && transform.position.x < -1.0f) {
            left_found = true;
            REQUIRE(mesh.metallic == Approx(0.03f));
            REQUIRE(mesh.roughness == Approx(0.78f));
        }
        if (transform.position.y > -1.0f && transform.position.x > 1.0f) {
            right_found = true;
            REQUIRE(mesh.metallic == Approx(0.38f));
            REQUIRE(mesh.roughness == Approx(0.18f));
        }
    }

    INFO("15.9 runtime expects one left and one right showcase mesh above ground");



    REQUIRE(left_found);
    REQUIRE(right_found);

    TickLuaRuntime(0.016f);
    ShutdownLuaRuntime();
    ConfigureLuaApiContext(LuaApiContext{});
    SetStartupLuaScriptPath("");
}


