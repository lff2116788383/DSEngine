/**
 * @file shader_manager_test.cpp
 * @brief Vulkan / DX11 ShaderManager 独立测试（纯 CPU 数据结构 + 默认状态）
 *
 * GL 侧测试已在 gl_shader_ubo_manager_test.cpp 中覆盖。
 * 本文件仅覆盖条件编译的 Vulkan 和 DX11 后端数据结构。
 */

#include <gtest/gtest.h>

// ============================================================
// Vulkan 数据结构（条件编译）
// ============================================================

#ifdef DSE_ENABLE_VULKAN
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
using namespace dse::render;

// 测试 描述符绑定信息：默认值
TEST(DescriptorBindingInfoTest, DefaultValues) {
    DescriptorBindingInfo info;
    EXPECT_EQ(info.set, 0u);
    EXPECT_EQ(info.binding, 0u);
    EXPECT_EQ(info.type, VK_DESCRIPTOR_TYPE_MAX_ENUM);
    EXPECT_EQ(info.stage_flags, 0u);
    EXPECT_EQ(info.count, 1u);
}

// 测试 描述符绑定信息：相等
TEST(DescriptorBindingInfoTest, Equality) {
    DescriptorBindingInfo a, b;
    a.set = 1; a.binding = 2; a.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    a.stage_flags = VK_SHADER_STAGE_VERTEX_BIT; a.count = 1;
    b = a;
    EXPECT_TRUE(a == b);
    b.binding = 3;
    EXPECT_FALSE(a == b);
}

// 测试 着色器反射：默认值
TEST(ShaderReflectionTest, DefaultValues) {
    ShaderReflection ref;
    EXPECT_TRUE(ref.bindings.empty());
    EXPECT_FALSE(ref.has_push_constant);
}

// 测试 Vulkan着色器程序：默认
TEST(VulkanShaderProgramTest, Default) {
    VulkanShaderProgram prog;
    EXPECT_EQ(prog.vert_module, VK_NULL_HANDLE);
    EXPECT_EQ(prog.frag_module, VK_NULL_HANDLE);
    EXPECT_EQ(prog.pipeline_layout, VK_NULL_HANDLE);
    EXPECT_TRUE(prog.descriptor_set_layouts.empty());
}

// 测试 Vulkan计算程序：默认值
TEST(VulkanComputeProgramTest, DefaultValues) {
    VulkanComputeProgram cp;
    EXPECT_EQ(cp.comp_module, VK_NULL_HANDLE);
    EXPECT_EQ(cp.pipeline, VK_NULL_HANDLE);
    EXPECT_EQ(cp.pipeline_layout, VK_NULL_HANDLE);
    EXPECT_EQ(cp.descriptor_set_layout, VK_NULL_HANDLE);
    EXPECT_EQ(cp.push_constant_size, 0u);
    EXPECT_FALSE(cp.uses_ssbo_bindings);
}

// 测试 Vulkan着色器管理器：默认
TEST(VulkanShaderManagerTest, Default) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.particle_shader_handle(), 0u);
    EXPECT_EQ(mgr.sprite_shader_handle(), 0u);
    EXPECT_EQ(mgr.postprocess_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

// 测试 Vulkan着色器管理器：获取程序返回无注册返回空指针
TEST(VulkanShaderManagerTest, GetProgram_ReturnWithoutRegistrationnullptr) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.GetProgram(999), nullptr);
    EXPECT_EQ(mgr.GetComputeProgram(999), nullptr);
}

#endif // DSE_ENABLE_VULKAN

// ============================================================
// DX11 数据结构（条件编译）
// ============================================================

#ifdef DSE_ENABLE_D3D11
#include "engine/render/rhi/dx11/dx11_shader_manager.h"
using namespace dse::render;

// 测试 DX 11着色器程序：默认值
TEST(DX11ShaderProgramTest, DefaultValues) {
    DX11ShaderProgram prog;
    EXPECT_TRUE(prog.vertex_shader.Get() == nullptr);
    EXPECT_TRUE(prog.pixel_shader.Get() == nullptr);
    EXPECT_TRUE(prog.vs_blob.Get() == nullptr);
}

// 测试 DX 11计算程序：默认值
TEST(DX11ComputeProgramTest, DefaultValues) {
    DX11ComputeProgram cp;
    EXPECT_TRUE(cp.cs.Get() == nullptr);
    EXPECT_TRUE(cp.params_cb.Get() == nullptr);
}

// 测试 DX 11着色器管理器：默认
TEST(DX11ShaderManagerTest, Default) {
    DX11ShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.particle_shader_handle(), 0u);
    EXPECT_EQ(mgr.sprite_shader_handle(), 0u);
    EXPECT_EQ(mgr.postprocess_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

// 测试 DX 11着色器管理器：获取程序返回无注册返回空指针
TEST(DX11ShaderManagerTest, GetProgram_ReturnWithoutRegistrationnullptr) {
    DX11ShaderManager mgr;
    EXPECT_TRUE(mgr.GetProgram(999) == nullptr);
    EXPECT_TRUE(mgr.GetComputeProgram(999) == nullptr);
}

#endif // DSE_ENABLE_D3D11
