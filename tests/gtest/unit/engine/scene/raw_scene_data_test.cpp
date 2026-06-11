/**
 * @file raw_scene_data_test.cpp
 * @brief RawSceneData 资产编译中间数据的单元测试
 *
 * 覆盖场景：
 * - VertexAttribute 位运算（OR / AND）
 * - MeshHeader / AnimHeader / SkelHeader 结构体默认值
 * - RawSceneData 数据组装
 */

#include <gtest/gtest.h>
#include "engine/assets/compiler/raw_scene_data.h"
#include <cstring>

using namespace dse::asset::compiler;

// ============================================================
// VertexAttribute 位运算
// ============================================================

TEST(VertexAttributeTest, ORCombineMultipleProperties) {
    auto combined = VertexAttribute::Position | VertexAttribute::Normal;
    EXPECT_TRUE(static_cast<bool>(combined & VertexAttribute::Position));
    EXPECT_TRUE(static_cast<bool>(combined & VertexAttribute::Normal));
    EXPECT_FALSE(static_cast<bool>(combined & VertexAttribute::TexCoord));
}

TEST(VertexAttributeTest, ORCombineAllProperties) {
    auto all = VertexAttribute::Position | VertexAttribute::Normal |
               VertexAttribute::Tangent | VertexAttribute::TexCoord |
               VertexAttribute::Color | VertexAttribute::Joints |
               VertexAttribute::Weights;
    EXPECT_TRUE(static_cast<bool>(all & VertexAttribute::Position));
    EXPECT_TRUE(static_cast<bool>(all & VertexAttribute::Weights));
}

TEST(VertexAttributeTest, AndSingle) {
    auto pos_norm = VertexAttribute::Position | VertexAttribute::Normal;
    EXPECT_TRUE(static_cast<bool>(pos_norm & VertexAttribute::Position));
    EXPECT_FALSE(static_cast<bool>(pos_norm & VertexAttribute::Color));
}

TEST(VertexAttributeTest, SingleANDIs) {
    EXPECT_TRUE(static_cast<bool>(VertexAttribute::Position & VertexAttribute::Position));
}

TEST(VertexAttributeTest, DifferentPropertiesANDisFalse) {
    EXPECT_FALSE(static_cast<bool>(VertexAttribute::Position & VertexAttribute::Normal));
}

TEST(VertexAttributeTest, Is2) {
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Position), 1u);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Normal), 2u);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Tangent), 4u);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::TexCoord), 8u);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Color), 16u);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Joints), 32u);
    EXPECT_EQ(static_cast<uint32_t>(VertexAttribute::Weights), 64u);
}

// ============================================================
// 二进制头结构体默认值
// ============================================================

TEST(MeshHeaderTest, DefaultmagicIsDSEM) {
    MeshHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'M');
}

TEST(MeshHeaderTest, DefaultIs1) {
    MeshHeader header;
    EXPECT_EQ(header.version, 1u);
}

TEST(MeshHeaderTest, DefaultIs0) {
    MeshHeader header;
    EXPECT_EQ(header.vertex_count, 0u);
    EXPECT_EQ(header.index_count, 0u);
    EXPECT_EQ(header.submesh_count, 0u);
}

TEST(AnimHeaderTest, DefaultmagicIsDSEA) {
    AnimHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'A');
}

TEST(AnimHeaderTest, DefaultIs2) {
    AnimHeader header;
    EXPECT_EQ(header.version, 2u);
    EXPECT_FLOAT_EQ(header.duration, 0.0f);
    EXPECT_EQ(header.channel_count, 0u);
}

TEST(SkelHeaderTest, DefaultmagicIsDSES) {
    SkelHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'S');
}

TEST(SkelHeaderTest, DefaultIs2) {
    SkelHeader header;
    EXPECT_EQ(header.version, 2u);
    EXPECT_EQ(header.bone_count, 0u);
}

// ============================================================
// RawSceneData 数据组装
// ============================================================

TEST(RawSceneDataTest, EmptyDataDefaultValues) {
    RawSceneData data;
    EXPECT_TRUE(data.meshes.empty());
    EXPECT_TRUE(data.materials.empty());
    EXPECT_TRUE(data.skeleton.empty());
    EXPECT_TRUE(data.animations.empty());
}

TEST(RawSceneDataTest, AddToSubMesh) {
    RawSceneData data;
    RawSubMesh mesh;
    mesh.name = "cube";
    mesh.positions = {glm::vec3(0), glm::vec3(1)};
    mesh.indices = {0, 1};
    data.meshes.push_back(mesh);

    ASSERT_EQ(data.meshes.size(), 1u);
    EXPECT_EQ(data.meshes[0].name, "cube");
    EXPECT_EQ(data.meshes[0].positions.size(), 2u);
}

TEST(RawSceneDataTest, AddToMaterial) {
    RawSceneData data;
    RawMaterial mat;
    mat.name = "default";
    mat.metallic_factor = 0.5f;
    data.materials.push_back(mat);

    ASSERT_EQ(data.materials.size(), 1u);
    EXPECT_EQ(data.materials[0].name, "default");
    EXPECT_FLOAT_EQ(data.materials[0].metallic_factor, 0.5f);
}

TEST(RawBoneTest, DefaultparentIs) {
    RawBone bone;
    bone.name = "root";
    EXPECT_EQ(bone.parent_index, -1);
}

TEST(RawAnimationTest, Empty) {
    RawAnimation anim;
    anim.name = "idle";
    EXPECT_TRUE(anim.channels.empty());
    EXPECT_FLOAT_EQ(anim.duration, 0.0f);
}
