/**
 * @file vehicle_system_test.cpp
 * @brief VehicleSystem / VehicleComponent 单元测试（纯 CPU 端）
 *
 * 测试策略：
 * - VehicleWheelConfig / VehicleWheelState 默认值
 * - VehicleComponent 默认值和输入范围
 * - VehicleSystem 构造安全
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/vehicle/vehicle_system.h"
#include "engine/ecs/components_3d_physics.h"
#include <glm/glm.hpp>

using namespace dse;
using namespace dse::gameplay3d;

// ============================================================
// VehicleWheelConfig 默认值
// ============================================================

#ifdef DSE_ENABLE_PHYSX

TEST(VehicleWheelConfigTest, 默认值) {
    VehicleWheelConfig cfg;
    EXPECT_FLOAT_EQ(cfg.position.x, 0.0f);
    EXPECT_FLOAT_EQ(cfg.position.y, 0.0f);
    EXPECT_FLOAT_EQ(cfg.position.z, 0.0f);
    EXPECT_FLOAT_EQ(cfg.radius, 0.3f);
    EXPECT_FLOAT_EQ(cfg.suspension_rest_length, 0.3f);
    EXPECT_FLOAT_EQ(cfg.suspension_stiffness, 30000.0f);
    EXPECT_FLOAT_EQ(cfg.suspension_damping, 4500.0f);
    EXPECT_FLOAT_EQ(cfg.friction, 1.5f);
    EXPECT_TRUE(cfg.is_drive_wheel);
    EXPECT_FALSE(cfg.is_steer_wheel);
}

// ============================================================
// VehicleWheelState 默认值
// ============================================================

TEST(VehicleWheelStateTest, 默认值) {
    VehicleWheelState ws;
    EXPECT_FLOAT_EQ(ws.compression, 0.0f);
    EXPECT_FLOAT_EQ(ws.angular_velocity, 0.0f);
    EXPECT_FLOAT_EQ(ws.rotation, 0.0f);
    EXPECT_FLOAT_EQ(ws.steer_angle, 0.0f);
    EXPECT_FALSE(ws.grounded);
    EXPECT_FLOAT_EQ(ws.contact_normal.y, 1.0f);
}

// ============================================================
// VehicleComponent 默认值
// ============================================================

TEST(VehicleComponentTest, 默认值) {
    VehicleComponent vc;
    EXPECT_TRUE(vc.enabled);
    EXPECT_FLOAT_EQ(vc.max_engine_force, 5000.0f);
    EXPECT_FLOAT_EQ(vc.max_brake_force, 3000.0f);
    EXPECT_FLOAT_EQ(vc.max_steer_angle, 35.0f);
    EXPECT_FLOAT_EQ(vc.throttle, 0.0f);
    EXPECT_FLOAT_EQ(vc.brake, 0.0f);
    EXPECT_FLOAT_EQ(vc.steering, 0.0f);
    EXPECT_TRUE(vc.wheels.empty());
    EXPECT_TRUE(vc.wheel_states.empty());
    EXPECT_FLOAT_EQ(vc.current_speed, 0.0f);
    EXPECT_FALSE(vc.initialized);
}

TEST(VehicleComponentTest, 添加车轮) {
    VehicleComponent vc;
    VehicleWheelConfig w;
    w.position = glm::vec3(-0.8f, 0.0f, 1.2f);
    w.is_steer_wheel = true;
    vc.wheels.push_back(w);
    EXPECT_EQ(vc.wheels.size(), 1u);
    EXPECT_TRUE(vc.wheels[0].is_steer_wheel);
}

#endif // DSE_ENABLE_PHYSX

// ============================================================
// VehicleSystem 构造安全（无条件编译保护）
// ============================================================

TEST(VehicleSystemBasicTest, 默认构造) {
    VehicleSystem sys;
    (void)sys;
    SUCCEED();
}

TEST(VehicleSystemBasicTest, SetPhysics3D_nullptr安全) {
    VehicleSystem sys;
    sys.SetPhysics3D(nullptr);
    SUCCEED();
}
