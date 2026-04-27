/**
 * @file ecs_transform_integration_test.cpp
 * @brief ECS + TransformSystem 集成测试
 *
 * 验证场景：
 * - 实体创建后挂载 TransformComponent，经 TransformSystem 更新后模型矩阵正确
 * - 父子层级关系下，子实体世界矩阵受父实体变换影响
 * - 多帧连续更新下 dirty 标记正确传播
 * - 批量实体变换更新的一致性
 */

// MSVC 下 gtest-port.h 依赖 <io.h> 中的 _isatty 等 POSIX 函数，
// 但因 include 路径顺序导致 gtest-port-arch.h 的 GTEST_OS_WINDOWS
// 宏可能未在 <io.h> 包含前生效，此处主动包含以确保可用。
#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/scene/transform_system.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

// ============================================================
// 基础 ECS + TransformSystem 协作
// ============================================================

class EcsTransformIntegrationTest : public ::testing::Test {
protected:
    World world;
    TransformSystem transform_system;

    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EcsTransformIntegrationTest, 单实体变换更新后模型矩阵非单位矩阵) {
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    transform.dirty = true;

    transform_system.Update(world);

    // 模型矩阵应反映平移
    EXPECT_FALSE(transform.dirty);
    EXPECT_FLOAT_EQ(transform.local_to_world[3][0], 1.0f);
    EXPECT_FLOAT_EQ(transform.local_to_world[3][1], 2.0f);
    EXPECT_FLOAT_EQ(transform.local_to_world[3][2], 3.0f);
}

TEST_F(EcsTransformIntegrationTest, 无变换实体模型矩阵为单位矩阵) {
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);
    // 默认 position=(0,0,0), rotation=identity, scale=(1,1,1)
    transform.dirty = true;

    transform_system.Update(world);

    EXPECT_FALSE(transform.dirty);
    // 单位变换应产生单位矩阵
    EXPECT_EQ(transform.local_to_world, glm::mat4(1.0f));
}

TEST_F(EcsTransformIntegrationTest, 缩放变换反映在模型矩阵中) {
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);
    transform.scale = glm::vec3(2.0f, 3.0f, 4.0f);
    transform.dirty = true;

    transform_system.Update(world);

    // 验证缩放分量
    EXPECT_FLOAT_EQ(glm::length(transform.local_to_world[0]), 2.0f);
    EXPECT_FLOAT_EQ(glm::length(transform.local_to_world[1]), 3.0f);
    EXPECT_FLOAT_EQ(glm::length(transform.local_to_world[2]), 4.0f);
}

TEST_F(EcsTransformIntegrationTest, dirty标记为false时跳过更新) {
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);
    transform.position = glm::vec3(5.0f, 0.0f, 0.0f);
    transform.dirty = true;

    transform_system.Update(world);
    glm::mat4 first_matrix = transform.local_to_world;

    // 修改位置但不标记 dirty
    transform.position = glm::vec3(100.0f, 0.0f, 0.0f);
    transform.dirty = false;

    transform_system.Update(world);

    // 矩阵不应改变
    EXPECT_EQ(transform.local_to_world, first_matrix);
}

// ============================================================
// 父子层级集成
// ============================================================

TEST_F(EcsTransformIntegrationTest, 父子层级子实体受父平移影响) {
    Entity parent = world.CreateEntity();
    auto& parent_transform = world.registry().emplace<TransformComponent>(parent);
    parent_transform.position = glm::vec3(10.0f, 0.0f, 0.0f);
    parent_transform.dirty = true;

    Entity child = world.CreateEntity();
    auto& child_transform = world.registry().emplace<TransformComponent>(child);
    child_transform.position = glm::vec3(1.0f, 0.0f, 0.0f);
    child_transform.dirty = true;
    world.registry().emplace<ParentComponent>(child, ParentComponent{parent});

    transform_system.Update(world);

    // 子实体的世界位置应叠加父实体的平移
    EXPECT_FLOAT_EQ(child_transform.local_to_world[3][0], 11.0f);
}

TEST_F(EcsTransformIntegrationTest, 多层嵌套父子变换正确传播) {
    Entity grandparent = world.CreateEntity();
    auto& gp_transform = world.registry().emplace<TransformComponent>(grandparent);
    gp_transform.position = glm::vec3(10.0f, 0.0f, 0.0f);
    gp_transform.dirty = true;

    Entity parent = world.CreateEntity();
    auto& p_transform = world.registry().emplace<TransformComponent>(parent);
    p_transform.position = glm::vec3(5.0f, 0.0f, 0.0f);
    p_transform.dirty = true;
    world.registry().emplace<ParentComponent>(parent, ParentComponent{grandparent});

    Entity child = world.CreateEntity();
    auto& c_transform = world.registry().emplace<TransformComponent>(child);
    c_transform.position = glm::vec3(1.0f, 0.0f, 0.0f);
    c_transform.dirty = true;
    world.registry().emplace<ParentComponent>(child, ParentComponent{parent});

    transform_system.Update(world);

    // 子实体的世界位置应为 10 + 5 + 1 = 16
    EXPECT_FLOAT_EQ(c_transform.local_to_world[3][0], 16.0f);
}

// ============================================================
// 批量实体更新
// ============================================================

TEST_F(EcsTransformIntegrationTest, 批量实体变换更新一致性) {
    constexpr int kEntityCount = 100;
    std::vector<Entity> entities;
    entities.reserve(kEntityCount);

    for (int i = 0; i < kEntityCount; ++i) {
        Entity e = world.CreateEntity();
        auto& transform = world.registry().emplace<TransformComponent>(e);
        transform.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
        transform.dirty = true;
        entities.push_back(e);
    }

    transform_system.Update(world);

    // 验证每个实体的世界位置
    for (int i = 0; i < kEntityCount; ++i) {
        auto& transform = world.registry().get<TransformComponent>(entities[i]);
        EXPECT_FALSE(transform.dirty);
        EXPECT_FLOAT_EQ(transform.local_to_world[3][0], static_cast<float>(i));
    }
}

// ============================================================
// 实体生命周期与变换一致性
// ============================================================

TEST_F(EcsTransformIntegrationTest, 销毁实体后其余实体变换不受影响) {
    Entity e1 = world.CreateEntity();
    auto& t1 = world.registry().emplace<TransformComponent>(e1);
    t1.position = glm::vec3(10.0f, 0.0f, 0.0f);
    t1.dirty = true;

    Entity e2 = world.CreateEntity();
    auto& t2 = world.registry().emplace<TransformComponent>(e2);
    t2.position = glm::vec3(20.0f, 0.0f, 0.0f);
    t2.dirty = true;

    transform_system.Update(world);
    EXPECT_FLOAT_EQ(t1.local_to_world[3][0], 10.0f);
    EXPECT_FLOAT_EQ(t2.local_to_world[3][0], 20.0f);

    // 销毁 e1
    world.DestroyEntity(e1);
    transform_system.Update(world);

    // e2 的变换不应受影响
    EXPECT_FLOAT_EQ(t2.local_to_world[3][0], 20.0f);
}

TEST_F(EcsTransformIntegrationTest, 连续多帧更新变换累积正确) {
    Entity e = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(e);

    // 模拟3帧，每帧移动位置
    for (int frame = 0; frame < 3; ++frame) {
        transform.position.x += 1.0f;
        transform.dirty = true;
        transform_system.Update(world);
    }

    // 最终位置应为 (3, 0, 0)
    EXPECT_FLOAT_EQ(transform.local_to_world[3][0], 3.0f);
}
