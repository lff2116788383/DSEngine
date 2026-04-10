#include "catch/catch.hpp"

#include "engine/scene/scene.h"
#include "engine/ecs/components_3d.h"
#include "apps/editor_cpp/src/editor_scene_io.h"

#include <filesystem>
#include <string>

namespace {

std::string MakeEditorSceneTempPath(const char* filename) {
    auto dir = std::filesystem::temp_directory_path() / "dse_editor_scene_bridge_tests";
    std::filesystem::create_directories(dir);
    return (dir / filename).string();
}

struct ScopedTempPath {
    explicit ScopedTempPath(std::string p) : path(std::move(p)) {}
    ~ScopedTempPath() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    std::string path;
};

} // namespace

TEST_CASE("Given_Engine3DMvpScene_When_CopiedIntoEditorRegistry_Then_EditorSceneIoRoundTripKeepsCore3DComponents", "[engine][unit][scene][3d][editor]") {
    scene::Scene runtime_scene("runtime-3d-mvp");
    REQUIRE(runtime_scene.Deserialize("assets/scenes/3d_mvp_minimal.scene.json"));

    entt::registry editor_registry;
    CopyRegistry(editor_registry, runtime_scene.GetWorld().registry());

    const std::string path = MakeEditorSceneTempPath("editor_scene_3d_bridge.json");
    ScopedTempPath cleanup(path);

    SaveScene(editor_registry, path);

    entt::registry loaded_registry;
    LoadScene(loaded_registry, path);

    REQUIRE(loaded_registry.view<dse::MeshRendererComponent>().begin() != loaded_registry.view<dse::MeshRendererComponent>().end());
    REQUIRE(loaded_registry.view<dse::Camera3DComponent>().begin() != loaded_registry.view<dse::Camera3DComponent>().end());
    REQUIRE(loaded_registry.view<dse::DirectionalLight3DComponent>().begin() != loaded_registry.view<dse::DirectionalLight3DComponent>().end());
    REQUIRE(loaded_registry.view<dse::PointLightComponent>().begin() != loaded_registry.view<dse::PointLightComponent>().end());
    REQUIRE(loaded_registry.view<dse::SkyboxComponent>().begin() != loaded_registry.view<dse::SkyboxComponent>().end());
    REQUIRE(loaded_registry.view<dse::Animator3DComponent>().begin() != loaded_registry.view<dse::Animator3DComponent>().end());
    REQUIRE(loaded_registry.view<dse::TerrainComponent>().begin() != loaded_registry.view<dse::TerrainComponent>().end());

    auto mesh_view = loaded_registry.view<dse::MeshRendererComponent>();
    const auto mesh_entity = *mesh_view.begin();
    const auto& mesh = mesh_view.get<dse::MeshRendererComponent>(mesh_entity);
    REQUIRE(mesh.mesh_path == "assets/meshes/mvp_cube.fbx");
    REQUIRE(mesh.visible);

    auto camera_view = loaded_registry.view<dse::Camera3DComponent>();
    const auto camera_entity = *camera_view.begin();
    const auto& camera = camera_view.get<dse::Camera3DComponent>(camera_entity);
    REQUIRE(camera.enabled);
    REQUIRE(camera.fov == Approx(60.0f));

    auto light_view = loaded_registry.view<dse::DirectionalLight3DComponent>();
    const auto light_entity = *light_view.begin();
    const auto& light = light_view.get<dse::DirectionalLight3DComponent>(light_entity);
    REQUIRE(light.enabled);
    REQUIRE(light.intensity == Approx(1.8f));
    REQUIRE(light.cast_shadow);
}
