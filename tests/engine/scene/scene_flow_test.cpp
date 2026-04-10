#include "catch/catch.hpp"
#include "engine/scene/scene.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
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

TEST_CASE("Given_Minimal3DScene_When_SerializedAndDeserialized_Then_Core3DComponentsRoundTrip", "[engine][unit][scene][3d]") {
    const std::string path = MakeSceneTempPath("scene_roundtrip_3d_test.json");
    ScopedTempPath cleanup(path);

    scene::Scene source("source-3d-scene");

    auto mesh_entity = source.GetWorld().CreateEntity();
    auto& transform = source.GetWorld().registry().emplace<TransformComponent>(mesh_entity);
    transform.position = glm::vec3(10.0f, 2.0f, -5.0f);
    transform.scale = glm::vec3(1.5f, 2.0f, 1.5f);

    auto& mesh = source.GetWorld().registry().emplace<dse::MeshRendererComponent>(mesh_entity);
    mesh.mesh_path = "assets/meshes/crate.fbx";
    mesh.material_instance_id = 42;
    mesh.shader_variant = "MESH_PBR";
    mesh.color = glm::vec4(0.9f, 0.8f, 0.7f, 1.0f);
    mesh.emissive = glm::vec3(0.1f, 0.2f, 0.3f);
    mesh.metallic = 0.65f;
    mesh.roughness = 0.35f;
    mesh.ao = 0.8f;
    mesh.normal_strength = 1.25f;
    mesh.receive_shadow = true;
    mesh.visible = true;

    auto camera_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(camera_entity);
    auto& camera = source.GetWorld().registry().emplace<dse::Camera3DComponent>(camera_entity);
    camera.enabled = true;
    camera.priority = 7;
    camera.fov = 75.0f;
    camera.aspect_ratio = 16.0f / 9.0f;
    camera.near_clip = 0.3f;
    camera.far_clip = 2048.0f;

    auto light_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(light_entity);
    auto& light = source.GetWorld().registry().emplace<dse::DirectionalLight3DComponent>(light_entity);
    light.enabled = true;
    light.direction = glm::vec3(-0.3f, -1.0f, -0.2f);
    light.color = glm::vec3(1.0f, 0.95f, 0.9f);
    light.intensity = 2.0f;
    light.ambient_intensity = 0.15f;
    light.shadow_strength = 0.5f;
    light.cast_shadow = true;
    light.cascade_splits[0] = 15.0f;
    light.cascade_splits[1] = 50.0f;
    light.cascade_splits[2] = 180.0f;

    auto skybox_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(skybox_entity);
    auto& skybox = source.GetWorld().registry().emplace<dse::SkyboxComponent>(skybox_entity);
    skybox.enabled = true;
    skybox.cubemap_handle = 77;
    skybox.cubemap_path = "assets/skyboxes/default_sky";

    auto point_light_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(point_light_entity);
    auto& point_light = source.GetWorld().registry().emplace<dse::PointLightComponent>(point_light_entity);
    point_light.enabled = true;
    point_light.color = glm::vec3(0.8f, 0.7f, 1.0f);
    point_light.intensity = 3.0f;
    point_light.radius = 12.5f;
    point_light.cast_shadow = false;

    auto animator_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(animator_entity);
    auto& animator = source.GetWorld().registry().emplace<dse::Animator3DComponent>(animator_entity);
    animator.enabled = true;
    animator.dskel_path = "assets/anims/hero.dskel";
    animator.danim_path = "assets/anims/hero_idle.danim";
    animator.speed = 1.1f;
    animator.loop = true;
    animator.use_anim_tree = false;

    auto terrain_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(terrain_entity);
    auto& terrain = source.GetWorld().registry().emplace<dse::TerrainComponent>(terrain_entity);
    terrain.enabled = true;
    terrain.heightmap_path = "assets/terrain/height.png";
    terrain.texture_handle = 88;
    terrain.width = 256.0f;
    terrain.depth = 256.0f;
    terrain.max_height = 32.0f;
    terrain.resolution_x = 128;
    terrain.resolution_z = 128;
    terrain.use_dynamic_lod = true;
    terrain.max_lod_levels = 5;
    terrain.lod_distance_factor = 80.0f;
    terrain.visible = true;

    REQUIRE(source.Serialize(path));

    scene::Scene loaded("loaded-3d-scene");
    REQUIRE(loaded.Deserialize(path));
    REQUIRE(loaded.GetName() == "source-3d-scene");

    auto mesh_view = loaded.GetWorld().registry().view<TransformComponent, dse::MeshRendererComponent>();
    REQUIRE(mesh_view.begin() != mesh_view.end());
    const auto loaded_mesh_entity = *mesh_view.begin();
    const auto& loaded_transform = mesh_view.get<TransformComponent>(loaded_mesh_entity);
    const auto& loaded_mesh = mesh_view.get<dse::MeshRendererComponent>(loaded_mesh_entity);
    REQUIRE(loaded_transform.position.x == Approx(10.0f));
    REQUIRE(loaded_transform.position.y == Approx(2.0f));
    REQUIRE(loaded_transform.position.z == Approx(-5.0f));
    REQUIRE(loaded_mesh.mesh_path == "assets/meshes/crate.fbx");
    REQUIRE(loaded_mesh.material_instance_id == 42);
    REQUIRE(loaded_mesh.shader_variant == "MESH_PBR");
    REQUIRE(loaded_mesh.color.r == Approx(0.9f));
    REQUIRE(loaded_mesh.emissive.z == Approx(0.3f));
    REQUIRE(loaded_mesh.metallic == Approx(0.65f));
    REQUIRE(loaded_mesh.roughness == Approx(0.35f));
    REQUIRE(loaded_mesh.ao == Approx(0.8f));
    REQUIRE(loaded_mesh.normal_strength == Approx(1.25f));
    REQUIRE(loaded_mesh.receive_shadow);
    REQUIRE(loaded_mesh.visible);

    auto camera_view = loaded.GetWorld().registry().view<dse::Camera3DComponent>();
    REQUIRE(camera_view.begin() != camera_view.end());
    const auto loaded_camera_entity = *camera_view.begin();
    const auto& loaded_camera = camera_view.get<dse::Camera3DComponent>(loaded_camera_entity);
    REQUIRE(loaded_camera.enabled);
    REQUIRE(loaded_camera.priority == 7);
    REQUIRE(loaded_camera.fov == Approx(75.0f));
    REQUIRE(loaded_camera.aspect_ratio == Approx(16.0f / 9.0f));
    REQUIRE(loaded_camera.near_clip == Approx(0.3f));
    REQUIRE(loaded_camera.far_clip == Approx(2048.0f));

    auto light_view = loaded.GetWorld().registry().view<dse::DirectionalLight3DComponent>();
    REQUIRE(light_view.begin() != light_view.end());
    const auto loaded_light_entity = *light_view.begin();
    const auto& loaded_light = light_view.get<dse::DirectionalLight3DComponent>(loaded_light_entity);
    REQUIRE(loaded_light.enabled);
    REQUIRE(loaded_light.direction.x == Approx(-0.3f));
    REQUIRE(loaded_light.color.y == Approx(0.95f));
    REQUIRE(loaded_light.intensity == Approx(2.0f));
    REQUIRE(loaded_light.ambient_intensity == Approx(0.15f));
    REQUIRE(loaded_light.shadow_strength == Approx(0.5f));
    REQUIRE(loaded_light.cast_shadow);
    REQUIRE(loaded_light.cascade_splits[0] == Approx(15.0f));
    REQUIRE(loaded_light.cascade_splits[1] == Approx(50.0f));
    REQUIRE(loaded_light.cascade_splits[2] == Approx(180.0f));

    auto skybox_view = loaded.GetWorld().registry().view<dse::SkyboxComponent>();
    REQUIRE(skybox_view.begin() != skybox_view.end());
    const auto loaded_skybox_entity = *skybox_view.begin();
    const auto& loaded_skybox = skybox_view.get<dse::SkyboxComponent>(loaded_skybox_entity);
    REQUIRE(loaded_skybox.enabled);
    REQUIRE(loaded_skybox.cubemap_handle == 77);
    REQUIRE(loaded_skybox.cubemap_path == "assets/skyboxes/default_sky");

    auto point_light_view = loaded.GetWorld().registry().view<dse::PointLightComponent>();
    REQUIRE(point_light_view.begin() != point_light_view.end());
    const auto loaded_point_light_entity = *point_light_view.begin();
    const auto& loaded_point_light = point_light_view.get<dse::PointLightComponent>(loaded_point_light_entity);
    REQUIRE(loaded_point_light.enabled);
    REQUIRE(loaded_point_light.color.x == Approx(0.8f));
    REQUIRE(loaded_point_light.color.y == Approx(0.7f));
    REQUIRE(loaded_point_light.color.z == Approx(1.0f));
    REQUIRE(loaded_point_light.intensity == Approx(3.0f));
    REQUIRE(loaded_point_light.radius == Approx(12.5f));
    REQUIRE_FALSE(loaded_point_light.cast_shadow);

    auto animator_view = loaded.GetWorld().registry().view<dse::Animator3DComponent>();
    REQUIRE(animator_view.begin() != animator_view.end());
    const auto loaded_animator_entity = *animator_view.begin();
    const auto& loaded_animator = animator_view.get<dse::Animator3DComponent>(loaded_animator_entity);
    REQUIRE(loaded_animator.enabled);
    REQUIRE(loaded_animator.dskel_path == "assets/anims/hero.dskel");
    REQUIRE(loaded_animator.danim_path == "assets/anims/hero_idle.danim");
    REQUIRE(loaded_animator.speed == Approx(1.1f));
    REQUIRE(loaded_animator.loop);
    REQUIRE_FALSE(loaded_animator.use_anim_tree);

    auto terrain_view = loaded.GetWorld().registry().view<dse::TerrainComponent>();
    REQUIRE(terrain_view.begin() != terrain_view.end());
    const auto loaded_terrain_entity = *terrain_view.begin();
    const auto& loaded_terrain = terrain_view.get<dse::TerrainComponent>(loaded_terrain_entity);
    REQUIRE(loaded_terrain.enabled);
    REQUIRE(loaded_terrain.heightmap_path == "assets/terrain/height.png");
    REQUIRE(loaded_terrain.texture_handle == 88);
    REQUIRE(loaded_terrain.width == Approx(256.0f));
    REQUIRE(loaded_terrain.depth == Approx(256.0f));
    REQUIRE(loaded_terrain.max_height == Approx(32.0f));
    REQUIRE(loaded_terrain.resolution_x == 128);
    REQUIRE(loaded_terrain.resolution_z == 128);
    REQUIRE(loaded_terrain.use_dynamic_lod);
    REQUIRE(loaded_terrain.max_lod_levels == 5);
    REQUIRE(loaded_terrain.lod_distance_factor == Approx(80.0f));
    REQUIRE(loaded_terrain.visible);
}

TEST_CASE("Given_SceneRegressionHelpers_When_Executed_Then_ReturnTrue", "[engine][unit][scene]") {
    const std::string roundtrip_path = MakeSceneTempPath("scene_regression_roundtrip.json");
    const std::string backward_path = MakeSceneTempPath("scene_regression_backward.json");
    ScopedTempPath cleanup_roundtrip(roundtrip_path);
    ScopedTempPath cleanup_backward(backward_path);

    REQUIRE(scene::RunSceneRoundTripRegressionSample(roundtrip_path));
    REQUIRE(scene::RunSceneBackwardCompatibilityRegressionSample(backward_path));
}
