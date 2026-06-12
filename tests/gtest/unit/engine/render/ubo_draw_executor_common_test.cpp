/**
 * @file ubo_draw_executor_common_test.cpp
 * @brief UBO 类型 + DrawExecutorGlobalState + Prepare*UBO 辅助函数的纯 CPU 测试
 *
 * 测试策略：
 * - UBO 结构体 std140 对齐 (static_assert 已涵盖，此处验证字段默认值)
 * - DrawExecutorGlobalState setter / getter
 * - PreparePerSceneUBO / PreparePerMaterialUBO / PreparePointLightsUBO
 *   / PrepareSpotLightsUBO / PrepareLightProbeUBO / PrepareSpotLightDataUBO
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/ubo_types.h"
#include "engine/render/rhi/draw_executor_common.h"
#include <glm/glm.hpp>
#include <cstring>

using namespace dse::render;

// ============================================================
// UBO 结构体大小与默认值
// ============================================================

TEST(UBOTypesTest, PerFrameUBOsize176) {
    EXPECT_EQ(sizeof(PerFrameUBO), 176u);
}

TEST(UBOTypesTest, PerSceneUBOsize304) {
    EXPECT_EQ(sizeof(PerSceneUBO), 304u);
}

TEST(UBOTypesTest, PerMaterialUBOsize128) {
    EXPECT_EQ(sizeof(PerMaterialUBO), 128u);
}

TEST(UBOTypesTest, PointLightEntrysize48) {
    EXPECT_EQ(sizeof(PointLightEntry), 48u);
}

TEST(UBOTypesTest, SpotLightEntrysize64) {
    EXPECT_EQ(sizeof(SpotLightEntry), 64u);
}

TEST(UBOTypesTest, BoneMatricesUBOsize16320) {
    EXPECT_EQ(sizeof(BoneMatricesUBO), 16320u);
}

TEST(UBOTypesTest, MorphWeightsUBOsize64) {
    EXPECT_EQ(sizeof(MorphWeightsUBO), 64u);
}

TEST(UBOTypesTest, LightProbeDataUBOsize160) {
    EXPECT_EQ(sizeof(LightProbeDataUBO), 160u);
}

TEST(UBOTypesTest, SpotLightDataUBOsize256) {
    EXPECT_EQ(sizeof(SpotLightDataUBO), 256u);
}

TEST(UBOTypesTest, BindingPointenumerationValue) {
    EXPECT_EQ(static_cast<unsigned int>(UBOBindingPoint::PerFrame), 0u);
    EXPECT_EQ(static_cast<unsigned int>(UBOBindingPoint::PerScene), 1u);
    EXPECT_EQ(static_cast<unsigned int>(UBOBindingPoint::PerMaterial), 2u);
    EXPECT_EQ(static_cast<unsigned int>(UBOBindingPoint::PointLights), 3u);
    EXPECT_EQ(static_cast<unsigned int>(UBOBindingPoint::SpotLights), 4u);
    EXPECT_EQ(static_cast<unsigned int>(UBOBindingPoint::BoneMatrices), 6u);
    EXPECT_EQ(static_cast<unsigned int>(UBOBindingPoint::LightProbeData), 8u);
    EXPECT_EQ(static_cast<unsigned int>(UBOBindingPoint::Count), 9u);
}

// ============================================================
// DrawExecutorGlobalState
// ============================================================

TEST(DrawExecutorGlobalStateTest, DefaultValues) {
    DrawExecutorGlobalState s;
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(s.shadow_map[i], 0u);
        EXPECT_FLOAT_EQ(s.cascade_splits[i], 0.0f);
    }
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(s.spot_shadow_map[i], 0u);
        EXPECT_EQ(s.point_shadow_map[i], 0u);
    }
    EXPECT_FALSE(s.light_probe_enabled);
    EXPECT_FALSE(s.gbuffer_rendering_mode);
    EXPECT_FALSE(s.force_unlit);
    EXPECT_FALSE(s.overdraw_mode);
}

TEST(DrawExecutorGlobalStateTest, SetShadowMap) {
    DrawExecutorGlobalState s;
    s.SetShadowMap(0, 42);
    s.SetShadowMap(2, 99);
    s.SetShadowMap(5, 123); // 越界，不应崩溃
    EXPECT_EQ(s.shadow_map[0], 42u);
    EXPECT_EQ(s.shadow_map[2], 99u);
}

TEST(DrawExecutorGlobalStateTest, SetSpotShadowMap) {
    DrawExecutorGlobalState s;
    s.SetSpotShadowMap(1, 77);
    EXPECT_EQ(s.spot_shadow_map[1], 77u);
    s.SetSpotShadowMap(10, 0); // 越界
}

TEST(DrawExecutorGlobalStateTest, SetPointShadowMap) {
    DrawExecutorGlobalState s;
    s.SetPointShadowMap(3, 55);
    EXPECT_EQ(s.point_shadow_map[3], 55u);
}

TEST(DrawExecutorGlobalStateTest, SetLightSpaceMatrix) {
    DrawExecutorGlobalState s;
    glm::mat4 m(2.0f);
    s.SetLightSpaceMatrix(1, m);
    EXPECT_FLOAT_EQ(s.light_space_matrix[1][0][0], 2.0f);
}

TEST(DrawExecutorGlobalStateTest, SetCascadeSplit) {
    DrawExecutorGlobalState s;
    s.SetCascadeSplit(0, 10.0f);
    s.SetCascadeSplit(1, 50.0f);
    s.SetCascadeSplit(2, 200.0f);
    EXPECT_FLOAT_EQ(s.cascade_splits[0], 10.0f);
    EXPECT_FLOAT_EQ(s.cascade_splits[1], 50.0f);
    EXPECT_FLOAT_EQ(s.cascade_splits[2], 200.0f);
}

TEST(DrawExecutorGlobalStateTest, SetLightProbeSH) {
    DrawExecutorGlobalState s;
    glm::vec4 sh[9];
    for (int i = 0; i < 9; ++i) sh[i] = glm::vec4(float(i));
    s.SetLightProbeSH(sh, true);
    EXPECT_TRUE(s.light_probe_enabled);
    EXPECT_FLOAT_EQ(s.light_probe_sh[3].x, 3.0f);
}

TEST(DrawExecutorGlobalStateTest, BeginEndFrame) {
    DrawExecutorGlobalState s;
    s.current_frame_stats.draw_calls = 10;
    s.EndFrame();
    EXPECT_EQ(s.last_frame_stats.draw_calls, 10);
    s.BeginFrame();
    EXPECT_EQ(s.current_frame_stats.draw_calls, 0);
}

TEST(DrawExecutorGlobalStateTest, SetGBufferTexture) {
    DrawExecutorGlobalState s;
    s.SetGBufferTexture(0, 111);
    s.SetGBufferTexture(3, 222);
    s.SetGBufferTexture(4, 333); // 越界
    EXPECT_EQ(s.gbuffer_texture[0], 111u);
    EXPECT_EQ(s.gbuffer_texture[3], 222u);
}

// ============================================================
// PreparePerSceneUBO
// ============================================================

TEST(PrepareUBOTest, PreparePerSceneUBO) {
    MeshDrawItem item;
    item.light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    item.lighting_enabled = true;
    item.light_color = glm::vec3(1.0f);
    item.ambient_intensity = 0.3f;
    item.light_intensity = 2.0f;
    item.shadow_strength = 0.8f;
    item.receive_shadow = true;
    item.shading_mode = 1;

    DrawExecutorGlobalState state;
    state.SetCascadeSplit(0, 10.0f);
    state.SetCascadeSplit(1, 50.0f);
    state.SetCascadeSplit(2, 200.0f);
    glm::mat4 lsm(3.0f);
    for (int i = 0; i < 3; ++i) state.SetLightSpaceMatrix(i, lsm);

    PerSceneUBO ubo = PreparePerSceneUBO(item, state);
    EXPECT_FLOAT_EQ(ubo.light_dir_and_enabled.y, -1.0f);
    EXPECT_FLOAT_EQ(ubo.light_dir_and_enabled.w, 1.0f); // enabled
    EXPECT_FLOAT_EQ(ubo.light_color_and_ambient.w, 0.3f);
    EXPECT_FLOAT_EQ(ubo.light_params.x, 2.0f); // intensity
    EXPECT_FLOAT_EQ(ubo.light_params.y, 0.8f); // shadow_strength
    EXPECT_FLOAT_EQ(ubo.cascade_splits.x, 10.0f);
    EXPECT_FLOAT_EQ(ubo.light_space_matrices[0][0][0], 3.0f);
}

// ============================================================
// PreparePerMaterialUBO
// ============================================================

TEST(PrepareUBOTest, PreparePerMaterialUBO) {
    MeshDrawItem item;
    item.material_albedo = glm::vec3(0.9f, 0.1f, 0.2f);
    item.material_metallic = 0.8f;
    item.material_roughness = 0.4f;
    item.material_ao = 0.9f;
    item.normal_map_handle = 5;
    item.emissive_map_handle = 0;

    DrawExecutorGlobalState state;
    PerMaterialUBO ubo = PreparePerMaterialUBO(item, state);
    EXPECT_FLOAT_EQ(ubo.albedo.x, 0.9f);
    EXPECT_FLOAT_EQ(ubo.albedo.w, 0.8f); // metallic
    EXPECT_FLOAT_EQ(ubo.roughness_ao.x, 0.4f);
    EXPECT_FLOAT_EQ(ubo.flags.x, 1.0f); // has_normal_map
    EXPECT_FLOAT_EQ(ubo.flags.z, 0.0f); // no emissive_map
}

// ============================================================
// Scene View Mode 行为验证
// ============================================================

TEST(DrawExecutorGlobalStateTest, ForceUnlitOffByDefault) {
    DrawExecutorGlobalState s;
    EXPECT_FALSE(s.force_unlit);
}

TEST(DrawExecutorGlobalStateTest, OverdrawModeOffByDefault) {
    DrawExecutorGlobalState s;
    EXPECT_FALSE(s.overdraw_mode);
}

TEST(DrawExecutorGlobalStateTest, ForceUnlitReadableAndWritable) {
    DrawExecutorGlobalState s;
    s.force_unlit = true;
    EXPECT_TRUE(s.force_unlit);
    s.force_unlit = false;
    EXPECT_FALSE(s.force_unlit);
}

TEST(DrawExecutorGlobalStateTest, OverdrawModeReadableAndWritable) {
    DrawExecutorGlobalState s;
    s.overdraw_mode = true;
    EXPECT_TRUE(s.overdraw_mode);
    s.overdraw_mode = false;
    EXPECT_FALSE(s.overdraw_mode);
}

TEST(PrepareUBOTest, PreparePerSceneUBO_ForceUnlitForceLightsOff) {
    MeshDrawItem item;
    item.light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    item.lighting_enabled = true;

    DrawExecutorGlobalState state;
    state.force_unlit = true;

    PerSceneUBO ubo = PreparePerSceneUBO(item, state);
    EXPECT_FLOAT_EQ(ubo.light_dir_and_enabled.w, 0.0f);
}

TEST(PrepareUBOTest, PreparePerSceneUBO_ForceUnlitDoesNotAffectTheEntityThatTurnsOffTheLights) {
    MeshDrawItem item;
    item.light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    item.lighting_enabled = false;

    DrawExecutorGlobalState state;
    state.force_unlit = false;

    PerSceneUBO ubo = PreparePerSceneUBO(item, state);
    EXPECT_FLOAT_EQ(ubo.light_dir_and_enabled.w, 0.0f);
}

TEST(PrepareUBOTest, PreparePerMaterialUBO_OverdrawfixedColor) {
    MeshDrawItem item;
    item.material_albedo = glm::vec3(1.0f, 0.0f, 0.0f);
    item.material_metallic = 1.0f;

    DrawExecutorGlobalState state;
    state.overdraw_mode = true;

    PerMaterialUBO ubo = PreparePerMaterialUBO(item, state);
    EXPECT_FLOAT_EQ(ubo.albedo.x, 0.1f);
    EXPECT_FLOAT_EQ(ubo.albedo.y, 0.04f);
    EXPECT_FLOAT_EQ(ubo.albedo.z, 0.02f);
    EXPECT_FLOAT_EQ(ubo.albedo.w, 0.0f);
}

TEST(PrepareUBOTest, PreparePerMaterialUBO_NormalModeRetainsOriginalColors) {
    MeshDrawItem item;
    item.material_albedo = glm::vec3(0.5f, 0.6f, 0.7f);
    item.material_metallic = 0.3f;

    DrawExecutorGlobalState state;
    state.overdraw_mode = false;

    PerMaterialUBO ubo = PreparePerMaterialUBO(item, state);
    EXPECT_FLOAT_EQ(ubo.albedo.x, 0.5f);
    EXPECT_FLOAT_EQ(ubo.albedo.y, 0.6f);
    EXPECT_FLOAT_EQ(ubo.albedo.z, 0.7f);
    EXPECT_FLOAT_EQ(ubo.albedo.w, 0.3f);
}

// ============================================================
// PreparePointLightsUBO / PrepareSpotLightsUBO
// ============================================================

TEST(PrepareUBOTest, PreparePointLightsUBO_EmptyLightSource) {
    MeshDrawItem item;
    PointLightsUBO ubo = PreparePointLightsUBO(item);
    EXPECT_EQ(ubo.u_point_light_count, 0);
}

TEST(PrepareUBOTest, PreparePointLightsUBO_ThereIsLightSource) {
    MeshDrawItem item;
    MeshDrawItem::PointLightData pl;
    pl.color = glm::vec3(1.0f, 0.5f, 0.0f);
    pl.intensity = 3.0f;
    pl.position = glm::vec3(10.0f, 20.0f, 30.0f);
    pl.radius = 50.0f;
    pl.cast_shadow = true;
    pl.shadow_index = 2;
    item.point_lights.push_back(pl);

    PointLightsUBO ubo = PreparePointLightsUBO(item);
    EXPECT_EQ(ubo.u_point_light_count, 1);
    EXPECT_FLOAT_EQ(ubo.u_point_lights[0].color.x, 1.0f);
    EXPECT_FLOAT_EQ(ubo.u_point_lights[0].intensity, 3.0f);
    EXPECT_EQ(ubo.u_point_lights[0].cast_shadow, 1);
    EXPECT_EQ(ubo.u_point_lights[0].shadow_index, 2);
}

TEST(PrepareUBOTest, PrepareSpotLightsUBO_ThereIsLightSource) {
    MeshDrawItem item;
    MeshDrawItem::SpotLightData sl;
    sl.color = glm::vec3(0.0f, 1.0f, 0.0f);
    sl.intensity = 5.0f;
    sl.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    sl.inner_cone = 0.9f;
    sl.outer_cone = 0.8f;
    item.spot_lights.push_back(sl);

    SpotLightsUBO ubo = PrepareSpotLightsUBO(item);
    EXPECT_EQ(ubo.u_spot_light_count, 1);
    EXPECT_FLOAT_EQ(ubo.u_spot_lights[0].inner_cone, 0.9f);
    EXPECT_FLOAT_EQ(ubo.u_spot_lights[0].outer_cone, 0.8f);
}

// ============================================================
// PrepareLightProbeUBO / PrepareSpotLightDataUBO
// ============================================================

TEST(PrepareUBOTest, PrepareLightProbeUBO) {
    DrawExecutorGlobalState state;
    glm::vec4 sh[9];
    for (int i = 0; i < 9; ++i) sh[i] = glm::vec4(float(i) * 0.1f);
    state.SetLightProbeSH(sh, true);

    LightProbeDataUBO ubo = PrepareLightProbeUBO(state);
    EXPECT_FLOAT_EQ(ubo.sh_coefficients[5].x, 0.5f);
    EXPECT_FLOAT_EQ(ubo.probe_params.x, 1.0f); // enabled
}

TEST(PrepareUBOTest, PrepareSpotLightDataUBO) {
    DrawExecutorGlobalState state;
    glm::mat4 m(7.0f);
    state.SetSpotLightSpaceMatrix(2, m);

    SpotLightDataUBO ubo = PrepareSpotLightDataUBO(state);
    EXPECT_FLOAT_EQ(ubo.u_spot_light_space_matrices[2][0][0], 7.0f);
}
