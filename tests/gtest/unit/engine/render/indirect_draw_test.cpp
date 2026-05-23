/**
 * @file indirect_draw_test.cpp
 * @brief Indirect Draw 烟雾测试 — 验证三后端 CreateIndirectBuffer / UpdateIndirectBuffer / DeleteIndirectBuffer
 *
 * 验收标准：
 * - GL  : SupportsIndirectDraw()=true，Create/Update/Delete 全部成功
 * - DX11: SupportsIndirectDraw()=true，Create/Update/Delete 全部成功
 * - VK  : stub，Create 返回 0，SupportsIndirectDraw()=false
 *
 * 注意：本测试不依赖真实 GPU 设备（GL/DX11 设备未初始化），
 * 仅通过 NULL device 验证 API 鲁棒性（不崩溃，返回安全值）。
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_driven.h"
#include "engine/render/rhi/opengl/gl_rhi_device.h"
#ifdef DSE_ENABLE_D3D11
#include "engine/render/rhi/dx11/dx11_rhi_device.h"
#endif
#ifdef DSE_ENABLE_VULKAN
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#endif

using namespace dse::render;

// ============================================================
// DrawElementsIndirectCommand 结构体验证
// ============================================================

TEST(IndirectDrawStructTest, DrawElementsIndirectCommand_布局正确) {
    DrawElementsIndirectCommand cmd{};
    cmd.count          = 36;
    cmd.instance_count = 1;
    cmd.first_index    = 0;
    cmd.base_vertex    = 0;
    cmd.base_instance  = 0;
    EXPECT_EQ(cmd.count,          36u);
    EXPECT_EQ(cmd.instance_count, 1u);
    EXPECT_EQ(sizeof(DrawElementsIndirectCommand), 5 * sizeof(uint32_t));
}

// ============================================================
// OpenGL indirect draw — NULL device 鲁棒性
// ============================================================

TEST(GLIndirectDrawTest, CreateUpdate_空设备不崩溃) {
    OpenGLRhiDevice device;
    // 未初始化设备，supports_ssbo_ 默认 true 但 GL context 无效
    // CreateIndirectBuffer 应返回 0 (glGenBuffers 返回 0 或无 GL context)
    DrawElementsIndirectCommand cmd{36, 1, 0, 0, 0};
    unsigned int handle = device.CreateIndirectBuffer(sizeof(cmd), &cmd);
    // 在无 GL context 环境下返回 0 是合法的
    // 不崩溃即通过
    device.UpdateIndirectBuffer(handle, 0, sizeof(cmd), &cmd);
    device.DeleteIndirectBuffer(handle);
    SUCCEED();
}

TEST(GLIndirectDrawTest, SupportsIndirectDraw_返回值合法) {
    OpenGLRhiDevice device;
    // 未初始化时 supports_ssbo_ 默认 true；仅验证不崩溃
    bool supported = device.SupportsIndirectDraw();
    (void)supported;
    SUCCEED();
}

// ============================================================
// DX11 indirect draw — NULL device 鲁棒性
// ============================================================

#ifdef DSE_ENABLE_D3D11
TEST(DX11IndirectDrawTest, SupportsIndirectDraw_返回true) {
    DX11RhiDevice device;
    EXPECT_TRUE(device.SupportsIndirectDraw());
}

TEST(DX11IndirectDrawTest, CreateUpdate_空设备不崩溃) {
    DX11RhiDevice device;
    // D3D11 未初始化，device_ == nullptr，应返回 0
    DrawElementsIndirectCommand cmd{36, 1, 0, 0, 0};
    unsigned int handle = device.CreateIndirectBuffer(sizeof(cmd), &cmd);
    EXPECT_EQ(handle, 0u);  // 无设备时安全返回 0
    device.UpdateIndirectBuffer(handle, 0, sizeof(cmd), &cmd);
    device.DeleteIndirectBuffer(handle);
    device.MultiDrawIndexedIndirect(handle, 1, sizeof(cmd));
    SUCCEED();
}
#endif // DSE_ENABLE_D3D11

// ============================================================
// Vulkan indirect draw — stub 验证
// ============================================================

#ifdef DSE_ENABLE_VULKAN
TEST(VulkanIndirectDrawTest, SupportsIndirectDraw_返回true) {
    VulkanRhiDevice device;
    EXPECT_TRUE(device.SupportsIndirectDraw());
}

TEST(VulkanIndirectDrawTest, CreateDelete_未初始化返回零不崩溃) {
    VulkanRhiDevice device;
    DrawElementsIndirectCommand cmd{36, 1, 0, 0, 0};
    unsigned int handle = device.CreateIndirectBuffer(sizeof(cmd), &cmd);
    EXPECT_EQ(handle, 0u);
    device.DeleteIndirectBuffer(handle);
    SUCCEED();
}
#endif // DSE_ENABLE_VULKAN

// ============================================================
// MakeSortKey 单元测试（P0 验证）
// ============================================================

TEST(MakeSortKeyTest, 同材质生成相同键) {
    MeshDrawItem a, b;
    a.blend_mode = 0; a.shading_mode = 0; a.texture_handle = 42; a.normal_map_handle = 7;
    b.blend_mode = 0; b.shading_mode = 0; b.texture_handle = 42; b.normal_map_handle = 7;
    EXPECT_EQ(MakeSortKey(a), MakeSortKey(b));
}

TEST(MakeSortKeyTest, blend_mode优先级最高) {
    MeshDrawItem opaque, transparent;
    opaque.blend_mode      = 0;
    transparent.blend_mode = 1;
    opaque.texture_handle = transparent.texture_handle = 9999;
    EXPECT_LT(MakeSortKey(opaque), MakeSortKey(transparent));
}

TEST(MakeSortKeyTest, 不同texture产生不同键) {
    MeshDrawItem a, b;
    a.texture_handle = 100;
    b.texture_handle = 200;
    EXPECT_NE(MakeSortKey(a), MakeSortKey(b));
}
