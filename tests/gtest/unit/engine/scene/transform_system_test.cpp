/**
 * @file transform_system_test.cpp
 * @brief TransformSystem 变换系统单元测试
 *
 * 覆盖场景：
 * - 单实体无父级的变换计算
 * - 父子层级变换计算
 * - dirty 标志清除
 * - 多级层级（祖父-父-子）
 * - 无 TransformComponent 的实体不影响系统
 */

#include <gtest/gtest.h>
#include "engine/scene/transform_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ============================================================
// 单实体变换
// ============================================================

TEST(TransformSystemTest, 单实体位置正确计算) {
    World world;
    TransformSystem sys;
    auto& reg = world.registry();

    Entity e = world.CreateEntity();
    auto& t = reg.emplace<TransformComponent>(e);
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);
    t.dirty = true;

    sys.Update(world);

    auto& result = reg.get<TransformComponent>(e);
    // 矩阵的 [3][0], [3][1], [3][2] 应包含位置（列主序）
    EXPECT_FLOAT_EQ(result.local_to_world[3][0], 1.0f);
    EXPECT_FLOAT_EQ(result.local_to_world[3][1], 2.0f);
    EXPECT_FLOAT_EQ(result.local_to_world[3][2], 3.0f);
    EXPECT_FALSE(result.dirty);
}

TEST(TransformSystemTest, 单实体缩放正确计算) {
    World world;
    TransformSystem sys;
    auto& reg = world.registry();

    Entity e = world.CreateEntity();
    auto& t = reg.emplace<TransformComponent>(e);
    t.scale = glm::vec3(2.0f, 3.0f, 4.0f);

    sys.Update(world);

    auto& result = reg.get<TransformComponent>(e);
    // 对角线元素应为缩放值（无旋转时）
    EXPECT_FLOAT_EQ(result.local_to_world[0][0], 2.0f);
    EXPECT_FLOAT_EQ(result.local_to_world[1][1], 3.0f);
    EXPECT_FLOAT_EQ(result.local_to_world[2][2], 4.0f);
}

TEST(TransformSystemTest, 默认Transform为单位矩阵) {
    World world;
    TransformSystem sys;
    auto& reg = world.registry();

    Entity e = world.CreateEntity();
    reg.emplace<TransformComponent>(e); // 默认值

    sys.Update(world);

    auto& result = reg.get<TransformComponent>(e);
    EXPECT_EQ(result.local_to_world, glm::mat4(1.0f));
}

// ============================================================
// 父子层级变换
// ============================================================

TEST(TransformSystemTest, 子实体继承父级变换) {
    World world;
    TransformSystem sys;
    auto& reg = world.registry();

    Entity parent = world.CreateEntity();
    auto& pt = reg.emplace<TransformComponent>(parent);
    pt.position = glm::vec3(10.0f, 0.0f, 0.0f);

    Entity child = world.CreateEntity();
    auto& ct = reg.emplace<TransformComponent>(child);
    ct.position = glm::vec3(1.0f, 0.0f, 0.0f);
    reg.emplace<ParentComponent>(child, ParentComponent{parent});

    sys.Update(world);

    auto& parent_result = reg.get<TransformComponent>(parent);
    auto& child_result = reg.get<TransformComponent>(child);

    // 父级位置: (10, 0, 0)
    EXPECT_FLOAT_EQ(parent_result.local_to_world[3][0], 10.0f);
    // 子级世界位置 = 父级位置 + 子级本地位置 = 11
    EXPECT_FLOAT_EQ(child_result.local_to_world[3][0], 11.0f);
}

TEST(TransformSystemTest, 父级缩放影响子级) {
    World world;
    TransformSystem sys;
    auto& reg = world.registry();

    Entity parent = world.CreateEntity();
    auto& pt = reg.emplace<TransformComponent>(parent);
    pt.scale = glm::vec3(2.0f);

    Entity child = world.CreateEntity();
    auto& ct = reg.emplace<TransformComponent>(child);
    ct.position = glm::vec3(5.0f, 0.0f, 0.0f);
    reg.emplace<ParentComponent>(child, ParentComponent{parent});

    sys.Update(world);

    auto& child_result = reg.get<TransformComponent>(child);
    // 子级世界位置 = 父级缩放 * 子级本地位置 = 2 * 5 = 10
    EXPECT_FLOAT_EQ(child_result.local_to_world[3][0], 10.0f);
}

// ============================================================
// dirty 标志
// ============================================================

TEST(TransformSystemTest, 更新后dirty标志被清除) {
    World world;
    TransformSystem sys;
    auto& reg = world.registry();

    Entity e = world.CreateEntity();
    auto& t = reg.emplace<TransformComponent>(e);
    EXPECT_TRUE(t.dirty); // 默认 dirty

    sys.Update(world);
    EXPECT_FALSE(reg.get<TransformComponent>(e).dirty);
}

// ============================================================
// 无变换组件的实体
// ============================================================

TEST(TransformSystemTest, 无TransformComponent的实体不影响系统) {
    World world;
    TransformSystem sys;
    auto& reg = world.registry();

    Entity e1 = world.CreateEntity(); // 无组件
    Entity e2 = world.CreateEntity();
    auto& t = reg.emplace<TransformComponent>(e2);
    t.position = glm::vec3(1.0f);

    EXPECT_NO_THROW(sys.Update(world));

    auto& result = reg.get<TransformComponent>(e2);
    EXPECT_FLOAT_EQ(result.local_to_world[3][0], 1.0f);
}

// ============================================================
// 多级层级
// ============================================================

TEST(TransformSystemTest, 三级层级变换正确) {
    World world;
    TransformSystem sys;
    auto& reg = world.registry();

    Entity grandparent = world.CreateEntity();
    auto& gt = reg.emplace<TransformComponent>(grandparent);
    gt.position = glm::vec3(10.0f, 0.0f, 0.0f);

    Entity parent = world.CreateEntity();
    auto& pt = reg.emplace<TransformComponent>(parent);
    pt.position = glm::vec3(5.0f, 0.0f, 0.0f);
    reg.emplace<ParentComponent>(parent, ParentComponent{grandparent});

    Entity child = world.CreateEntity();
    auto& ct = reg.emplace<TransformComponent>(child);
    ct.position = glm::vec3(1.0f, 0.0f, 0.0f);
    reg.emplace<ParentComponent>(child, ParentComponent{parent});

    sys.Update(world);

    auto& child_result = reg.get<TransformComponent>(child);
    // 子级世界位置 = 10 + 5 + 1 = 16
    EXPECT_FLOAT_EQ(child_result.local_to_world[3][0], 16.0f);
}
