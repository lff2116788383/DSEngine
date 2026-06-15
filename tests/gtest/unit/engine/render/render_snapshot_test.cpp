/**
 * @file render_snapshot_test.cpp
 * @brief RenderThinSnapshot 默认值 / Reset / 字段填充 / constexpr 限制测试
 */

#include <gtest/gtest.h>
#include "engine/render/render_snapshot.h"

using namespace dse::render;

// 测试 渲染快照：默认相机3D无效
TEST(RenderSnapshotTest, DefaultCamera3DInvalid) {
    RenderThinSnapshot snap;
    EXPECT_FALSE(snap.camera_3d.valid);
    EXPECT_FLOAT_EQ(snap.camera_3d.fov, 60.0f);
    EXPECT_FLOAT_EQ(snap.camera_3d.near_clip, 0.1f);
    EXPECT_FLOAT_EQ(snap.camera_3d.far_clip, 1000.0f);
}

// 测试 渲染快照：默认相机2D无效
TEST(RenderSnapshotTest, DefaultCamera2DInvalid) {
    RenderThinSnapshot snap;
    EXPECT_FALSE(snap.camera_2d.valid);
}

// 测试 渲染快照：默认天空盒无效
TEST(RenderSnapshotTest, DefaultSkyboxInvalid) {
    RenderThinSnapshot snap;
    EXPECT_FALSE(snap.skybox.valid);
    EXPECT_EQ(snap.skybox.cubemap_handle, 0u);
}

// 测试 渲染快照：默认方向光灯光
TEST(RenderSnapshotTest, DefaultDirectionalLight) {
    RenderThinSnapshot snap;
    EXPECT_FALSE(snap.directional_light.valid);
    EXPECT_FALSE(snap.directional_light.cast_shadow);
    EXPECT_FLOAT_EQ(snap.directional_light.intensity, 1.0f);
    EXPECT_FLOAT_EQ(snap.directional_light.ambient_intensity, 0.1f);
}

// 测试 渲染快照：默认后期处理
TEST(RenderSnapshotTest, DefaultPostProcess) {
    RenderThinSnapshot snap;
    EXPECT_FALSE(snap.post_process.valid);
    EXPECT_TRUE(snap.post_process.enabled);
    EXPECT_TRUE(snap.post_process.bloom_enabled);
    EXPECT_FLOAT_EQ(snap.post_process.gamma, 2.2f);
    EXPECT_TRUE(snap.post_process.fxaa_enabled);
    EXPECT_FALSE(snap.post_process.taa_enabled);
    EXPECT_FALSE(snap.post_process.ssao_enabled);
    EXPECT_FALSE(snap.post_process.dof_enabled);
}

// 测试 渲染快照：Constexpr Limits
TEST(RenderSnapshotTest, ConstexprLimits) {
    EXPECT_EQ(RenderThinSnapshot::kMaxSpotShadowLights, 4);
    EXPECT_EQ(RenderThinSnapshot::kMaxPointShadowLights, 4);
    EXPECT_EQ(RenderThinSnapshot::kMaxWaterSurfaces, 8);
    EXPECT_EQ(RenderThinSnapshot::kMaxDecals, 32);
}

// 测试 渲染快照：默认Counts零
TEST(RenderSnapshotTest, DefaultCountsZero) {
    RenderThinSnapshot snap;
    EXPECT_EQ(snap.spot_shadow_count, 0);
    EXPECT_EQ(snap.point_shadow_count, 0);
    EXPECT_EQ(snap.water_count, 0);
    EXPECT_EQ(snap.decal_count, 0);
}

// 测试 渲染快照：默认灯光探针SH无效
TEST(RenderSnapshotTest, DefaultLightProbeSHInvalid) {
    RenderThinSnapshot snap;
    EXPECT_FALSE(snap.light_probe_sh.valid);
}

// 测试 渲染快照：默认DDGI禁用
TEST(RenderSnapshotTest, DefaultDDGIDisabled) {
    RenderThinSnapshot snap;
    EXPECT_FALSE(snap.ddgi_config.enabled);
    EXPECT_FALSE(snap.ddgi_config.needs_reinit);
    EXPECT_FLOAT_EQ(snap.ddgi_config.hysteresis, 0.97f);
    EXPECT_EQ(snap.ddgi_config.rays_per_probe, 128);
}

// 测试 渲染快照：Populate且重置
TEST(RenderSnapshotTest, PopulateAndReset) {
    RenderThinSnapshot snap;

    snap.camera_3d.valid = true;
    snap.camera_3d.fov = 90.0f;
    snap.camera_3d.position = glm::vec3(10.0f, 20.0f, 30.0f);

    snap.directional_light.valid = true;
    snap.directional_light.intensity = 5.0f;

    snap.spot_shadow_count = 3;
    snap.point_shadow_count = 2;
    snap.water_count = 1;
    snap.decal_count = 5;

    snap.post_process.valid = true;
    snap.post_process.bloom_threshold = 2.0f;
    snap.ddgi_config.enabled = true;

    EXPECT_TRUE(snap.camera_3d.valid);
    EXPECT_TRUE(snap.directional_light.valid);
    EXPECT_EQ(snap.spot_shadow_count, 3);
    EXPECT_EQ(snap.decal_count, 5);

    snap.Reset();

    EXPECT_FALSE(snap.camera_3d.valid);
    EXPECT_FLOAT_EQ(snap.camera_3d.fov, 60.0f);
    EXPECT_FALSE(snap.directional_light.valid);
    EXPECT_FLOAT_EQ(snap.directional_light.intensity, 1.0f);
    EXPECT_EQ(snap.spot_shadow_count, 0);
    EXPECT_EQ(snap.point_shadow_count, 0);
    EXPECT_EQ(snap.water_count, 0);
    EXPECT_EQ(snap.decal_count, 0);
    EXPECT_FALSE(snap.post_process.valid);
    EXPECT_FALSE(snap.ddgi_config.enabled);
}

// 测试 渲染快照：聚光灯光默认值
TEST(RenderSnapshotTest, SpotLightDefaultValues) {
    RenderThinSnapshot::SpotLight sl;
    EXPECT_FLOAT_EQ(sl.outer_cone_angle, 17.5f);
    EXPECT_FLOAT_EQ(sl.radius, 20.0f);
}

// 测试 渲染快照：Water Surface默认值
TEST(RenderSnapshotTest, WaterSurfaceDefaultValues) {
    RenderThinSnapshot::WaterSurface ws;
    EXPECT_FLOAT_EQ(ws.water_level, 0.0f);
    EXPECT_FLOAT_EQ(ws.max_depth, 30.0f);
    EXPECT_FLOAT_EQ(ws.wave_amplitude, 0.15f);
    EXPECT_FLOAT_EQ(ws.specular_power, 128.0f);
}
