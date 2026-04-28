/**
 * @file animation_system_2d_test.cpp
 * @brief AnimationSystem (2D) 帧动画系统的单元测试
 *
 * 覆盖场景：
 * - Update 调用不崩溃（空 World）
 * - 带动画组件实体的 Update
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/animation/animation_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/animation.h"

TEST(AnimationSystem2DTest, 空World调用Update不崩溃) {
    World world;
    AnimationSystem sys;
    sys.Update(world, 1.0f / 60.0f);
}

TEST(AnimationSystem2DTest, 带动画组件实体Update不崩溃) {
    World world;
    AnimationSystem sys;
    auto e = world.CreateEntity();
    auto& reg = world.registry();
    auto& anim = reg.emplace<AnimatorComponent>(e);
    anim.SetBool("walking", true);

    sys.Update(world, 1.0f / 60.0f);
}
