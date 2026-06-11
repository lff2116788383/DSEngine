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

using namespace dse::render;

// ============================================================
// ReflectionProbeComponent ECS 默认值
// ============================================================

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

TEST(ReflectionProbeSystemTest, DefaultUninitialized) {
    ReflectionProbeSystem sys;
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
    EXPECT_FALSE(sys.IsIBLAvailable());
}

TEST(ReflectionProbeSystemTest, Init_NullptrSafety) {
    ReflectionProbeSystem sys;
    sys.Init(nullptr);
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
    EXPECT_FALSE(sys.IsIBLAvailable());
}

TEST(ReflectionProbeSystemTest, ShutdownUninitializedSecurity) {
    ReflectionProbeSystem sys;
    sys.Shutdown(nullptr);
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
}

TEST(ReflectionProbeSystemTest, ShutdownSafety) {
    ReflectionProbeSystem sys;
    sys.Shutdown(nullptr);
    sys.Shutdown(nullptr);
}

TEST(ReflectionProbeSystemTest, WhenNotInitializedIBLNotCan) {
    ReflectionProbeSystem sys;
    EXPECT_FALSE(sys.IsIBLAvailable());
}

// ============================================================
// ReflectionProbeComponent 手动修改
// ============================================================

TEST(ReflectionProbeComponentTest, Reviseresolution) {
    dse::ReflectionProbeComponent comp;
    comp.resolution = 256;
    EXPECT_EQ(comp.resolution, 256);
}

TEST(ReflectionProbeComponentTest, Revisecubemap_handle) {
    dse::ReflectionProbeComponent comp;
    comp.cubemap_handle = 42;
    comp.needs_rebake = false;
    EXPECT_EQ(comp.cubemap_handle, 42u);
    EXPECT_FALSE(comp.needs_rebake);
}

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
