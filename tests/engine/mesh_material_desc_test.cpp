#include "catch/catch.hpp"
#include "engine/ecs/components_3d.h"

TEST_CASE("Given_MeshRendererComponent_When_BuildingMaterialDesc_Then_PbrAndSelectionFieldsArePreserved", "[engine][unit][3d][material]") {
    dse::MeshRendererComponent mesh;
    mesh.mesh_path = "assets/meshes/test.fbx";
    mesh.material_instance_id = 42;
    mesh.shader_variant = "MESH_PBR";
    mesh.color = glm::vec4(0.8f, 0.7f, 0.6f, 1.0f);
    mesh.emissive = glm::vec3(0.1f, 0.2f, 0.3f);
    mesh.metallic = 0.5f;
    mesh.roughness = 0.25f;
    mesh.ao = 0.9f;
    mesh.normal_strength = 1.5f;
    mesh.receive_shadow = false;
    mesh.visible = true;

    const dse::MeshMaterialDesc desc = mesh.MaterialDesc();
    REQUIRE(desc.material_instance_id == 42);
    REQUIRE(desc.shader_variant == "MESH_PBR");
    REQUIRE(desc.color.r == Approx(0.8f));
    REQUIRE(desc.emissive.z == Approx(0.3f));
    REQUIRE(desc.metallic == Approx(0.5f));
    REQUIRE(desc.roughness == Approx(0.25f));
    REQUIRE(desc.ao == Approx(0.9f));
    REQUIRE(desc.normal_strength == Approx(1.5f));
}

TEST_CASE("Given_MeshMaterialDesc_When_AppliedBackToMeshRenderer_Then_ComponentFieldsStayConsistent", "[engine][unit][3d][material]") {
    dse::MeshRendererComponent mesh;
    dse::MeshMaterialDesc desc;
    desc.material_instance_id = 7;
    desc.shader_variant = "MESH_UNLIT";
    desc.color = glm::vec4(0.2f, 0.3f, 0.4f, 1.0f);
    desc.emissive = glm::vec3(1.0f, 0.0f, 0.0f);
    desc.metallic = 0.8f;
    desc.roughness = 0.1f;
    desc.ao = 0.6f;
    desc.normal_strength = 2.0f;

    mesh.ApplyMaterialDesc(desc);

    REQUIRE(mesh.material_instance_id == 7);
    REQUIRE(mesh.shader_variant == "MESH_UNLIT");
    REQUIRE(mesh.color.g == Approx(0.3f));
    REQUIRE(mesh.emissive.x == Approx(1.0f));
    REQUIRE(mesh.metallic == Approx(0.8f));
    REQUIRE(mesh.roughness == Approx(0.1f));
    REQUIRE(mesh.ao == Approx(0.6f));
    REQUIRE(mesh.normal_strength == Approx(2.0f));
}
