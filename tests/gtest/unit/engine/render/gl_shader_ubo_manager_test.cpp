/**
 * @file gl_shader_ubo_manager_test.cpp
 * @brief GLShaderManager + UBOManager 无 GPU 单元测试
 *
 * 测试策略：
 * - PBRShaderLocations / SkyboxShaderLocations 默认值
 * - GLShaderManager 构造、访问器、SSBO 标志
 * - UBOManager 构造、initialized 状态
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/opengl/ubo_manager.h"

using namespace dse::render;

// ============================================================
// PBRShaderLocations 默认值
// ============================================================

// 测试 PBR着色器位置：默认值
TEST(PBRShaderLocationsTest, DefaultValues) {
    PBRShaderLocations loc;
    EXPECT_EQ(loc.per_frame_block_index, 0u);
    EXPECT_EQ(loc.per_scene_block_index, 0u);
    EXPECT_EQ(loc.per_material_block_index, 0u);
    EXPECT_EQ(loc.texture, -1);
    EXPECT_EQ(loc.normal_map, -1);
    EXPECT_EQ(loc.model, -1);
    EXPECT_EQ(loc.skinned, -1);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(loc.shadow_map[i], -1);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(loc.spot_shadow_map[i], -1);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(loc.point_shadow_map[i], -1);
}

// 测试 天空盒着色器位置：默认值
TEST(SkyboxShaderLocationsTest, DefaultValues) {
    SkyboxShaderLocations loc;
    EXPECT_EQ(loc.vp, -1);
    EXPECT_EQ(loc.tex, -1);
}

// ============================================================
// GLShaderManager
// ============================================================

// 测试 GL着色器管理器：默认安全
TEST(GLShaderManagerTest, DefaultSafety) {
    GLShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

// 测试 GL着色器管理器：SSBO Ologo
TEST(GLShaderManagerTest, SSBOlogo) {
    GLShaderManager mgr;
    EXPECT_TRUE(mgr.supports_ssbo()); // 默认 true
    mgr.set_supports_ssbo(false);
    EXPECT_FALSE(mgr.supports_ssbo());
    mgr.set_supports_ssbo(true);
    EXPECT_TRUE(mgr.supports_ssbo());
}

// 测试 GL着色器管理器：Gen PP着色器未知特效返回零
TEST(GLShaderManagerTest, GenPPShader_UnknownEffectReturnsZero) {
    GLShaderManager mgr;
    EXPECT_EQ(mgr.GetOrCreateGenPPShader("__nonexistent__"), 0u);
    EXPECT_EQ(mgr.GetOrCreateGenPPShader(""), 0u);
}

// 测试 GL着色器管理器：设置天空盒句柄
TEST(GLShaderManagerTest, SetSkyboxHandle) {
    GLShaderManager mgr;
    mgr.set_skybox_shader_handle(42);
    EXPECT_EQ(mgr.skybox_shader_handle(), 42u);
}

// ============================================================
// UBOManager
// ============================================================

// 测试 UBO管理器：默认安全
TEST(UBOManagerTest, DefaultSafety) {
    UBOManager mgr;
    EXPECT_FALSE(mgr.initialized());
    EXPECT_EQ(mgr.per_frame_buffer(), 0u);
    EXPECT_EQ(mgr.per_scene_buffer(), 0u);
    EXPECT_EQ(mgr.per_material_buffer(), 0u);
    EXPECT_EQ(mgr.point_lights_buffer(), 0u);
    EXPECT_EQ(mgr.spot_lights_buffer(), 0u);
    EXPECT_EQ(mgr.bone_matrices_buffer(), 0u);
    EXPECT_EQ(mgr.morph_weights_buffer(), 0u);
    EXPECT_EQ(mgr.light_probe_data_buffer(), 0u);
}
