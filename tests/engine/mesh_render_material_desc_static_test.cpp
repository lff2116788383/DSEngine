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

TEST_CASE("Given_MeshRenderSystemSource_When_RenderingMeshMaterials_Then_RuntimePrefersMaterialInstanceData", "[engine][unit][3d][material][static]") {
    const std::string source = ReadAllText("modules/gameplay_3d/rendering/mesh_render_system.cpp");
    REQUIRE(source.find("GetMaterialInstance(mesh_renderer.material_instance_id)") != std::string::npos);
    REQUIRE(source.find("resolved_shader_variant") != std::string::npos);
    REQUIRE(source.find("resolved_base_color") != std::string::npos);
    REQUIRE(source.find("resolved_texture_slots") != std::string::npos);
    REQUIRE(source.find("resolved_scalars") != std::string::npos);
    REQUIRE(source.find("item.normal_map_handle = resolved_texture_slots.normal") != std::string::npos);
    REQUIRE(source.find("MaterialDesc()") == std::string::npos);
}
