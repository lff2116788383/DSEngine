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
    mesh.albedo_texture_handle = 1001;
    mesh.normal_texture_handle = 1002;
    mesh.metallic_roughness_texture_handle = 1003;
    mesh.emissive_texture_handle = 1004;
    mesh.occlusion_texture_handle = 1005;
    mesh.receive_shadow = true;
    mesh.visible = true;
    mesh.material_alpha_cutoff = 0.42f;
    mesh.material_alpha_test = true;
    mesh.material_double_sided = true;
    mesh.sorting_layer = 3;
    mesh.order_in_layer = 8;
    mesh.material_data_source = dse::MeshRendererComponent::MaterialDataSource::MaterialInstance;

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
    point_light.falloff = 2.25f;
    point_light.cast_shadow = false;

    auto spot_light_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(spot_light_entity);
    auto& spot_light = source.GetWorld().registry().emplace<dse::SpotLightComponent>(spot_light_entity);
    spot_light.enabled = true;
    spot_light.direction = glm::vec3(0.2f, -1.0f, -0.1f);
    spot_light.color = glm::vec3(1.0f, 0.85f, 0.6f);
    spot_light.intensity = 5.0f;
    spot_light.radius = 18.0f;
    spot_light.falloff = 1.75f;
    spot_light.inner_cone_angle = 18.0f;
    spot_light.outer_cone_angle = 27.5f;
    spot_light.cast_shadow = true;

    auto sky_light_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(sky_light_entity);
    auto& sky_light = source.GetWorld().registry().emplace<dse::SkyLightComponent>(sky_light_entity);
    sky_light.enabled = true;
    sky_light.up_color = glm::vec3(0.45f, 0.55f, 0.8f);
    sky_light.down_color = glm::vec3(0.08f, 0.09f, 0.12f);
    sky_light.intensity = 1.35f;

    auto animator_entity = source.GetWorld().CreateEntity();
    source.GetWorld().registry().emplace<TransformComponent>(animator_entity);
    auto& animator = source.GetWorld().registry().emplace<dse::Animator3DComponent>(animator_entity);
    animator.enabled = true;
    animator.dskel_path = "assets/anims/hero.dskel";
    animator.danim_path = "assets/anims/hero_idle.danim";
    animator.speed = 1.1f;
    animator.loop = true;
    animator.use_anim_tree = true;
    animator.blend_parameter = "speed";
    animator.blend_parameter_value = 3.5f;
    animator.blend_nodes.push_back({"idle", "assets/anims/hero_idle.danim", 0.25f, 0.0f});
    animator.blend_nodes.push_back({"run", "assets/anims/hero_run.danim", 0.75f, 4.0f});

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
    REQUIRE(loaded_mesh.albedo_texture_handle == 1001);
    REQUIRE(loaded_mesh.normal_texture_handle == 1002);
    REQUIRE(loaded_mesh.metallic_roughness_texture_handle == 1003);
    REQUIRE(loaded_mesh.emissive_texture_handle == 1004);
    REQUIRE(loaded_mesh.occlusion_texture_handle == 1005);
    REQUIRE(loaded_mesh.receive_shadow);
    REQUIRE(loaded_mesh.visible);
    REQUIRE(loaded_mesh.material_alpha_cutoff == Approx(0.42f));
    REQUIRE(loaded_mesh.material_alpha_test);
    REQUIRE(loaded_mesh.material_double_sided);
    REQUIRE(loaded_mesh.sorting_layer == 3);
    REQUIRE(loaded_mesh.order_in_layer == 8);
    REQUIRE(loaded_mesh.material_data_source == dse::MeshRendererComponent::MaterialDataSource::MaterialInstance);

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
    REQUIRE(loaded_point_light.falloff == Approx(2.25f));
    REQUIRE_FALSE(loaded_point_light.cast_shadow);

    auto spot_light_view = loaded.GetWorld().registry().view<dse::SpotLightComponent>();
    REQUIRE(spot_light_view.begin() != spot_light_view.end());
    const auto loaded_spot_light_entity = *spot_light_view.begin();
    const auto& loaded_spot_light = spot_light_view.get<dse::SpotLightComponent>(loaded_spot_light_entity);
    REQUIRE(loaded_spot_light.enabled);
    REQUIRE(loaded_spot_light.direction.x == Approx(0.2f));
    REQUIRE(loaded_spot_light.direction.y == Approx(-1.0f));
    REQUIRE(loaded_spot_light.direction.z == Approx(-0.1f));
    REQUIRE(loaded_spot_light.color.y == Approx(0.85f));
    REQUIRE(loaded_spot_light.intensity == Approx(5.0f));
    REQUIRE(loaded_spot_light.radius == Approx(18.0f));
    REQUIRE(loaded_spot_light.falloff == Approx(1.75f));
    REQUIRE(loaded_spot_light.inner_cone_angle == Approx(18.0f));
    REQUIRE(loaded_spot_light.outer_cone_angle == Approx(27.5f));
    REQUIRE(loaded_spot_light.cast_shadow);

    auto sky_light_view = loaded.GetWorld().registry().view<dse::SkyLightComponent>();
    REQUIRE(sky_light_view.begin() != sky_light_view.end());
    const auto loaded_sky_light_entity = *sky_light_view.begin();
    const auto& loaded_sky_light = sky_light_view.get<dse::SkyLightComponent>(loaded_sky_light_entity);
    REQUIRE(loaded_sky_light.enabled);
    REQUIRE(loaded_sky_light.up_color.x == Approx(0.45f));
    REQUIRE(loaded_sky_light.up_color.y == Approx(0.55f));
    REQUIRE(loaded_sky_light.up_color.z == Approx(0.8f));
    REQUIRE(loaded_sky_light.down_color.x == Approx(0.08f));
    REQUIRE(loaded_sky_light.down_color.y == Approx(0.09f));
    REQUIRE(loaded_sky_light.down_color.z == Approx(0.12f));
    REQUIRE(loaded_sky_light.intensity == Approx(1.35f));

    auto animator_view = loaded.GetWorld().registry().view<dse::Animator3DComponent>();
    REQUIRE(animator_view.begin() != animator_view.end());
    const auto loaded_animator_entity = *animator_view.begin();
    const auto& loaded_animator = animator_view.get<dse::Animator3DComponent>(loaded_animator_entity);
    REQUIRE(loaded_animator.enabled);
    REQUIRE(loaded_animator.dskel_path == "assets/anims/hero.dskel");
    REQUIRE(loaded_animator.danim_path == "assets/anims/hero_idle.danim");
    REQUIRE(loaded_animator.speed == Approx(1.1f));
    REQUIRE(loaded_animator.loop);
    REQUIRE(loaded_animator.use_anim_tree);
    REQUIRE(loaded_animator.blend_parameter == "speed");
    REQUIRE(loaded_animator.blend_parameter_value == Approx(3.5f));
    REQUIRE(loaded_animator.blend_nodes.size() == 2);
    REQUIRE(loaded_animator.blend_nodes[0].name == "idle");
    REQUIRE(loaded_animator.blend_nodes[0].danim_path == "assets/anims/hero_idle.danim");
    REQUIRE(loaded_animator.blend_nodes[0].weight == Approx(0.25f));
    REQUIRE(loaded_animator.blend_nodes[0].threshold == Approx(0.0f));
    REQUIRE(loaded_animator.blend_nodes[1].name == "run");
    REQUIRE(loaded_animator.blend_nodes[1].danim_path == "assets/anims/hero_run.danim");
    REQUIRE(loaded_animator.blend_nodes[1].weight == Approx(0.75f));
    REQUIRE(loaded_animator.blend_nodes[1].threshold == Approx(4.0f));

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

TEST_CASE("Given_CheckedInMinimal3DMvpScene_When_Deserialized_Then_Core3DComponentsArePresent", "[engine][unit][scene][3d][mvp]") {
    scene::Scene loaded("3d-mvp-sample");
    REQUIRE(loaded.Deserialize("assets/scenes/3d_mvp_minimal.scene.json"));
    REQUIRE(loaded.GetName() == "3d_mvp_minimal");

    REQUIRE(loaded.GetWorld().registry().view<dse::MeshRendererComponent>().begin() != loaded.GetWorld().registry().view<dse::MeshRendererComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::Camera3DComponent>().begin() != loaded.GetWorld().registry().view<dse::Camera3DComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::DirectionalLight3DComponent>().begin() != loaded.GetWorld().registry().view<dse::DirectionalLight3DComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::PointLightComponent>().begin() != loaded.GetWorld().registry().view<dse::PointLightComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::SkyboxComponent>().begin() != loaded.GetWorld().registry().view<dse::SkyboxComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::TerrainComponent>().begin() != loaded.GetWorld().registry().view<dse::TerrainComponent>().end());
    REQUIRE(scene::RunMinimal3DMvpSceneRegressionSample("assets/scenes/3d_mvp_minimal.scene.json"));
}

TEST_CASE("Given_CheckedInReferenceDemo158Scene_When_Deserialized_Then_ReferenceDemoBaselineComponentsArePresent", "[engine][unit][scene][3d][reference_demo]") {
    scene::Scene loaded("reference-demo-15-8");
    REQUIRE(loaded.Deserialize("assets/scenes/reference_demo_15_8.scene.json"));
    REQUIRE(loaded.GetName() == "reference_demo_15_8");

    auto mesh_view = loaded.GetWorld().registry().view<dse::MeshRendererComponent>();
    REQUIRE(mesh_view.begin() != mesh_view.end());
    size_t mesh_count = 0;
    for (auto it = mesh_view.begin(); it != mesh_view.end(); ++it) {
        ++mesh_count;
    }
    REQUIRE(mesh_count >= 2);

    REQUIRE(loaded.GetWorld().registry().view<dse::Camera3DComponent>().begin() != loaded.GetWorld().registry().view<dse::Camera3DComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::DirectionalLight3DComponent>().begin() != loaded.GetWorld().registry().view<dse::DirectionalLight3DComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::SkyLightComponent>().begin() != loaded.GetWorld().registry().view<dse::SkyLightComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::SkyboxComponent>().begin() != loaded.GetWorld().registry().view<dse::SkyboxComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::Animator3DComponent>().begin() != loaded.GetWorld().registry().view<dse::Animator3DComponent>().end());
}

TEST_CASE("Given_CheckedInReferenceDemo159Scene_When_Deserialized_Then_MaterialInteractionBaselineComponentsArePresent", "[engine][unit][scene][3d][reference_demo]") {
    scene::Scene loaded("reference-demo-15-9");
    REQUIRE(loaded.Deserialize("assets/scenes/reference_demo_15_9.scene.json"));
    REQUIRE(loaded.GetName() == "reference_demo_15_9");

    auto mesh_view = loaded.GetWorld().registry().view<dse::MeshRendererComponent>();
    REQUIRE(mesh_view.begin() != mesh_view.end());
    size_t mesh_count = 0;
    size_t interactive_material_mesh_count = 0;
    for (auto entity : mesh_view) {
        ++mesh_count;
        const auto& mesh = mesh_view.get<dse::MeshRendererComponent>(entity);
        if (mesh.material_instance_id == 430001 || mesh.material_instance_id == 430002) {
            ++interactive_material_mesh_count;
        }
    }
    REQUIRE(mesh_count >= 3);
    REQUIRE(interactive_material_mesh_count == 2);

    auto animator_view = loaded.GetWorld().registry().view<dse::Animator3DComponent>();
    size_t animator_count = 0;
    for (auto entity : animator_view) {
        (void)entity;
        ++animator_count;
    }
    REQUIRE(animator_count == 2);

    REQUIRE(loaded.GetWorld().registry().view<dse::Camera3DComponent>().begin() != loaded.GetWorld().registry().view<dse::Camera3DComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::DirectionalLight3DComponent>().begin() != loaded.GetWorld().registry().view<dse::DirectionalLight3DComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::SkyLightComponent>().begin() != loaded.GetWorld().registry().view<dse::SkyLightComponent>().end());
    REQUIRE(loaded.GetWorld().registry().view<dse::SkyboxComponent>().begin() != loaded.GetWorld().registry().view<dse::SkyboxComponent>().end());
}

TEST_CASE("Given_SceneRegressionHelpers_When_Executed_Then_ReturnTrue", "[engine][unit][scene]") {
    const std::string roundtrip_path = MakeSceneTempPath("scene_regression_roundtrip.json");
    const std::string backward_path = MakeSceneTempPath("scene_regression_backward.json");
    ScopedTempPath cleanup_roundtrip(roundtrip_path);
    ScopedTempPath cleanup_backward(backward_path);

    REQUIRE(scene::RunSceneRoundTripRegressionSample(roundtrip_path));
    REQUIRE(scene::RunSceneBackwardCompatibilityRegressionSample(backward_path));
}
