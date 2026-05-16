/**
 * @file softbody_system_test.cpp
 * @brief SoftBodySystem PBD 软体模拟单元测试（无 GPU）
 *
 * 测试策略：
 * - SoftBodyComponent / SoftBodyDistConstraint 默认值
 * - 手动构建粒子数据 → Simulate → 验证运动方向
 * - 距离约束投影收敛性
 * - 体积保持约束
 * - 系统空 World 不崩溃
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/softbody/softbody_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include <glm/glm.hpp>
#include <cmath>

using namespace dse;
using namespace dse::gameplay3d;

// ============================================================
// SoftBodyDistConstraint 默认值
// ============================================================

TEST(SoftBodyDistConstraintTest, 默认值) {
    SoftBodyDistConstraint c;
    EXPECT_EQ(c.i0, 0u);
    EXPECT_EQ(c.i1, 0u);
    EXPECT_FLOAT_EQ(c.rest_length, 0.0f);
}

// ============================================================
// SoftBodyComponent 默认值
// ============================================================

TEST(SoftBodyComponentTest, 默认值) {
    SoftBodyComponent sb;
    EXPECT_TRUE(sb.enabled);
    EXPECT_FLOAT_EQ(sb.stiffness, 0.5f);
    EXPECT_EQ(sb.solver_iterations, 4);
    EXPECT_FLOAT_EQ(sb.damping, 0.99f);
    EXPECT_TRUE(sb.use_gravity);
    EXPECT_FLOAT_EQ(sb.gravity_scale, 1.0f);
    EXPECT_TRUE(sb.positions.empty());
    EXPECT_TRUE(sb.prev_positions.empty());
    EXPECT_TRUE(sb.velocities.empty());
    EXPECT_TRUE(sb.inv_masses.empty());
    EXPECT_TRUE(sb.constraints.empty());
    EXPECT_FLOAT_EQ(sb.rest_volume, 0.0f);
    EXPECT_FLOAT_EQ(sb.volume_stiffness, 0.5f);
    EXPECT_FALSE(sb.initialized);
    EXPECT_FALSE(sb.mesh_dirty);
}

// ============================================================
// SoftBodySystem 空 World
// ============================================================

TEST(SoftBodySystemTest, 默认构造安全) {
    SoftBodySystem sys;
    (void)sys;
}

TEST(SoftBodySystemTest, SetAssetManager_nullptr安全) {
    SoftBodySystem sys;
    sys.SetAssetManager(nullptr);
}

TEST(SoftBodySystemTest, 空World不崩溃) {
    SoftBodySystem sys;
    World world;
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

// ============================================================
// 手动粒子模拟测试
// ============================================================

namespace {

SoftBodyComponent MakeSimpleSoftBody() {
    SoftBodyComponent sb;
    sb.initialized = true;
    sb.enabled = true;
    sb.use_gravity = true;
    sb.gravity_scale = 1.0f;
    sb.stiffness = 0.8f;
    sb.solver_iterations = 4;
    sb.damping = 0.99f;

    // 两个粒子：A 在 (0,10,0)，B 在 (1,10,0)，抬高避免地面碰撞钳回
    sb.positions = { glm::vec3(0,10,0), glm::vec3(1,10,0) };
    sb.prev_positions = sb.positions;
    sb.velocities = { glm::vec3(0), glm::vec3(0) };
    sb.inv_masses = { 1.0f, 1.0f };

    // 一个距离约束
    SoftBodyDistConstraint c;
    c.i0 = 0; c.i1 = 1;
    c.rest_length = 1.0f;
    sb.constraints.push_back(c);
    sb.rest_volume = 0.0f;
    sb.volume_stiffness = 0.0f;
    return sb;
}

} // anonymous namespace

TEST(SoftBodySimulateTest, 重力使粒子下落) {
    // 直接测试 SoftBodyComponent 的模拟效果
    // 创建 World + SoftBody 实体
    World world;
    auto entity = world.registry().create();
    auto& sb = world.registry().emplace<SoftBodyComponent>(entity, MakeSimpleSoftBody());

    SoftBodySystem sys;
    float dt = 1.0f / 60.0f;
    sys.FixedUpdate(world, dt);

    auto& sb_after = world.registry().get<SoftBodyComponent>(entity);
    // 两个粒子应因重力而 Y 减小
    EXPECT_LT(sb_after.positions[0].y, 10.0f);
    EXPECT_LT(sb_after.positions[1].y, 10.0f);
}

TEST(SoftBodySimulateTest, 固定点不移动) {
    World world;
    auto entity = world.registry().create();
    auto sb = MakeSimpleSoftBody();
    sb.inv_masses[0] = 0.0f; // 固定点 A
    world.registry().emplace<SoftBodyComponent>(entity, sb);

    SoftBodySystem sys;
    sys.FixedUpdate(world, 1.0f / 60.0f);

    auto& sb_after = world.registry().get<SoftBodyComponent>(entity);
    EXPECT_FLOAT_EQ(sb_after.positions[0].x, 0.0f);
    EXPECT_FLOAT_EQ(sb_after.positions[0].y, 10.0f);
    EXPECT_FLOAT_EQ(sb_after.positions[0].z, 0.0f);
    // B 应下落
    EXPECT_LT(sb_after.positions[1].y, 10.0f);
}

TEST(SoftBodySimulateTest, 禁用不模拟) {
    World world;
    auto entity = world.registry().create();
    auto sb = MakeSimpleSoftBody();
    sb.enabled = false;
    world.registry().emplace<SoftBodyComponent>(entity, sb);

    SoftBodySystem sys;
    sys.FixedUpdate(world, 1.0f / 60.0f);

    auto& sb_after = world.registry().get<SoftBodyComponent>(entity);
    EXPECT_FLOAT_EQ(sb_after.positions[0].y, 10.0f);
    EXPECT_FLOAT_EQ(sb_after.positions[1].y, 10.0f);
}

TEST(SoftBodySimulateTest, 零dt不移动) {
    World world;
    auto entity = world.registry().create();
    world.registry().emplace<SoftBodyComponent>(entity, MakeSimpleSoftBody());

    SoftBodySystem sys;
    sys.FixedUpdate(world, 0.0f);

    auto& sb_after = world.registry().get<SoftBodyComponent>(entity);
    EXPECT_FLOAT_EQ(sb_after.positions[0].y, 10.0f);
}

TEST(SoftBodySimulateTest, 距离约束保持间距) {
    World world;
    auto entity = world.registry().create();
    auto sb = MakeSimpleSoftBody();
    sb.use_gravity = false; // 关闭重力，只测约束
    // 将 B 拉远到 (2,0,0)
    sb.positions[1] = glm::vec3(2.0f, 0.0f, 0.0f);
    sb.prev_positions[1] = glm::vec3(2.0f, 0.0f, 0.0f);
    world.registry().emplace<SoftBodyComponent>(entity, sb);

    SoftBodySystem sys;
    // 多次迭代让约束收敛
    for (int i = 0; i < 30; ++i) {
        sys.FixedUpdate(world, 1.0f / 60.0f);
    }

    auto& sb_after = world.registry().get<SoftBodyComponent>(entity);
    float dist = glm::length(sb_after.positions[1] - sb_after.positions[0]);
    EXPECT_NEAR(dist, 1.0f, 0.15f); // 允许一定误差
}

TEST(SoftBodySimulateTest, mesh_dirty标记) {
    World world;
    auto entity = world.registry().create();
    world.registry().emplace<SoftBodyComponent>(entity, MakeSimpleSoftBody());

    SoftBodySystem sys;
    sys.FixedUpdate(world, 1.0f / 60.0f);

    auto& sb_after = world.registry().get<SoftBodyComponent>(entity);
    EXPECT_TRUE(sb_after.mesh_dirty);
}
