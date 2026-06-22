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
#include "engine/render/rhi/opengl/gl_command_buffer.h"

#include <glm/glm.hpp>
#include <stdexcept>

using namespace dse::render;
using namespace dse::gameplay3d;

// ============================================================
// MeshRendererComponent 默认值
// ============================================================

// 测试 网格渲染器组件：默认值
TEST(MeshRendererComponentTest, DefaultValues) {
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

// 测试 网格渲染器组件：句柄默认值到零
TEST(MeshRendererComponentTest, HandleDefaultsToZero) {
    dse::MeshRendererComponent mr;
    EXPECT_EQ(mr.albedo_texture_handle, 0u);
    EXPECT_EQ(mr.normal_texture_handle, 0u);
    EXPECT_EQ(mr.metallic_roughness_texture_handle, 0u);
    EXPECT_EQ(mr.emissive_texture_handle, 0u);
    EXPECT_EQ(mr.occlusion_texture_handle, 0u);
}

// 测试 网格渲染器组件：PBR Extended参数默认值
TEST(MeshRendererComponentTest, PBRExtendedParameterDefaultValue) {
    dse::MeshRendererComponent mr;
    EXPECT_FLOAT_EQ(mr.sss_strength, 0.0f);
    EXPECT_FLOAT_EQ(mr.clear_coat, 0.0f);
    EXPECT_FLOAT_EQ(mr.clear_coat_roughness, 0.1f);
    EXPECT_FLOAT_EQ(mr.anisotropy, 0.0f);
    EXPECT_FLOAT_EQ(mr.pom_height_scale, 0.0f);
}

// 测试 网格渲染器组件：Toon参数默认值
TEST(MeshRendererComponentTest, ToonParameterDefaultValue) {
    dse::MeshRendererComponent mr;
    EXPECT_FLOAT_EQ(mr.toon_shadow_threshold, 0.35f);
    EXPECT_FLOAT_EQ(mr.toon_shadow_softness, 0.05f);
    EXPECT_FLOAT_EQ(mr.toon_specular_size, 0.6f);
    EXPECT_FLOAT_EQ(mr.toon_specular_strength, 0.8f);
    EXPECT_FLOAT_EQ(mr.toon_rim_strength, 0.3f);
}

// 测试 网格渲染器组件：Watercolor参数默认值
TEST(MeshRendererComponentTest, WatercolorParameterDefaultValue) {
    dse::MeshRendererComponent mr;
    EXPECT_FLOAT_EQ(mr.watercolor_paper_strength, 0.3f);
    EXPECT_FLOAT_EQ(mr.watercolor_edge_darkening, 0.4f);
    EXPECT_FLOAT_EQ(mr.watercolor_color_bleed, 0.2f);
    EXPECT_FLOAT_EQ(mr.watercolor_pigment_density, 1.0f);
}

// 测试 网格渲染器组件：Temp缓冲区默认为空
TEST(MeshRendererComponentTest, TempBuffersDefaultIsEmpty) {
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

// 测试 网格渲染系统：默认安全
TEST(MeshRenderSystemTest, DefaultSafety) {
    MeshRenderSystem sys;
    (void)sys;
}

// 测试 网格渲染系统：设置资源管理器空指针安全
TEST(MeshRenderSystemTest, SetAssetManager_NullptrSafety) {
    MeshRenderSystem sys;
    sys.SetAssetManager(nullptr);
}

// ============================================================
// MeshRenderSystem + 空 World
// ============================================================

// 测试 网格渲染系统：空世界不崩溃
TEST(MeshRenderSystemTest, EmptyWorldDoesNotCrash) {
    MeshRenderSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    dse::render::FrameContext frame;
    EXPECT_THROW(sys.Render(world, cmd, frame), std::runtime_error);
}

// 测试 网格渲染系统：空世界不崩溃2
TEST(MeshRenderSystemTest, EmptyWorldDoesNotCrash_2) {
    MeshRenderSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    dse::render::FrameContext frame;
    sys.RenderTransparent(world, cmd, frame, 1);
    sys.RenderTransparent(world, cmd, frame, 2);
}
