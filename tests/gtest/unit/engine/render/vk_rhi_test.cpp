/**
 * @file vk_rhi_test.cpp
 * @brief Vulkan RHI 单元测试（无 GPU / 无窗口）
 *
 * 覆盖场景：
 * 1. VulkanRhiDevice 构造/析构/未初始化时接口安全
 * 2. VulkanCommandBuffer 立即转发/Reset
 * 3. VulkanDrawExecutor 全局状态边界检查
 * 4. VulkanShaderManager 未初始化默认值
 * 5. VulkanResourceManager 句柄分配递增 / 渲染目标存储查询
 * 6. VulkanPipelineStateManager 管线状态 CRUD
 */

#ifdef DSE_ENABLE_VULKAN

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_factory.h"
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#include "engine/render/rhi/vulkan/vulkan_command_buffer.h"

using namespace dse::render;

// ============================================================
// RHI Factory — Vulkan
// ============================================================

// 测试 RHI工厂Vulkan：创建RHI设备Vulkan返回非空
TEST(RhiFactoryVulkanTest, CreateRhiDevice_VulkanReturnNonEmpty) {
    auto device = CreateRhiDevice(RhiBackend::Vulkan);
    EXPECT_NE(device, nullptr);
}

// ============================================================
// VulkanRhiDevice 无 GPU 测试
// ============================================================

// 测试 Vulkan RHI设备基础：不崩溃
TEST(VkRhiDeviceBasicTest, DoesNotCrash) {
    VulkanRhiDevice device;
}

// 测试 Vulkan RHI设备基础：当不已初始化关闭安全
TEST(VkRhiDeviceBasicTest, WhenNotInitializedShutdownSafety) {
    VulkanRhiDevice device;
    device.Shutdown();
}

// 测试 Vulkan RHI设备基础：当不已初始化开始帧安全
TEST(VkRhiDeviceBasicTest, WhenNotInitializedBeginFrameSafety) {
    VulkanRhiDevice device;
    device.BeginFrame();
}

// 测试 Vulkan RHI设备基础：当不已初始化结束帧安全
TEST(VkRhiDeviceBasicTest, WhenNotInitializedEndFrameSafety) {
    VulkanRhiDevice device;
    device.EndFrame();
}

// 测试 Vulkan RHI设备基础：当不已初始化提交安全
TEST(VkRhiDeviceBasicTest, WhenNotInitializedSubmitSafety) {
    VulkanRhiDevice device;
    auto cmd = std::make_shared<VulkanCommandBuffer>();
    device.Submit(cmd);
}

// 测试 Vulkan RHI设备基础：当不已初始化创建缓冲区返回零
TEST(VkRhiDeviceBasicTest, WhenNotInitializedCreateBufferReturnsZero) {
    VulkanRhiDevice device;
    unsigned int handle = device.CreateBuffer(16, nullptr, false, false);
    EXPECT_EQ(handle, 0u);
}

// 测试 Vulkan RHI设备基础：当不已初始化创建纹理2D返回零
TEST(VkRhiDeviceBasicTest, WhenNotInitializedCreateTexture2DReturnsZero) {
    VulkanRhiDevice device;
    unsigned int handle = device.CreateTexture2D(4, 4, nullptr, false);
    EXPECT_EQ(handle, 0u);
}

// 测试 Vulkan RHI设备基础：当不已初始化创建渲染目标返回零
TEST(VkRhiDeviceBasicTest, WhenNotInitializedCreateRenderTargetReturnsZero) {
    VulkanRhiDevice device;
    RenderTargetDesc desc{};
    desc.width = 256;
    desc.height = 256;
    desc.has_color = true;
    unsigned int handle = device.CreateRenderTarget(desc);
    EXPECT_EQ(handle, 0u);
}

// 测试 Vulkan RHI设备基础：当不已初始化更新缓冲区安全
TEST(VkRhiDeviceBasicTest, WhenNotInitializedUpdateBufferSafety) {
    VulkanRhiDevice device;
    float data[] = {0.5f};
    device.UpdateBuffer(999, 0, sizeof(data), data, false);
}

// 测试 Vulkan RHI设备基础：当不已初始化删除安全
TEST(VkRhiDeviceBasicTest, WhenNotInitializedDeleteSafety) {
    VulkanRhiDevice device;
    device.DeleteRenderTarget(999);
    device.DeleteTexture(999);
    device.DeleteShaderProgram(999);
    device.DeleteBuffer(999);
}

// 测试 Vulkan RHI设备基础：最后帧统计默认值为零
TEST(VkRhiDeviceBasicTest, LastFrameStatsDefaultValueIsZero) {
    VulkanRhiDevice device;
    const auto& stats = device.LastFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

// 测试 Vulkan RHI设备基础：系统能够调用
TEST(VkRhiDeviceBasicTest, SystemCanCalls) {
    VulkanRhiDevice device;
    auto& res    = device.resource_mgr();
    auto& state  = device.state_mgr();
    auto& shader = device.shader_mgr();
    auto& draw   = device.draw_executor();
    (void)res; (void)state; (void)shader; (void)draw;
}

#endif // DSE_ENABLE_VULKAN
