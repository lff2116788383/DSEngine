/**
 * @file scene_bind_world_test.cpp
 * @brief Scene 构造 / BindWorld / UnbindWorld / 名称测试
 */

#include <gtest/gtest.h>
#include "engine/scene/scene.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"

using namespace scene;

TEST(SceneBindWorldTest, ConstructionWithName) {
    Scene s("TestScene");
    EXPECT_EQ(s.GetName(), "TestScene");
}

TEST(SceneBindWorldTest, DefaultWorldUsable) {
    Scene s("DefaultWorld");
    auto& world = s.GetWorld();
    auto entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(entity).position = {1, 2, 3};

    auto& t = world.registry().get<TransformComponent>(entity);
    EXPECT_FLOAT_EQ(t.position.x, 1.0f);
    EXPECT_FLOAT_EQ(t.position.y, 2.0f);
    EXPECT_FLOAT_EQ(t.position.z, 3.0f);
}

TEST(SceneBindWorldTest, BindExternalWorld) {
    Scene s("BindTest");
    World external_world;

    auto ext_entity = external_world.CreateEntity();
    external_world.registry().emplace<TransformComponent>(ext_entity).position = {10, 20, 30};

    s.BindWorld(&external_world);
    auto& bound = s.GetWorld();

    auto view = bound.registry().view<TransformComponent>();
    int count = 0;
    for (auto e : view) {
        auto& t = view.get<TransformComponent>(e);
        EXPECT_FLOAT_EQ(t.position.x, 10.0f);
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(SceneBindWorldTest, UnbindFallsBackToOwnedWorld) {
    Scene s("UnbindTest");

    // 在内置 world 里创建一个实体
    auto& owned = s.GetWorld();
    auto owned_entity = owned.CreateEntity();
    owned.registry().emplace<TransformComponent>(owned_entity);

    // 绑定外部 world
    World external;
    s.BindWorld(&external);

    // 外部 world 应该是空的
    EXPECT_EQ(s.GetWorld().registry().view<TransformComponent>().size(), 0u);

    // 解绑回内置 world
    s.UnbindWorld();
    EXPECT_EQ(s.GetWorld().registry().view<TransformComponent>().size(), 1u);
}

TEST(SceneBindWorldTest, BindNullptrFallsBack) {
    Scene s("NullBind");
    s.BindWorld(nullptr);
    // 传 nullptr 应安全回退到内置 world
    EXPECT_NO_THROW(s.GetWorld());
}
