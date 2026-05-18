/**
 * @file mesh_render_system_test.cpp
 * @brief MeshRenderSystem 无 GPU 单元测试
 *
 * 测试策略：
 * - 构造 / 默认状态
 * - SetAssetManager 注入
 * - 空 World + OpenGLCommandBuffer 调用 Render 不崩溃
 * - 有 MeshRendererComponent 但无 mesh 数据时安全跳过
 * - MeshRendererComponent 默认值完整性
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/gl_command_buffer.h"

#include <glm/glm.hpp>
#include <stdexcept>

using namespace dse::gameplay3d;

// ============================================================
// MeshRendererComponent 默认值
// ============================================================

TEST(MeshRendererComponentTest, 默认值) {
    dse::MeshRendererComponent mr;
    EXPECT_TRUE(mr.mesh_path.empty());
    EXPECT_EQ(mr.material_instance_id, 0u);
    EXPECT_EQ(mr.shader_variant, "MESH_UNLIT");
    EXPECT_FLOAT_EQ(mr.metallic, 0.0f);
    EXPECT_FLOAT_EQ(mr.roughness, 0.5f);
    EXPECT_FLOAT_EQ(mr.ao, 1.0f);
    EXPECT_FLOAT_EQ(mr.normal_strength, 1.0f);
    EXPECT_TRUE(mr.receive_shadow);
    EXPECT_TRUE(mr.depth_test_enabled);
    EXPECT_TRUE(mr.depth_write_enabled);
    EXPECT_TRUE(mr.visible);
    EXPECT_EQ(mr.sorting_layer, 0);
    EXPECT_EQ(mr.order_in_layer, 0);
}

TEST(MeshRendererComponentTest, 纹理句柄默认为零) {
    dse::MeshRendererComponent mr;
    EXPECT_EQ(mr.albedo_texture_handle, 0u);
    EXPECT_EQ(mr.normal_texture_handle, 0u);
    EXPECT_EQ(mr.metallic_roughness_texture_handle, 0u);
    EXPECT_EQ(mr.emissive_texture_handle, 0u);
    EXPECT_EQ(mr.occlusion_texture_handle, 0u);
}

TEST(MeshRendererComponentTest, PBR扩展参数默认值) {
    dse::MeshRendererComponent mr;
    EXPECT_FLOAT_EQ(mr.sss_strength, 0.0f);
    EXPECT_FLOAT_EQ(mr.clear_coat, 0.0f);
    EXPECT_FLOAT_EQ(mr.clear_coat_roughness, 0.1f);
    EXPECT_FLOAT_EQ(mr.anisotropy, 0.0f);
    EXPECT_FLOAT_EQ(mr.pom_height_scale, 0.0f);
}

TEST(MeshRendererComponentTest, Toon参数默认值) {
    dse::MeshRendererComponent mr;
    EXPECT_FLOAT_EQ(mr.toon_shadow_threshold, 0.35f);
    EXPECT_FLOAT_EQ(mr.toon_shadow_softness, 0.05f);
    EXPECT_FLOAT_EQ(mr.toon_specular_size, 0.6f);
    EXPECT_FLOAT_EQ(mr.toon_specular_strength, 0.8f);
    EXPECT_FLOAT_EQ(mr.toon_rim_strength, 0.3f);
}

TEST(MeshRendererComponentTest, Watercolor参数默认值) {
    dse::MeshRendererComponent mr;
    EXPECT_FLOAT_EQ(mr.watercolor_paper_strength, 0.3f);
    EXPECT_FLOAT_EQ(mr.watercolor_edge_darkening, 0.4f);
    EXPECT_FLOAT_EQ(mr.watercolor_color_bleed, 0.2f);
    EXPECT_FLOAT_EQ(mr.watercolor_pigment_density, 1.0f);
}

TEST(MeshRendererComponentTest, TempBuffers默认为空) {
    dse::MeshRendererComponent mr;
    EXPECT_TRUE(mr.temp_vertices.empty());
    EXPECT_TRUE(mr.temp_indices.empty());
    EXPECT_TRUE(mr.temp_uvs.empty());
    EXPECT_TRUE(mr.temp_normals.empty());
    EXPECT_TRUE(mr.temp_tangents.empty());
    EXPECT_EQ(mr.dmesh_vertex_stride, 20);
}

// ============================================================
// MeshRenderSystem 构造与注入
// ============================================================

TEST(MeshRenderSystemTest, 默认构造安全) {
    MeshRenderSystem sys;
    (void)sys;
}

TEST(MeshRenderSystemTest, SetAssetManager_nullptr安全) {
    MeshRenderSystem sys;
    sys.SetAssetManager(nullptr);
}

// ============================================================
// MeshRenderSystem + 空 World
// ============================================================

TEST(MeshRenderSystemTest, 空World不崩溃) {
    MeshRenderSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    EXPECT_THROW(sys.Render(world, cmd), std::runtime_error);
}

TEST(MeshRenderSystemTest, 空World透明渲染不崩溃) {
    MeshRenderSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    sys.RenderTransparent(world, cmd, 1);
    sys.RenderTransparent(world, cmd, 2);
}
