/**
* @file animator_system_test.cpp
* @brief 3D 动画系统单元测试，验证 AnimatorSystem::Update 对空/有实体 World 的行为
*/

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/animation.h"
#include "modules/gameplay_3d/animation/animator_system.h"

using namespace dse;
using namespace gameplay3d;

TEST(AnimatorSystemTest, 空World调用Update不崩溃) {
    World world;
    // 设置 AssetManager 为 nullptr（仅测试不崩溃）
    AnimatorSystem::SetAssetManager(nullptr);
    EXPECT_NO_THROW(AnimatorSystem::Update(world, 0.016f));
}

TEST(AnimatorSystemTest, 带AnimatorComponent实体Update不崩溃) {
    World world;
    AnimatorSystem::SetAssetManager(nullptr);
    auto entity = world.CreateEntity();
    world.registry().emplace<AnimatorComponent>(entity);
    EXPECT_NO_THROW(AnimatorSystem::Update(world, 0.016f));
}
