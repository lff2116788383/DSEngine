#include "catch/catch.hpp"

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
