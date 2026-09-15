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

// 测试 渲染目标描述符：默认值
TEST(RenderTargetDescTest, DefaultValues) {
    RenderTargetDesc desc{};
    EXPECT_EQ(desc.width, 0);
    EXPECT_EQ(desc.height, 0);
    EXPECT_TRUE(desc.has_color);
    EXPECT_FALSE(desc.has_depth);
    EXPECT_FALSE(desc.has_stencil);  // ADR-2 第 2 步：默认关闭，保证零行为变化
    EXPECT_FALSE(desc.generate_mipmaps);
    EXPECT_FALSE(desc.cube_map);
}

// 测试 管线状态描述符：默认值
TEST(PipelineStateDescTest, DefaultValues) {
    PipelineStateDesc desc{};
    EXPECT_TRUE(desc.blend_enabled);
    EXPECT_TRUE(desc.depth_test_enabled);
    EXPECT_TRUE(desc.depth_write_enabled);
    EXPECT_TRUE(desc.culling_enabled);
    EXPECT_FALSE(desc.stencil.enabled);
    EXPECT_EQ(desc.stencil.read_mask, 0xFFu);
    EXPECT_EQ(desc.stencil.write_mask, 0xFFu);
    EXPECT_EQ(desc.stencil.reference, 0u);
    EXPECT_EQ(desc.stencil.front.compare, CompareFunc::Always);
    EXPECT_EQ(desc.stencil.front.fail_op, StencilOp::Keep);
    EXPECT_EQ(desc.stencil.front.depth_fail_op, StencilOp::Keep);
    EXPECT_EQ(desc.stencil.front.pass_op, StencilOp::Keep);
    EXPECT_EQ(desc.stencil.back.compare, CompareFunc::Always);
    EXPECT_EQ(desc.stencil.back.fail_op, StencilOp::Keep);
    EXPECT_EQ(desc.stencil.back.depth_fail_op, StencilOp::Keep);
    EXPECT_EQ(desc.stencil.back.pass_op, StencilOp::Keep);
    // StencilOp 的显式取值是四端映射函数的契约（ADR-2 第 3 步按此实现），
    // 逐值钉住以防将来无意重编号。
    EXPECT_EQ(static_cast<int>(StencilOp::Keep), 0);
    EXPECT_EQ(static_cast<int>(StencilOp::Zero), 1);
    EXPECT_EQ(static_cast<int>(StencilOp::Replace), 2);
    EXPECT_EQ(static_cast<int>(StencilOp::IncrementClamp), 3);
    EXPECT_EQ(static_cast<int>(StencilOp::DecrementClamp), 4);
    EXPECT_EQ(static_cast<int>(StencilOp::Invert), 5);
    EXPECT_EQ(static_cast<int>(StencilOp::IncrementWrap), 6);
    EXPECT_EQ(static_cast<int>(StencilOp::DecrementWrap), 7);
}

// 测试 渲染通道描述符：默认值
TEST(RenderPassDescTest, DefaultValues) {
    RenderPassDesc desc;
    EXPECT_FALSE(desc.render_target);
    EXPECT_EQ(desc.clear_color, glm::vec4(0.0f));
    EXPECT_FALSE(desc.clear_color_enabled);
}

// 测试 渲染目标回读：默认值
TEST(RenderTargetReadbackTest, DefaultValues) {
    RenderTargetReadback rb;
    EXPECT_EQ(rb.width, 0);
    EXPECT_EQ(rb.height, 0);
    EXPECT_TRUE(rb.pixels.empty());
}

// ============================================================
// 绘制项
// ============================================================

// 测试 精灵绘制项：默认值
TEST(SpriteDrawItemTest, DefaultValues) {
    SpriteDrawItem item;
    EXPECT_FALSE(item.texture_handle);
    EXPECT_EQ(item.material_instance_id, 0u);
    EXPECT_EQ(item.sorting_layer, 0);
    EXPECT_EQ(item.order_in_layer, 0);
    EXPECT_EQ(item.model, glm::mat4(1.0f));
    EXPECT_EQ(item.color, glm::vec4(1.0f));
    EXPECT_EQ(item.uv, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

// 测试 批次顶点：默认值
TEST(BatchVertexTest, DefaultValues) {
    BatchVertex v;
    EXPECT_EQ(v.normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(v.tangent, glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(v.weights, glm::vec4(0.0f));
    EXPECT_EQ(v.joints, glm::vec4(0.0f));
}

// 测试 网格绘制项：默认值
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

// ============================================================
// RHI 统一回归测试 — OpenGL ProjectionCorrection
// ============================================================

// 测试 打开GL投影校正：返回单位
TEST(OpenGLProjectionCorrectionTest, ReturnsIdentity) {
    // OpenGL 不需要投影修正，应返回单位矩阵
    OpenGLRhiDevice device;
    glm::mat4 corr = device.GetProjectionCorrection();
    EXPECT_EQ(corr, glm::mat4(1.0f));
}

// ============================================================
// 渲染统计
// ============================================================

// 测试 渲染统计：默认值Allis零
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

// 测试 纹理采样器描述符：默认值线性重复
TEST(TextureSamplerDescTest, Defaults_LinearRepeat) {
    TextureSamplerDesc d;
    EXPECT_EQ(d.filter, TextureFilter::Linear);
    EXPECT_EQ(d.wrap, TextureWrap::Repeat);
}

// 测试 纹理采样器描述符：从线性标志真为线性重复
TEST(TextureSamplerDescTest, FromLinearFlag_True_IsLinearRepeat) {
    TextureSamplerDesc d = TextureSamplerDesc::FromLinearFlag(true);
    EXPECT_EQ(d.filter, TextureFilter::Linear);
    EXPECT_EQ(d.wrap, TextureWrap::Repeat);
}

// 测试 纹理采样器描述符：从线性标志假为最近重复
TEST(TextureSamplerDescTest, FromLinearFlag_False_IsNearestRepeat) {
    TextureSamplerDesc d = TextureSamplerDesc::FromLinearFlag(false);
    EXPECT_EQ(d.filter, TextureFilter::Nearest);
    EXPECT_EQ(d.wrap, TextureWrap::Repeat);
}

// 测试 纹理采样器描述符：Pixel Art钳制Preset
TEST(TextureSamplerDescTest, PixelArtClampPreset) {
    TextureSamplerDesc d;
    d.filter = TextureFilter::Nearest;
    d.wrap = TextureWrap::ClampToEdge;
    EXPECT_EQ(d.filter, TextureFilter::Nearest);
    EXPECT_EQ(d.wrap, TextureWrap::ClampToEdge);
}
