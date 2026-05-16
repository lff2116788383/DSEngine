/**
 * @file cluster_grid_test.cpp
 * @brief ClusterGrid / LightBuffer 单元测试（纯 CPU 端数据结构）
 *
 * 测试策略：
 * - GPU 结构体 static_assert 对齐验证
 * - 常量正确性
 * - ClusterGrid 默认状态
 * - LightBuffer 默认状态
 */

#include <gtest/gtest.h>
#include "engine/render/cluster_grid.h"
#include "engine/render/light_buffer.h"
#include <glm/glm.hpp>

using namespace dse::render;

// ============================================================
// 常量验证
// ============================================================

TEST(ClusterGridConstantsTest, TileSize) {
    EXPECT_EQ(kClusterTileSize, 16);
}

TEST(ClusterGridConstantsTest, ZSlices) {
    EXPECT_EQ(kClusterZSlices, 24);
}

TEST(ClusterGridConstantsTest, SSBO绑定点不重叠) {
    EXPECT_NE(kSSBOBindingClusterInfo, kSSBOBindingLightIndices);
    EXPECT_NE(kSSBOBindingClusterInfo, kSSBOBindingPointLights);
    EXPECT_NE(kSSBOBindingClusterInfo, kSSBOBindingSpotLights);
    EXPECT_NE(kSSBOBindingLightIndices, kSSBOBindingPointLights);
    EXPECT_NE(kSSBOBindingLightIndices, kSSBOBindingSpotLights);
}

// ============================================================
// ClusterGridHeader 结构体
// ============================================================

TEST(ClusterGridHeaderTest, 大小32字节) {
    EXPECT_EQ(sizeof(ClusterGridHeader), 32u);
}

TEST(ClusterGridHeaderTest, 默认值全零) {
    ClusterGridHeader h{};
    EXPECT_EQ(h.tiles_x, 0u);
    EXPECT_EQ(h.tiles_y, 0u);
    EXPECT_EQ(h.z_slices, 0u);
    EXPECT_FLOAT_EQ(h.near_plane, 0.0f);
    EXPECT_FLOAT_EQ(h.far_plane, 0.0f);
}

// ============================================================
// ClusterInfo 结构体
// ============================================================

TEST(ClusterInfoTest, 大小16字节) {
    EXPECT_EQ(sizeof(ClusterInfo), 16u);
}

TEST(ClusterInfoTest, 默认值全零) {
    ClusterInfo ci{};
    EXPECT_EQ(ci.offset, 0u);
    EXPECT_EQ(ci.point_count, 0u);
    EXPECT_EQ(ci.spot_count, 0u);
}

// ============================================================
// GPUPointLight 结构体
// ============================================================

TEST(GPUPointLightTest, 大小48字节) {
    EXPECT_EQ(sizeof(GPUPointLight), 48u);
}

TEST(GPUPointLightTest, 默认值) {
    GPUPointLight pl{};
    EXPECT_FLOAT_EQ(pl.color.x, 0.0f);
    EXPECT_FLOAT_EQ(pl.intensity, 0.0f);
    EXPECT_FLOAT_EQ(pl.radius, 0.0f);
    EXPECT_EQ(pl.cast_shadow, 0);
}

// ============================================================
// GPUSpotLight 结构体
// ============================================================

TEST(GPUSpotLightTest, 大小64字节) {
    EXPECT_EQ(sizeof(GPUSpotLight), 64u);
}

TEST(GPUSpotLightTest, 默认值) {
    GPUSpotLight sl{};
    EXPECT_FLOAT_EQ(sl.color.x, 0.0f);
    EXPECT_FLOAT_EQ(sl.inner_cone, 0.0f);
    EXPECT_FLOAT_EQ(sl.outer_cone, 0.0f);
}

// ============================================================
// LightBufferHeader 结构体
// ============================================================

TEST(LightBufferHeaderTest, 大小16字节) {
    EXPECT_EQ(sizeof(LightBufferHeader), 16u);
}

// ============================================================
// 最大光源常量
// ============================================================

TEST(LightBufferConstantsTest, 最大光源数) {
    EXPECT_EQ(kMaxClusteredPointLights, 256);
    EXPECT_EQ(kMaxClusteredSpotLights, 256);
}

// ============================================================
// ClusterGrid 默认状态
// ============================================================

TEST(ClusterGridTest, 默认状态) {
    ClusterGrid grid;
    EXPECT_EQ(grid.tiles_x(), 0);
    EXPECT_EQ(grid.tiles_y(), 0);
    EXPECT_EQ(grid.total_clusters(), 0);
}

// ============================================================
// LightBuffer 默认状态
// ============================================================

TEST(LightBufferTest, 默认状态) {
    LightBuffer buf;
    EXPECT_EQ(buf.point_light_count(), 0);
    EXPECT_EQ(buf.spot_light_count(), 0);
    EXPECT_TRUE(buf.point_lights().empty());
    EXPECT_TRUE(buf.spot_lights().empty());
}
