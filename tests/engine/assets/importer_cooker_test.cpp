#include "catch/catch.hpp"
#include "engine/assets/compiler/importer.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <filesystem>
#include <fstream>

using dse::asset::compiler::GltfImporter;
using dse::asset::compiler::MeshCooker;
using dse::asset::compiler::RawAnimation;
using dse::asset::compiler::RawAnimationChannel;
using dse::asset::compiler::RawBone;
using dse::asset::compiler::RawMaterial;
using dse::asset::compiler::RawSceneData;
using dse::asset::compiler::RawSubMesh;

namespace {

std::filesystem::path MakeImporterTempDir() {
    auto dir = std::filesystem::temp_directory_path() / "dse_importer_cooker_tests";
    std::filesystem::create_directories(dir);
    return dir;
}

struct ScopedFileCleanup {
    explicit ScopedFileCleanup(std::filesystem::path p) : path(std::move(p)) {}
    ~ScopedFileCleanup() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    std::filesystem::path path;
};

RawSceneData BuildMinimalScene() {
    RawSceneData scene;

    RawSubMesh mesh;
    mesh.name = "quad";
    mesh.positions = {
        glm::vec3(-1.0f, -1.0f, 0.0f),
        glm::vec3( 1.0f, -1.0f, 0.0f),
        glm::vec3( 1.0f,  1.0f, 0.0f),
        glm::vec3(-1.0f,  1.0f, 0.0f)
    };
    mesh.normals = std::vector<glm::vec3>(4, glm::vec3(0.0f, 0.0f, 1.0f));
    mesh.texcoords = {
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(0.0f, 1.0f)
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.material_index = 0;
    scene.meshes.push_back(mesh);

    RawMaterial material;
    material.name = "mat";
    material.base_color_factor = glm::vec4(0.25f, 0.5f, 0.75f, 1.0f);
    material.base_color_texture = "base.png";
    material.normal_texture = "normal.png";
    material.metallic_roughness_texture = "mr.png";
    scene.materials.push_back(material);

    RawBone root;
    root.name = "root";
    root.parent_index = -1;
    root.local_transform = glm::mat4(1.0f);
    root.inverse_bind_matrix = glm::mat4(1.0f);
    scene.skeleton.push_back(root);

    RawAnimation animation;
    animation.name = "idle";
    animation.duration = 1.0f;
    RawAnimationChannel channel;
    channel.target_node_index = 0;
    channel.time_keys = {0.0f, 1.0f};
    channel.position_keys = {glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};
    channel.rotation_keys = {glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    channel.scale_keys = {glm::vec3(1.0f), glm::vec3(1.0f)};
    animation.channels.push_back(channel);
    scene.animations.push_back(animation);

    return scene;
}

} // namespace

TEST_CASE("Given_MissingGltfFile_When_ImportCalled_Then_ReturnsFalse", "[engine][unit][asset_compiler]") {
    GltfImporter importer;
    RawSceneData scene;

    REQUIRE_FALSE(importer.Import("definitely_missing_test_scene.gltf", scene));
    REQUIRE(scene.meshes.empty());
    REQUIRE(scene.materials.empty());
}

TEST_CASE("Given_EmptyRawScene_When_CookersRun_Then_AllReturnFalse", "[engine][unit][asset_compiler]") {
    MeshCooker cooker;
    RawSceneData scene;
    const auto dir = MakeImporterTempDir();

    REQUIRE_FALSE(cooker.CookToDmesh(scene, (dir / "empty.dmesh").string()));
    REQUIRE_FALSE(cooker.CookToDmat(scene, dir.string(), "empty"));
    REQUIRE_FALSE(cooker.CookToDanim(scene, dir.string(), "empty"));
    REQUIRE_FALSE(cooker.CookToDskel(scene, dir.string(), "empty"));
}

TEST_CASE("Given_MinimalRawScene_When_CookersRun_Then_OutputFilesAreCreated", "[engine][unit][asset_compiler]") {
    MeshCooker cooker;
    RawSceneData scene = BuildMinimalScene();
    const auto dir = MakeImporterTempDir();

    const auto dmesh_path = dir / "minimal_scene.dmesh";
    const auto dmat_path = dir / "minimal_scene.dmat";
    const auto danim_path = dir / "minimal_scene.danim";
    const auto dskel_path = dir / "minimal_scene.dskel";
    ScopedFileCleanup cleanup_mesh(dmesh_path);
    ScopedFileCleanup cleanup_mat(dmat_path);
    ScopedFileCleanup cleanup_anim(danim_path);
    ScopedFileCleanup cleanup_skel(dskel_path);

    REQUIRE(cooker.CookToDmesh(scene, dmesh_path.string()));
    REQUIRE(cooker.CookToDmat(scene, dir.string(), "minimal_scene"));
    REQUIRE(cooker.CookToDanim(scene, dir.string(), "minimal_scene"));
    REQUIRE(cooker.CookToDskel(scene, dir.string(), "minimal_scene"));

    REQUIRE(std::filesystem::exists(dmesh_path));
    REQUIRE(std::filesystem::exists(dmat_path));
    REQUIRE(std::filesystem::exists(danim_path));
    REQUIRE(std::filesystem::exists(dskel_path));
    REQUIRE(std::filesystem::file_size(dmesh_path) > 0);
    REQUIRE(std::filesystem::file_size(dmat_path) > 0);
    REQUIRE(std::filesystem::file_size(danim_path) > 0);
    REQUIRE(std::filesystem::file_size(dskel_path) > 0);
}
