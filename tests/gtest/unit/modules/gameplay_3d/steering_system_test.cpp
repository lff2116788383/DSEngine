/**
 * @file steering_system_test.cpp
 * @brief SteeringSystem 三维转向行为系统的单元测试
 *
 * 覆盖场景：
 * - Update 调用不崩溃（空 World）
 * - 带实体和 TransformComponent 的 Update
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/ai/steering_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"

using namespace dse::gameplay3d;

TEST(SteeringSystemTest, 空World调用Update不崩溃) {
    World world;
    SteeringSystem sys;
    sys.Update(world, 1.0f / 60.0f);
}

TEST(SteeringSystemTest, 带TransformComponent实体Update不崩溃) {
    World world;
    SteeringSystem sys;
    auto e = world.CreateEntity();
    auto& reg = world.registry();
    auto& transform = reg.emplace<TransformComponent>(e);
    transform.position = glm::vec3(0.0f);

    sys.Update(world, 1.0f / 60.0f);
}
