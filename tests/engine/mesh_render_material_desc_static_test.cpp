#include "catch/catch.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string ReadAllText(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    REQUIRE(in.is_open());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST_CASE("Given_MeshRenderSystemSource_When_RenderingMeshMaterials_Then_RuntimeReadsDirectlyFromMeshRendererComponent", "[engine][unit][3d][material][static]") {
    const std::string source = ReadAllText("modules/gameplay_3d/rendering/mesh_render_system.cpp");
    REQUIRE(source.find("material_instance_id") != std::string::npos);
    REQUIRE(source.find("shader_variant") != std::string::npos);
    REQUIRE(source.find("metallic") != std::string::npos);
    REQUIRE(source.find("roughness") != std::string::npos);
    REQUIRE(source.find("mesh.") != std::string::npos);
    REQUIRE(source.find("MaterialDesc()") == std::string::npos);
}
