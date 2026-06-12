/**
 * @file rhi_types_test.cpp
 * @brief RHI 层公共类型默认值的单元测试
 *
 * 覆盖场景：
 * - RenderTargetDesc / PipelineStateDesc / RenderPassDesc 默认值
 * - SpriteDrawItem / BatchVertex / MeshDrawItem 默认值
 * - RenderStats 默认值
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include <glm/glm.hpp>

using namespace dse::render;

// ============================================================
// 渲染目标与管线状态描述
// ============================================================

TEST(RenderTargetDescTest, DefaultValues) {
    RenderTargetDesc desc;
    EXPECT_EQ(desc.width, 0);
    EXPECT_EQ(desc.height, 0);
    EXPECT_TRUE(desc.has_color);
    EXPECT_FALSE(desc.has_depth);
    EXPECT_FALSE(desc.generate_mipmaps);
    EXPECT_FALSE(desc.cube_map);
}

TEST(PipelineStateDescTest, DefaultValues) {
    PipelineStateDesc desc;
    EXPECT_TRUE(desc.blend_enabled);
    EXPECT_TRUE(desc.depth_test_enabled);
    EXPECT_TRUE(desc.depth_write_enabled);
    EXPECT_TRUE(desc.culling_enabled);
}

TEST(RenderPassDescTest, DefaultValues) {
    RenderPassDesc desc;
    EXPECT_EQ(desc.render_target, 0u);
    EXPECT_EQ(desc.clear_color, glm::vec4(0.0f));
    EXPECT_FALSE(desc.clear_color_enabled);
}

TEST(RenderTargetReadbackTest, DefaultValues) {
    RenderTargetReadback rb;
    EXPECT_EQ(rb.width, 0);
    EXPECT_EQ(rb.height, 0);
    EXPECT_TRUE(rb.pixels.empty());
}

// ============================================================
// 绘制项
// ============================================================

TEST(SpriteDrawItemTest, DefaultValues) {
    SpriteDrawItem item;
    EXPECT_EQ(item.texture_handle, 0u);
    EXPECT_EQ(item.material_instance_id, 0u);
    EXPECT_EQ(item.sorting_layer, 0);
    EXPECT_EQ(item.order_in_layer, 0);
    EXPECT_EQ(item.model, glm::mat4(1.0f));
    EXPECT_EQ(item.color, glm::vec4(1.0f));
    EXPECT_EQ(item.uv, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

TEST(BatchVertexTest, DefaultValues) {
    BatchVertex v;
    EXPECT_EQ(v.normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(v.tangent, glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(v.weights, glm::vec4(0.0f));
    EXPECT_EQ(v.joints, glm::vec4(0.0f));
}

TEST(MeshDrawItemTest, DefaultValues) {
    MeshDrawItem item;
    EXPECT_FALSE(static_cast<bool>(item.vao_override));
    EXPECT_EQ(item.index_count_override, 0u);
    EXPECT_EQ(item.model, glm::mat4(1.0f));
    EXPECT_FALSE(item.lighting_enabled);
    EXPECT_FALSE(item.skinned);
    EXPECT_FALSE(item.morph_enabled);
    EXPECT_TRUE(item.receive_shadow);
    EXPECT_TRUE(item.vertices.empty());
    EXPECT_TRUE(item.indices.empty());
    EXPECT_TRUE(item.point_lights.empty());
    EXPECT_TRUE(item.spot_lights.empty());
    EXPECT_TRUE(item.bone_matrices.empty());
    EXPECT_TRUE(item.morph_weights.empty());
}

TEST(Particle3DDrawItemTest, DefaultValues) {
    Particle3DDrawItem item;
    EXPECT_EQ(item.texture_handle, 0u);
    EXPECT_EQ(item.material_instance_id, 0u);
    EXPECT_EQ(item.particle_count, 0);
    EXPECT_EQ(item.instance_vbo, 0u);
}

// ============================================================
// 渲染统计
// ============================================================

// ============================================================
// RHI 统一回归测试 — OpenGL ProjectionCorrection
// ============================================================

TEST(OpenGLProjectionCorrectionTest, ReturnsIdentity) {
    // OpenGL 不需要投影修正，应返回单位矩阵
    OpenGLRhiDevice device;
    glm::mat4 corr = device.GetProjectionCorrection();
    EXPECT_EQ(corr, glm::mat4(1.0f));
}

// ============================================================
// 渲染统计
// ============================================================

TEST(RenderStatsTest, DefaultValuesAllisZero) {
    RenderStats stats;
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.material_switches, 0);
    EXPECT_EQ(stats.max_batch_sprites, 0);
    EXPECT_EQ(stats.render_passes, 0);
    EXPECT_EQ(stats.shadow_passes, 0);
}

// ============================================================
// 纹理采样描述 TextureSamplerDesc / TextureFilter / TextureWrap
// ============================================================

TEST(TextureSamplerDescTest, Defaults_LinearRepeat) {
    TextureSamplerDesc d;
    EXPECT_EQ(d.filter, TextureFilter::Linear);
    EXPECT_EQ(d.wrap, TextureWrap::Repeat);
}

TEST(TextureSamplerDescTest, FromLinearFlag_True_IsLinearRepeat) {
    TextureSamplerDesc d = TextureSamplerDesc::FromLinearFlag(true);
    EXPECT_EQ(d.filter, TextureFilter::Linear);
    EXPECT_EQ(d.wrap, TextureWrap::Repeat);
}

TEST(TextureSamplerDescTest, FromLinearFlag_False_IsNearestRepeat) {
    TextureSamplerDesc d = TextureSamplerDesc::FromLinearFlag(false);
    EXPECT_EQ(d.filter, TextureFilter::Nearest);
    EXPECT_EQ(d.wrap, TextureWrap::Repeat);
}

TEST(TextureSamplerDescTest, PixelArtClampPreset) {
    TextureSamplerDesc d;
    d.filter = TextureFilter::Nearest;
    d.wrap = TextureWrap::ClampToEdge;
    EXPECT_EQ(d.filter, TextureFilter::Nearest);
    EXPECT_EQ(d.wrap, TextureWrap::ClampToEdge);
}
