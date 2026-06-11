/**
 * @file projection_correction_test.cpp
 * @brief 三端坐标系统一验证 — GetProjectionCorrection / GetShadowSampleCorrection
 *
 * GL 基础 identity 测试已在 rhi_types_test.cpp 中覆盖。
 * 本文件补充：VK/DX11 投影修正矩阵元素验证、NDC 变换数学验证、
 * ShadowSampleCorrection 一致性、HairDrawItem 默认值。
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include <glm/glm.hpp>
#include <cmath>

#ifdef DSE_ENABLE_VULKAN
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#endif
#ifdef DSE_ENABLE_D3D11
#include "engine/render/rhi/dx11/dx11_rhi_device.h"
#endif

using namespace dse::render;

// ============================================================
// 辅助：比较矩阵近似相等
// ============================================================
static void ExpectMatNear(const glm::mat4& a, const glm::mat4& b, float eps = 1e-5f) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_NEAR(a[c][r], b[c][r], eps) << "col=" << c << " row=" << r;
}

// ============================================================
// OpenGL 补充：ShadowSampleCorrection
// ============================================================

TEST(ProjectionCorrectionTest, GL_ShadowSampleCorrection_Identity) {
    OpenGLRhiDevice dev;
    glm::mat4 sc = dev.GetShadowSampleCorrection();
    ExpectMatNear(sc, glm::mat4(1.0f));
}

TEST(ProjectionCorrectionTest, GL_ZRangekeepMinusOneToOne) {
    OpenGLRhiDevice dev;
    glm::mat4 corr = dev.GetProjectionCorrection();

    glm::vec4 near_pt(0.0f, 0.0f, -1.0f, 1.0f);
    glm::vec4 far_pt(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 n = corr * near_pt;
    glm::vec4 f = corr * far_pt;
    EXPECT_NEAR(n.z, -1.0f, 1e-5f);
    EXPECT_NEAR(f.z, 1.0f, 1e-5f);
}

// ============================================================
// Vulkan：Y-flip + Z remap
// ============================================================
#ifdef DSE_ENABLE_VULKAN

TEST(ProjectionCorrectionTest, VK_ProjectionCorrection_YFlip_ZRemap) {
    VulkanRhiDevice dev;
    glm::mat4 corr = dev.GetProjectionCorrection();

    EXPECT_FLOAT_EQ(corr[1][1], -1.0f);
    EXPECT_FLOAT_EQ(corr[2][2], 0.5f);
    EXPECT_FLOAT_EQ(corr[3][2], 0.5f);
    EXPECT_FLOAT_EQ(corr[0][0], 1.0f);
    EXPECT_FLOAT_EQ(corr[3][3], 1.0f);
}

TEST(ProjectionCorrectionTest, VK_ShadowSampleCorrection_YFlip_NoZRemap) {
    VulkanRhiDevice dev;
    glm::mat4 sc = dev.GetShadowSampleCorrection();

    EXPECT_FLOAT_EQ(sc[1][1], -1.0f);
    EXPECT_FLOAT_EQ(sc[2][2], 1.0f);
    EXPECT_FLOAT_EQ(sc[3][2], 0.0f);
}

TEST(ProjectionCorrectionTest, VK_NeedsReadbackYFlip_False) {
    VulkanRhiDevice dev;
    EXPECT_FALSE(dev.NeedsReadbackYFlip());
}

TEST(ProjectionCorrectionTest, VK_NDCtransform) {
    VulkanRhiDevice dev;
    glm::mat4 corr = dev.GetProjectionCorrection();

    glm::vec4 gl_ndc(0.0f, 1.0f, -1.0f, 1.0f);
    glm::vec4 vk_ndc = corr * gl_ndc;
    EXPECT_NEAR(vk_ndc.y, -1.0f, 1e-5f);
    EXPECT_NEAR(vk_ndc.z, 0.0f, 1e-5f);
}

#endif // DSE_ENABLE_VULKAN

// ============================================================
// DX11：Z remap only
// ============================================================
#ifdef DSE_ENABLE_D3D11

TEST(ProjectionCorrectionTest, DX11_ProjectionCorrection_ZRemapOnly) {
    DX11RhiDevice dev;
    glm::mat4 corr = dev.GetProjectionCorrection();

    EXPECT_FLOAT_EQ(corr[1][1], 1.0f);
    EXPECT_FLOAT_EQ(corr[2][2], 0.5f);
    EXPECT_FLOAT_EQ(corr[3][2], 0.5f);
    EXPECT_FLOAT_EQ(corr[0][0], 1.0f);
}

TEST(ProjectionCorrectionTest, DX11_ShadowSampleCorrection_YFlip) {
    DX11RhiDevice dev;
    glm::mat4 sc = dev.GetShadowSampleCorrection();
    EXPECT_FLOAT_EQ(sc[0][0], 1.0f);
    EXPECT_FLOAT_EQ(sc[1][1], -1.0f);  // Y-flip: DX11 纹理 V=0 在顶部
    EXPECT_FLOAT_EQ(sc[2][2], 1.0f);
    EXPECT_FLOAT_EQ(sc[3][3], 1.0f);
}

TEST(ProjectionCorrectionTest, DX11_NeedsReadbackYFlip_False) {
    DX11RhiDevice dev;
    EXPECT_FALSE(dev.NeedsReadbackYFlip());
}

TEST(ProjectionCorrectionTest, DX11_NDCtransform) {
    DX11RhiDevice dev;
    glm::mat4 corr = dev.GetProjectionCorrection();

    glm::vec4 gl_ndc(0.0f, 0.0f, -1.0f, 1.0f);
    glm::vec4 dx_ndc = corr * gl_ndc;
    EXPECT_NEAR(dx_ndc.y, 0.0f, 1e-5f);
    EXPECT_NEAR(dx_ndc.z, 0.0f, 1e-5f);
}

#endif // DSE_ENABLE_D3D11

// ============================================================
// RHI 类型补充：RenderTargetDesc 相等性、HairDrawItem
// ============================================================

TEST(RenderTargetDescExtTest, MSAAequality) {
    RenderTargetDesc a, b;
    a.width = 1920; a.height = 1080; a.has_depth = true;
    b = a;
    EXPECT_TRUE(a == b);
    b.msaa_samples = 4;
    EXPECT_FALSE(a == b);
}

TEST(HairDrawItemTest, DefaultValues) {
    HairDrawItem hdi;
    EXPECT_FALSE(hdi.position_ssbo);
    EXPECT_FALSE(hdi.tangent_ssbo);
    EXPECT_EQ(hdi.total_vertex_count, 0u);
    EXPECT_EQ(hdi.strand_count, 0u);
    EXPECT_FLOAT_EQ(hdi.fiber_radius, 0.04f);
    EXPECT_FLOAT_EQ(hdi.opacity, 0.9f);
}

TEST(DrawElementsIndirectCommandExtTest, TestCase13) {
    DrawElementsIndirectCommand cmd{};
    cmd.count = 36;
    cmd.instance_count = 10;
    cmd.first_index = 100;
    cmd.base_vertex = 50;
    cmd.base_instance = 0;
    EXPECT_EQ(cmd.count, 36u);
    EXPECT_EQ(cmd.instance_count, 10u);
    EXPECT_EQ(cmd.first_index, 100u);
    EXPECT_EQ(cmd.base_vertex, 50);
}
