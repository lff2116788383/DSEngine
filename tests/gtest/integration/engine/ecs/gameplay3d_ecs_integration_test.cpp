/**
 * @file gameplay3d_ecs_integration_test.cpp
 * @brief Gameplay3D ↔ ECS 集成测试
 *
 * 验证场景：
 * - 3D 动画状态机与 ECS 实体的绑定
 * - Animator3DComponent 状态转换
 * - TerrainComponent 默认值
 * - SkyboxComponent 默认值
 * - MorphComponent 与实体的关联
 * - 复合组件实体（Transform+Mesh+Animator3D）的创建和查询
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/animation/animation_state_machine.h"

using namespace dse;

class Gameplay3dEcsIntegrationTest : public ::testing::Test {
protected:
    World world;
};

TEST_F(Gameplay3dEcsIntegrationTest, 复合3D实体创建与查询) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    world.registry().emplace<MeshRendererComponent>(e);
    world.registry().emplace<Animator3DComponent>(e);

    EXPECT_TRUE(world.registry().all_of<TransformComponent>(e));
    EXPECT_TRUE(world.registry().all_of<MeshRendererComponent>(e));
    EXPECT_TRUE(world.registry().all_of<Animator3DComponent>(e));
}

TEST_F(Gameplay3dEcsIntegrationTest, Animator3D状态转换) {
    auto e = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(e);

    // 初始状态
    anim.current_state_name = "idle";
    anim.state_time = 0.0f;

    // 模拟状态转换
    anim.is_transitioning = true;
    anim.next_state_name = "walk";
    anim.transition_duration = 0.3f;
    anim.transition_progress = 0.0f;

    // 推进转换进度
    anim.transition_progress = 0.5f;
    EXPECT_FLOAT_EQ(anim.transition_progress, 0.5f);

    // 完成转换
    anim.transition_progress = 1.0f;
    anim.current_state_name = anim.next_state_name;
    anim.is_transitioning = false;
    anim.state_time = 0.0f;

    EXPECT_EQ(anim.current_state_name, "walk");
    EXPECT_FALSE(anim.is_transitioning);
}

TEST_F(Gameplay3dEcsIntegrationTest, TerrainComponent默认值) {
    TerrainComponent terrain;
    EXPECT_TRUE(terrain.enabled);
    EXPECT_FLOAT_EQ(terrain.width, 100.0f);
    EXPECT_FLOAT_EQ(terrain.depth, 100.0f);
    EXPECT_FLOAT_EQ(terrain.max_height, 20.0f);
    EXPECT_EQ(terrain.resolution_x, 64);
    EXPECT_EQ(terrain.resolution_z, 64);
    EXPECT_TRUE(terrain.use_dynamic_lod);
    EXPECT_EQ(terrain.max_lod_levels, 4);
    EXPECT_TRUE(terrain.is_dirty);
}

TEST_F(Gameplay3dEcsIntegrationTest, SkyboxComponent默认值) {
    SkyboxComponent skybox;
    EXPECT_TRUE(skybox.enabled);
    EXPECT_EQ(skybox.cubemap_handle, 0u);
    EXPECT_TRUE(skybox.cubemap_path.empty());
}

TEST_F(Gameplay3dEcsIntegrationTest, SkyLightComponent默认值) {
    SkyLightComponent sky;
    EXPECT_TRUE(sky.enabled);
    EXPECT_FLOAT_EQ(sky.intensity, 1.0f);
}

TEST_F(Gameplay3dEcsIntegrationTest, MorphComponent与实体关联) {
    auto e = world.CreateEntity();
    auto& morph = world.registry().emplace<MorphComponent>(e);
    morph.targets.push_back({"blink", 0.5f});
    morph.targets.push_back({"smile", 0.0f});

    ASSERT_EQ(morph.targets.size(), 2u);
    EXPECT_EQ(morph.targets[0].name, "blink");
    EXPECT_FLOAT_EQ(morph.targets[0].weight, 0.5f);
    EXPECT_EQ(morph.targets[1].name, "smile");
    EXPECT_FLOAT_EQ(morph.targets[1].weight, 0.0f);
}

TEST_F(Gameplay3dEcsIntegrationTest, 多个3D实体批量查询) {
    for (int i = 0; i < 5; ++i) {
        auto e = world.CreateEntity();
        auto& tf = world.registry().emplace<TransformComponent>(e);
        tf.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
        world.registry().emplace<MeshRendererComponent>(e);
    }

    // 查询所有带 MeshRenderer 的实体
    int count = 0;
    auto view = world.registry().view<MeshRendererComponent>();
    for (auto entity : view) {
        (void)entity;
        ++count;
    }
    EXPECT_EQ(count, 5);
}

TEST_F(Gameplay3dEcsIntegrationTest, AnimationStateMachine基础) {
    dse::gameplay3d::AnimationStateMachine sm;
    dse::gameplay3d::AnimState idle_state;
    idle_state.name = "idle";
    idle_state.danim_path = "idle.danim";
    idle_state.loop = true;
    sm.AddState(idle_state);

    dse::gameplay3d::AnimState walk_state;
    walk_state.name = "walk";
    walk_state.danim_path = "walk.danim";
    walk_state.loop = true;
    sm.AddState(walk_state);

    sm.SetDefaultState("idle");
    EXPECT_EQ(sm.GetDefaultState(), "idle");
    EXPECT_EQ(sm.GetStates().size(), 2u);
}
