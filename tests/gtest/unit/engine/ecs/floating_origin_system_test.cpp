/**
 * @file floating_origin_system_test.cpp
 * @brief FloatingOriginSystem 单元测试
 *
 * 覆盖场景：
 * - 阈值内不触发 rebase
 * - 超过阈值时触发 rebase 并平移所有 Transform
 * - 累积原点跟踪正确
 * - ToAbsolute / ToLocal 坐标转换
 * - 无相机时不触发 rebase
 * - 自定义阈值
 * - 多次 rebase 累积
 * - EventBus 广播验证
 */

#include <gtest/gtest.h>
#include "engine/ecs/floating_origin_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/core/event_bus.h"
#include <glm/glm.hpp>
#include <cmath>

using namespace dse;

class FloatingOriginTest : public ::testing::Test {
protected:
    void SetUp() override {
        system_ = std::make_unique<FloatingOriginSystem>();
    }

    Entity CreateCamera(World& world, const glm::vec3& pos, bool enabled = true, int priority = 0) {
        auto& reg = world.registry();
        auto e = reg.create();
        auto& tf = reg.emplace<TransformComponent>(e);
        tf.position = pos;
        auto& cam = reg.emplace<Camera3DComponent>(e);
        cam.enabled = enabled;
        cam.priority = priority;
        return e;
    }

    Entity CreateEntity(World& world, const glm::vec3& pos) {
        auto& reg = world.registry();
        auto e = reg.create();
        auto& tf = reg.emplace<TransformComponent>(e);
        tf.position = pos;
        return e;
    }

    std::unique_ptr<FloatingOriginSystem> system_;
};

// 测试 浮动原点：默认为5000
TEST_F(FloatingOriginTest, DefaultIs5000) {
    EXPECT_FLOAT_EQ(system_->rebase_threshold(), 5000.0f);
}

// 测试 浮动原点：可设置上
TEST_F(FloatingOriginTest, CansetUp) {
    system_->set_rebase_threshold(100.0f);
    EXPECT_FLOAT_EQ(system_->rebase_threshold(), 100.0f);
}

// 测试 浮动原点：Pointis零
TEST_F(FloatingOriginTest, PointisZero) {
    auto origin = system_->accumulated_origin();
    EXPECT_DOUBLE_EQ(origin.x, 0.0);
    EXPECT_DOUBLE_EQ(origin.y, 0.0);
    EXPECT_DOUBLE_EQ(origin.z, 0.0);
}

// 测试 浮动原点：存在内部不触发变基
TEST_F(FloatingOriginTest, ExistInsideDoesNotTriggerRebase) {
    World world;
    system_->set_rebase_threshold(100.0f);
    CreateCamera(world, glm::vec3(50.0f, 0.0f, 0.0f));
    CreateEntity(world, glm::vec3(10.0f, 20.0f, 30.0f));

    system_->Tick(world, nullptr, nullptr);

    auto origin = system_->accumulated_origin();
    EXPECT_DOUBLE_EQ(origin.x, 0.0);

    auto& reg = world.registry();
    auto view = reg.view<TransformComponent>();
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e);
        if (std::abs(tf.position.x - 50.0f) < 0.01f) {
            EXPECT_FLOAT_EQ(tf.position.x, 50.0f);
        }
    }
}

// 测试 浮动原点：触发变基带变换
TEST_F(FloatingOriginTest, TriggersRebaseWithTransform) {
    World world;
    system_->set_rebase_threshold(100.0f);

    CreateCamera(world, glm::vec3(200.0f, 0.0f, 0.0f));
    auto other = CreateEntity(world, glm::vec3(300.0f, 50.0f, 100.0f));

    system_->Tick(world, nullptr, nullptr);

    auto& tf = world.registry().get<TransformComponent>(other);
    EXPECT_NEAR(tf.position.x, 100.0f, 0.01f);
    EXPECT_NEAR(tf.position.y, 50.0f, 0.01f);
    EXPECT_NEAR(tf.position.z, 100.0f, 0.01f);
}

// 测试 浮动原点：变基重置相机位置到零
TEST_F(FloatingOriginTest, RebaseResetCameraPositionToZero) {
    World world;
    system_->set_rebase_threshold(100.0f);

    auto cam = CreateCamera(world, glm::vec3(200.0f, 0.0f, 0.0f));

    system_->Tick(world, nullptr, nullptr);

    auto& tf = world.registry().get<TransformComponent>(cam);
    EXPECT_NEAR(tf.position.x, 0.0f, 0.01f);
}

// 测试 浮动原点：变基后期Accumulation原点为正确
TEST_F(FloatingOriginTest, RebaseThePostAccumulationOriginIsCorrect) {
    World world;
    system_->set_rebase_threshold(100.0f);

    CreateCamera(world, glm::vec3(200.0f, 50.0f, 0.0f));

    system_->Tick(world, nullptr, nullptr);

    auto origin = system_->accumulated_origin();
    EXPECT_NEAR(origin.x, 200.0, 0.01);
    EXPECT_NEAR(origin.y, 50.0, 0.01);
    EXPECT_NEAR(origin.z, 0.0, 0.01);
}

// 测试 浮动原点：多次数变基正确
TEST_F(FloatingOriginTest, MultiTimesRebaseCorrect) {
    World world;
    system_->set_rebase_threshold(100.0f);

    auto cam = CreateCamera(world, glm::vec3(200.0f, 0.0f, 0.0f));
    system_->Tick(world, nullptr, nullptr);

    world.registry().get<TransformComponent>(cam).position = glm::vec3(300.0f, 0.0f, 0.0f);
    system_->Tick(world, nullptr, nullptr);

    auto origin = system_->accumulated_origin();
    EXPECT_NEAR(origin.x, 500.0, 0.01);
}

// 测试 浮动原点：无当不触发变基
TEST_F(FloatingOriginTest, WithoutWhenDoesNotTriggerRebase) {
    World world;
    system_->set_rebase_threshold(100.0f);
    auto e = CreateEntity(world, glm::vec3(9999.0f, 0.0f, 0.0f));

    system_->Tick(world, nullptr, nullptr);

    auto& tf = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(tf.position.x, 9999.0f);

    auto origin = system_->accumulated_origin();
    EXPECT_DOUBLE_EQ(origin.x, 0.0);
}

// 测试 浮动原点：禁用不且
TEST_F(FloatingOriginTest, DisabledNotAnd) {
    World world;
    system_->set_rebase_threshold(100.0f);

    CreateCamera(world, glm::vec3(9999.0f, 0.0f, 0.0f), false);
    auto e = CreateEntity(world, glm::vec3(50.0f, 0.0f, 0.0f));

    system_->Tick(world, nullptr, nullptr);

    auto& tf = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(tf.position.x, 50.0f);
}

// 测试 浮动原点：Prioritypriority
TEST_F(FloatingOriginTest, Prioritypriority) {
    World world;
    system_->set_rebase_threshold(100.0f);

    CreateCamera(world, glm::vec3(50.0f, 0.0f, 0.0f), true, 0);
    CreateCamera(world, glm::vec3(200.0f, 0.0f, 0.0f), true, 10);

    system_->Tick(world, nullptr, nullptr);

    auto origin = system_->accumulated_origin();
    EXPECT_NEAR(origin.x, 200.0, 0.01);
}

// 测试 浮动原点：到绝对转换本地坐标到绝对坐标
TEST_F(FloatingOriginTest, ToAbsoluteConvertLocalCoordinatesToAbsoluteCoordinates) {
    World world;
    system_->set_rebase_threshold(100.0f);
    CreateCamera(world, glm::vec3(200.0f, 0.0f, 0.0f));
    system_->Tick(world, nullptr, nullptr);

    glm::dvec3 abs = system_->ToAbsolute(glm::vec3(50.0f, 0.0f, 0.0f));
    EXPECT_NEAR(abs.x, 250.0, 0.01);
}

// 测试 浮动原点：到本地转换绝对坐标到本地坐标
TEST_F(FloatingOriginTest, ToLocalConvertAbsoluteCoordinatesToLocalCoordinates) {
    World world;
    system_->set_rebase_threshold(100.0f);
    CreateCamera(world, glm::vec3(200.0f, 0.0f, 0.0f));
    system_->Tick(world, nullptr, nullptr);

    glm::vec3 local = system_->ToLocal(glm::dvec3(250.0, 0.0, 0.0));
    EXPECT_NEAR(local.x, 50.0f, 0.01f);
}

// 测试 浮动原点：变基之后Transformmarkdirty
TEST_F(FloatingOriginTest, RebaseAfterTransformmarkdirty) {
    World world;
    system_->set_rebase_threshold(100.0f);

    auto e = CreateEntity(world, glm::vec3(10.0f, 0.0f, 0.0f));
    world.registry().get<TransformComponent>(e).dirty = false;
    CreateCamera(world, glm::vec3(200.0f, 0.0f, 0.0f));

    system_->Tick(world, nullptr, nullptr);

    EXPECT_TRUE(world.registry().get<TransformComponent>(e).dirty);
}

// 测试 浮动原点：事件总线广播原点Rebased事件
TEST_F(FloatingOriginTest, EventBusBroadcastsOriginRebasedEvent) {
    World world;
    core::EventBus bus;
    system_->set_rebase_threshold(100.0f);

    bool received = false;
    glm::vec3 received_offset(0.0f);
    bus.Subscribe<core::OriginRebasedEvent>([&](const core::OriginRebasedEvent& evt) {
        received = true;
        received_offset = evt.offset;
    });

    CreateCamera(world, glm::vec3(200.0f, 0.0f, 0.0f));
    system_->Tick(world, nullptr, &bus);

    EXPECT_TRUE(received);
    EXPECT_NEAR(received_offset.x, 200.0f, 0.01f);
}
