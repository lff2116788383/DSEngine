/**
 * @file steering_system_test.cpp
 * @brief SteeringSystem 三维转向行为系统的单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - delta_time <= 0 时跳过更新
 * - Seek 行为：实体朝目标移动
 * - Flee 行为：实体远离目标
 * - Arrive 行为：接近目标时减速
 * - max_velocity 限制最大速度
 * - max_force 限制最大转向力
 * - 禁用的 SteeringComponent 不更新
 * - 速度更新反映到 Transform 位置
 * - 移动方向更新旋转
 */

#include <gtest/gtest.h>
#include <glm/gtx/norm.hpp>
#include "modules/gameplay_3d/ai/steering_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"

using namespace dse;
using namespace gameplay3d;

class SteeringSystemTest : public ::testing::Test {
protected:
    World world;
    SteeringSystem sys;
    static constexpr float kDt = 1.0f / 60.0f;

    /// 创建一个带 Steering+Transform 的实体
    std::tuple<Entity, SteeringComponent*, TransformComponent*>
    CreateSteeringEntity() {
        auto e = world.CreateEntity();
        auto& steering = world.registry().emplace<SteeringComponent>(e);
        auto& tf = world.registry().emplace<TransformComponent>(e);
        return {e, &steering, &tf};
    }
};

TEST_F(SteeringSystemTest, 空World调用Update不崩溃) {
    EXPECT_NO_THROW(sys.Update(world, kDt));
}

TEST_F(SteeringSystemTest, DeltaTime为零时跳过更新) {
    auto [e, steering, tf] = CreateSteeringEntity();
    steering->seek_enabled = true;
    steering->seek_target = glm::vec3(100.0f, 0.0f, 0.0f);

    sys.Update(world, 0.0f);
    EXPECT_EQ(steering->velocity, glm::vec3(0.0f));
}

TEST_F(SteeringSystemTest, Seek行为朝目标移动) {
    auto [e, steering, tf] = CreateSteeringEntity();
    tf->position = glm::vec3(0.0f);
    steering->seek_enabled = true;
    steering->seek_target = glm::vec3(10.0f, 0.0f, 0.0f);
    steering->max_velocity = 5.0f;
    steering->max_force = 50.0f;

    sys.Update(world, kDt);

    // 速度的 X 分量应为正（朝目标方向）
    EXPECT_GT(steering->velocity.x, 0.0f);
    // 位置也应朝目标移动
    EXPECT_GT(tf->position.x, 0.0f);
}

TEST_F(SteeringSystemTest, Flee行为远离目标) {
    auto [e, steering, tf] = CreateSteeringEntity();
    tf->position = glm::vec3(0.0f);
    steering->flee_enabled = true;
    steering->flee_target = glm::vec3(10.0f, 0.0f, 0.0f);
    steering->max_velocity = 5.0f;
    steering->max_force = 50.0f;

    sys.Update(world, kDt);

    // 速度的 X 分量应为负（远离目标方向）
    EXPECT_LT(steering->velocity.x, 0.0f);
    // 位置应远离目标
    EXPECT_LT(tf->position.x, 0.0f);
}

TEST_F(SteeringSystemTest, Arrive行为接近目标时减速) {
    auto [e, steering, tf] = CreateSteeringEntity();
    tf->position = glm::vec3(0.0f);
    steering->arrive_enabled = true;
    steering->arrive_target = glm::vec3(1.0f, 0.0f, 0.0f);
    steering->arrive_deceleration_radius = 5.0f;
    steering->max_velocity = 10.0f;
    steering->max_force = 100.0f;

    // 远距离
    tf->position = glm::vec3(0.0f);
    steering->velocity = glm::vec3(0.0f);
    sys.Update(world, kDt);
    float far_speed = glm::length(steering->velocity);

    // 近距离（在减速区内）
    tf->position = glm::vec3(0.9f, 0.0f, 0.0f);
    steering->velocity = glm::vec3(0.0f);
    sys.Update(world, kDt);
    float near_speed = glm::length(steering->velocity);

    // 近距离时速度更小
    EXPECT_LT(near_speed, far_speed);
}

TEST_F(SteeringSystemTest, Arrive到达目标时速度归零) {
    auto [e, steering, tf] = CreateSteeringEntity();
    tf->position = glm::vec3(5.0f, 0.0f, 0.0f);
    steering->arrive_enabled = true;
    steering->arrive_target = glm::vec3(5.0f, 0.0f, 0.0f); // 已在目标点
    steering->max_velocity = 10.0f;

    sys.Update(world, kDt);

    EXPECT_EQ(steering->velocity, glm::vec3(0.0f));
}

TEST_F(SteeringSystemTest, MaxVelocity限制最大速度) {
    auto [e, steering, tf] = CreateSteeringEntity();
    tf->position = glm::vec3(0.0f);
    steering->seek_enabled = true;
    steering->seek_target = glm::vec3(100.0f, 0.0f, 0.0f);
    steering->max_velocity = 2.0f;
    steering->max_force = 1000.0f; // 大力

    // 多次更新使速度累积
    for (int i = 0; i < 100; ++i) {
        sys.Update(world, kDt);
    }

    EXPECT_LE(glm::length(steering->velocity), steering->max_velocity + 0.01f);
}

TEST_F(SteeringSystemTest, 禁用的SteeringComponent不更新) {
    auto [e, steering, tf] = CreateSteeringEntity();
    tf->position = glm::vec3(0.0f);
    steering->enabled = false;
    steering->seek_enabled = true;
    steering->seek_target = glm::vec3(100.0f, 0.0f, 0.0f);

    sys.Update(world, kDt);

    EXPECT_EQ(steering->velocity, glm::vec3(0.0f));
    EXPECT_EQ(tf->position, glm::vec3(0.0f));
}

TEST_F(SteeringSystemTest, 速度更新反映到Transform位置) {
    auto [e, steering, tf] = CreateSteeringEntity();
    tf->position = glm::vec3(0.0f);
    steering->seek_enabled = true;
    steering->seek_target = glm::vec3(10.0f, 0.0f, 0.0f);
    steering->max_velocity = 5.0f;
    steering->max_force = 50.0f;

    glm::vec3 pos_before = tf->position;
    sys.Update(world, kDt);

    EXPECT_NE(tf->position, pos_before);
}

TEST_F(SteeringSystemTest, 移动方向更新旋转) {
    auto [e, steering, tf] = CreateSteeringEntity();
    tf->position = glm::vec3(0.0f);
    steering->seek_enabled = true;
    steering->seek_target = glm::vec3(10.0f, 0.0f, 0.0f);
    steering->max_velocity = 5.0f;
    steering->max_force = 50.0f;

    sys.Update(world, kDt);

    // 有速度时旋转应更新
    if (glm::length2(steering->velocity) > 0.001f) {
        EXPECT_NE(tf->rotation, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    }
}
