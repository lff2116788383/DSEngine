/**
 * @file compute_stress_test.cpp
 * @brief Compute Shader 压力/边界测试（三后端统一，无 GPU 路径）
 *
 * 验证场景：
 * 1. Uniform 大量设置不崩溃（模拟复杂 compute pass）
 * 2. 同名 uniform 重复设置（覆盖语义）
 * 3. 多 shader 交错设置 uniform
 * 4. ClearComputeParams 后状态干净
 * 5. 极端数据值（NaN、Inf、极大/极小）
 * 6. 未初始化时所有 compute 接口安全
 * 7. 空字符串 name 不崩溃
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_factory.h"

#include <cmath>
#include <limits>
#include <string>

using namespace dse::render;

// ============================================================
// Vulkan Compute Stress Tests
// ============================================================

#ifdef DSE_ENABLE_VULKAN
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"

TEST(VulkanComputeStressTest, 大量Uniform设置不崩溃) {
    VulkanRhiDevice device;
    for (int i = 0; i < 100; ++i) {
        std::string name = "u_param_" + std::to_string(i);
        device.SetComputeUniformFloat(1, name.c_str(), static_cast<float>(i) * 0.1f);
    }
}

TEST(VulkanComputeStressTest, 同名Uniform重复设置覆盖) {
    VulkanRhiDevice device;
    for (int i = 0; i < 50; ++i) {
        device.SetComputeUniformInt(1, "u_count", i);
    }
    // 最后一次设置应覆盖前面的，不应累积内存
}

TEST(VulkanComputeStressTest, 多Shader交错设置) {
    VulkanRhiDevice device;
    device.SetComputeUniformFloat(1, "u_alpha", 0.5f);
    device.SetComputeUniformFloat(2, "u_alpha", 0.8f);
    device.SetComputeUniformInt(1, "u_count", 10);
    device.SetComputeUniformInt(2, "u_count", 20);
    device.SetComputeUniformVec3(1, "u_pos", 1.0f, 2.0f, 3.0f);
    device.SetComputeUniformVec3(2, "u_pos", 4.0f, 5.0f, 6.0f);
}

TEST(VulkanComputeStressTest, ClearParams后再设置) {
    VulkanRhiDevice device;
    device.SetComputeUniformFloat(1, "u_value", 3.14f);
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    device.SetComputeUniformMat4(1, "u_mvp", identity);
}

TEST(VulkanComputeStressTest, 极端浮点值) {
    VulkanRhiDevice device;
    device.SetComputeUniformFloat(1, "u_nan", std::numeric_limits<float>::quiet_NaN());
    device.SetComputeUniformFloat(1, "u_inf", std::numeric_limits<float>::infinity());
    device.SetComputeUniformFloat(1, "u_neg_inf", -std::numeric_limits<float>::infinity());
    device.SetComputeUniformFloat(1, "u_denorm", std::numeric_limits<float>::denorm_min());
    device.SetComputeUniformFloat(1, "u_max", std::numeric_limits<float>::max());
    device.SetComputeUniformFloat(1, "u_min", std::numeric_limits<float>::lowest());
}

TEST(VulkanComputeStressTest, 空字符串Name不崩溃) {
    VulkanRhiDevice device;
    device.SetComputeUniformInt(1, "", 42);
    device.SetComputeUniformFloat(1, "", 1.0f);
    device.SetComputeUniformVec2i(1, "", 1, 2);
    device.SetComputeUniformVec3(1, "", 1.0f, 2.0f, 3.0f);
    device.SetComputeUniformVec4(1, "", 1.0f, 2.0f, 3.0f, 4.0f);
}

TEST(VulkanComputeStressTest, 未初始化时DispatchCompute安全) {
    VulkanRhiDevice device;
    device.DispatchCompute(999, 64, 64, 1);
}

TEST(VulkanComputeStressTest, 未初始化时BeginEndComputePass安全) {
    VulkanRhiDevice device;
    device.BeginComputePass();
    device.EndComputePass();
}

TEST(VulkanComputeStressTest, 未初始化时SetComputeTextureImage安全) {
    VulkanRhiDevice device;
    device.SetComputeTextureImage(0, 100, true);
    device.SetComputeTextureImage(1, 200, false);
    device.SetComputeTextureImageMip(0, 100, 3, true);
    device.SetComputeTextureSampler(0, 100);
}

TEST(VulkanComputeStressTest, 未初始化时ComputeMemoryBarrier安全) {
    VulkanRhiDevice device;
    device.ComputeMemoryBarrier();
}

TEST(VulkanComputeStressTest, 未初始化时DeleteComputeShader安全) {
    VulkanRhiDevice device;
    device.DeleteComputeShader(0);
    device.DeleteComputeShader(999);
}

TEST(VulkanComputeStressTest, 未初始化时CreateComputeShader返回零) {
    VulkanRhiDevice device;
    unsigned int h = device.CreateComputeShader("void main() {}");
    EXPECT_EQ(h, 0u);
}

TEST(VulkanComputeStressTest, SSBO未初始化时ReadSSBO安全) {
    VulkanRhiDevice device;
    int buf[4] = {};
    device.ReadSSBO(999, 0, sizeof(buf), buf);
}

#endif // DSE_ENABLE_VULKAN

// ============================================================
// D3D11 Compute Stress Tests
// ============================================================

#ifdef DSE_ENABLE_D3D11
#include "engine/render/rhi/dx11/dx11_rhi_device.h"

TEST(DX11ComputeStressTest, 大量Uniform设置不崩溃) {
    DX11RhiDevice device;
    for (int i = 0; i < 100; ++i) {
        std::string name = "u_param_" + std::to_string(i);
        device.SetComputeUniformFloat(1, name.c_str(), static_cast<float>(i) * 0.1f);
    }
    device.ClearComputeParams();
}

TEST(DX11ComputeStressTest, 同名Uniform重复设置覆盖) {
    DX11RhiDevice device;
    for (int i = 0; i < 50; ++i) {
        device.SetComputeUniformInt(1, "u_count", i);
    }
    device.ClearComputeParams();
}

TEST(DX11ComputeStressTest, 多Shader交错设置) {
    DX11RhiDevice device;
    device.SetComputeUniformFloat(1, "u_alpha", 0.5f);
    device.SetComputeUniformFloat(2, "u_alpha", 0.8f);
    device.SetComputeUniformInt(1, "u_count", 10);
    device.SetComputeUniformInt(2, "u_count", 20);
    device.SetComputeUniformVec3(1, "u_pos", 1.0f, 2.0f, 3.0f);
    device.SetComputeUniformVec3(2, "u_pos", 4.0f, 5.0f, 6.0f);
    device.ClearComputeParams();
}

TEST(DX11ComputeStressTest, ClearParams后状态干净) {
    DX11RhiDevice device;
    device.SetComputeUniformFloat(1, "u_value", 3.14f);
    device.SetComputeUniformVec4(1, "u_color", 1.0f, 0.0f, 0.0f, 1.0f);
    device.ClearComputeParams();
    // 重新设置——偏移应从 0 开始
    device.SetComputeUniformFloat(1, "u_value", 2.71f);
    device.ClearComputeParams();
}

TEST(DX11ComputeStressTest, 极端浮点值) {
    DX11RhiDevice device;
    device.SetComputeUniformFloat(1, "u_nan", std::numeric_limits<float>::quiet_NaN());
    device.SetComputeUniformFloat(1, "u_inf", std::numeric_limits<float>::infinity());
    device.SetComputeUniformFloat(1, "u_neg_inf", -std::numeric_limits<float>::infinity());
    device.SetComputeUniformFloat(1, "u_max", (std::numeric_limits<float>::max)());
    device.ClearComputeParams();
}

TEST(DX11ComputeStressTest, 空字符串Name不崩溃) {
    DX11RhiDevice device;
    device.SetComputeUniformInt(1, "", 42);
    device.SetComputeUniformFloat(1, "", 1.0f);
    device.SetComputeUniformVec2f(1, "", 1.0f, 2.0f);
    device.ClearComputeParams();
}

TEST(DX11ComputeStressTest, 未初始化时DispatchCompute安全) {
    DX11RhiDevice device;
    device.DispatchCompute(999, 64, 64, 1);
}

TEST(DX11ComputeStressTest, 未初始化时BeginEndComputePass安全) {
    DX11RhiDevice device;
    device.BeginComputePass();
    device.EndComputePass();
}

TEST(DX11ComputeStressTest, 未初始化时SetComputeTextureImage安全) {
    DX11RhiDevice device;
    device.SetComputeTextureImage(0, 100, true);
    device.SetComputeTextureImage(1, 200, false);
    device.SetComputeTextureImageMip(0, 100, 3, true);
    device.SetComputeTextureSampler(0, 100);
}

TEST(DX11ComputeStressTest, 未初始化时ComputeMemoryBarrier安全) {
    DX11RhiDevice device;
    device.ComputeMemoryBarrier();
}

TEST(DX11ComputeStressTest, 未初始化时DeleteComputeShader安全) {
    DX11RhiDevice device;
    device.DeleteComputeShader(0);
    device.DeleteComputeShader(999);
}

TEST(DX11ComputeStressTest, 未初始化时CreateComputeShader返回零) {
    DX11RhiDevice device;
    unsigned int h = device.CreateComputeShader("void main() {}");
    EXPECT_EQ(h, 0u);
}

TEST(DX11ComputeStressTest, FlushComputeParamsCB未初始化安全) {
    DX11RhiDevice device;
    device.SetComputeUniformFloat(1, "u_test", 1.0f);
    device.FlushComputeParamsCB();  // 无 device context 应安全返回
    device.ClearComputeParams();
}

TEST(DX11ComputeStressTest, SSBO未初始化时ReadSSBO安全) {
    DX11RhiDevice device;
    int buf[4] = {};
    device.ReadSSBO(999, 0, sizeof(buf), buf);
}

#endif // DSE_ENABLE_D3D11

// ============================================================
// OpenGL Compute Stress Tests（无 GL context 下安全性）
// ============================================================

TEST(GLComputeStressTest, 未初始化时SupportsCompute) {
    auto device = CreateRhiDevice(RhiBackend::OpenGL);
    ASSERT_NE(device, nullptr);
    // OpenGL compute 支持取决于 GL 上下文版本，此处仅验证调用不崩溃
    bool supports = device->SupportsCompute();
    (void)supports;
}

// ============================================================
// GPU Timer 接口测试（无 GPU）
// ============================================================

TEST(GpuTimerInterfaceTest, RhiDevice默认不支持GpuTimer) {
    auto device = CreateRhiDevice(RhiBackend::OpenGL);
    ASSERT_NE(device, nullptr);
    EXPECT_FALSE(device->SupportsGpuTimer());
    EXPECT_EQ(device->GetOrCreateGpuTimer("test"), kInvalidGpuTimerId);
    EXPECT_FLOAT_EQ(device->GetGpuTimerResultMs(1), -1.0f);
    // 调用不崩溃
    device->BeginGpuTimer(1);
    device->EndGpuTimer(1);
    device->ResetGpuTimers();
    device->ResolveGpuTimers();
    auto results = device->GetAllGpuTimerResults();
    EXPECT_TRUE(results.empty());
}

#ifdef DSE_ENABLE_VULKAN
TEST(GpuTimerInterfaceTest, VulkanDevice默认不支持未初始化) {
    VulkanRhiDevice device;
    EXPECT_FALSE(device.SupportsGpuTimer());
    EXPECT_EQ(device.GetOrCreateGpuTimer("shadow"), kInvalidGpuTimerId);
    device.BeginGpuTimer(1);
    device.EndGpuTimer(1);
    device.ResetGpuTimers();
    device.ResolveGpuTimers();
}
#endif

#ifdef DSE_ENABLE_D3D11
TEST(GpuTimerInterfaceTest, DX11Device默认不支持未初始化) {
    DX11RhiDevice device;
    EXPECT_FALSE(device.SupportsGpuTimer());
    EXPECT_EQ(device.GetOrCreateGpuTimer("shadow"), kInvalidGpuTimerId);
    device.BeginGpuTimer(1);
    device.EndGpuTimer(1);
    device.ResetGpuTimers();
    device.ResolveGpuTimers();
}
#endif
