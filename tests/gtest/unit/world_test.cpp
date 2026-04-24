/**
 * @file world_test.cpp
 * @brief World (ECS) 的单元测试
 *
 * 覆盖场景：
 * - 实体创建与销毁
 * - 实体存活检查
 * - 实体计数
 * - 清空世界
 * - Instance() 委托到 ServiceLocator
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/core/service_locator.h"

using namespace dse::core;

// ============================================================
// World 基础功能测试
// ============================================================

TEST(WorldTest, 创建实体返回有效ID) {
    World world;
    Entity e = world.CreateEntity();
    // 创建的实体应当存活
    EXPECT_TRUE(world.IsAlive(e));
}

TEST(WorldTest, 创建实体后计数为1) {
    World world;
    world.CreateEntity();
    EXPECT_EQ(world.EntityCount(), 1u);
}

TEST(WorldTest, 创建多个实体计数递增) {
    World world;
    world.CreateEntity();
    world.CreateEntity();
    world.CreateEntity();
    EXPECT_EQ(world.EntityCount(), 3u);
}

TEST(WorldTest, 销毁实体后计数递减) {
    World world;
    Entity e = world.CreateEntity();
    EXPECT_EQ(world.EntityCount(), 1u);

    world.DestroyEntity(e);
    EXPECT_EQ(world.EntityCount(), 0u);
}

TEST(WorldTest, 销毁不存在的实体不崩溃) {
    World world;
    Entity invalid = static_cast<Entity>(99999);
    world.DestroyEntity(invalid);
    EXPECT_EQ(world.EntityCount(), 0u);
}

TEST(WorldTest, IsAlive检查实体存活状态) {
    World world;
    Entity e = world.CreateEntity();
    EXPECT_TRUE(world.IsAlive(e));

    world.DestroyEntity(e);
    EXPECT_FALSE(world.IsAlive(e));
}

TEST(WorldTest, Clear清空所有实体) {
    World world;
    world.CreateEntity();
    world.CreateEntity();
    world.CreateEntity();
    EXPECT_EQ(world.EntityCount(), 3u);

    world.Clear();
    EXPECT_EQ(world.EntityCount(), 0u);
}

TEST(WorldTest, registry返回有效引用) {
    World world;
    auto& reg = world.registry();
    // 通过 registry 直接创建实体应与 World 创建一致
    Entity e = reg.create();
    EXPECT_TRUE(reg.valid(e));
    reg.destroy(e);
}

// ============================================================
// World Instance() 测试
// ============================================================

class WorldInstanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        ServiceLocator::Instance().Reset<World>();
    }
    void TearDown() override {
        ServiceLocator::Instance().Reset<World>();
    }
};

TEST_F(WorldInstanceTest, Instance委托到ServiceLocator) {
    // 先注册一个 World 实例
    auto world = std::make_shared<World>();
    world->CreateEntity();
    ServiceLocator::Instance().Register<World, World>(world);

    // Instance() 应返回已注册的实例
    auto& instance = World::Instance();
    EXPECT_EQ(instance.EntityCount(), 1u);
}

TEST_F(WorldInstanceTest, Instance未注册时自动创建) {
    // 未注册时 Instance() 应自动创建并注册
    auto& instance = World::Instance();
    EXPECT_EQ(instance.EntityCount(), 0u);

    // 验证已注册到 ServiceLocator
    EXPECT_TRUE(ServiceLocator::Instance().Has<World>());
}

// ============================================================
// 多 World 实例测试
// ============================================================

TEST(WorldTest, 多个World实例互不干扰) {
    World world_a;
    World world_b;

    Entity e_a = world_a.CreateEntity();
    Entity e_b = world_b.CreateEntity();
    world_b.CreateEntity();

    EXPECT_EQ(world_a.EntityCount(), 1u);
    EXPECT_EQ(world_b.EntityCount(), 2u);

    // world_b 中销毁不影响 world_a
    world_b.DestroyEntity(e_b);
    EXPECT_EQ(world_a.EntityCount(), 1u);
    EXPECT_EQ(world_b.EntityCount(), 1u);

    // world_a 中实体在 world_b 中无效
    EXPECT_FALSE(world_b.IsAlive(e_a));
}
