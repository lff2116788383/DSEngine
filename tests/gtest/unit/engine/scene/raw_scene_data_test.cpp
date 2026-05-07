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

TEST(VertexAttributeTest, OR组合多个属性) {
    auto combined = VertexAttribute::Position | VertexAttribute::Normal;
    EXPECT_TRUE(static_cast<bool>(combined & VertexAttribute::Position));
    EXPECT_TRUE(static_cast<bool>(combined & VertexAttribute::Normal));
    EXPECT_FALSE(static_cast<bool>(combined & VertexAttribute::TexCoord));
}

TEST(VertexAttributeTest, OR组合所有属性) {
    auto all = VertexAttribute::Position | VertexAttribute::Normal |
               VertexAttribute::Tangent | VertexAttribute::TexCoord |
               VertexAttribute::Color | VertexAttribute::Joints |
               VertexAttribute::Weights;
    EXPECT_TRUE(static_cast<bool>(all & VertexAttribute::Position));
    EXPECT_TRUE(static_cast<bool>(all & VertexAttribute::Weights));
}

TEST(VertexAttributeTest, 位与检测单个属性) {
    auto pos_norm = VertexAttribute::Position | VertexAttribute::Normal;
    EXPECT_TRUE(static_cast<bool>(pos_norm & VertexAttribute::Position));
    EXPECT_FALSE(static_cast<bool>(pos_norm & VertexAttribute::Color));
}

TEST(VertexAttributeTest, 单个属性自身AND为真) {
    EXPECT_TRUE(static_cast<bool>(VertexAttribute::Position & VertexAttribute::Position));
}

TEST(VertexAttributeTest, 不同属性AND为假) {
    EXPECT_FALSE(static_cast<bool>(VertexAttribute::Position & VertexAttribute::Normal));
}

TEST(VertexAttributeTest, 属性位值为2的幂) {
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

TEST(MeshHeaderTest, 默认magic为DSEM) {
    MeshHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'M');
}

TEST(MeshHeaderTest, 默认版本为1) {
    MeshHeader header;
    EXPECT_EQ(header.version, 1u);
}

TEST(MeshHeaderTest, 默认计数值为0) {
    MeshHeader header;
    EXPECT_EQ(header.vertex_count, 0u);
    EXPECT_EQ(header.index_count, 0u);
    EXPECT_EQ(header.submesh_count, 0u);
}

TEST(AnimHeaderTest, 默认magic为DSEA) {
    AnimHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'A');
}

TEST(AnimHeaderTest, 默认版本为2) {
    AnimHeader header;
    EXPECT_EQ(header.version, 2u);
    EXPECT_FLOAT_EQ(header.duration, 0.0f);
    EXPECT_EQ(header.channel_count, 0u);
}

TEST(SkelHeaderTest, 默认magic为DSES) {
    SkelHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'S');
    EXPECT_EQ(header.magic[2], 'E');
    EXPECT_EQ(header.magic[3], 'S');
}

TEST(SkelHeaderTest, 默认版本为2) {
    SkelHeader header;
    EXPECT_EQ(header.version, 2u);
    EXPECT_EQ(header.bone_count, 0u);
}

// ============================================================
// RawSceneData 数据组装
// ============================================================

TEST(RawSceneDataTest, 空数据默认值) {
    RawSceneData data;
    EXPECT_TRUE(data.meshes.empty());
    EXPECT_TRUE(data.materials.empty());
    EXPECT_TRUE(data.skeleton.empty());
    EXPECT_TRUE(data.animations.empty());
}

TEST(RawSceneDataTest, 添加SubMesh) {
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

TEST(RawSceneDataTest, 添加Material) {
    RawSceneData data;
    RawMaterial mat;
    mat.name = "default";
    mat.metallic_factor = 0.5f;
    data.materials.push_back(mat);

    ASSERT_EQ(data.materials.size(), 1u);
    EXPECT_EQ(data.materials[0].name, "default");
    EXPECT_FLOAT_EQ(data.materials[0].metallic_factor, 0.5f);
}

TEST(RawBoneTest, 默认parent为根骨骼) {
    RawBone bone;
    bone.name = "root";
    EXPECT_EQ(bone.parent_index, -1);
}

TEST(RawAnimationTest, 空动画通道) {
    RawAnimation anim;
    anim.name = "idle";
    EXPECT_TRUE(anim.channels.empty());
    EXPECT_FLOAT_EQ(anim.duration, 0.0f);
}
