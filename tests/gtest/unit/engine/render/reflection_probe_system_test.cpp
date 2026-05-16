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

TEST(ReflectionProbeComponentTest, 默认值) {
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

TEST(ReflectionProbeSystemTest, 默认未初始化) {
    ReflectionProbeSystem sys;
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
    EXPECT_FALSE(sys.IsIBLAvailable());
}

TEST(ReflectionProbeSystemTest, Init_nullptr安全) {
    ReflectionProbeSystem sys;
    sys.Init(nullptr);
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
    EXPECT_FALSE(sys.IsIBLAvailable());
}

TEST(ReflectionProbeSystemTest, Shutdown未初始化安全) {
    ReflectionProbeSystem sys;
    sys.Shutdown(nullptr);
    EXPECT_EQ(sys.brdf_lut_handle(), 0u);
}

TEST(ReflectionProbeSystemTest, 重复Shutdown安全) {
    ReflectionProbeSystem sys;
    sys.Shutdown(nullptr);
    sys.Shutdown(nullptr);
}

TEST(ReflectionProbeSystemTest, 未初始化时IBL不可用) {
    ReflectionProbeSystem sys;
    EXPECT_FALSE(sys.IsIBLAvailable());
}

// ============================================================
// ReflectionProbeComponent 手动修改
// ============================================================

TEST(ReflectionProbeComponentTest, 修改resolution) {
    dse::ReflectionProbeComponent comp;
    comp.resolution = 256;
    EXPECT_EQ(comp.resolution, 256);
}

TEST(ReflectionProbeComponentTest, 修改cubemap_handle) {
    dse::ReflectionProbeComponent comp;
    comp.cubemap_handle = 42;
    comp.needs_rebake = false;
    EXPECT_EQ(comp.cubemap_handle, 42u);
    EXPECT_FALSE(comp.needs_rebake);
}

TEST(ReflectionProbeComponentTest, BoxProjection参数) {
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
