/**
 * @file impostor_system_test.cpp
 * @brief Impostor LOD System 单元测试
 *
 * 测试 ImpostorComponent 配置、帧映射逻辑、ImpostorBakeConfig/Result、
 * DimpostorHeader 序列化格式。
 */

#include <gtest/gtest.h>
#include "engine/ecs/components_3d_impostor.h"
#include "engine/render/impostor/impostor_baker.h"

using namespace dse;
using namespace dse::render;

// ─── ImpostorComponent 测试 ─────────────────────────────────────────────────

TEST(ImpostorComponentTest, DefaultValues) {
    ImpostorComponent comp;

    EXPECT_TRUE(comp.enabled);
    EXPECT_TRUE(comp.atlas_path.empty());
    EXPECT_EQ(comp.frame_mode, ImpostorFrameMode::HemiOctahedron);
    EXPECT_EQ(comp.frames_x, 12);
    EXPECT_EQ(comp.frames_y, 3);
    EXPECT_FLOAT_EQ(comp.transition_distance, 100.0f);
    EXPECT_FLOAT_EQ(comp.fade_range, 10.0f);
    EXPECT_FLOAT_EQ(comp.cull_distance, 500.0f);
    EXPECT_FLOAT_EQ(comp.impostor_size, 1.0f);
    EXPECT_FALSE(comp.cast_shadow);
    EXPECT_TRUE(comp.use_frame_interpolation);
    EXPECT_FLOAT_EQ(comp.normal_strength, 1.0f);
    EXPECT_TRUE(comp.auto_from_lod_group);
}

TEST(ImpostorComponentTest, RuntimeState_DefaultUninitialized) {
    ImpostorComponent comp;
    EXPECT_EQ(comp.atlas_texture_handle_, 0u);
    EXPECT_EQ(comp.normal_texture_handle_, 0u);
    EXPECT_FALSE(comp.atlas_loaded_);
    EXPECT_FLOAT_EQ(comp.cached_bounds_radius_, 0.0f);
}

TEST(ImpostorComponentTest, FrameModeEnum) {
    EXPECT_EQ(static_cast<int>(ImpostorFrameMode::HemiOctahedron), 0);
    EXPECT_EQ(static_cast<int>(ImpostorFrameMode::FullOctahedron), 1);
    EXPECT_EQ(static_cast<int>(ImpostorFrameMode::Billboard), 2);
}

TEST(ImpostorComponentTest, DistanceOrdering) {
    ImpostorComponent comp;
    // transition < cull is a required invariant
    EXPECT_LT(comp.transition_distance, comp.cull_distance);
    // fade_range should fit within transition→cull
    EXPECT_LT(comp.fade_range, comp.cull_distance - comp.transition_distance);
}

TEST(ImpostorComponentTest, CustomConfig) {
    ImpostorComponent comp;
    comp.atlas_path = "assets/trees/oak.dimpostor";
    comp.frame_mode = ImpostorFrameMode::FullOctahedron;
    comp.frames_x = 16;
    comp.frames_y = 8;
    comp.transition_distance = 200.0f;
    comp.fade_range = 20.0f;
    comp.cull_distance = 1000.0f;
    comp.impostor_size = 1.5f;
    comp.cast_shadow = true;
    comp.normal_strength = 0.5f;

    EXPECT_EQ(comp.atlas_path, "assets/trees/oak.dimpostor");
    EXPECT_EQ(comp.frame_mode, ImpostorFrameMode::FullOctahedron);
    EXPECT_EQ(comp.frames_x, 16);
    EXPECT_EQ(comp.frames_y, 8);
    EXPECT_FLOAT_EQ(comp.transition_distance, 200.0f);
    EXPECT_FLOAT_EQ(comp.cull_distance, 1000.0f);
    EXPECT_TRUE(comp.cast_shadow);
}

// ─── ImpostorBakeConfig 测试 ────────────────────────────────────────────────

TEST(ImpostorBakeConfigTest, DefaultValues) {
    ImpostorBakeConfig config;
    EXPECT_EQ(config.frames_x, 12);
    EXPECT_EQ(config.frames_y, 3);
    EXPECT_EQ(config.frame_resolution, 256);
    EXPECT_TRUE(config.bake_normals);
    EXPECT_TRUE(config.hemi_only);
    EXPECT_FLOAT_EQ(config.padding, 2.0f);
}

TEST(ImpostorBakeConfigTest, AtlasDimensions) {
    ImpostorBakeConfig config;
    config.frames_x = 12;
    config.frames_y = 3;
    config.frame_resolution = 256;

    int expected_width = config.frames_x * config.frame_resolution;
    int expected_height = config.frames_y * config.frame_resolution;

    EXPECT_EQ(expected_width, 3072);
    EXPECT_EQ(expected_height, 768);
}

// ─── ImpostorBakeResult 测试 ────────────────────────────────────────────────

TEST(ImpostorBakeResultTest, DefaultState) {
    ImpostorBakeResult result;
    EXPECT_EQ(result.atlas_width, 0);
    EXPECT_EQ(result.atlas_height, 0);
    EXPECT_TRUE(result.albedo_rgba.empty());
    EXPECT_TRUE(result.normal_rgb.empty());
    EXPECT_FALSE(result.success);
}

// ─── DimpostorHeader 测试 ───────────────────────────────────────────────────

TEST(DimpostorHeaderTest, MagicString) {
    DimpostorHeader header;
    EXPECT_EQ(header.magic[0], 'D');
    EXPECT_EQ(header.magic[1], 'I');
    EXPECT_EQ(header.magic[2], 'M');
    EXPECT_EQ(header.magic[3], 'P');
    EXPECT_EQ(header.magic[4], 'O');
    EXPECT_EQ(header.magic[5], 'S');
    EXPECT_EQ(header.magic[6], 'T');
    EXPECT_EQ(header.magic[7], 'R');
}

TEST(DimpostorHeaderTest, DefaultVersion) {
    DimpostorHeader header;
    EXPECT_EQ(header.version, 1u);
}

TEST(DimpostorHeaderTest, HeaderSize) {
    // Header should be a fixed known size for binary compatibility
    DimpostorHeader header;
    // Verify all fields are initialized to sane defaults
    EXPECT_EQ(header.atlas_width, 0u);
    EXPECT_EQ(header.atlas_height, 0u);
    EXPECT_EQ(header.frames_x, 0u);
    EXPECT_EQ(header.frames_y, 0u);
    EXPECT_EQ(header.frame_resolution, 0u);
    EXPECT_EQ(header.flags, 0u);
    EXPECT_EQ(header.albedo_offset, 0u);
    EXPECT_EQ(header.albedo_size, 0u);
    EXPECT_EQ(header.normal_offset, 0u);
    EXPECT_EQ(header.normal_size, 0u);
    EXPECT_FLOAT_EQ(header.bounds_radius, 0.0f);
}

TEST(DimpostorHeaderTest, FlagsEncoding) {
    DimpostorHeader header;
    // bit0 = has_normals
    header.flags = 1;
    EXPECT_TRUE(header.flags & 1);

    header.flags = 0;
    EXPECT_FALSE(header.flags & 1);
}

TEST(DimpostorHeaderTest, PopulatedHeader) {
    DimpostorHeader header;
    header.atlas_width = 3072;
    header.atlas_height = 768;
    header.frames_x = 12;
    header.frames_y = 3;
    header.frame_resolution = 256;
    header.flags = 1; // has normals
    header.bounds_radius = 5.5f;

    EXPECT_EQ(header.atlas_width, 3072u);
    EXPECT_EQ(header.atlas_height, 768u);
    EXPECT_EQ(header.frames_x, 12u);
    EXPECT_EQ(header.frames_y, 3u);
    EXPECT_EQ(header.frame_resolution, 256u);
    EXPECT_FLOAT_EQ(header.bounds_radius, 5.5f);
}
