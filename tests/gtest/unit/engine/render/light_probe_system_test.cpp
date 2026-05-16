/**
 * @file light_probe_system_test.cpp
 * @brief LightProbeSystem 单元测试（CPU 端 SH 积分 + 系统状态）
 *
 * 测试策略：
 * - IntegrateFaceSH 是纯 CPU 函数，可直接测试
 * - SHL2 / BakedProbe 数据结构默认值
 * - LightProbeSystem 生命周期（无 GPU）
 * - LightProbeComponent 默认值
 */

#include <gtest/gtest.h>
#include "engine/render/light_probe_system.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/ecs/components_3d.h"
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

using namespace dse::render;

// ============================================================
// SHL2 / BakedProbe 数据结构
// ============================================================

TEST(SHL2Test, 默认值全零) {
    SHL2 sh;
    for (int i = 0; i < 9; ++i) {
        EXPECT_FLOAT_EQ(sh.coeffs[i].x, 0.0f);
        EXPECT_FLOAT_EQ(sh.coeffs[i].y, 0.0f);
        EXPECT_FLOAT_EQ(sh.coeffs[i].z, 0.0f);
    }
}

TEST(BakedProbeTest, 默认值) {
    BakedProbe bp;
    EXPECT_FLOAT_EQ(bp.position.x, 0.0f);
    EXPECT_FLOAT_EQ(bp.position.y, 0.0f);
    EXPECT_FLOAT_EQ(bp.position.z, 0.0f);
    EXPECT_FLOAT_EQ(bp.influence_radius, 10.0f);
}

// ============================================================
// LightProbeComponent ECS 默认值
// ============================================================

TEST(LightProbeComponentTest, 默认值) {
    dse::LightProbeComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_FLOAT_EQ(comp.influence_radius, 10.0f);
    EXPECT_TRUE(comp.needs_rebake);
    EXPECT_TRUE(comp.show_debug);
    for (int i = 0; i < 9; ++i) {
        EXPECT_FLOAT_EQ(comp.sh_coefficients[i].x, 0.0f);
        EXPECT_FLOAT_EQ(comp.sh_coefficients[i].y, 0.0f);
        EXPECT_FLOAT_EQ(comp.sh_coefficients[i].z, 0.0f);
    }
}

// ============================================================
// LightProbeSystem 生命周期（无 GPU）
// ============================================================

TEST(LightProbeSystemTest, 默认未初始化) {
    LightProbeSystem sys;
    EXPECT_TRUE(sys.baked_probes().empty());
}

TEST(LightProbeSystemTest, Init_nullptr安全) {
    LightProbeSystem sys;
    sys.Init(nullptr);
    EXPECT_TRUE(sys.baked_probes().empty());
}

TEST(LightProbeSystemTest, Shutdown安全) {
    LightProbeSystem sys;
    sys.Shutdown();
    EXPECT_TRUE(sys.baked_probes().empty());
}

TEST(LightProbeSystemTest, 重复Shutdown安全) {
    LightProbeSystem sys;
    sys.Shutdown();
    sys.Shutdown();
}

// ============================================================
// IntegrateFaceSH — 纯 CPU 积分测试
// ============================================================

TEST(IntegrateFaceSHTest, 全黑像素积分为零) {
    const int w = 4, h = 4;
    std::vector<unsigned char> black(w * h * 4, 0);
    SHL2 sh;
    LightProbeSystem::IntegrateFaceSH(black.data(), w, h, 0, sh);
    for (int i = 0; i < 9; ++i) {
        EXPECT_FLOAT_EQ(sh.coeffs[i].x, 0.0f);
        EXPECT_FLOAT_EQ(sh.coeffs[i].y, 0.0f);
        EXPECT_FLOAT_EQ(sh.coeffs[i].z, 0.0f);
    }
}

TEST(IntegrateFaceSHTest, 全白像素积分非零) {
    const int w = 4, h = 4;
    std::vector<unsigned char> white(w * h * 4, 255);
    SHL2 sh;
    LightProbeSystem::IntegrateFaceSH(white.data(), w, h, 0, sh);
    // L0 (DC) 系数应正
    EXPECT_GT(sh.coeffs[0].x, 0.0f);
    EXPECT_GT(sh.coeffs[0].y, 0.0f);
    EXPECT_GT(sh.coeffs[0].z, 0.0f);
}

TEST(IntegrateFaceSHTest, 六面全白积分DC近似4PI) {
    const int w = 16, h = 16;
    std::vector<unsigned char> white(w * h * 4, 255);

    SHL2 total;
    for (int face = 0; face < 6; ++face) {
        LightProbeSystem::IntegrateFaceSH(white.data(), w, h, face, total);
    }

    // Y00 = 1 / (2*sqrt(pi)) ≈ 0.282095
    // 全白 L0 积分 ≈ Y00 * 4π（总立体角）≈ 3.544
    // 实际值受离散化误差影响，允许 20% 容差
    float expected_dc = 0.282095f * 4.0f * 3.14159265f;
    EXPECT_NEAR(total.coeffs[0].x, expected_dc, expected_dc * 0.2f);
}

TEST(IntegrateFaceSHTest, 纯红像素只有R通道) {
    const int w = 4, h = 4;
    std::vector<unsigned char> red(w * h * 4, 0);
    for (int i = 0; i < w * h; ++i) {
        red[i * 4 + 0] = 255;  // R
        red[i * 4 + 3] = 255;  // A
    }
    SHL2 sh;
    LightProbeSystem::IntegrateFaceSH(red.data(), w, h, 0, sh);
    EXPECT_GT(sh.coeffs[0].x, 0.0f);
    EXPECT_FLOAT_EQ(sh.coeffs[0].y, 0.0f);
    EXPECT_FLOAT_EQ(sh.coeffs[0].z, 0.0f);
}

TEST(IntegrateFaceSHTest, 各面索引不崩溃) {
    const int w = 2, h = 2;
    std::vector<unsigned char> pixels(w * h * 4, 128);
    for (int face = 0; face < 6; ++face) {
        SHL2 sh;
        LightProbeSystem::IntegrateFaceSH(pixels.data(), w, h, face, sh);
    }
}

TEST(IntegrateFaceSHTest, BakeSHAtPosition_nullptr安全) {
    RenderPassContext ctx;
    SHL2 sh = LightProbeSystem::BakeSHAtPosition(glm::vec3(0.0f), 64, nullptr, 0, ctx);
    for (int i = 0; i < 9; ++i) {
        EXPECT_FLOAT_EQ(sh.coeffs[i].x, 0.0f);
    }
}
