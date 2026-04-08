#include "catch/catch.hpp"
#include "engine/scene/scene.h"
#include "engine/ecs/components_2d.h"
#include <fstream>
#include <filesystem>

namespace {

std::string MakeSceneTempPath(const char* filename) {
    auto dir = std::filesystem::temp_directory_path() / "dse_scene_flow_tests";
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

TEST_CASE("Given_BoundExternalWorld_When_GetWorldCalled_Then_SceneUsesExternalWorld", "[engine][unit][scene]") {
    scene::Scene scene("bind-world");
    World external_world;

    scene.BindWorld(&external_world);

    auto entity = scene.GetWorld().CreateEntity();
    scene.GetWorld().registry().emplace<TransformComponent>(entity).position = glm::vec3(1.0f, 2.0f, 3.0f);

    REQUIRE(&scene.GetWorld() == &external_world);
    REQUIRE(external_world.registry().all_of<TransformComponent>(entity));
}

TEST_CASE("Given_UnboundScene_When_GetWorldCalled_Then_RevertsToOwnedWorld", "[engine][unit][scene]") {
    scene::Scene scene("owned-world");
    World external_world;
    scene.BindWorld(&external_world);
    scene.UnbindWorld();

    auto entity = scene.GetWorld().CreateEntity();
    scene.GetWorld().registry().emplace<TransformComponent>(entity);

    REQUIRE(&scene.GetWorld() != &external_world);
    REQUIRE(scene.GetWorld().registry().all_of<TransformComponent>(entity));
    REQUIRE(external_world.registry().storage<TransformComponent>().size() == 0);
}

TEST_CASE("Given_SceneWithComponents_When_SerializedAndDeserialized_Then_CoreDataRoundTrips", "[engine][unit][scene]") {
    const std::string path = MakeSceneTempPath("scene_roundtrip_test.json");
    ScopedTempPath cleanup(path);

    scene::Scene source("source-scene");
    auto entity = source.GetWorld().CreateEntity();
    auto& transform = source.GetWorld().registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(3.0f, 4.0f, 5.0f);
    transform.scale = glm::vec3(2.0f, 2.0f, 1.0f);

    auto& sprite = source.GetWorld().registry().emplace<SpriteRendererComponent>(entity);
    sprite.texture_handle = 99;
    sprite.shader_variant = "SPRITE_LIT";
    sprite.sorting_layer = 7;
    sprite.order_in_layer = 11;
    sprite.visible = false;

    auto& script = source.GetWorld().registry().emplace<ScriptComponent>(entity);
    script.script_path = "script/test.lua";
    script.enabled = false;

    REQUIRE(source.Serialize(path));

    scene::Scene loaded("loaded-scene");
    REQUIRE(loaded.Deserialize(path));
    REQUIRE(loaded.GetName() == "source-scene");

    auto view = loaded.GetWorld().registry().view<TransformComponent>();
    REQUIRE(view.begin() != view.end());
    const auto loaded_entity = *view.begin();
    const auto& loaded_transform = loaded.GetWorld().registry().get<TransformComponent>(loaded_entity);
    REQUIRE(loaded_transform.position.x == Approx(3.0f));
    REQUIRE(loaded_transform.position.y == Approx(4.0f));
    REQUIRE(loaded_transform.position.z == Approx(5.0f));
    REQUIRE(loaded_transform.scale.x == Approx(2.0f));

    REQUIRE(loaded.GetWorld().registry().all_of<SpriteRendererComponent>(loaded_entity));
    const auto& loaded_sprite = loaded.GetWorld().registry().get<SpriteRendererComponent>(loaded_entity);
    REQUIRE(loaded_sprite.texture_handle == 99);
    REQUIRE(loaded_sprite.shader_variant == "SPRITE_LIT");
    REQUIRE(loaded_sprite.sorting_layer == 7);
    REQUIRE(loaded_sprite.order_in_layer == 11);
    REQUIRE_FALSE(loaded_sprite.visible);

    REQUIRE(loaded.GetWorld().registry().all_of<ScriptComponent>(loaded_entity));
    const auto& loaded_script = loaded.GetWorld().registry().get<ScriptComponent>(loaded_entity);
    REQUIRE(loaded_script.script_path == "script/test.lua");
    REQUIRE_FALSE(loaded_script.enabled);
}

TEST_CASE("Given_InvalidSceneJson_When_DeserializeCalled_Then_ReturnsFalse", "[engine][unit][scene]") {
    const std::string path = MakeSceneTempPath("scene_invalid_test.json");
    ScopedTempPath cleanup(path);

    std::ofstream out(path, std::ios::trunc);
    REQUIRE(out.is_open());
    out << "{ invalid json";
    out.close();

    scene::Scene scene("invalid-scene");
    REQUIRE_FALSE(scene.Deserialize(path));
}

TEST_CASE("Given_SceneRegressionHelpers_When_Executed_Then_ReturnTrue", "[engine][unit][scene]") {
    const std::string roundtrip_path = MakeSceneTempPath("scene_regression_roundtrip.json");
    const std::string backward_path = MakeSceneTempPath("scene_regression_backward.json");
    ScopedTempPath cleanup_roundtrip(roundtrip_path);
    ScopedTempPath cleanup_backward(backward_path);

    REQUIRE(scene::RunSceneRoundTripRegressionSample(roundtrip_path));
    REQUIRE(scene::RunSceneBackwardCompatibilityRegressionSample(backward_path));
}
