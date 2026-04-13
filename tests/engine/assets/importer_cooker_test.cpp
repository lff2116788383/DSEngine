#include "catch/catch.hpp"
#include "engine/assets/compiler/importer.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <filesystem>
#include <fstream>
#include <sstream>

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
    material.metallic_factor = 0.35f;
    material.roughness_factor = 0.65f;
    material.emissive_factor = glm::vec3(0.1f, 0.2f, 0.3f);
    material.normal_scale = 0.8f;
    material.occlusion_strength = 0.45f;
    material.alpha_cutoff = 0.33f;
    material.double_sided = true;
    material.alpha_test = true;
    material.base_color_texture = "base.png";
    material.normal_texture = "normal.png";
    material.metallic_roughness_texture = "mr.png";
    material.emissive_texture = "emissive.png";
    material.occlusion_texture = "occlusion.png";
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


TEST_CASE("Given_GltfImporterSource_When_Reviewed_Then_CurrentSupportMatrixAndKnownGapsAreExplicit", "[engine][unit][asset_compiler][gltf][static]") {
    std::ifstream in("engine/assets/compiler/importer.cpp", std::ios::in | std::ios::binary);
    REQUIRE(in.is_open());
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string source = ss.str();

    SECTION("当前已支持的 glTF 输入字段") {
        REQUIRE(source.find("primitive.attributes.find(\"POSITION\")") != std::string::npos);
        REQUIRE(source.find("primitive.attributes.find(\"NORMAL\")") != std::string::npos);
        REQUIRE(source.find("primitive.attributes.find(\"TANGENT\")") != std::string::npos);
        REQUIRE(source.find("primitive.attributes.find(\"TEXCOORD_0\")") != std::string::npos);
        REQUIRE(source.find("primitive.attributes.find(\"WEIGHTS_0\")") != std::string::npos);
        REQUIRE(source.find("primitive.attributes.find(\"JOINTS_0\")") != std::string::npos);
        REQUIRE(source.find("mat.doubleSided") != std::string::npos);
        REQUIRE(source.find("mat.alphaMode == \"MASK\"") != std::string::npos);
        REQUIRE(source.find("channel.target_path == \"translation\"") != std::string::npos);
        REQUIRE(source.find("channel.target_path == \"rotation\"") != std::string::npos);
        REQUIRE(source.find("channel.target_path == \"scale\"") != std::string::npos);
    }

    SECTION("当前仍未接入的能力在源码中可明确识别") {
        REQUIRE(source.find("COLOR_0") == std::string::npos);
        REQUIRE(source.find("targets") == std::string::npos);
        REQUIRE(source.find("weights") == std::string::npos);
    }
}

TEST_CASE("Given_MinimalGltfFile_When_Imported_Then_PbrSkeletonAndAnimationFieldsReachRawScene", "[engine][unit][asset_compiler][gltf][3d]") {

    GltfImporter importer;
    RawSceneData scene;
    const auto dir = MakeImporterTempDir();
    const auto gltf_path = dir / "minimal_import_scene.gltf";
    ScopedFileCleanup cleanup_gltf(gltf_path);

    {
        std::ofstream out(gltf_path);
        REQUIRE(out.is_open());
        out << R"JSON({
  "asset": { "version": "2.0" },
  "buffers": [
    {
      "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAACAPwAAAAAAAAAAAACAPwAAAAAAAAAAAACAPwAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAEAAAABAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAIA/AAABAAAAAQAAAAAAAAAAgD8AAAAAAACAPwAAgD8AAAAAAACAPwAAgD8=",
      "byteLength": 288
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 72, "byteLength": 24 },
    { "buffer": 0, "byteOffset": 96, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 144, "byteLength": 16 },
    { "buffer": 0, "byteOffset": 160, "byteLength": 12 },
    { "buffer": 0, "byteOffset": 172, "byteLength": 64 },
    { "buffer": 0, "byteOffset": 236, "byteLength": 8 },
    { "buffer": 0, "byteOffset": 244, "byteLength": 24 },
    { "buffer": 0, "byteOffset": 268, "byteLength": 16 },
    { "buffer": 0, "byteOffset": 284, "byteLength": 4 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3" },
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2" },
    { "bufferView": 3, "componentType": 5126, "count": 3, "type": "VEC4" },
    { "bufferView": 4, "componentType": 5121, "count": 3, "type": "VEC4" },
    { "bufferView": 5, "componentType": 5123, "count": 6, "type": "SCALAR" },
    { "bufferView": 6, "componentType": 5126, "count": 1, "type": "MAT4" },
    { "bufferView": 7, "componentType": 5126, "count": 2, "type": "SCALAR" },
    { "bufferView": 8, "componentType": 5126, "count": 2, "type": "VEC3" },
    { "bufferView": 9, "componentType": 5126, "count": 1, "type": "VEC4" },
    { "bufferView": 10, "componentType": 5126, "count": 1, "type": "VEC4" }
  ],
  "images": [
    { "uri": "base.png" },
    { "uri": "normal.png" },
    { "uri": "mr.png" },
    { "uri": "emissive.png" },
    { "uri": "occlusion.png" }
  ],
  "textures": [
    { "source": 0 },
    { "source": 1 },
    { "source": 2 },
    { "source": 3 },
    { "source": 4 }
  ],
  "materials": [
    {
      "name": "pbr_mat",
      "pbrMetallicRoughness": {
        "baseColorFactor": [0.2, 0.4, 0.6, 1.0],
        "metallicFactor": 0.8,
        "roughnessFactor": 0.3,
        "baseColorTexture": { "index": 0 },
        "metallicRoughnessTexture": { "index": 2 }
      },
      "normalTexture": { "index": 1, "scale": 0.7 },
      "occlusionTexture": { "index": 4, "strength": 0.55 },
      "emissiveTexture": { "index": 3 },
      "emissiveFactor": [0.1, 0.2, 0.3],
      "alphaMode": "MASK",
      "alphaCutoff": 0.42,
      "doubleSided": true
    }
  ],
  "meshes": [
    {
      "name": "tri_mesh",
      "primitives": [
        {
          "attributes": {
            "POSITION": 0,
            "NORMAL": 1,
            "TEXCOORD_0": 2,
            "WEIGHTS_0": 3,
            "JOINTS_0": 4
          },
          "indices": 5,
          "material": 0
        }
      ]
    }
  ],
  "nodes": [
    { "name": "root_joint", "mesh": 0, "skin": 0, "children": [1] },
    { "name": "child_joint", "translation": [0.0, 1.0, 0.0] }
  ],
  "skins": [
    {
      "joints": [0, 1],
      "inverseBindMatrices": 6
    }
  ],
  "animations": [
    {
      "name": "idle",
      "channels": [
        { "sampler": 0, "target": { "node": 1, "path": "translation" } }
      ],
      "samplers": [
        { "input": 7, "output": 8 }
      ]
    }
  ],
  "scenes": [
    { "nodes": [0] }
  ],
  "scene": 0
})JSON";
    }

    REQUIRE(importer.Import(gltf_path.string(), scene));
    REQUIRE(scene.meshes.size() == 1);
    REQUIRE(scene.materials.size() == 1);
    REQUIRE(scene.skeleton.size() == 2);
    REQUIRE(scene.animations.size() == 1);

    const auto& mesh = scene.meshes.front();
    REQUIRE(mesh.name == "tri_mesh");
    REQUIRE(mesh.material_index == 0u);
    REQUIRE(mesh.positions.size() == 3);
    REQUIRE(mesh.normals.size() == 3);
    REQUIRE(mesh.texcoords.size() == 3);
    REQUIRE(mesh.joint_weights.size() == 3);
    REQUIRE(mesh.joint_indices.size() == 3);
    REQUIRE(mesh.indices.size() == 6);
    REQUIRE(mesh.joint_indices.front().x == 0);
    REQUIRE(mesh.joint_indices.front().y == 1);
    REQUIRE(mesh.joint_weights.front().x == Approx(1.0f));

    const auto& material = scene.materials.front();
    REQUIRE(material.name == "pbr_mat");
    REQUIRE(material.base_color_factor.x == Approx(0.2f));
    REQUIRE(material.base_color_factor.y == Approx(0.4f));
    REQUIRE(material.base_color_factor.z == Approx(0.6f));
    REQUIRE(material.metallic_factor == Approx(0.8f));
    REQUIRE(material.roughness_factor == Approx(0.3f));
    REQUIRE(material.emissive_factor.z == Approx(0.3f));
    REQUIRE(material.normal_scale == Approx(0.7f));
    REQUIRE(material.occlusion_strength == Approx(0.55f));
    REQUIRE(material.alpha_cutoff == Approx(0.42f));
    REQUIRE(material.alpha_test);
    REQUIRE(material.double_sided);
    REQUIRE(material.base_color_texture == "base.png");
    REQUIRE(material.normal_texture == "normal.png");
    REQUIRE(material.metallic_roughness_texture == "mr.png");
    REQUIRE(material.emissive_texture == "emissive.png");
    REQUIRE(material.occlusion_texture == "occlusion.png");

    REQUIRE(scene.skeleton[0].name == "root_joint");
    REQUIRE(scene.skeleton[0].parent_index == -1);
    REQUIRE(scene.skeleton[1].name == "child_joint");
    REQUIRE(scene.skeleton[1].parent_index == 0);
    REQUIRE(scene.skeleton[1].local_transform[3][1] == Approx(1.0f));

    const auto& animation = scene.animations.front();
    REQUIRE(animation.name == "idle");
    REQUIRE(animation.duration == Approx(1.0f));
    REQUIRE(animation.channels.size() == 1);
    REQUIRE(animation.channels.front().target_node_index == 1);
    REQUIRE(animation.channels.front().time_keys.size() == 2);
    REQUIRE(animation.channels.front().position_keys.size() == 2);
    REQUIRE(animation.channels.front().position_keys.back().x == Approx(1.0f));
}

