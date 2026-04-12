#include "catch/catch.hpp"

#include <algorithm>

#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/world.h"

namespace {

class RecordingCommandBuffer final : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc&) override {}
    void EndRenderPass() override {}
    void SetPipelineState(unsigned int) override {}
    void SetCamera(const glm::mat4&, const glm::mat4&) override {}
    void DrawBatch(const std::vector<DrawBatchItem>&) override {}
    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override {
        draw_mesh_batch_calls++;
        last_mesh_batch = items;
    }
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>&) override {}
    void ClearColor(const glm::vec4&) override {}
    void SetGlobalMat4(const std::string&, const glm::mat4&) override {}
    void SetGlobalMat4Array(const std::string&, const std::vector<glm::mat4>&) override {}
    void SetGlobalFloatArray(const std::string&, const std::vector<float>&) override {}
    void DrawSkybox(unsigned int) override {}
    void DrawPostProcess(unsigned int, const std::string&, const std::vector<float>&) override {}
    void DrawParticles3D(const std::vector<Particle3DDrawItem>&, const glm::mat4&, const glm::mat4&) override {}

    int draw_mesh_batch_calls = 0;
    std::vector<MeshDrawItem> last_mesh_batch;
};

void AttachMinimalRenderableMesh(World& world, Entity entity) {
    auto& mesh = world.registry().get<dse::MeshRendererComponent>(entity);
    mesh.temp_vertices = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };
    mesh.temp_indices = {0, 1, 2};
}

} // namespace

TEST_CASE("Given_MeshRendererPrefersComponentFallback_When_Rendering_Then_ComponentMaterialFieldsReachDrawItem", "[engine][unit][3d][material][mesh_render_system]") {
    World world;
    dse::gameplay3d::MeshRenderSystem system;
    RecordingCommandBuffer cmd;
    AssetManager asset_manager;
    system.SetAssetManager(&asset_manager);

    const auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.local_to_world = glm::mat4(1.0f);

    auto& mesh = world.registry().emplace<dse::MeshRendererComponent>(entity);
    mesh.visible = true;
    mesh.shader_variant = "MESH_UNLIT";
    mesh.color = glm::vec4(0.7f, 0.6f, 0.5f, 1.0f);
    mesh.emissive = glm::vec3(0.1f, 0.2f, 0.3f);
    mesh.metallic = 0.25f;
    mesh.roughness = 0.75f;
    mesh.ao = 0.85f;
    mesh.normal_strength = 1.4f;
    mesh.material_alpha_cutoff = 0.61f;
    mesh.albedo_texture_handle = 21;
    mesh.normal_texture_handle = 22;
    mesh.material_data_source = dse::MeshRendererComponent::MaterialDataSource::ComponentFallback;
    AttachMinimalRenderableMesh(world, entity);

    system.Render(world, cmd);

    REQUIRE(cmd.draw_mesh_batch_calls == 1);
    REQUIRE(cmd.last_mesh_batch.size() == 1);
    const auto& item = cmd.last_mesh_batch.front();
    REQUIRE_FALSE(item.material_uses_instance_data);
    REQUIRE(item.texture_handle == 21);
    REQUIRE(item.normal_map_handle == 22);
    REQUIRE(item.material_albedo.r == Approx(0.7f));
    REQUIRE(item.material_emissive.z == Approx(0.3f));
    REQUIRE(item.material_metallic == Approx(0.25f));
    REQUIRE(item.material_roughness == Approx(0.75f));
    REQUIRE(item.material_ao == Approx(0.85f));
    REQUIRE(item.material_normal_strength == Approx(1.4f));
    REQUIRE(item.material_alpha_cutoff == Approx(0.61f));
    REQUIRE_FALSE(item.lighting_enabled);
}

TEST_CASE("Given_MeshRendererPrefersMaterialInstance_When_Rendering_Then_MaterialInstanceFieldsOverrideComponentFallback", "[engine][unit][3d][material][mesh_render_system]") {
    World world;
    dse::gameplay3d::MeshRenderSystem system;
    RecordingCommandBuffer cmd;
    AssetManager asset_manager;
    system.SetAssetManager(&asset_manager);

    auto material = asset_manager.CreateMaterialInstance("mesh_pbr_runtime_resolution");
    REQUIRE(material != nullptr);
    material->SetShaderVariant("MESH_PBR");
    material->SetTextureHandle(44);
    material->SetBaseColor(glm::vec4(0.2f, 0.4f, 0.6f, 1.0f));
    material->SetEmissiveColor(glm::vec3(0.9f, 0.1f, 0.2f));
    MaterialAsset::TextureSlots slots = material->GetTextureSlots();
    slots.albedo = 41;
    slots.normal = 42;
    material->SetTextureSlots(slots);
    MaterialAsset::ScalarOverrides scalars = material->GetScalarOverrides();
    scalars.metallic = 0.91f;
    scalars.roughness = 0.14f;
    scalars.ao = 0.73f;
    scalars.normal_strength = 1.8f;
    scalars.alpha_cutoff = 0.27f;
    material->SetScalarOverrides(scalars);

    const auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.local_to_world = glm::mat4(1.0f);

    auto& mesh = world.registry().emplace<dse::MeshRendererComponent>(entity);
    mesh.visible = true;
    mesh.material_instance_id = material->GetId();
    mesh.material_data_source = dse::MeshRendererComponent::MaterialDataSource::MaterialInstance;
    mesh.shader_variant = "MESH_UNLIT";
    mesh.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    mesh.emissive = glm::vec3(0.0f);
    mesh.metallic = 0.0f;
    mesh.roughness = 1.0f;
    mesh.ao = 0.2f;
    mesh.normal_strength = 0.4f;
    mesh.material_alpha_cutoff = 0.88f;
    mesh.albedo_texture_handle = 99;
    mesh.normal_texture_handle = 98;
    AttachMinimalRenderableMesh(world, entity);

    system.Render(world, cmd);

    REQUIRE(cmd.draw_mesh_batch_calls == 1);
    REQUIRE(cmd.last_mesh_batch.size() == 1);
    const auto& item = cmd.last_mesh_batch.front();
    REQUIRE(item.material_uses_instance_data);
    REQUIRE(item.texture_handle == 41);
    REQUIRE(item.normal_map_handle == 42);
    REQUIRE(item.material_albedo.r == Approx(0.2f));
    REQUIRE(item.material_albedo.g == Approx(0.4f));
    REQUIRE(item.material_albedo.b == Approx(0.6f));
    REQUIRE(item.material_emissive.x == Approx(0.9f));
    REQUIRE(item.material_emissive.z == Approx(0.2f));
    REQUIRE(item.material_metallic == Approx(0.91f));
    REQUIRE(item.material_roughness == Approx(0.14f));
    REQUIRE(item.material_ao == Approx(0.73f));
    REQUIRE(item.material_normal_strength == Approx(1.8f));
    REQUIRE(item.material_alpha_cutoff == Approx(0.27f));
    REQUIRE(item.lighting_enabled);
}

TEST_CASE("Given_MultipleLightTypes_When_RenderingMesh_Then_DrawItemCollectsDirectionalPointSpotAndSkyContributions", "[engine][unit][3d][lighting][mesh_render_system]") {
    World world;
    dse::gameplay3d::MeshRenderSystem system;
    RecordingCommandBuffer cmd;
    AssetManager asset_manager;
    system.SetAssetManager(&asset_manager);

    const auto mesh_entity = world.CreateEntity();
    auto& mesh_transform = world.registry().emplace<TransformComponent>(mesh_entity);
    mesh_transform.local_to_world = glm::mat4(1.0f);
    auto& mesh = world.registry().emplace<dse::MeshRendererComponent>(mesh_entity);
    mesh.visible = true;
    mesh.shader_variant = "MESH_PBR";
    mesh.emissive = glm::vec3(0.2f, 0.3f, 0.4f);
    AttachMinimalRenderableMesh(world, mesh_entity);

    const auto directional_entity = world.CreateEntity();
    auto& directional = world.registry().emplace<dse::DirectionalLight3DComponent>(directional_entity);
    directional.direction = glm::vec3(-0.2f, -1.0f, -0.4f);
    directional.color = glm::vec3(0.9f, 0.8f, 0.7f);
    directional.intensity = 2.5f;
    directional.ambient_intensity = 0.15f;
    directional.shadow_strength = 0.65f;

    const auto point_a_entity = world.CreateEntity();
    auto& point_a_transform = world.registry().emplace<TransformComponent>(point_a_entity);
    point_a_transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    auto& point_a = world.registry().emplace<dse::PointLightComponent>(point_a_entity);
    point_a.color = glm::vec3(1.0f, 0.2f, 0.1f);
    point_a.intensity = 3.0f;
    point_a.radius = 8.0f;
    point_a.falloff = 1.5f;
    point_a.cast_shadow = true;

    const auto point_b_entity = world.CreateEntity();
    auto& point_b_transform = world.registry().emplace<TransformComponent>(point_b_entity);
    point_b_transform.position = glm::vec3(-4.0f, 1.0f, 0.5f);
    auto& point_b = world.registry().emplace<dse::PointLightComponent>(point_b_entity);
    point_b.color = glm::vec3(0.3f, 0.4f, 1.0f);
    point_b.intensity = 1.5f;
    point_b.radius = 6.0f;
    point_b.falloff = 0.05f;

    const auto disabled_point_entity = world.CreateEntity();
    auto& disabled_point_transform = world.registry().emplace<TransformComponent>(disabled_point_entity);
    disabled_point_transform.position = glm::vec3(9.0f, 9.0f, 9.0f);
    auto& disabled_point = world.registry().emplace<dse::PointLightComponent>(disabled_point_entity);
    disabled_point.enabled = false;
    disabled_point.intensity = 99.0f;

    const auto sky_entity = world.CreateEntity();
    auto& sky = world.registry().emplace<dse::SkyLightComponent>(sky_entity);
    sky.up_color = glm::vec3(0.6f, 0.8f, 1.0f);
    sky.down_color = glm::vec3(0.2f, 0.1f, 0.0f);
    sky.intensity = 0.75f;

    const auto spot_entity = world.CreateEntity();
    auto& spot_transform = world.registry().emplace<TransformComponent>(spot_entity);
    spot_transform.position = glm::vec3(5.0f, 6.0f, 7.0f);
    spot_transform.rotation = glm::mat4(1.0f);
    auto& spot = world.registry().emplace<dse::SpotLightComponent>(spot_entity);
    spot.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    spot.color = glm::vec3(0.7f, 0.6f, 0.2f);
    spot.intensity = 4.0f;
    spot.radius = 10.0f;
    spot.falloff = 2.0f;
    spot.inner_cone_angle = 11.0f;
    spot.outer_cone_angle = 24.0f;
    spot.cast_shadow = true;

    const auto non_shadow_spot_entity = world.CreateEntity();
    auto& non_shadow_spot_transform = world.registry().emplace<TransformComponent>(non_shadow_spot_entity);
    non_shadow_spot_transform.position = glm::vec3(-2.0f, 3.0f, 4.0f);
    non_shadow_spot_transform.rotation = glm::mat4(1.0f);
    auto& non_shadow_spot = world.registry().emplace<dse::SpotLightComponent>(non_shadow_spot_entity);
    non_shadow_spot.direction = glm::vec3(1.0f, 0.0f, 0.0f);
    non_shadow_spot.color = glm::vec3(0.1f, 0.9f, 0.4f);
    non_shadow_spot.intensity = 1.25f;
    non_shadow_spot.radius = 9.0f;
    non_shadow_spot.falloff = 1.0f;
    non_shadow_spot.inner_cone_angle = 8.0f;
    non_shadow_spot.outer_cone_angle = 18.0f;
    non_shadow_spot.cast_shadow = false;

    system.Render(world, cmd);

    REQUIRE(cmd.draw_mesh_batch_calls == 1);
    REQUIRE(cmd.last_mesh_batch.size() == 1);
    const auto& item = cmd.last_mesh_batch.front();
    REQUIRE(item.lighting_enabled);
    REQUIRE(item.light_direction.x == Approx(-0.2f));
    REQUIRE(item.light_direction.y == Approx(-1.0f));
    REQUIRE(item.light_direction.z == Approx(-0.4f));
    REQUIRE(item.light_color.r == Approx(0.9f));
    REQUIRE(item.light_intensity == Approx(2.5f));
    REQUIRE(item.shadow_strength == Approx(0.65f));
    REQUIRE(item.ambient_intensity == Approx(0.9f));
    REQUIRE(item.material_emissive.x == Approx(0.215f));
    REQUIRE(item.material_emissive.y == Approx(0.316875f));
    REQUIRE(item.material_emissive.z == Approx(0.41875f));

    REQUIRE(item.point_lights.size() == 2);
    const auto first_point_it = std::find_if(item.point_lights.begin(), item.point_lights.end(), [](const MeshDrawItem::PointLightData& light) {
        return light.position.x == Approx(1.0f) && light.position.y == Approx(2.0f) && light.position.z == Approx(3.0f);
    });
    REQUIRE(first_point_it != item.point_lights.end());
    REQUIRE(first_point_it->color.r == Approx(1.0f));
    REQUIRE(first_point_it->intensity == Approx(3.0f));
    REQUIRE(first_point_it->radius == Approx(12.0f));
    REQUIRE(first_point_it->cast_shadow);
    REQUIRE(first_point_it->shadow_index == 0);

    const auto second_point_it = std::find_if(item.point_lights.begin(), item.point_lights.end(), [](const MeshDrawItem::PointLightData& light) {
        return light.position.x == Approx(-4.0f) && light.position.y == Approx(1.0f) && light.position.z == Approx(0.5f);
    });
    REQUIRE(second_point_it != item.point_lights.end());
    REQUIRE(second_point_it->color.b == Approx(1.0f));
    REQUIRE(second_point_it->intensity == Approx(1.5f));
    REQUIRE(second_point_it->radius == Approx(0.6f));
    REQUIRE_FALSE(second_point_it->cast_shadow);
    REQUIRE(second_point_it->shadow_index == -1);

    REQUIRE(item.spot_lights.size() == 2);
    const auto shadow_spot_it = std::find_if(item.spot_lights.begin(), item.spot_lights.end(), [](const MeshDrawItem::SpotLightData& light) {
        return light.position.x == Approx(5.0f) && light.position.y == Approx(6.0f) && light.position.z == Approx(7.0f);
    });
    REQUIRE(shadow_spot_it != item.spot_lights.end());
    REQUIRE(shadow_spot_it->direction.x == Approx(0.0f));
    REQUIRE(shadow_spot_it->direction.y == Approx(-1.0f));
    REQUIRE(shadow_spot_it->direction.z == Approx(0.0f));
    REQUIRE(shadow_spot_it->color.r == Approx(0.7f));
    REQUIRE(shadow_spot_it->intensity == Approx(4.0f));
    REQUIRE(shadow_spot_it->radius == Approx(20.0f));
    REQUIRE(shadow_spot_it->inner_cone == Approx(11.0f));
    REQUIRE(shadow_spot_it->outer_cone == Approx(24.0f));
    REQUIRE(shadow_spot_it->cast_shadow);
    REQUIRE(shadow_spot_it->shadow_index == 0);

    const auto non_shadow_spot_it = std::find_if(item.spot_lights.begin(), item.spot_lights.end(), [](const MeshDrawItem::SpotLightData& light) {
        return light.position.x == Approx(-2.0f) && light.position.y == Approx(3.0f) && light.position.z == Approx(4.0f);
    });
    REQUIRE(non_shadow_spot_it != item.spot_lights.end());
    REQUIRE_FALSE(non_shadow_spot_it->cast_shadow);
    REQUIRE(non_shadow_spot_it->shadow_index == -1);
}
