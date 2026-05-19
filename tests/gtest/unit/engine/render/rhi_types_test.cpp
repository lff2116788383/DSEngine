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

TEST(RenderTargetDescTest, 默认值) {
    RenderTargetDesc desc;
    EXPECT_EQ(desc.width, 0);
    EXPECT_EQ(desc.height, 0);
    EXPECT_TRUE(desc.has_color);
    EXPECT_FALSE(desc.has_depth);
    EXPECT_FALSE(desc.generate_mipmaps);
    EXPECT_FALSE(desc.cube_map);
}

TEST(PipelineStateDescTest, 默认值) {
    PipelineStateDesc desc;
    EXPECT_TRUE(desc.blend_enabled);
    EXPECT_TRUE(desc.depth_test_enabled);
    EXPECT_TRUE(desc.depth_write_enabled);
    EXPECT_TRUE(desc.culling_enabled);
}

TEST(RenderPassDescTest, 默认值) {
    RenderPassDesc desc;
    EXPECT_EQ(desc.render_target, 0u);
    EXPECT_EQ(desc.clear_color, glm::vec4(0.0f));
    EXPECT_FALSE(desc.clear_color_enabled);
}

TEST(RenderTargetReadbackTest, 默认值) {
    RenderTargetReadback rb;
    EXPECT_EQ(rb.width, 0);
    EXPECT_EQ(rb.height, 0);
    EXPECT_TRUE(rb.pixels.empty());
}

// ============================================================
// 绘制项
// ============================================================

TEST(SpriteDrawItemTest, 默认值) {
    SpriteDrawItem item;
    EXPECT_EQ(item.texture_handle, 0u);
    EXPECT_EQ(item.material_instance_id, 0u);
    EXPECT_EQ(item.sorting_layer, 0);
    EXPECT_EQ(item.order_in_layer, 0);
    EXPECT_EQ(item.model, glm::mat4(1.0f));
    EXPECT_EQ(item.color, glm::vec4(1.0f));
    EXPECT_EQ(item.uv, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

TEST(BatchVertexTest, 默认值) {
    BatchVertex v;
    EXPECT_EQ(v.normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(v.tangent, glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(v.weights, glm::vec4(0.0f));
    EXPECT_EQ(v.joints, glm::vec4(0.0f));
}

TEST(MeshDrawItemTest, 默认值) {
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

TEST(Particle3DDrawItemTest, 默认值) {
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

TEST(OpenGLProjectionCorrectionTest, 返回Identity) {
    // OpenGL 不需要投影修正，应返回单位矩阵
    OpenGLRhiDevice device;
    glm::mat4 corr = device.GetProjectionCorrection();
    EXPECT_EQ(corr, glm::mat4(1.0f));
}

// ============================================================
// 渲染统计
// ============================================================

TEST(RenderStatsTest, 默认值全为零) {
    RenderStats stats;
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.material_switches, 0);
    EXPECT_EQ(stats.max_batch_sprites, 0);
    EXPECT_EQ(stats.render_passes, 0);
    EXPECT_EQ(stats.shadow_passes, 0);
}
