/**
 * @file reflection_probe_system_test.cpp
 * @brief ReflectionProbeSystem 单元测试（CPU 端 BRDF/IBL 辅助 + 系统状态）
 *
 * 测试策略：
 * - ReflectionProbeComponent ECS 默认值
 * - ReflectionProbeSystem 生命周期（无 GPU）
 * - ProbeEntry / IBL 可用性查询
 */

#include <gtest/gtest.h>
#include "engine/render/reflection_probe_system.h"
#include "engine/ecs/components_3d.h"
#include <glm/glm.hpp>
#include <array>
#include <vector>

using namespace dse::render;

// ============================================================
// ReflectionProbeComponent ECS 默认值
// ============================================================

// 测试 反射探针组件：默认值
TEST(ReflectionProbeComponentTest, DefaultValues) {
    dse::ReflectionProbeComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_FLOAT_EQ(comp.influence_radius, 15.0f);
    EXPECT_FLOAT_EQ(comp.box_size_x, 10.0f);
    EXPECT_FLOAT_EQ(comp.box_size_y, 10.0f);
    EXPECT_FLOAT_EQ(comp.box_size_z, 10.0f);
    EXPECT_FALSE(comp.use_box_projection);
    EXPECT_EQ(comp.resolution, 128);
    EXPECT_EQ(comp.cubemap_handle, 0u);
    EXPECT_TRUE(comp.needs_rebake);
    EXPECT_TRUE(comp.show_debug);
}

// ============================================================
// ReflectionProbeSystem 生命周期
// ============================================================

// 测试 反射探针系统：默认未初始化
TEST(ReflectionProbeSystemTest, DefaultUninitialized) {
    ReflectionProbeSystem sys;
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
    EXPECT_FALSE(sys.IsIBLAvailable());
}

// 测试 反射探针系统：初始化空指针安全
TEST(ReflectionProbeSystemTest, Init_NullptrSafety) {
    ReflectionProbeSystem sys;
    sys.Init(nullptr);
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
    EXPECT_FALSE(sys.IsIBLAvailable());
}

// 测试 反射探针系统：关闭未初始化安全
TEST(ReflectionProbeSystemTest, ShutdownUninitializedSecurity) {
    ReflectionProbeSystem sys;
    sys.Shutdown(nullptr);
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
}

// 测试 反射探针系统：关闭安全
TEST(ReflectionProbeSystemTest, ShutdownSafety) {
    ReflectionProbeSystem sys;
    sys.Shutdown(nullptr);
    sys.Shutdown(nullptr);
}

// 测试 反射探针系统：当不已初始化IBL不能够
TEST(ReflectionProbeSystemTest, WhenNotInitializedIBLNotCan) {
    ReflectionProbeSystem sys;
    EXPECT_FALSE(sys.IsIBLAvailable());
}

// ============================================================
// ReflectionProbeComponent 手动修改
// ============================================================

// 测试 反射探针组件：Reviseresolution
TEST(ReflectionProbeComponentTest, Reviseresolution) {
    dse::ReflectionProbeComponent comp;
    comp.resolution = 256;
    EXPECT_EQ(comp.resolution, 256);
}

// 测试 反射探针组件：Revisecubemap句柄
TEST(ReflectionProbeComponentTest, Revisecubemap_handle) {
    dse::ReflectionProbeComponent comp;
    comp.cubemap_handle = 42;
    comp.needs_rebake = false;
    EXPECT_EQ(comp.cubemap_handle, 42u);
    EXPECT_FALSE(comp.needs_rebake);
}

// 测试 反射探针组件：盒投影参数
TEST(ReflectionProbeComponentTest, BoxProjectionParameters) {
    dse::ReflectionProbeComponent comp;
    comp.use_box_projection = true;
    comp.box_size_x = 20.0f;
    comp.box_size_y = 5.0f;
    comp.box_size_z = 30.0f;
    EXPECT_TRUE(comp.use_box_projection);
    EXPECT_FLOAT_EQ(comp.box_size_x, 20.0f);
    EXPECT_FLOAT_EQ(comp.box_size_y, 5.0f);
    EXPECT_FLOAT_EQ(comp.box_size_z, 30.0f);
}

// ============================================================
// A3 IBL 预滤波环境贴图（CPU 端，无 GPU）：mip 链结构 + 能量守恒
// 运行时 PBR 以 textureLod(roughness*MAX_LOD) 采样预滤波 cubemap，故必须生成完整
// mip 链（设计 §3 A3，立方体 mip 采样 ES3.0/WebGL2 原生支持）。
// ============================================================

namespace {
// 6 面常量色 base cubemap
std::array<std::vector<unsigned char>, 6> MakeConstantCube(int res, unsigned char r,
                                                           unsigned char g, unsigned char b) {
    std::array<std::vector<unsigned char>, 6> faces;
    for (int f = 0; f < 6; ++f) {
        faces[f].resize(static_cast<size_t>(res) * res * 4);
        for (int i = 0; i < res * res; ++i) {
            faces[f][i * 4 + 0] = r;
            faces[f][i * 4 + 1] = g;
            faces[f][i * 4 + 2] = b;
            faces[f][i * 4 + 3] = 255;
        }
    }
    return faces;
}
}  // namespace

// 预滤波生成完整 mip 链（floor(log2(res))+1 级），各级分辨率逐级减半。
TEST(ReflectionProbeIBLTest, PrefilterProducesFullMipChain) {
    const int res = 16;
    auto faces = MakeConstantCube(res, 100, 150, 200);
    const unsigned char* ptrs[6];
    for (int f = 0; f < 6; ++f) ptrs[f] = faces[f].data();

    auto data = ReflectionProbeSystem::ComputePrefilteredCube(ptrs, res);
    EXPECT_EQ(data.base_resolution, res);
    EXPECT_EQ(data.num_mips, 5);  // floor(log2(16))+1
    ASSERT_EQ(static_cast<int>(data.mips.size()), data.num_mips);
    for (int mip = 0; mip < data.num_mips; ++mip) {
        const int mres = std::max(1, res >> mip);
        for (int f = 0; f < 6; ++f) {
            EXPECT_EQ(data.mips[mip][f].size(), static_cast<size_t>(mres) * mres * 4) << "mip " << mip;
        }
    }
}

// mip0 为锐利 base：逐字节等于输入（roughness 0 不卷积）。
TEST(ReflectionProbeIBLTest, PrefilterMip0EqualsBase) {
    const int res = 8;
    auto faces = MakeConstantCube(res, 12, 34, 56);
    const unsigned char* ptrs[6];
    for (int f = 0; f < 6; ++f) ptrs[f] = faces[f].data();

    auto data = ReflectionProbeSystem::ComputePrefilteredCube(ptrs, res);
    ASSERT_GE(data.num_mips, 1);
    for (int f = 0; f < 6; ++f) {
        EXPECT_EQ(data.mips[0][f], faces[f]) << "face " << f;
    }
}

// 常量环境的预滤波结果应能量守恒（每个 mip 仍≈原常量色），绝不退化为黑色。
// 这正是没有 mip 链时 textureLod>0 采样会失败/变黑的回归守护。
TEST(ReflectionProbeIBLTest, PrefilterPreservesConstantEnergy) {
    const int res = 16;
    auto faces = MakeConstantCube(res, 100, 150, 200);
    const unsigned char* ptrs[6];
    for (int f = 0; f < 6; ++f) ptrs[f] = faces[f].data();

    auto data = ReflectionProbeSystem::ComputePrefilteredCube(ptrs, res);
    for (int mip = 1; mip < data.num_mips; ++mip) {
        const int mres = std::max(1, res >> mip);
        for (int f = 0; f < 6; ++f) {
            const auto& buf = data.mips[mip][f];
            for (int i = 0; i < mres * mres; ++i) {
                EXPECT_NEAR(buf[i * 4 + 0], 100, 6) << "mip " << mip << " face " << f;
                EXPECT_NEAR(buf[i * 4 + 1], 150, 6) << "mip " << mip << " face " << f;
                EXPECT_NEAR(buf[i * 4 + 2], 200, 6) << "mip " << mip << " face " << f;
                EXPECT_EQ(buf[i * 4 + 3], 255);
            }
        }
    }
}

// 退化输入安全：res<=0 返回空。
TEST(ReflectionProbeIBLTest, PrefilterHandlesInvalidResolution) {
    auto data = ReflectionProbeSystem::ComputePrefilteredCube(nullptr, 0);
    EXPECT_EQ(data.num_mips, 0);
    EXPECT_TRUE(data.mips.empty());
}
