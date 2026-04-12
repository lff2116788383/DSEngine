#include "catch/catch.hpp"

#include "engine/scene/scene.h"
#include "engine/ecs/components_2d.h"
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

TEST_CASE("Given_ReferenceDemo158Scene_When_CopiedIntoEditorRegistry_Then_EditorSceneIoRoundTripKeepsReferenceBaseline", "[engine][unit][scene][3d][editor][reference_demo]") {
    scene::Scene runtime_scene("runtime-reference-demo-15-8");
    REQUIRE(runtime_scene.Deserialize("assets/scenes/reference_demo_15_8.scene.json"));

    entt::registry editor_registry;
    CopyRegistry(editor_registry, runtime_scene.GetWorld().registry());

    const std::string path = MakeEditorSceneTempPath("editor_scene_reference_demo_15_8_bridge.json");
    ScopedTempPath cleanup(path);

    SaveScene(editor_registry, path);

    entt::registry loaded_registry;
    LoadScene(loaded_registry, path);

    auto mesh_view = loaded_registry.view<dse::MeshRendererComponent>();
    REQUIRE(mesh_view.begin() != mesh_view.end());
    size_t mesh_count = 0;
    for (auto it = mesh_view.begin(); it != mesh_view.end(); ++it) {
        ++mesh_count;
    }
    REQUIRE(mesh_count >= 2);
    REQUIRE(loaded_registry.view<dse::Camera3DComponent>().begin() != loaded_registry.view<dse::Camera3DComponent>().end());
    REQUIRE(loaded_registry.view<dse::DirectionalLight3DComponent>().begin() != loaded_registry.view<dse::DirectionalLight3DComponent>().end());
    REQUIRE(loaded_registry.view<dse::SkyLightComponent>().begin() != loaded_registry.view<dse::SkyLightComponent>().end());
    REQUIRE(loaded_registry.view<dse::SkyboxComponent>().begin() != loaded_registry.view<dse::SkyboxComponent>().end());
    REQUIRE(loaded_registry.view<dse::Animator3DComponent>().begin() != loaded_registry.view<dse::Animator3DComponent>().end());

    const auto mesh_entity = *mesh_view.begin();
    const auto& mesh = mesh_view.get<dse::MeshRendererComponent>(mesh_entity);
    REQUIRE(mesh.visible);
}

TEST_CASE("Given_ReferenceDemo159Scene_When_CopiedIntoEditorRegistry_Then_EditorSceneIoRoundTripKeepsMaterialInteractionBaseline", "[engine][unit][scene][3d][editor][reference_demo]") {
    scene::Scene runtime_scene("runtime-reference-demo-15-9");
    REQUIRE(runtime_scene.Deserialize("assets/scenes/reference_demo_15_9.scene.json"));

    entt::registry editor_registry;
    CopyRegistry(editor_registry, runtime_scene.GetWorld().registry());

    const std::string path = MakeEditorSceneTempPath("editor_scene_reference_demo_15_9_bridge.json");
    ScopedTempPath cleanup(path);

    SaveScene(editor_registry, path);

    entt::registry loaded_registry;
    LoadScene(loaded_registry, path);

    auto mesh_view = loaded_registry.view<dse::MeshRendererComponent>();
    REQUIRE(mesh_view.begin() != mesh_view.end());
    size_t mesh_count = 0;
    size_t interactive_material_mesh_count = 0;
    for (auto entity : mesh_view) {
        ++mesh_count;
        const auto& loaded_mesh = mesh_view.get<dse::MeshRendererComponent>(entity);
        if (loaded_mesh.material_instance_id == 430001 || loaded_mesh.material_instance_id == 430002) {
            ++interactive_material_mesh_count;
        }
    }
    REQUIRE(mesh_count >= 3);
    REQUIRE(interactive_material_mesh_count == 2);
    REQUIRE(loaded_registry.view<dse::Camera3DComponent>().begin() != loaded_registry.view<dse::Camera3DComponent>().end());
    REQUIRE(loaded_registry.view<dse::DirectionalLight3DComponent>().begin() != loaded_registry.view<dse::DirectionalLight3DComponent>().end());
    REQUIRE(loaded_registry.view<dse::SkyLightComponent>().begin() != loaded_registry.view<dse::SkyLightComponent>().end());
    REQUIRE(loaded_registry.view<dse::SkyboxComponent>().begin() != loaded_registry.view<dse::SkyboxComponent>().end());

    auto animator_view = loaded_registry.view<dse::Animator3DComponent>();
    size_t animator_count = 0;
    for (auto entity : animator_view) {
        (void)entity;
        ++animator_count;
    }
    REQUIRE(animator_count == 2);
}

TEST_CASE("Given_RuntimeOnly2DState_When_CopiedBetweenRegistries_Then_RuntimeFieldsAreResetForPlayExitRestore", "[engine][unit][scene][editor][copy_registry]") {
    entt::registry runtime_registry;
    const auto entity = runtime_registry.create();

    auto& label = runtime_registry.emplace<UILabelComponent>(entity);
    label.text = "Score: 99";
    label.dirty = false;
    label.runtime_glyph_entities = {entt::entity{42}, entt::entity{77}};

    auto& rigidbody = runtime_registry.emplace<RigidBody2DComponent>(entity);
    rigidbody.type = RigidBody2DType::Dynamic;
    rigidbody.velocity = glm::vec2(4.0f, -2.0f);
    rigidbody.gravity_scale = 2.5f;
    rigidbody.fixed_rotation = true;
    rigidbody.runtime_body = reinterpret_cast<b2Body*>(static_cast<uintptr_t>(0x1));

    auto& emitter = runtime_registry.emplace<ParticleEmitterComponent>(entity);
    emitter.max_particles = 64;
    emitter.emit_rate = 12.0f;
    emitter.emit_rate_scale = 1.5f;
    emitter.emit_accumulator = 3.25f;
    emitter.pending_burst = 5;
    emitter.particles.push_back(Particle2D{});
    emitter.particles.push_back(Particle2D{});

    entt::registry copied_registry;
    CopyRegistry(copied_registry, runtime_registry);

    auto copied_view = copied_registry.view<UILabelComponent, RigidBody2DComponent, ParticleEmitterComponent>();
    REQUIRE(copied_view.begin() != copied_view.end());
    const auto copied_entity = *copied_view.begin();

    const auto& copied_label = copied_registry.get<UILabelComponent>(copied_entity);
    REQUIRE(copied_label.text == "Score: 99");
    REQUIRE(copied_label.dirty);
    REQUIRE(copied_label.runtime_glyph_entities.empty());

    const auto& copied_rigidbody = copied_registry.get<RigidBody2DComponent>(copied_entity);
    REQUIRE(copied_rigidbody.type == RigidBody2DType::Dynamic);
    REQUIRE(copied_rigidbody.velocity.x == Approx(4.0f));
    REQUIRE(copied_rigidbody.velocity.y == Approx(-2.0f));
    REQUIRE(copied_rigidbody.gravity_scale == Approx(2.5f));
    REQUIRE(copied_rigidbody.fixed_rotation);
    REQUIRE(copied_rigidbody.runtime_body == nullptr);

    const auto& copied_emitter = copied_registry.get<ParticleEmitterComponent>(copied_entity);
    REQUIRE(copied_emitter.max_particles == 64);
    REQUIRE(copied_emitter.emit_rate == Approx(12.0f));
    REQUIRE(copied_emitter.emit_rate_scale == Approx(1.5f));
    REQUIRE(copied_emitter.emit_accumulator == Approx(0.0f));
    REQUIRE(copied_emitter.pending_burst == 0);
    REQUIRE(copied_emitter.particles.empty());
}
