/**
 * @file systems_2d_extended_test.cpp
 * @brief 2D 扩展系统单元测试 (Parallax, Light2D, SpriteSheet, Atlas, CameraController, Trail, Line, AudioSpatial)
 */

#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/light_2d.h"
#include "engine/ecs/trail_renderer_2d.h"
#include "engine/ecs/line_renderer_2d.h"
#include "engine/ecs/camera_controller_2d.h"
#include "engine/ecs/audio_spatial_2d.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/audio.h"
#include "engine/assets/sprite_sheet_asset.h"
#include "engine/assets/atlas_asset.h"

// ============================================================
// #1 Parallax Component Tests
// ============================================================

TEST(ParallaxComponent, DefaultValues) {
    ParallaxComponent pc;
    EXPECT_TRUE(pc.enabled);
    EXPECT_TRUE(pc.layers.empty());
    EXPECT_FLOAT_EQ(pc.accumulated_scroll.x, 0.0f);
    EXPECT_FLOAT_EQ(pc.accumulated_scroll.y, 0.0f);
}

TEST(ParallaxComponent, AddLayers) {
    ParallaxComponent pc;
    ParallaxLayer layer;
    layer.scroll_factor_x = 0.5f;
    layer.scroll_factor_y = 0.3f;
    layer.opacity = 0.8f;
    layer.auto_scroll_x = 1.0f;
    pc.layers.push_back(layer);

    EXPECT_EQ(pc.layers.size(), 1u);
    EXPECT_FLOAT_EQ(pc.layers[0].scroll_factor_x, 0.5f);
    EXPECT_FLOAT_EQ(pc.layers[0].scroll_factor_y, 0.3f);
    EXPECT_FLOAT_EQ(pc.layers[0].opacity, 0.8f);
    EXPECT_FLOAT_EQ(pc.layers[0].auto_scroll_x, 1.0f);
}

TEST(ParallaxComponent, LayerRepeatFlags) {
    ParallaxLayer layer;
    EXPECT_TRUE(layer.repeat_x);
    EXPECT_FALSE(layer.repeat_y);
    EXPECT_TRUE(layer.visible);
}

// ============================================================
// #2 Light2D Component Tests
// ============================================================

TEST(Light2DComponent, DefaultValues) {
    Light2DComponent lc;
    EXPECT_EQ(lc.type, Light2DType::Point);
    EXPECT_FLOAT_EQ(lc.intensity, 1.0f);
    EXPECT_FLOAT_EQ(lc.range, 5.0f);
    EXPECT_EQ(lc.shadow_mode, Shadow2DMode::None);
    EXPECT_TRUE(lc.enabled);
}

TEST(Light2DComponent, SpotLightConfig) {
    Light2DComponent lc;
    lc.type = Light2DType::Spot;
    lc.spot_angle = 30.0f;
    lc.spot_outer_angle = 45.0f;
    lc.direction_angle = 90.0f;
    lc.shadow_mode = Shadow2DMode::Soft;

    EXPECT_EQ(lc.type, Light2DType::Spot);
    EXPECT_FLOAT_EQ(lc.spot_angle, 30.0f);
    EXPECT_FLOAT_EQ(lc.spot_outer_angle, 45.0f);
    EXPECT_EQ(lc.shadow_mode, Shadow2DMode::Soft);
}

TEST(Light2DComponent, AmbientDefaults) {
    Ambient2DComponent amb;
    EXPECT_FLOAT_EQ(amb.intensity, 0.5f);
    EXPECT_FLOAT_EQ(amb.color.r, 0.2f);
}

TEST(Light2DComponent, NormalMapDefaults) {
    NormalMap2DComponent nm;
    EXPECT_FLOAT_EQ(nm.normal_strength, 1.0f);
    EXPECT_EQ(nm.normal_handle, 0u);
}

// ============================================================
// #3 SpriteSheet Asset Tests
// ============================================================

TEST(SpriteSheetAsset, GetFrameUV_ValidIndex) {
    SpriteSheetAsset sheet;
    sheet.texture_width = 256;
    sheet.texture_height = 256;

    SpriteFrame f;
    f.name = "frame_0";
    f.index = 0;
    f.uv_rect = glm::vec4(0.0f, 0.0f, 0.25f, 0.25f);
    f.pixel_rect = glm::ivec4(0, 0, 64, 64);
    sheet.frames.push_back(f);

    glm::vec4 uv = sheet.GetFrameUV(0);
    EXPECT_FLOAT_EQ(uv.x, 0.0f);
    EXPECT_FLOAT_EQ(uv.z, 0.25f);
}

TEST(SpriteSheetAsset, GetFrameUV_InvalidIndex) {
    SpriteSheetAsset sheet;
    glm::vec4 uv = sheet.GetFrameUV(999);
    EXPECT_FLOAT_EQ(uv.x, 0.0f);
    EXPECT_FLOAT_EQ(uv.z, 1.0f);
}

TEST(SpriteSheetAsset, FindFrame) {
    SpriteSheetAsset sheet;
    SpriteFrame f;
    f.name = "walk_01";
    f.index = 0;
    f.uv_rect = glm::vec4(0.0f, 0.0f, 0.5f, 0.5f);
    sheet.frames.push_back(f);

    auto* found = sheet.FindFrame("walk_01");
    ASSERT_NE(found, nullptr);
    EXPECT_FLOAT_EQ(found->uv_rect.z, 0.5f);

    EXPECT_EQ(sheet.FindFrame("nonexistent"), nullptr);
}

// ============================================================
// #4 Atlas Asset Tests
// ============================================================

TEST(AtlasAsset, GetEntryUV_ValidName) {
    AtlasAsset atlas;
    atlas.atlas_width = 512;
    atlas.atlas_height = 512;

    AtlasEntry entry;
    entry.name = "player_idle";
    entry.uv_rect = glm::vec4(0.0f, 0.0f, 0.125f, 0.25f);
    atlas.entries.push_back(entry);
    atlas.RebuildIndex();

    glm::vec4 uv = atlas.GetEntryUV("player_idle");
    EXPECT_FLOAT_EQ(uv.x, 0.0f);
    EXPECT_FLOAT_EQ(uv.z, 0.125f);
}

TEST(AtlasAsset, GetEntryUV_NotFound) {
    AtlasAsset atlas;
    glm::vec4 uv = atlas.GetEntryUV("missing");
    EXPECT_FLOAT_EQ(uv.z, 1.0f);
}

TEST(AtlasAsset, FindEntry) {
    AtlasAsset atlas;
    AtlasEntry entry;
    entry.name = "coin";
    entry.rotated = true;
    atlas.entries.push_back(entry);
    atlas.RebuildIndex();

    auto* found = atlas.FindEntry("coin");
    ASSERT_NE(found, nullptr);
    EXPECT_TRUE(found->rotated);
}

TEST(AtlasAsset, RebuildIndex) {
    AtlasAsset atlas;
    for (int i = 0; i < 10; ++i) {
        AtlasEntry e;
        e.name = "sprite_" + std::to_string(i);
        atlas.entries.push_back(e);
    }
    atlas.RebuildIndex();
    EXPECT_EQ(atlas.name_index.size(), 10u);
    EXPECT_NE(atlas.FindEntry("sprite_5"), nullptr);
}

// ============================================================
// #5 Camera Controller Tests
// ============================================================

TEST(CameraController2D, DefaultValues) {
    CameraController2DComponent ctrl;
    EXPECT_FLOAT_EQ(ctrl.target_zoom, 1.0f);
    EXPECT_FLOAT_EQ(ctrl.min_zoom, 0.2f);
    EXPECT_FLOAT_EQ(ctrl.max_zoom, 5.0f);
    EXPECT_TRUE(ctrl.enabled);
    EXPECT_FALSE(ctrl.bounds.enabled);
}

TEST(CameraController2D, ShakeAccumulation) {
    CameraController2DComponent ctrl;
    ctrl.shake.trauma = 0.5f;
    ctrl.shake.trauma = std::min(ctrl.shake.trauma + 0.3f, 1.0f);
    EXPECT_FLOAT_EQ(ctrl.shake.trauma, 0.8f);
    ctrl.shake.trauma = std::min(ctrl.shake.trauma + 0.5f, 1.0f);
    EXPECT_FLOAT_EQ(ctrl.shake.trauma, 1.0f);
}

TEST(CameraController2D, BoundsConfig) {
    CameraController2DComponent ctrl;
    ctrl.bounds.enabled = true;
    ctrl.bounds.min_x = -50.0f;
    ctrl.bounds.max_x = 50.0f;
    ctrl.bounds.min_y = -25.0f;
    ctrl.bounds.max_y = 25.0f;
    EXPECT_TRUE(ctrl.bounds.enabled);
    EXPECT_FLOAT_EQ(ctrl.bounds.min_x, -50.0f);
}

// ============================================================
// #6 Trail Renderer Tests
// ============================================================

TEST(TrailRenderer2D, DefaultValues) {
    TrailRenderer2DComponent trail;
    EXPECT_FLOAT_EQ(trail.lifetime, 0.5f);
    EXPECT_FLOAT_EQ(trail.start_width, 0.5f);
    EXPECT_FLOAT_EQ(trail.end_width, 0.0f);
    EXPECT_EQ(trail.max_points, 128);
    EXPECT_TRUE(trail.emitting);
    EXPECT_TRUE(trail.world_space);
}

TEST(TrailRenderer2D, PointManagement) {
    TrailRenderer2DComponent trail;
    TrailPoint p;
    p.position = glm::vec2(1.0f, 2.0f);
    p.width = 0.3f;
    p.life_remaining = 0.5f;
    p.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    trail.points.push_back(p);

    EXPECT_EQ(trail.points.size(), 1u);
    EXPECT_FLOAT_EQ(trail.points[0].position.x, 1.0f);
}

TEST(TrailRenderer2D, PointAging) {
    TrailRenderer2DComponent trail;
    TrailPoint p;
    p.life_remaining = 1.0f;
    trail.points.push_back(p);

    trail.points[0].life_remaining -= 0.5f;
    EXPECT_FLOAT_EQ(trail.points[0].life_remaining, 0.5f);

    trail.points[0].life_remaining -= 0.6f;
    EXPECT_LT(trail.points[0].life_remaining, 0.0f);
}

// ============================================================
// #7 Line Renderer Tests
// ============================================================

TEST(LineRenderer2D, DefaultValues) {
    LineRenderer2DComponent line;
    EXPECT_FLOAT_EQ(line.width, 0.1f);
    EXPECT_FALSE(line.closed);
    EXPECT_FALSE(line.use_world_space);
    EXPECT_TRUE(line.visible);
    EXPECT_EQ(line.cap, LineCapMode::None);
    EXPECT_EQ(line.join, LineJoinMode::Miter);
}

TEST(LineRenderer2D, PointManagement) {
    LineRenderer2DComponent line;
    line.points.push_back(glm::vec2(0, 0));
    line.points.push_back(glm::vec2(1, 0));
    line.points.push_back(glm::vec2(1, 1));
    EXPECT_EQ(line.points.size(), 3u);
}

TEST(LineRenderer2D, ClosedLoop) {
    LineRenderer2DComponent line;
    line.closed = true;
    line.points = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    EXPECT_TRUE(line.closed);
    EXPECT_EQ(line.points.size(), 4u);
}

// ============================================================
// #8 Audio Spatial 2D Tests
// ============================================================

TEST(AudioSpatial2D, DefaultValues) {
    AudioSpatial2DComponent spatial;
    EXPECT_FLOAT_EQ(spatial.min_distance, 1.0f);
    EXPECT_FLOAT_EQ(spatial.max_distance, 20.0f);
    EXPECT_TRUE(spatial.enable_pan);
    EXPECT_FALSE(spatial.enable_doppler);
    EXPECT_EQ(spatial.attenuation, AudioAttenuation2DModel::InverseDistance);
}

TEST(AudioSpatial2D, AttenuationCalculation_Linear) {
    AudioSpatial2DComponent spatial;
    spatial.attenuation = AudioAttenuation2DModel::Linear;
    spatial.min_distance = 1.0f;
    spatial.max_distance = 10.0f;

    // Simulate linear attenuation
    float distance = 5.5f;
    float norm_dist = (distance - spatial.min_distance) /
        (spatial.max_distance - spatial.min_distance);
    float attenuation = 1.0f - norm_dist;
    EXPECT_NEAR(attenuation, 0.5f, 0.01f);
}

TEST(AudioSpatial2D, ListenerDefaults) {
    AudioListener2DComponent listener;
    EXPECT_TRUE(listener.enabled);
    EXPECT_FLOAT_EQ(listener.global_volume, 1.0f);
}

TEST(AudioSpatial2D, PanCalculation) {
    AudioSpatial2DComponent spatial;
    spatial.enable_pan = true;
    spatial.pan_strength = 1.0f;
    spatial.max_distance = 20.0f;

    // Source to the right
    glm::vec2 listener_pos = {0, 0};
    glm::vec2 source_pos = {10, 0};
    glm::vec2 dir = source_pos - listener_pos;
    float pan = std::clamp(dir.x / spatial.max_distance, -1.0f, 1.0f);
    EXPECT_FLOAT_EQ(pan, 0.5f);
}

// ============================================================
// Integration: ECS Registry Tests
// ============================================================

TEST(ECSIntegration, CreateEntityWithParallax) {
    entt::registry reg;
    auto entity = reg.create();
    auto& pc = reg.emplace<ParallaxComponent>(entity);
    pc.layers.resize(3);
    EXPECT_EQ(reg.view<ParallaxComponent>().size(), 1u);
}

TEST(ECSIntegration, CreateEntityWithLight2D) {
    entt::registry reg;
    auto entity = reg.create();
    auto& lc = reg.emplace<Light2DComponent>(entity);
    lc.type = Light2DType::Spot;
    lc.intensity = 2.0f;
    EXPECT_EQ(reg.view<Light2DComponent>().size(), 1u);
}

TEST(ECSIntegration, CreateEntityWithTrail) {
    entt::registry reg;
    auto entity = reg.create();
    reg.emplace<TransformComponent>(entity);
    auto& trail = reg.emplace<TrailRenderer2DComponent>(entity);
    trail.lifetime = 1.0f;
    EXPECT_EQ(reg.view<TrailRenderer2DComponent>().size(), 1u);
}

TEST(ECSIntegration, CreateEntityWithLineRenderer) {
    entt::registry reg;
    auto entity = reg.create();
    reg.emplace<TransformComponent>(entity);
    auto& line = reg.emplace<LineRenderer2DComponent>(entity);
    line.points = {{0, 0}, {5, 5}};
    EXPECT_EQ(reg.view<LineRenderer2DComponent>().size(), 1u);
}

TEST(ECSIntegration, CreateEntityWithCameraController) {
    entt::registry reg;
    auto entity = reg.create();
    reg.emplace<TransformComponent>(entity);
    reg.emplace<CameraComponent>(entity);
    auto& ctrl = reg.emplace<CameraController2DComponent>(entity);
    ctrl.target_zoom = 2.0f;
    EXPECT_EQ(reg.view<CameraController2DComponent>().size(), 1u);
}

TEST(ECSIntegration, CreateEntityWithAudioSpatial) {
    entt::registry reg;
    auto entity = reg.create();
    reg.emplace<TransformComponent>(entity);
    reg.emplace<AudioSourceComponent>(entity);
    auto& spatial = reg.emplace<AudioSpatial2DComponent>(entity);
    spatial.max_distance = 50.0f;
    EXPECT_EQ(reg.view<AudioSpatial2DComponent>().size(), 1u);
}
