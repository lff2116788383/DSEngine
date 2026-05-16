/**
 * @file gl_shader_ubo_manager_test.cpp
 * @brief GLShaderManager + UBOManager 无 GPU 单元测试
 *
 * 测试策略：
 * - PBRShaderLocations / SkyboxShaderLocations / ParticleShaderLocations 默认值
 * - GLShaderManager 构造、访问器、SSBO 标志
 * - UBOManager 构造、initialized 状态
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/gl_shader_manager.h"
#include "engine/render/rhi/ubo_manager.h"

using namespace dse::render;

// ============================================================
// PBRShaderLocations 默认值
// ============================================================

TEST(PBRShaderLocationsTest, 默认值) {
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

TEST(SkyboxShaderLocationsTest, 默认值) {
    SkyboxShaderLocations loc;
    EXPECT_EQ(loc.vp, -1);
    EXPECT_EQ(loc.tex, -1);
}

TEST(ParticleShaderLocationsTest, 默认值) {
    ParticleShaderLocations loc;
    EXPECT_EQ(loc.per_frame_block_index, 0u);
    EXPECT_EQ(loc.texture, -1);
}

// ============================================================
// GLShaderManager
// ============================================================

TEST(GLShaderManagerTest, 默认构造安全) {
    GLShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.particle_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

TEST(GLShaderManagerTest, SSBO标志) {
    GLShaderManager mgr;
    EXPECT_TRUE(mgr.supports_ssbo()); // 默认 true
    mgr.set_supports_ssbo(false);
    EXPECT_FALSE(mgr.supports_ssbo());
    mgr.set_supports_ssbo(true);
    EXPECT_TRUE(mgr.supports_ssbo());
}

TEST(GLShaderManagerTest, HasPostProcessShader_空缓存) {
    GLShaderManager mgr;
    EXPECT_FALSE(mgr.HasPostProcessShader("bloom"));
    EXPECT_FALSE(mgr.HasPostProcessShader("tonemap"));
    EXPECT_FALSE(mgr.HasPostProcessShader(""));
}

TEST(GLShaderManagerTest, SetSkyboxHandle) {
    GLShaderManager mgr;
    mgr.set_skybox_shader_handle(42);
    EXPECT_EQ(mgr.skybox_shader_handle(), 42u);
}

TEST(GLShaderManagerTest, SetParticleHandle) {
    GLShaderManager mgr;
    mgr.set_particle_shader_handle(99);
    EXPECT_EQ(mgr.particle_shader_handle(), 99u);
}

// ============================================================
// UBOManager
// ============================================================

TEST(UBOManagerTest, 默认构造安全) {
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
