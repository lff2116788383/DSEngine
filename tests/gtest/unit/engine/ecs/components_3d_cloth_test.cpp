/**
 * @file components_3d_cloth_test.cpp
 * @brief 布料模拟组件默认值与字段修改单元测试
 */

#include <gtest/gtest.h>
#include "engine/ecs/components_3d_cloth.h"

using namespace dse;

// ─── ClothDistanceConstraint ───────────────────────────────────────────

TEST(ClothDistanceConstraintTest, 默认值) {
    ClothDistanceConstraint c;
    EXPECT_EQ(c.i, 0u);
    EXPECT_EQ(c.j, 0u);
    EXPECT_FLOAT_EQ(c.rest_length, 0.0f);
}

TEST(ClothDistanceConstraintTest, 字段修改) {
    ClothDistanceConstraint c;
    c.i = 5;
    c.j = 12;
    c.rest_length = 0.35f;
    EXPECT_EQ(c.i, 5u);
    EXPECT_EQ(c.j, 12u);
    EXPECT_FLOAT_EQ(c.rest_length, 0.35f);
}

// ─── ClothBendConstraint ───────────────────────────────────────────────

TEST(ClothBendConstraintTest, 默认值) {
    ClothBendConstraint c;
    EXPECT_EQ(c.i0, 0u);
    EXPECT_EQ(c.i1, 0u);
    EXPECT_EQ(c.i2, 0u);
    EXPECT_EQ(c.i3, 0u);
    EXPECT_FLOAT_EQ(c.rest_angle, 0.0f);
}

// ─── ClothSphereCollider ───────────────────────────────────────────────

TEST(ClothSphereColliderTest, 默认值) {
    ClothSphereCollider col;
    EXPECT_EQ(col.entity_id, UINT32_MAX);
    EXPECT_FLOAT_EQ(col.radius, 0.5f);
}

// ─── ClothCapsuleCollider ──────────────────────────────────────────────

TEST(ClothCapsuleColliderTest, 默认值) {
    ClothCapsuleCollider col;
    EXPECT_EQ(col.entity_id, UINT32_MAX);
    EXPECT_FLOAT_EQ(col.radius, 0.3f);
    EXPECT_FLOAT_EQ(col.half_height, 0.5f);
}

// ─── ClothComponent ───────────────────────────────────────────────────

TEST(ClothComponentTest, 默认配置值) {
    ClothComponent cloth;
    EXPECT_TRUE(cloth.enabled);
    EXPECT_TRUE(cloth.source_mesh_path.empty());
    EXPECT_EQ(cloth.solver_iterations, 8u);
    EXPECT_FLOAT_EQ(cloth.damping, 0.01f);
    EXPECT_FLOAT_EQ(cloth.stiffness, 1.0f);
    EXPECT_FLOAT_EQ(cloth.bend_stiffness, 0.5f);
    EXPECT_FLOAT_EQ(cloth.friction, 0.3f);
}

TEST(ClothComponentTest, 默认外力) {
    ClothComponent cloth;
    EXPECT_FLOAT_EQ(cloth.gravity.x, 0.0f);
    EXPECT_FLOAT_EQ(cloth.gravity.y, -9.81f);
    EXPECT_FLOAT_EQ(cloth.gravity.z, 0.0f);
    EXPECT_FLOAT_EQ(cloth.wind.x, 0.0f);
    EXPECT_FLOAT_EQ(cloth.wind_turbulence, 0.0f);
}

TEST(ClothComponentTest, 默认碰撞) {
    ClothComponent cloth;
    EXPECT_FLOAT_EQ(cloth.collision_radius, 0.02f);
    EXPECT_TRUE(cloth.sphere_colliders.empty());
    EXPECT_TRUE(cloth.capsule_colliders.empty());
    EXPECT_TRUE(cloth.pinned_vertices.empty());
}

TEST(ClothComponentTest, 默认运行时状态) {
    ClothComponent cloth;
    EXPECT_FALSE(cloth.initialized);
    EXPECT_EQ(cloth.particle_count, 0u);
    EXPECT_TRUE(cloth.positions.empty());
    EXPECT_TRUE(cloth.prev_positions.empty());
    EXPECT_TRUE(cloth.velocities.empty());
    EXPECT_TRUE(cloth.inv_masses.empty());
    EXPECT_TRUE(cloth.rest_positions.empty());
    EXPECT_TRUE(cloth.distance_constraints.empty());
    EXPECT_TRUE(cloth.bend_constraints.empty());
    EXPECT_TRUE(cloth.triangle_indices.empty());
    EXPECT_TRUE(cloth.normals.empty());
    EXPECT_TRUE(cloth.uvs.empty());
    EXPECT_FALSE(cloth.mesh_dirty);
}

TEST(ClothComponentTest, 添加碰撞体) {
    ClothComponent cloth;
    ClothSphereCollider sc;
    sc.radius = 1.0f;
    cloth.sphere_colliders.push_back(sc);
    ClothCapsuleCollider cc;
    cc.radius = 0.5f;
    cc.half_height = 1.0f;
    cloth.capsule_colliders.push_back(cc);
    EXPECT_EQ(cloth.sphere_colliders.size(), 1u);
    EXPECT_EQ(cloth.capsule_colliders.size(), 1u);
    EXPECT_FLOAT_EQ(cloth.capsule_colliders[0].half_height, 1.0f);
}

TEST(ClothComponentTest, 固定顶点设置) {
    ClothComponent cloth;
    cloth.pinned_vertices = {0, 1, 10, 11};
    EXPECT_EQ(cloth.pinned_vertices.size(), 4u);
    EXPECT_EQ(cloth.pinned_vertices[2], 10u);
}
