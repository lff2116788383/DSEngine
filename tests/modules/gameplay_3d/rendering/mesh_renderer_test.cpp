#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;

// 正向测试：默认 MeshRendererComponent 状态应拥有正确的PBR默认参数
TEST_CASE("Given_DefaultMeshRendererComponent_When_Created_Then_PBRParametersAreValid", "[engine][unit][mesh_renderer]") {
    MeshRendererComponent mesh_renderer;
    
    // 检查默认PBR材质参数
    REQUIRE(mesh_renderer.shader_variant == "MESH_UNLIT");
    REQUIRE(mesh_renderer.color == glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    REQUIRE(mesh_renderer.emissive == glm::vec3(0.0f, 0.0f, 0.0f));
    REQUIRE(mesh_renderer.metallic == 0.0f);
    REQUIRE(mesh_renderer.roughness == 0.5f);
    REQUIRE(mesh_renderer.ao == 1.0f);
    REQUIRE(mesh_renderer.normal_strength == 1.0f);
    REQUIRE(mesh_renderer.receive_shadow == true);
    REQUIRE(mesh_renderer.visible == true);
}

// 边界测试：修改 MeshRendererComponent 的极端PBR参数
TEST_CASE("Given_MeshRendererComponent_When_ModifyingPBRParameters_Then_ValuesAreUpdated", "[engine][unit][mesh_renderer]") {
    MeshRendererComponent mesh_renderer;
    
    mesh_renderer.metallic = 1.0f;
    mesh_renderer.roughness = 0.0f;
    mesh_renderer.emissive = glm::vec3(100.0f, 100.0f, 100.0f); // 高强度自发光
    
    REQUIRE(mesh_renderer.metallic == 1.0f);
    REQUIRE(mesh_renderer.roughness == 0.0f);
    REQUIRE(mesh_renderer.emissive.x == 100.0f);
}

// 反向测试：测试异常属性赋值的安全保持
TEST_CASE("Given_MeshRendererComponent_When_InvalidValuesAssigned_Then_ValuesAreMaintained", "[engine][unit][mesh_renderer]") {
    MeshRendererComponent mesh_renderer;
    
    mesh_renderer.metallic = -1.0f; // 负金属度（逻辑上应在渲染时被钳制，但组件层需确保数据不丢失）
    REQUIRE(mesh_renderer.metallic == -1.0f);
    
    mesh_renderer.temp_vertices.clear();
    REQUIRE(mesh_renderer.temp_vertices.empty() == true);
}
