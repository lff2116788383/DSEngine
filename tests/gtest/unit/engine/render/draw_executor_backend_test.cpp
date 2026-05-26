/**
 * @file draw_executor_backend_test.cpp
 * @brief DrawExecutor 三端独立测试 — DX11 CB / Vulkan UBO 结构体对齐
 *
 * 共用的 DrawExecutorGlobalState / PrepareXxxUBO 测试已在
 * ubo_draw_executor_common_test.cpp 中覆盖。
 * 本文件仅补充条件编译的 DX11/Vulkan 后端数据结构对齐验证。
 */

#include <gtest/gtest.h>

// ============================================================
// DX11 CB 结构体对齐
// ============================================================

#ifdef DSE_ENABLE_D3D11
#include "engine/render/rhi/dx11/dx11_draw_executor.h"
using namespace dse::render;

TEST(DX11CBAlignmentTest, PerFrameCB_16B对齐) {
    EXPECT_EQ(sizeof(DX11PerFrameCB) % 16, 0u);
}

TEST(DX11CBAlignmentTest, PerObjectCB_16B对齐) {
    EXPECT_EQ(sizeof(DX11PerObjectCB) % 16, 0u);
}

TEST(DX11CBAlignmentTest, PerSceneCB_16B对齐) {
    EXPECT_EQ(sizeof(DX11PerSceneCB) % 16, 0u);
}

TEST(DX11CBAlignmentTest, PerMaterialCB_16B对齐) {
    EXPECT_EQ(sizeof(DX11PerMaterialCB) % 16, 0u);
}

TEST(DX11CBAlignmentTest, PointLightEntry_16B对齐) {
    EXPECT_EQ(sizeof(DX11PointLightEntry) % 16, 0u);
}

TEST(DX11CBAlignmentTest, PointLightsCB_16B对齐) {
    EXPECT_EQ(sizeof(DX11PointLightsCB) % 16, 0u);
}

TEST(DX11CBAlignmentTest, SpotLightEntry_16B对齐) {
    EXPECT_EQ(sizeof(DX11SpotLightEntry) % 16, 0u);
}

TEST(DX11CBAlignmentTest, SpotLightsCB_16B对齐) {
    EXPECT_EQ(sizeof(DX11SpotLightsCB) % 16, 0u);
}

TEST(DX11CBAlignmentTest, SpotMatricesCB_16B对齐) {
    EXPECT_EQ(sizeof(DX11SpotMatricesCB) % 16, 0u);
}

TEST(DX11CBAlignmentTest, LightProbeDataCB_16B对齐) {
    EXPECT_EQ(sizeof(DX11LightProbeDataCB) % 16, 0u);
}

TEST(DX11PerFrameCBTest, 默认值) {
    DX11PerFrameCB cb{};
    EXPECT_FLOAT_EQ(cb.camera_pos.x, 0.0f);
    EXPECT_FLOAT_EQ(cb.camera_pos.y, 0.0f);
    EXPECT_FLOAT_EQ(cb.camera_pos.z, 0.0f);
}

TEST(DX11PerObjectCBTest, 默认值) {
    DX11PerObjectCB cb{};
    EXPECT_EQ(cb.skinned, 0);
    EXPECT_EQ(cb.morph_enabled, 0);
    EXPECT_EQ(cb.bone_offset, 0);
}

#endif // DSE_ENABLE_D3D11

// ============================================================
// Vulkan UBO 结构体对齐
// ============================================================

#ifdef DSE_ENABLE_VULKAN
#include "engine/render/rhi/vulkan/vulkan_draw_executor.h"
using namespace dse::render;

TEST(VulkanUBOAlignmentTest, PerFrameUBO_16B对齐) {
    EXPECT_EQ(sizeof(VulkanPerFrameUBO) % 16, 0u);
}

TEST(VulkanUBOAlignmentTest, PerSceneUBO_16B对齐) {
    EXPECT_EQ(sizeof(VulkanPerSceneUBO) % 16, 0u);
}

TEST(VulkanUBOAlignmentTest, PerMaterialUBO_16B对齐) {
    EXPECT_EQ(sizeof(VulkanPerMaterialUBO) % 16, 0u);
}

TEST(VulkanPerFrameUBOTest, 默认值) {
    VulkanPerFrameUBO ubo{};
    EXPECT_FLOAT_EQ(ubo.camera_pos.x, 0.0f);
}

TEST(VulkanPerSceneUBOTest, CascadeMatrix数组大小) {
    VulkanPerSceneUBO ubo{};
    // 3 个 CSM cascade
    static_assert(sizeof(ubo.light_space_matrices) / sizeof(glm::mat4) == 3);
    SUCCEED();
}

#endif // DSE_ENABLE_VULKAN
