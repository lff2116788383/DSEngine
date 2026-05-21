/**
 * @file asset_compiler_test.cpp
 * @brief 资产编译器数据结构单元测试
 */

#include "gtest/gtest.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include <glm/glm.hpp>

using namespace dse::asset::compiler;

// ============================================================
// VertexAttribute 测试
// ============================================================

TEST(VertexAttributeTest, 枚举值) {
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Position), 1u << 0);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Normal), 1u << 1);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Tangent), 1u << 2);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::TexCoord), 1u << 3);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Color), 1u << 4);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Joints), 1u << 5);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Weights), 1u << 6);
}

TEST(VertexAttributeTest, 按位或运算) {
    VertexAttribute attr = VertexAttribute::Position | VertexAttribute::Normal;
    EXPECT_EQ(static_cast<uint32_t>(attr), (1u << 0) | (1u << 1));
}

TEST(VertexAttributeTest, 按位与运算) {
    VertexAttribute attr1 = VertexAttribute::Position | VertexAttribute::Normal;
    VertexAttribute attr2 = VertexAttribute::Position | VertexAttribute::TexCoord;
    VertexAttribute result = attr1 & attr2;
    EXPECT_EQ(static_cast<uint32_t>(result), 1u << 0);
}

// ============================================================
// RawSubMesh 测试
// ============================================================

TEST(RawSubMeshTest, 默认值) {
    RawSubMesh mesh;
    EXPECT_TRUE(mesh.name.empty());
    EXPECT_EQ(mesh.material_index, 0u);
    EXPECT_TRUE(mesh.positions.empty());
    EXPECT_TRUE(mesh.normals.empty());
    EXPECT_TRUE(mesh.tangents.empty());
    EXPECT_TRUE(mesh.texcoords.empty());
    EXPECT_TRUE(mesh.colors.empty());
    EXPECT_TRUE(mesh.joint_indices.empty());
    EXPECT_TRUE(mesh.joint_weights.empty());
    EXPECT_TRUE(mesh.indices.empty());
}

TEST(RawSubMeshTest, 自定义值) {
    RawSubMesh mesh;
    mesh.name = "TestMesh";
    mesh.material_index = 2;
    mesh.positions.push_back({0,0,0});
    mesh.positions.push_back({1,0,0});
    mesh.indices = {0, 1, 0};

    EXPECT_EQ(mesh.name, "TestMesh");
    EXPECT_EQ(mesh.material_index, 2u);
    EXPECT_EQ(mesh.positions.size(), 2u);
    EXPECT_EQ(mesh.indices.size(), 3u);
}

// ============================================================
// RawBone 测试
// ============================================================

TEST(RawBoneTest, 默认值) {
    RawBone bone;
    EXPECT_TRUE(bone.name.empty());
    EXPECT_EQ(bone.parent_index, -1);
    EXPECT_EQ(bone.inverse_bind_matrix, glm::mat4(1.0f));
    EXPECT_EQ(bone.local_transform, glm::mat4(1.0f));
}

TEST(RawBoneTest, 自定义值) {
    RawBone bone;
    bone.name = "Hip";
    bone.parent_index = 0;
    bone.inverse_bind_matrix = glm::mat4(2.0f);
    bone.local_transform = glm::mat4(3.0f);

    EXPECT_EQ(bone.name, "Hip");
    EXPECT_EQ(bone.parent_index, 0);
    EXPECT_EQ(bone.inverse_bind_matrix, glm::mat4(2.0f));
    EXPECT_EQ(bone.local_transform, glm::mat4(3.0f));
}

// ============================================================
// RawMaterial 测试
// ============================================================

TEST(RawMaterialTest, 默认值) {
    RawMaterial mat;
    EXPECT_TRUE(mat.name.empty());
    EXPECT_EQ(mat.base_color_factor, glm::vec4(1.0f));
    EXPECT_FLOAT_EQ(mat.metallic_factor, 0.0f);
    EXPECT_FLOAT_EQ(mat.roughness_factor, 0.5f);
    EXPECT_EQ(mat.emissive_factor, glm::vec3(0.0f));
    EXPECT_FLOAT_EQ(mat.normal_scale, 1.0f);
    EXPECT_FLOAT_EQ(mat.occlusion_strength, 1.0f);
    EXPECT_FLOAT_EQ(mat.alpha_cutoff, 0.5f);
    EXPECT_FALSE(mat.double_sided);
    EXPECT_FALSE(mat.alpha_test);
    EXPECT_TRUE(mat.base_color_texture.empty());
    EXPECT_TRUE(mat.normal_texture.empty());
    EXPECT_TRUE(mat.metallic_roughness_texture.empty());
    EXPECT_TRUE(mat.emissive_texture.empty());
    EXPECT_TRUE(mat.occlusion_texture.empty());
}

TEST(RawMaterialTest, 自定义值) {
    RawMaterial mat;
    mat.name = "TestMat";
    mat.base_color_factor = {0.8f, 0.2f, 0.1f, 1.0f};
    mat.metallic_factor = 0.9f;
    mat.roughness_factor = 0.3f;
    mat.emissive_factor = {1.0f, 0.5f, 0.0f};
    mat.alpha_cutoff = 0.8f;
    mat.double_sided = true;
    mat.alpha_test = true;
    mat.base_color_texture = "diffuse.png";

    EXPECT_EQ(mat.name, "TestMat");
    EXPECT_EQ(mat.base_color_factor, glm::vec4(0.8f, 0.2f, 0.1f, 1.0f));
    EXPECT_FLOAT_EQ(mat.metallic_factor, 0.9f);
    EXPECT_FLOAT_EQ(mat.roughness_factor, 0.3f);
    EXPECT_FLOAT_EQ(mat.alpha_cutoff, 0.8f);
    EXPECT_TRUE(mat.double_sided);
    EXPECT_TRUE(mat.alpha_test);
    EXPECT_EQ(mat.base_color_texture, "diffuse.png");
}

// ============================================================
// RawAnimationChannel 测试
// ============================================================

TEST(RawAnimationChannelTest, 默认值) {
    RawAnimationChannel channel;
    EXPECT_EQ(channel.target_node_index, -1);
    EXPECT_TRUE(channel.target_node_name.empty());
    EXPECT_TRUE(channel.time_keys.empty());
    EXPECT_TRUE(channel.position_keys.empty());
    EXPECT_TRUE(channel.rotation_keys.empty());
    EXPECT_TRUE(channel.scale_keys.empty());
}

TEST(RawAnimationChannelTest, 自定义值) {
    RawAnimationChannel channel;
    channel.target_node_index = 5;
    channel.target_node_name = "LeftArm";
    channel.time_keys = {0.0f, 0.5f, 1.0f};
    channel.position_keys = {{0,0,0}, {1,0,0}, {2,0,0}};
    channel.rotation_keys = {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}};
    channel.scale_keys = {{1,1,1}, {1,1,1}, {1,1,1}};

    EXPECT_EQ(channel.target_node_index, 5);
    EXPECT_EQ(channel.target_node_name, "LeftArm");
    EXPECT_EQ(channel.time_keys.size(), 3u);
    EXPECT_EQ(channel.position_keys.size(), 3u);
    EXPECT_EQ(channel.rotation_keys.size(), 3u);
    EXPECT_EQ(channel.scale_keys.size(), 3u);
}

// ============================================================
// RawAnimation 测试
// ============================================================

TEST(RawAnimationTest, 默认值) {
    RawAnimation anim;
    EXPECT_TRUE(anim.name.empty());
    EXPECT_FLOAT_EQ(anim.duration, 0.0f);
    EXPECT_TRUE(anim.channels.empty());
}

TEST(RawAnimationTest, 自定义值) {
    RawAnimation anim;
    anim.name = "Walk";
    anim.duration = 2.5f;
    RawAnimationChannel channel;
    channel.target_node_index = 0;
    anim.channels.push_back(channel);

    EXPECT_EQ(anim.name, "Walk");
    EXPECT_FLOAT_EQ(anim.duration, 2.5f);
    EXPECT_EQ(anim.channels.size(), 1u);
}

// ============================================================
// RawSceneData 测试
// ============================================================

TEST(RawSceneDataTest, 默认值) {
    RawSceneData scene;
    EXPECT_TRUE(scene.meshes.empty());
    EXPECT_TRUE(scene.materials.empty());
    EXPECT_TRUE(scene.skeleton.empty());
    EXPECT_TRUE(scene.animations.empty());
}

TEST(RawSceneDataTest, 自定义值) {
    RawSceneData scene;
    RawSubMesh mesh;
    mesh.name = "Mesh1";
    scene.meshes.push_back(mesh);

    RawMaterial mat;
    mat.name = "Mat1";
    scene.materials.push_back(mat);

    RawBone bone;
    bone.name = "Bone1";
    scene.skeleton.push_back(bone);

    RawAnimation anim;
    anim.name = "Anim1";
    scene.animations.push_back(anim);

    EXPECT_EQ(scene.meshes.size(), 1u);
    EXPECT_EQ(scene.materials.size(), 1u);
    EXPECT_EQ(scene.skeleton.size(), 1u);
    EXPECT_EQ(scene.animations.size(), 1u);
}

// ============================================================
// MeshHeader 测试
// ============================================================

TEST(MeshHeaderTest, 默认值) {
    MeshHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'M');
    EXPECT_EQ(header.version, 1u);
    EXPECT_EQ(header.vertex_count, 0u);
    EXPECT_EQ(header.index_count, 0u);
    EXPECT_EQ(header.submesh_count, 0u);
    EXPECT_EQ(header.attribute_mask, 0u);
    EXPECT_EQ(header.vertex_data_offset, 0ull);
    EXPECT_EQ(header.index_data_offset, 0ull);
    EXPECT_EQ(header.submesh_data_offset, 0ull);
}

TEST(MeshHeaderTest, 自定义值) {
    MeshHeader header;
    header.vertex_count = 1000;
    header.index_count = 3000;
    header.submesh_count = 5;
    header.attribute_mask = static_cast<uint32_t>(VertexAttribute::Position | VertexAttribute::Normal);
    header.vertex_data_offset = 128;
    header.index_data_offset = 512;
    header.submesh_data_offset = 2048;

    EXPECT_EQ(header.vertex_count, 1000u);
    EXPECT_EQ(header.index_count, 3000u);
    EXPECT_EQ(header.submesh_count, 5u);
    EXPECT_EQ(header.attribute_mask, (1u << 0) | (1u << 1));
    EXPECT_EQ(header.vertex_data_offset, 128ull);
    EXPECT_EQ(header.index_data_offset, 512ull);
    EXPECT_EQ(header.submesh_data_offset, 2048ull);
}

// ============================================================
// SubMeshDesc 测试
// ============================================================

TEST(SubMeshDescTest, 默认值) {
    SubMeshDesc desc{};
    EXPECT_EQ(desc.index_start, 0u);
    EXPECT_EQ(desc.index_count, 0u);
    EXPECT_EQ(desc.base_vertex, 0u);
    EXPECT_EQ(desc.material_id, 0u);
    EXPECT_EQ(desc.bounding_box_min, glm::vec3(0.0f));
    EXPECT_EQ(desc.bounding_box_max, glm::vec3(0.0f));
}

TEST(SubMeshDescTest, 自定义值) {
    SubMeshDesc desc;
    desc.index_start = 100;
    desc.index_count = 300;
    desc.base_vertex = 50;
    desc.material_id = 2;
    desc.bounding_box_min = {-1.0f, 0.0f, -1.0f};
    desc.bounding_box_max = {1.0f, 2.0f, 1.0f};

    EXPECT_EQ(desc.index_start, 100u);
    EXPECT_EQ(desc.index_count, 300u);
    EXPECT_EQ(desc.base_vertex, 50u);
    EXPECT_EQ(desc.material_id, 2u);
    EXPECT_EQ(desc.bounding_box_min, glm::vec3(-1.0f, 0.0f, -1.0f));
    EXPECT_EQ(desc.bounding_box_max, glm::vec3(1.0f, 2.0f, 1.0f));
}

// ============================================================
// AnimHeader 测试
// ============================================================

TEST(AnimHeaderTest, 默认值) {
    AnimHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'A');
    EXPECT_EQ(header.version, 2u);
    EXPECT_FLOAT_EQ(header.duration, 0.0f);
    EXPECT_EQ(header.channel_count, 0u);
}

TEST(AnimHeaderTest, 自定义值) {
    AnimHeader header;
    header.duration = 3.5f;
    header.channel_count = 10;

    EXPECT_FLOAT_EQ(header.duration, 3.5f);
    EXPECT_EQ(header.channel_count, 10u);
}

// ============================================================
// AnimChannelDesc 测试
// ============================================================

TEST(AnimChannelDescTest, 默认值) {
    AnimChannelDesc desc{};
    EXPECT_EQ(desc.target_node_index, 0);
    EXPECT_EQ(desc.position_key_count, 0u);
    EXPECT_EQ(desc.rotation_key_count, 0u);
    EXPECT_EQ(desc.scale_key_count, 0u);
    EXPECT_EQ(desc.time_offset, 0ull);
    EXPECT_EQ(desc.position_offset, 0ull);
    EXPECT_EQ(desc.rotation_offset, 0ull);
    EXPECT_EQ(desc.scale_offset, 0ull);
}

TEST(AnimChannelDescTest, 自定义值) {
    AnimChannelDesc desc;
    desc.target_node_index = 5;
    desc.position_key_count = 10;
    desc.rotation_key_count = 10;
    desc.scale_key_count = 10;
    desc.time_offset = 128;
    desc.position_offset = 256;
    desc.rotation_offset = 512;
    desc.scale_offset = 768;

    EXPECT_EQ(desc.target_node_index, 5);
    EXPECT_EQ(desc.position_key_count, 10u);
    EXPECT_EQ(desc.rotation_key_count, 10u);
    EXPECT_EQ(desc.scale_key_count, 10u);
    EXPECT_EQ(desc.time_offset, 128ull);
    EXPECT_EQ(desc.position_offset, 256ull);
    EXPECT_EQ(desc.rotation_offset, 512ull);
    EXPECT_EQ(desc.scale_offset, 768ull);
}

// ============================================================
// SkelHeader 测试
// ============================================================

TEST(SkelHeaderTest, 默认值) {
    SkelHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'S');
    EXPECT_EQ(header.version, 2u);
    EXPECT_EQ(header.bone_count, 0u);
}

TEST(SkelHeaderTest, 自定义值) {
    SkelHeader header;
    header.bone_count = 20;

    EXPECT_EQ(header.bone_count, 20u);
}

// ============================================================
// BoneDesc 测试
// ============================================================

TEST(BoneDescTest, 默认值) {
    BoneDesc desc{};
    EXPECT_EQ(desc.parent_index, 0);
    EXPECT_EQ(desc.inverse_bind_matrix, glm::mat4(0.0f));
    EXPECT_EQ(desc.local_transform, glm::mat4(0.0f));
}

TEST(BoneDescTest, 自定义值) {
    BoneDesc desc;
    desc.parent_index = 3;
    desc.inverse_bind_matrix = glm::mat4(2.0f);
    desc.local_transform = glm::mat4(3.0f);

    EXPECT_EQ(desc.parent_index, 3);
    EXPECT_EQ(desc.inverse_bind_matrix, glm::mat4(2.0f));
    EXPECT_EQ(desc.local_transform, glm::mat4(3.0f));
}

// ============================================================
// Pack 对齐测试
// ============================================================

TEST(PackAlignmentTest, MeshHeader紧凑布局) {
    EXPECT_EQ(sizeof(MeshHeader), 48u);  // pack(1): 4 magic + 4 ver + 4*4 uint32 + 3*8 uint64 = 48
}

TEST(PackAlignmentTest, SubMeshDesc紧凑布局) {
    EXPECT_EQ(sizeof(SubMeshDesc), 40u);  // pack(1): 4*4 uint32 + 2*12 vec3 = 40
}

TEST(PackAlignmentTest, AnimHeader紧凑布局) {
    EXPECT_EQ(sizeof(AnimHeader), 16u);  // 4 magic + 4 version + 4 float + 4 uint32 = 16
}

TEST(PackAlignmentTest, AnimChannelDesc紧凑布局) {
    EXPECT_EQ(sizeof(AnimChannelDesc), 48u);  // 1 int + 3*uint32 + 5*uint64 = 4+12+40 = 56 (padding to 48)
}

TEST(PackAlignmentTest, SkelHeader紧凑布局) {
    EXPECT_EQ(sizeof(SkelHeader), 12u);  // pack(1): 4 magic + 4 ver + 4 uint32 = 12
}

TEST(PackAlignmentTest, BoneDesc紧凑布局) {
    EXPECT_EQ(sizeof(BoneDesc), 132u);  // pack(1): 4 int + 2*64 mat4 = 132
}
