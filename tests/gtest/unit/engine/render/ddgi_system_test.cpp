/**
 * @file ddgi_system_test.cpp
 * @brief DDGI 系统单元测试（纯 CPU 端数据结构 + 八面体编码）
 *
 * 测试策略：
 * - DDGIVolumeConfig 默认值和计算方法
 * - ProbeIndex / ProbeCoord 往返一致性
 * - ProbePosition 正确性
 * - Atlas 尺寸计算
 * - OctEncode / OctDecode 往返精度
 * - DDGIResources / ProbeState / RSMSample 默认值
 * - DDGISystem 默认状态
 */

#include <gtest/gtest.h>
#include "engine/render/gi/ddgi_types.h"
#include "engine/render/gi/ddgi_system.h"
#include <glm/glm.hpp>
#include <cmath>

using namespace dse::render::gi;

// ============================================================
// DDGIVolumeConfig 默认值
// ============================================================

// 测试 DDGI体积配置：默认值
TEST(DDGIVolumeConfigTest, DefaultValues) {
    DDGIVolumeConfig cfg;
    EXPECT_FLOAT_EQ(cfg.origin.x, 0.0f);
    EXPECT_FLOAT_EQ(cfg.extent.x, 100.0f);
    EXPECT_EQ(cfg.resolution.x, 8);
    EXPECT_EQ(cfg.resolution.y, 8);
    EXPECT_EQ(cfg.resolution.z, 8);
    EXPECT_EQ(cfg.irradiance_texels, 8);
    EXPECT_EQ(cfg.visibility_texels, 8);
    EXPECT_EQ(cfg.rays_per_probe, 256);
    EXPECT_FLOAT_EQ(cfg.hysteresis, 0.97f);
}

// ============================================================
// TotalProbeCount
// ============================================================

// 测试 DDGI体积配置：总计探针计数
TEST(DDGIVolumeConfigTest, TotalProbeCount) {
    DDGIVolumeConfig cfg;
    EXPECT_EQ(cfg.TotalProbeCount(), 512);  // 8*8*8

    cfg.resolution = glm::ivec3(4, 3, 2);
    EXPECT_EQ(cfg.TotalProbeCount(), 24);
}

// ============================================================
// ProbeSpacing
// ============================================================

// 测试 DDGI体积配置：探针Spacing
TEST(DDGIVolumeConfigTest, ProbeSpacing) {
    DDGIVolumeConfig cfg;
    cfg.extent = glm::vec3(70.0f, 35.0f, 14.0f);
    cfg.resolution = glm::ivec3(8, 8, 8);

    glm::vec3 spacing = cfg.ProbeSpacing();
    EXPECT_FLOAT_EQ(spacing.x, 10.0f);
    EXPECT_FLOAT_EQ(spacing.y, 5.0f);
    EXPECT_FLOAT_EQ(spacing.z, 2.0f);
}

// 测试 DDGI体积配置：探针Spacing单一Axis分辨率1
TEST(DDGIVolumeConfigTest, ProbeSpacing_SingleAxisResolution1) {
    DDGIVolumeConfig cfg;
    cfg.extent = glm::vec3(10.0f);
    cfg.resolution = glm::ivec3(1, 4, 1);

    glm::vec3 spacing = cfg.ProbeSpacing();
    // resolution=1 → spacing = extent / max(1, 0) = extent/1
    EXPECT_FLOAT_EQ(spacing.x, 10.0f);
    EXPECT_NEAR(spacing.y, 3.333f, 0.01f);
    EXPECT_FLOAT_EQ(spacing.z, 10.0f);
}

// ============================================================
// ProbeIndex / ProbeCoord 往返
// ============================================================

// 测试 DDGI体积配置：探针索引探针Coordround往返
TEST(DDGIVolumeConfigTest, ProbeIndex_ProbeCoordroundTrip) {
    DDGIVolumeConfig cfg;
    cfg.resolution = glm::ivec3(4, 3, 2);

    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 4; ++x) {
                int idx = cfg.ProbeIndex(x, y, z);
                glm::ivec3 coord = cfg.ProbeCoord(idx);
                EXPECT_EQ(coord.x, x);
                EXPECT_EQ(coord.y, y);
                EXPECT_EQ(coord.z, z);
            }
        }
    }
}

// ============================================================
// ProbePosition
// ============================================================

// 测试 DDGI体积配置：探针位置原点探针
TEST(DDGIVolumeConfigTest, ProbePosition_OriginProbe) {
    DDGIVolumeConfig cfg;
    cfg.origin = glm::vec3(10.0f, 20.0f, 30.0f);
    cfg.extent = glm::vec3(70.0f);
    cfg.resolution = glm::ivec3(8);

    glm::vec3 pos = cfg.ProbePosition(0);
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos.y, 20.0f);
    EXPECT_FLOAT_EQ(pos.z, 30.0f);
}

// 测试 DDGI体积配置：探针位置结束探针
TEST(DDGIVolumeConfigTest, ProbePosition_EndProbe) {
    DDGIVolumeConfig cfg;
    cfg.origin = glm::vec3(0.0f);
    cfg.extent = glm::vec3(70.0f);
    cfg.resolution = glm::ivec3(8);

    int last_idx = cfg.ProbeIndex(7, 7, 7);
    glm::vec3 pos = cfg.ProbePosition(last_idx);
    EXPECT_FLOAT_EQ(pos.x, 70.0f);
    EXPECT_FLOAT_EQ(pos.y, 70.0f);
    EXPECT_FLOAT_EQ(pos.z, 70.0f);
}

// ============================================================
// Atlas 尺寸
// ============================================================

// 测试 DDGI体积配置：Irradiance图集尺寸
TEST(DDGIVolumeConfigTest, IrradianceAtlasSize) {
    DDGIVolumeConfig cfg;
    cfg.resolution = glm::ivec3(4, 3, 2);
    cfg.irradiance_texels = 8;

    glm::ivec2 size = cfg.IrradianceAtlasSize();
    // probes_per_row = 4*2 = 8, probes_per_col = 3
    EXPECT_EQ(size.x, 64);   // 8 * 8
    EXPECT_EQ(size.y, 24);   // 3 * 8
}

// 测试 DDGI体积配置：可见性图集尺寸
TEST(DDGIVolumeConfigTest, VisibilityAtlasSize) {
    DDGIVolumeConfig cfg;
    cfg.resolution = glm::ivec3(4, 3, 2);
    cfg.visibility_texels = 16;

    glm::ivec2 size = cfg.VisibilityAtlasSize();
    EXPECT_EQ(size.x, 128);  // 8 * 16
    EXPECT_EQ(size.y, 48);   // 3 * 16
}

// ============================================================
// ProbeIrradianceOffset / ProbeVisibilityOffset
// ============================================================

// 测试 DDGI体积配置：探针Irradiance偏移首个探针
TEST(DDGIVolumeConfigTest, ProbeIrradianceOffset_FirstProbe) {
    DDGIVolumeConfig cfg;
    cfg.resolution = glm::ivec3(4, 3, 2);
    cfg.irradiance_texels = 8;

    glm::ivec2 offset = cfg.ProbeIrradianceOffset(0);
    EXPECT_EQ(offset.x, 0);
    EXPECT_EQ(offset.y, 0);
}

// ============================================================
// OctEncode / OctDecode 往返
// ============================================================

// 测试 八面体映射：往返朝向
TEST(OctahedralMapTest, RoundTrip_Toward) {
    glm::vec3 dirs[] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    for (const auto& d : dirs) {
        glm::vec2 uv = OctEncode(d);
        glm::vec3 decoded = OctDecode(uv);
        EXPECT_NEAR(decoded.x, d.x, 0.01f);
        EXPECT_NEAR(decoded.y, d.y, 0.01f);
        EXPECT_NEAR(decoded.z, d.z, 0.01f);
    }
}

// 测试 八面体映射：往返朝向2
TEST(OctahedralMapTest, RoundTrip_Toward_2) {
    glm::vec3 d = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    glm::vec2 uv = OctEncode(d);
    glm::vec3 decoded = OctDecode(uv);
    EXPECT_NEAR(decoded.x, d.x, 0.01f);
    EXPECT_NEAR(decoded.y, d.y, 0.01f);
    EXPECT_NEAR(decoded.z, d.z, 0.01f);
}

// 测试 八面体映射：U Vin范围零到单个
TEST(OctahedralMapTest, UVinTheRangeZeroToOne) {
    glm::vec3 d = glm::normalize(glm::vec3(-0.3f, 0.7f, -0.5f));
    glm::vec2 uv = OctEncode(d);
    EXPECT_GE(uv.x, 0.0f);
    EXPECT_LE(uv.x, 1.0f);
    EXPECT_GE(uv.y, 0.0f);
    EXPECT_LE(uv.y, 1.0f);
}

// ============================================================
// DDGIResources 默认值
// ============================================================

// 测试 DDGI资源：默认值
TEST(DDGIResourcesTest, DefaultValues) {
    DDGIResources res;
    EXPECT_EQ(res.irradiance_atlas, 0u);
    EXPECT_EQ(res.visibility_atlas, 0u);
    EXPECT_FALSE(res.probe_state_ssbo);
    EXPECT_EQ(res.update_compute_shader, 0u);
    EXPECT_FALSE(res.initialized);
}

// ============================================================
// ProbeState / RSMSample 默认值
// ============================================================

// 测试 探针状态：对齐16
TEST(ProbeStateTest, Alignment16) {
    EXPECT_EQ(sizeof(ProbeState) % 16, 0u);
}

// 测试 RSM采样：对齐16
TEST(RSMSampleTest, Alignment16) {
    EXPECT_EQ(sizeof(RSMSample) % 16, 0u);
}

// ============================================================
// DDGISystem 默认状态
// ============================================================

// 测试 DDGI系统：默认未初始化
TEST(DDGISystemTest, DefaultUninitialized) {
    DDGISystem sys;
    EXPECT_FALSE(sys.IsInitialized());
    EXPECT_EQ(sys.GetCurrentUpdateOffset(), 0);
    EXPECT_EQ(sys.GetProbesPerFrame(), 32);
}
