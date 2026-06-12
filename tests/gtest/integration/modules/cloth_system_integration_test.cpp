/**
 * @file cloth_system_integration_test.cpp
 * @brief 布料模拟系统集成测试
 *
 * 验证场景：
 * - ClothSystem 从 MeshRendererComponent 初始化布料粒子
 * - XPBD 距离约束保持布料不拉伸
 * - 重力作用下布料下落
 * - 固定顶点保持不动
 * - 球体碰撞约束
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_cloth.h"
#include "modules/gameplay_3d/cloth/cloth_system.h"

using namespace dse;
using namespace dse::gameplay3d;

class ClothSystemIntegrationTest : public ::testing::Test {
protected:
    World world;
    ClothSystem system;

    /// 创建 4x4 方形布料网格（16个顶点，18个三角形）
    entt::entity CreateClothEntity(bool pin_top_row = true) {
        auto e = world.CreateEntity();

        TransformComponent tc;
        tc.position = glm::vec3(0.0f, 10.0f, 0.0f);
        world.registry().emplace<TransformComponent>(e, tc);

        // 4x4 网格
        MeshRendererComponent mr;
        mr.mesh_path = "cloth_quad.obj";
        mr.visible = true;
        mr.dmesh_vertex_stride = 3;

        // 16 个顶点: 4行 x 4列，间距 1.0
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                mr.temp_vertices.push_back(static_cast<float>(col)); // x
                mr.temp_vertices.push_back(0.0f);                    // y
                mr.temp_vertices.push_back(static_cast<float>(row)); // z
            }
        }

        // 三角形索引（3x3 quads = 18 triangles）
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                unsigned short tl = static_cast<unsigned short>(row * 4 + col);
                unsigned short tr = tl + 1;
                unsigned short bl = tl + 4;
                unsigned short br = bl + 1;
                mr.temp_indices.push_back(tl);
                mr.temp_indices.push_back(bl);
                mr.temp_indices.push_back(tr);
                mr.temp_indices.push_back(tr);
                mr.temp_indices.push_back(bl);
                mr.temp_indices.push_back(br);
            }
        }
        world.registry().emplace<MeshRendererComponent>(e, mr);

        ClothComponent cloth;
        cloth.enabled = true;
        cloth.solver_iterations = 4;
        cloth.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        cloth.damping = 0.01f;
        cloth.stiffness = 1.0f;

        if (pin_top_row) {
            // 固定顶部一行（索引 0,1,2,3）
            cloth.pinned_vertices = {0, 1, 2, 3};
        }

        world.registry().emplace<ClothComponent>(e, cloth);
        return e;
    }
};

TEST_F(ClothSystemIntegrationTest, InitializeCorrectsetUp) {
    auto e = CreateClothEntity();
    // FixedUpdate 会触发初始化
    system.FixedUpdate(world, 1.0f / 60.0f);

    auto& cloth = world.registry().get<ClothComponent>(e);
    EXPECT_TRUE(cloth.initialized);
    EXPECT_EQ(cloth.particle_count, 16u);
    EXPECT_EQ(cloth.positions.size(), 16u);
    EXPECT_EQ(cloth.velocities.size(), 16u);
    EXPECT_EQ(cloth.inv_masses.size(), 16u);
}

TEST_F(ClothSystemIntegrationTest, Initialize) {
    auto e = CreateClothEntity();
    system.FixedUpdate(world, 1.0f / 60.0f);

    auto& cloth = world.registry().get<ClothComponent>(e);
    // 4x4 网格应有一定数量的距离约束（边数）
    EXPECT_GT(cloth.distance_constraints.size(), 0u);
    // 每个约束的 rest_length 应 > 0
    for (const auto& c : cloth.distance_constraints) {
        EXPECT_GT(c.rest_length, 0.0f);
        EXPECT_NE(c.i, c.j);
    }
}

TEST_F(ClothSystemIntegrationTest, PointisZero) {
    auto e = CreateClothEntity(true);
    system.FixedUpdate(world, 1.0f / 60.0f);

    auto& cloth = world.registry().get<ClothComponent>(e);
    // 固定顶点（0,1,2,3）的逆质量应为 0
    EXPECT_FLOAT_EQ(cloth.inv_masses[0], 0.0f);
    EXPECT_FLOAT_EQ(cloth.inv_masses[1], 0.0f);
    EXPECT_FLOAT_EQ(cloth.inv_masses[2], 0.0f);
    EXPECT_FLOAT_EQ(cloth.inv_masses[3], 0.0f);
    // 非固定顶点逆质量应 > 0
    EXPECT_GT(cloth.inv_masses[4], 0.0f);
    EXPECT_GT(cloth.inv_masses[15], 0.0f);
}

TEST_F(ClothSystemIntegrationTest, MakeNon) {
    auto e = CreateClothEntity(true);
    float dt = 1.0f / 60.0f;
    // 初始化
    system.FixedUpdate(world, dt);
    auto& cloth = world.registry().get<ClothComponent>(e);

    // 记录底部粒子初始 Y
    float initial_y_12 = cloth.positions[12].y; // 底部行第一个

    // 模拟多步
    for (int i = 0; i < 30; ++i) {
        system.FixedUpdate(world, dt);
    }

    // 底部粒子应该下落
    EXPECT_LT(cloth.positions[12].y, initial_y_12)
        << "重力应使非固定粒子向下移动";

    // 固定粒子应保持不动（world space = transform.y + rest.y）
    float expected_pinned_y = world.registry().get<TransformComponent>(e).position.y
                            + cloth.rest_positions[0].y;
    EXPECT_FLOAT_EQ(cloth.positions[0].y, expected_pinned_y)
        << "固定粒子 Y 不应改变";
}

TEST_F(ClothSystemIntegrationTest, WithoutpointAll) {
    auto e = CreateClothEntity(false); // 不固定任何点
    float dt = 1.0f / 60.0f;
    system.FixedUpdate(world, dt);
    auto& cloth = world.registry().get<ClothComponent>(e);

    float initial_avg_y = 0.0f;
    for (const auto& p : cloth.positions) {
        initial_avg_y += p.y;
    }
    initial_avg_y /= static_cast<float>(cloth.positions.size());

    for (int i = 0; i < 10; ++i) {
        system.FixedUpdate(world, dt);
    }

    float final_avg_y = 0.0f;
    for (const auto& p : cloth.positions) {
        final_avg_y += p.y;
    }
    final_avg_y /= static_cast<float>(cloth.positions.size());

    EXPECT_LT(final_avg_y, initial_avg_y) << "所有粒子应该自由下落";
}

TEST_F(ClothSystemIntegrationTest, TestCase6) {
    auto e = CreateClothEntity(true);
    float dt = 1.0f / 60.0f;
    system.FixedUpdate(world, dt);
    auto& cloth = world.registry().get<ClothComponent>(e);

    // 模拟多步
    for (int i = 0; i < 60; ++i) {
        system.FixedUpdate(world, dt);
    }

    // 检查所有距离约束是否近似满足
    float max_stretch_ratio = 0.0f;
    for (const auto& c : cloth.distance_constraints) {
        float dist = glm::distance(cloth.positions[c.i], cloth.positions[c.j]);
        float ratio = dist / c.rest_length;
        max_stretch_ratio = std::max(max_stretch_ratio, ratio);
    }
    // XPBD 约束应限制拉伸比不超过 1.5（允许一定松弛）
    EXPECT_LT(max_stretch_ratio, 1.5f)
        << "距离约束应限制过度拉伸";
}

TEST_F(ClothSystemIntegrationTest, By) {
    auto e = CreateClothEntity(true);
    system.FixedUpdate(world, 1.0f / 60.0f);
    auto& cloth = world.registry().get<ClothComponent>(e);

    ASSERT_EQ(cloth.normals.size(), cloth.particle_count);
    // 至少有部分法线不为零
    bool has_nonzero = false;
    for (const auto& n : cloth.normals) {
        if (glm::length(n) > 0.001f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "初始化后应有非零法线";
}

TEST_F(ClothSystemIntegrationTest, mesh_Dirtymark) {
    auto e = CreateClothEntity(true);
    system.FixedUpdate(world, 1.0f / 60.0f);
    auto& cloth = world.registry().get<ClothComponent>(e);
    EXPECT_TRUE(cloth.mesh_dirty) << "模拟后应标记 mesh_dirty";
}

TEST_F(ClothSystemIntegrationTest, DisabledNot) {
    auto e = CreateClothEntity(true);
    auto& cloth = world.registry().get<ClothComponent>(e);
    cloth.enabled = false;
    system.FixedUpdate(world, 1.0f / 60.0f);
    EXPECT_FALSE(cloth.initialized) << "禁用时不应初始化";
    EXPECT_EQ(cloth.particle_count, 0u);
}
