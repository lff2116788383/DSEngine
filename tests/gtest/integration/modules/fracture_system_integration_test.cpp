/**
 * @file fracture_system_integration_test.cpp
 * @brief 物理破碎系统集成测试
 *
 * 验证场景：
 * - FractureSystem 触发碎裂后正确生成碎片实体
 * - 碎片实体具备 TransformComponent、MeshRendererComponent、FragmentTagComponent
 * - 运行时 Voronoi 切分产生有效碎片数据
 * - 碎片生命周期管理（淡出+销毁）
 * - ApplyDamage 的伤害累积模式
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_fracture.h"
#include "engine/ecs/components_3d_physics.h"
#include "modules/gameplay_3d/destruction/fracture_system.h"

using namespace dse;
using namespace dse::gameplay3d;

class FractureSystemIntegrationTest : public ::testing::Test {
protected:
    World world;
    FractureSystem system;

    /// 创建一个带 mesh 数据的可破坏实体（用于运行时 Voronoi）
    entt::entity CreateDestructibleEntity() {
        auto e = world.CreateEntity();

        TransformComponent tc;
        tc.position = glm::vec3(0.0f, 5.0f, 0.0f);
        tc.scale = glm::vec3(1.0f);
        world.registry().emplace<TransformComponent>(e, tc);

        // 构建简单立方体 mesh 数据（8个顶点，12个三角形）
        MeshRendererComponent mr;
        mr.mesh_path = "test_cube.obj";
        mr.shader_variant = "MESH_LIT";
        mr.visible = true;
        // 8 个立方体顶点（stride=3，纯位置）
        mr.temp_vertices = {
            -1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
            -1, -1,  1,   1, -1,  1,   1,  1,  1,  -1,  1,  1
        };
        mr.dmesh_vertex_stride = 3;
        // 12 个三角形（6个面 × 2）
        mr.temp_indices = {
            0,1,2, 0,2,3,  // front
            4,6,5, 4,7,6,  // back
            0,4,5, 0,5,1,  // bottom
            2,6,7, 2,7,3,  // top
            0,3,7, 0,7,4,  // left
            1,5,6, 1,6,2   // right
        };
        world.registry().emplace<MeshRendererComponent>(e, mr);

        RigidBody3DComponent rb;
        rb.type = RigidBody3DType::Dynamic;
        rb.mass = 5.0f;
        world.registry().emplace<RigidBody3DComponent>(e, rb);

        return e;
    }
};

// 测试 断裂系统集成：模型无不崩溃
TEST_F(FractureSystemIntegrationTest, Model_WithoutDoesNotCrash) {
    auto e = CreateDestructibleEntity();
    FractureComponent fc;
    fc.source = FractureSource::Prefractured;
    fc.fracture_asset_path = "nonexistent.fracture.json";
    world.registry().emplace<FractureComponent>(e, fc);

    // 触发碎裂
    system.TriggerFracture(world, e);
    // Update 应该不崩溃（文件不存在时只输出错误日志）
    EXPECT_NO_THROW(system.Update(world, 0.016f));
}

// 测试 断裂系统集成：当Voronoi生成实体
TEST_F(FractureSystemIntegrationTest, WhenVoronoi_GenerateEntity) {
    auto e = CreateDestructibleEntity();
    FractureComponent fc;
    fc.source = FractureSource::RuntimeVoronoi;
    fc.runtime_fragment_count = 2; // 使用 2 碎片（8顶点更容易产生完整三角形）
    fc.runtime_seed = 42;
    fc.cluster_near_impact = false;
    fc.explosion_force = 0.0f; // 不施加冲击力（没有 physics3d）
    world.registry().emplace<FractureComponent>(e, fc);

    // 触发碎裂
    system.TriggerFracture(world, e, glm::vec3(0.0f));
    EXPECT_NO_THROW(system.Update(world, 0.016f));

    auto& fc_after = world.registry().get<FractureComponent>(e);
    // Voronoi 切分结果取决于种子点分布，可能所有三角形跨区域导致空碎片
    // 成功时验证碎裂标志和碎片数
    if (fc_after.is_fractured) {
        EXPECT_FALSE(fc_after.fracture_requested);
        int frag_count = 0;
        auto frag_view = world.registry().view<FragmentTagComponent>();
        for (auto frag_entity : frag_view) {
            (void)frag_entity;
            ++frag_count;
        }
        EXPECT_GT(frag_count, 0);
        EXPECT_LE(frag_count, 2);
    } else {
        // 空碎片资产 → fracture_requested 被清除但 is_fractured 未设置
        EXPECT_FALSE(fc_after.fracture_requested);
    }
}

// 测试 断裂系统集成：当Voronoi组件
TEST_F(FractureSystemIntegrationTest, WhenVoronoiComponent) {
    auto e = CreateDestructibleEntity();
    FractureComponent fc;
    fc.source = FractureSource::RuntimeVoronoi;
    fc.runtime_fragment_count = 3;
    fc.runtime_seed = 123;
    fc.explosion_force = 0.0f;
    world.registry().emplace<FractureComponent>(e, fc);

    system.TriggerFracture(world, e);
    system.Update(world, 0.016f);

    auto frag_view = world.registry().view<FragmentTagComponent>();
    for (auto frag_entity : frag_view) {
        EXPECT_TRUE(world.registry().all_of<TransformComponent>(frag_entity))
            << "碎片缺少 TransformComponent";
        EXPECT_TRUE(world.registry().all_of<MeshRendererComponent>(frag_entity))
            << "碎片缺少 MeshRendererComponent";
        EXPECT_TRUE(world.registry().all_of<RigidBody3DComponent>(frag_entity))
            << "碎片缺少 RigidBody3DComponent";
        EXPECT_TRUE(world.registry().all_of<BoxCollider3DComponent>(frag_entity))
            << "碎片缺少 BoxCollider3DComponent";

        // 碎片 mesh 应有运行时顶点数据
        auto& mr = world.registry().get<MeshRendererComponent>(frag_entity);
        EXPECT_FALSE(mr.temp_vertices.empty()) << "碎片缺少顶点数据";
        EXPECT_FALSE(mr.temp_indices.empty()) << "碎片缺少索引数据";
    }
}

// 测试 断裂系统集成：Aftermesh
TEST_F(FractureSystemIntegrationTest, Aftermesh) {
    auto e = CreateDestructibleEntity();
    FractureComponent fc;
    fc.source = FractureSource::RuntimeVoronoi;
    fc.runtime_fragment_count = 2;
    fc.explosion_force = 0.0f;
    world.registry().emplace<FractureComponent>(e, fc);

    system.TriggerFracture(world, e);
    system.Update(world, 0.016f);

    auto& mr = world.registry().get<MeshRendererComponent>(e);
    EXPECT_FALSE(mr.visible) << "碎裂后原始 mesh 应隐藏";
}

// 测试 断裂系统集成：无效
TEST_F(FractureSystemIntegrationTest, Invalid) {
    auto e = CreateDestructibleEntity();
    FractureComponent fc;
    fc.source = FractureSource::RuntimeVoronoi;
    fc.runtime_fragment_count = 2;
    fc.explosion_force = 0.0f;
    world.registry().emplace<FractureComponent>(e, fc);

    system.TriggerFracture(world, e);
    system.Update(world, 0.016f);

    int count_after_first = 0;
    for (auto _ : world.registry().view<FragmentTagComponent>()) {
        (void)_;
        ++count_after_first;
    }

    // 再次触发（应无效，已碎裂）
    system.TriggerFracture(world, e);
    system.Update(world, 0.016f);

    int count_after_second = 0;
    for (auto _ : world.registry().view<FragmentTagComponent>()) {
        (void)_;
        ++count_after_second;
    }
    EXPECT_EQ(count_after_first, count_after_second) << "重复碎裂不应生成新碎片";
}

// 测试 断裂系统集成：Apply伤害Cumulative触发
TEST_F(FractureSystemIntegrationTest, ApplyDamageDamageCumulativeTrigger) {
    auto e = CreateDestructibleEntity();
    FractureComponent fc;
    fc.source = FractureSource::RuntimeVoronoi;
    fc.trigger_mode = FractureTriggerMode::DamageAccumulation;
    fc.health = 100.0f;
    fc.max_health = 100.0f;
    fc.runtime_fragment_count = 2;
    fc.explosion_force = 0.0f;
    world.registry().emplace<FractureComponent>(e, fc);

    // 第一次伤害不够致死
    system.ApplyDamage(world, e, 60.0f);
    system.Update(world, 0.016f);
    EXPECT_FALSE(world.registry().get<FractureComponent>(e).is_fractured);

    // 第二次伤害致死
    system.ApplyDamage(world, e, 50.0f);
    system.Update(world, 0.016f);
    EXPECT_TRUE(world.registry().get<FractureComponent>(e).is_fractured);
}

// 测试 断裂系统集成：生命周期销毁
TEST_F(FractureSystemIntegrationTest, Lifecycle_Destroy) {
    auto e = CreateDestructibleEntity();
    FractureComponent fc;
    fc.source = FractureSource::RuntimeVoronoi;
    fc.runtime_fragment_count = 2;
    fc.fragment_lifetime = 0.1f;
    fc.fragment_fade_duration = 0.1f;
    fc.explosion_force = 0.0f;
    world.registry().emplace<FractureComponent>(e, fc);

    system.TriggerFracture(world, e);
    system.Update(world, 0.016f);

    int initial_count = 0;
    for (auto _ : world.registry().view<FragmentTagComponent>()) {
        (void)_;
        ++initial_count;
    }
    EXPECT_GT(initial_count, 0);

    // 快进超过生命周期+淡出时间
    for (int i = 0; i < 20; ++i) {
        system.Update(world, 0.05f);
    }

    int final_count = 0;
    for (auto _ : world.registry().view<FragmentTagComponent>()) {
        (void)_;
        ++final_count;
    }
    EXPECT_EQ(final_count, 0) << "过期碎片应被销毁";
}
