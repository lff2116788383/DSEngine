/**
 * @file grass_system_test.cpp
 * @brief GrassSystem 单元测试（无 GPU）
 *
 * 测试策略：
 * - GrassComponent ECS 默认值
 * - GrassInstanceLayout / GrassGPUInstance / GrassChunkData 默认值
 * - GrassSystem 默认状态
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/rendering/grass_system.h"
#include "engine/ecs/components_3d.h"
#include <glm/glm.hpp>

using namespace dse;
using namespace dse::gameplay3d;

// ============================================================
// GrassComponent 默认值
// ============================================================

TEST(GrassComponentTest, DefaultValues_Parameters) {
    GrassComponent gc;
    EXPECT_TRUE(gc.enabled);
    EXPECT_FLOAT_EQ(gc.density, 1.0f);
    EXPECT_FLOAT_EQ(gc.spawn_radius, 50.0f);
    EXPECT_EQ(gc.seed, 42u);
    EXPECT_FLOAT_EQ(gc.chunk_size, 8.0f);
}

TEST(GrassComponentTest, DefaultValues_Outside) {
    GrassComponent gc;
    EXPECT_FLOAT_EQ(gc.blade_width, 0.1f);
    EXPECT_FLOAT_EQ(gc.blade_height, 1.0f);
    EXPECT_FLOAT_EQ(gc.blade_height_variation, 0.3f);
    EXPECT_FLOAT_EQ(gc.base_color.g, 0.45f);
    EXPECT_EQ(gc.albedo_texture, 0u);
}

TEST(GrassComponentTest, DefaultValues_Case) {
    GrassComponent gc;
    EXPECT_FLOAT_EQ(gc.wind_direction.x, 1.0f);
    EXPECT_FLOAT_EQ(gc.wind_direction.y, 0.0f);
    EXPECT_FLOAT_EQ(gc.wind_speed, 1.0f);
    EXPECT_FLOAT_EQ(gc.wind_strength, 0.3f);
    EXPECT_FLOAT_EQ(gc.wind_turbulence, 0.2f);
}

TEST(GrassComponentTest, DefaultValues_LOD) {
    GrassComponent gc;
    EXPECT_FLOAT_EQ(gc.lod_near, 30.0f);
    EXPECT_FLOAT_EQ(gc.lod_far, 80.0f);
    EXPECT_FLOAT_EQ(gc.fade_range, 5.0f);
}

TEST(GrassComponentTest, DefaultValues_Case_2) {
    GrassComponent gc;
    EXPECT_FALSE(gc.cast_shadow);
    EXPECT_FLOAT_EQ(gc.shadow_distance, 20.0f);
}

TEST(GrassComponentTest, WhenDefaultValues) {
    GrassComponent gc;
    EXPECT_EQ(gc.cached_instance_count_, 0);
}

// ============================================================
// GrassInstanceLayout 默认值
// ============================================================

TEST(GrassInstanceLayoutTest, DefaultValuesAllZero) {
    GrassInstanceLayout layout{};
    EXPECT_FLOAT_EQ(layout.position.x, 0.0f);
    EXPECT_FLOAT_EQ(layout.yaw, 0.0f);
    EXPECT_FLOAT_EQ(layout.width, 0.0f);
    EXPECT_FLOAT_EQ(layout.height, 0.0f);
    EXPECT_FLOAT_EQ(layout.wind_phase, 0.0f);
}

// ============================================================
// GrassGPUInstance 布局
// ============================================================

TEST(GrassGPUInstanceTest, Size32) {
    EXPECT_EQ(sizeof(GrassGPUInstance), 32u);
}

TEST(GrassGPUInstanceTest, DefaultValues) {
    GrassGPUInstance inst{};
    EXPECT_FLOAT_EQ(inst.pos_yaw.x, 0.0f);
    EXPECT_FLOAT_EQ(inst.pos_yaw.w, 0.0f);
    EXPECT_FLOAT_EQ(inst.wh_phase_fade.x, 0.0f);
}

// ============================================================
// GrassChunkData 默认值
// ============================================================

TEST(GrassChunkDataTest, DefaultValues) {
    GrassChunkData chunk;
    EXPECT_TRUE(chunk.layouts.empty());
    EXPECT_FALSE(chunk.valid);
    EXPECT_FLOAT_EQ(chunk.aabb_min.x, 0.0f);
    EXPECT_FLOAT_EQ(chunk.aabb_max.x, 0.0f);
}

// ============================================================
// GrassSystem 默认状态
// ============================================================

TEST(GrassSystemTest, DefaultDoesNotCrash) {
    GrassSystem sys;
    // 默认构造应无异常
    (void)sys;
    SUCCEED();
}
