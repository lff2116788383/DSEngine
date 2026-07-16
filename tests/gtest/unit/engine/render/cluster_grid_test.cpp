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
#include <glm/gtc/matrix_transform.hpp>

using namespace dse::render;

// ============================================================
// 常量验证
// ============================================================

// 测试 簇网格常量：瓦片尺寸
TEST(ClusterGridConstantsTest, TileSize) {
    EXPECT_EQ(kClusterTileSize, 16);
}

// 测试 簇网格常量：Z Slices
TEST(ClusterGridConstantsTest, ZSlices) {
    EXPECT_EQ(kClusterZSlices, 24);
}

// 测试 簇网格常量：SSBO绑定点执行不重叠
TEST(ClusterGridConstantsTest, SSBOBindingPointsDoNotOverlap) {
    EXPECT_NE(kSSBOBindingClusterInfo, kSSBOBindingLightIndices);
    EXPECT_NE(kSSBOBindingClusterInfo, kSSBOBindingPointLights);
    EXPECT_NE(kSSBOBindingClusterInfo, kSSBOBindingSpotLights);
    EXPECT_NE(kSSBOBindingLightIndices, kSSBOBindingPointLights);
    EXPECT_NE(kSSBOBindingLightIndices, kSSBOBindingSpotLights);
}

// ============================================================
// ClusterGridHeader 结构体
// ============================================================

// 测试 簇网格头：尺寸32
TEST(ClusterGridHeaderTest, Size32) {
    EXPECT_EQ(sizeof(ClusterGridHeader), 32u);
}

// 测试 簇网格头：默认值全部零
TEST(ClusterGridHeaderTest, DefaultValuesAllZero) {
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

// 测试 簇信息：尺寸16
TEST(ClusterInfoTest, Size16) {
    EXPECT_EQ(sizeof(ClusterInfo), 16u);
}

// 测试 簇信息：默认值全部零
TEST(ClusterInfoTest, DefaultValuesAllZero) {
    ClusterInfo ci{};
    EXPECT_EQ(ci.offset, 0u);
    EXPECT_EQ(ci.point_count, 0u);
    EXPECT_EQ(ci.spot_count, 0u);
}

// ============================================================
// GPUPointLight 结构体
// ============================================================

// 测试 GPU点灯光：尺寸48
TEST(GPUPointLightTest, Size48) {
    EXPECT_EQ(sizeof(GPUPointLight), 48u);
}

// 测试 GPU点灯光：默认值
TEST(GPUPointLightTest, DefaultValues) {
    GPUPointLight pl{};
    EXPECT_FLOAT_EQ(pl.color.x, 0.0f);
    EXPECT_FLOAT_EQ(pl.intensity, 0.0f);
    EXPECT_FLOAT_EQ(pl.radius, 0.0f);
    EXPECT_EQ(pl.cast_shadow, 0);
}

// ============================================================
// GPUSpotLight 结构体
// ============================================================

// 测试 GPU聚光灯光：尺寸64
TEST(GPUSpotLightTest, Size64) {
    EXPECT_EQ(sizeof(GPUSpotLight), 64u);
}

// 测试 GPU聚光灯光：默认值
TEST(GPUSpotLightTest, DefaultValues) {
    GPUSpotLight sl{};
    EXPECT_FLOAT_EQ(sl.color.x, 0.0f);
    EXPECT_FLOAT_EQ(sl.inner_cone, 0.0f);
    EXPECT_FLOAT_EQ(sl.outer_cone, 0.0f);
}

// ============================================================
// LightBufferHeader 结构体
// ============================================================

// 测试 灯光缓冲区头：尺寸16
TEST(LightBufferHeaderTest, Size16) {
    EXPECT_EQ(sizeof(LightBufferHeader), 16u);
}

// ============================================================
// 最大光源常量
// ============================================================

// 测试 灯光缓冲区常量：最大
TEST(LightBufferConstantsTest, Max) {
    EXPECT_EQ(kMaxClusteredPointLights, 256);
    EXPECT_EQ(kMaxClusteredSpotLights, 256);
}

// ============================================================
// ClusterGrid 默认状态
// ============================================================

// 测试 簇网格：默认状态
TEST(ClusterGridTest, DefaultState) {
    ClusterGrid grid;
    EXPECT_EQ(grid.tiles_x(), 0);
    EXPECT_EQ(grid.tiles_y(), 0);
    EXPECT_EQ(grid.total_clusters(), 0);
}

// ============================================================
// LightBuffer 默认状态
// ============================================================

// 测试 灯光缓冲区：默认状态
TEST(LightBufferTest, DefaultState) {
    LightBuffer buf;
    EXPECT_EQ(buf.point_light_count(), 0);
    EXPECT_EQ(buf.spot_light_count(), 0);
    EXPECT_TRUE(buf.point_lights().empty());
    EXPECT_TRUE(buf.spot_lights().empty());
}

// ============================================================
// ClusterGrid::Build —— AABB 缓存路径（CPU-only，无需 RHI 设备）
// ============================================================

// 测试 簇网格：Build 计算瓦片维度
TEST(ClusterGridBuildTest, TileDimensions) {
    ClusterGrid grid;
    const glm::mat4 view(1.0f);
    const glm::mat4 proj = glm::perspective(glm::radians(60.0f), 640.0f / 480.0f, 0.1f, 100.0f);
    std::vector<GPUPointLight> points;
    std::vector<GPUSpotLight> spots;

    grid.Build(view, proj, 0.1f, 100.0f, 640, 480, points, spots);
    // 640/16=40, 480/16=30
    EXPECT_EQ(grid.tiles_x(), 40);
    EXPECT_EQ(grid.tiles_y(), 30);
    EXPECT_EQ(grid.total_clusters(), 40 * 30 * kClusterZSlices);
}

// 测试 簇网格：相同投影重复 Build（命中 AABB 缓存）结果稳定
TEST(ClusterGridBuildTest, RepeatedBuildSameProjectionStable) {
    ClusterGrid grid;
    const glm::mat4 proj = glm::perspective(glm::radians(60.0f), 4.0f / 3.0f, 0.1f, 100.0f);
    std::vector<GPUPointLight> points(1);
    points[0].position = glm::vec3(0.0f, 0.0f, -5.0f);
    points[0].radius = 10.0f;
    std::vector<GPUSpotLight> spots;

    // 相机移动（view 变化）但投影/屏幕不变 —— AABB 缓存应命中且维度稳定
    grid.Build(glm::mat4(1.0f), proj, 0.1f, 100.0f, 800, 600, points, spots);
    const int total_a = grid.total_clusters();
    grid.Build(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f)),
               proj, 0.1f, 100.0f, 800, 600, points, spots);
    EXPECT_EQ(grid.total_clusters(), total_a);
    EXPECT_EQ(grid.tiles_x(), 50);
    EXPECT_EQ(grid.tiles_y(), 38);  // ceil(600/16)=38
}

// 测试 簇网格：屏幕尺寸变化触发 AABB 缓存失效并重算维度
TEST(ClusterGridBuildTest, ScreenResizeInvalidatesCache) {
    ClusterGrid grid;
    const glm::mat4 view(1.0f);
    const glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    std::vector<GPUPointLight> points;
    std::vector<GPUSpotLight> spots;

    grid.Build(view, proj, 0.1f, 100.0f, 1280, 720, points, spots);
    EXPECT_EQ(grid.tiles_x(), 80);
    EXPECT_EQ(grid.tiles_y(), 45);

    grid.Build(view, proj, 0.1f, 100.0f, 1920, 1080, points, spots);
    EXPECT_EQ(grid.tiles_x(), 120);
    EXPECT_EQ(grid.tiles_y(), 68);  // ceil(1080/16)=68
}
