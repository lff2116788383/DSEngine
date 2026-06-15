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

// 测试 DX 11回调对齐：每帧回调情形16 B对齐
TEST(DX11CBAlignmentTest, PerFrameCB_Case16BAlignment) {
    EXPECT_EQ(sizeof(DX11PerFrameCB) % 16, 0u);
}

// 测试 DX 11回调对齐：每对象回调情形16 B对齐
TEST(DX11CBAlignmentTest, PerObjectCB_Case16BAlignment) {
    EXPECT_EQ(sizeof(DX11PerObjectCB) % 16, 0u);
}

// 测试 DX 11回调对齐：每场景回调情形16 B对齐
TEST(DX11CBAlignmentTest, PerSceneCB_Case16BAlignment) {
    EXPECT_EQ(sizeof(DX11PerSceneCB) % 16, 0u);
}

// 测试 DX 11回调对齐：每材质回调情形16 B对齐
TEST(DX11CBAlignmentTest, PerMaterialCB_Case16BAlignment) {
    EXPECT_EQ(sizeof(DX11PerMaterialCB) % 16, 0u);
}

// 测试 DX 11回调对齐：点灯光条目情形16 B对齐
TEST(DX11CBAlignmentTest, PointLightEntry_Case16BAlignment) {
    EXPECT_EQ(sizeof(PointLightEntry) % 16, 0u);
}

// 测试 DX 11回调对齐：点灯光回调情形16 B对齐
TEST(DX11CBAlignmentTest, PointLightsCB_Case16BAlignment) {
    EXPECT_EQ(sizeof(DX11PointLightsCB) % 16, 0u);
}

// 测试 DX 11回调对齐：聚光灯光条目情形16 B对齐
TEST(DX11CBAlignmentTest, SpotLightEntry_Case16BAlignment) {
    EXPECT_EQ(sizeof(SpotLightEntry) % 16, 0u);
}

// 测试 DX 11回调对齐：聚光灯光回调情形16 B对齐
TEST(DX11CBAlignmentTest, SpotLightsCB_Case16BAlignment) {
    EXPECT_EQ(sizeof(DX11SpotLightsCB) % 16, 0u);
}

// 测试 DX 11回调对齐：聚光矩阵回调情形16 B对齐
TEST(DX11CBAlignmentTest, SpotMatricesCB_Case16BAlignment) {
    EXPECT_EQ(sizeof(DX11SpotMatricesCB) % 16, 0u);
}

// 测试 DX 11回调对齐：灯光探针数据回调情形16 B对齐
TEST(DX11CBAlignmentTest, LightProbeDataCB_Case16BAlignment) {
    EXPECT_EQ(sizeof(DX11LightProbeDataCB) % 16, 0u);
}

// 测试 DX 11每帧回调：默认值
TEST(DX11PerFrameCBTest, DefaultValues) {
    DX11PerFrameCB cb{};
    EXPECT_FLOAT_EQ(cb.camera_pos.x, 0.0f);
    EXPECT_FLOAT_EQ(cb.camera_pos.y, 0.0f);
    EXPECT_FLOAT_EQ(cb.camera_pos.z, 0.0f);
}

// 测试 DX 11每对象回调：默认值
TEST(DX11PerObjectCBTest, DefaultValues) {
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

// 测试 Vulkan UBO对齐：每帧UBO情形16 B对齐
TEST(VulkanUBOAlignmentTest, PerFrameUBO_Case16BAlignment) {
    EXPECT_EQ(sizeof(VulkanPerFrameUBO) % 16, 0u);
}

// 测试 Vulkan UBO对齐：每场景UBO情形16 B对齐
TEST(VulkanUBOAlignmentTest, PerSceneUBO_Case16BAlignment) {
    EXPECT_EQ(sizeof(VulkanPerSceneUBO) % 16, 0u);
}

// 测试 Vulkan UBO对齐：每材质UBO情形16 B对齐
TEST(VulkanUBOAlignmentTest, PerMaterialUBO_Case16BAlignment) {
    EXPECT_EQ(sizeof(VulkanPerMaterialUBO) % 16, 0u);
}

// 测试 Vulkan每帧UBO：默认值
TEST(VulkanPerFrameUBOTest, DefaultValues) {
    VulkanPerFrameUBO ubo{};
    EXPECT_FLOAT_EQ(ubo.camera_pos.x, 0.0f);
}

// 测试 Vulkan每场景UBO：级联Matrixarray尺寸
TEST(VulkanPerSceneUBOTest, CascadeMatrixarraySize) {
    VulkanPerSceneUBO ubo{};
    // 3 个 CSM cascade
    static_assert(sizeof(ubo.light_space_matrices) / sizeof(glm::mat4) == 3);
    SUCCEED();
}

#endif // DSE_ENABLE_VULKAN
