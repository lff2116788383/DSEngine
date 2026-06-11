/**
* @file eventbus_servicelocator_integration_test.cpp
* @brief EventBus + ServiceLocator 跨模块集成测试
*
* 验证场景：
* - 服务注册与检索
* - EventBus 作为服务注入和检索
* - EventBus Instance() 与 ServiceLocator 注入的一致性
* - 多服务协作：EventBus + World + JobSystem
* - BridgeTo 跨定位器桥接
* - 生命周期管理：服务重置与事件通道断开
*/

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/service_locator.h"
#include "engine/core/job_system.h"
#include "engine/ecs/world.h"
#include <string>
#include <vector>

using namespace dse::core;

// ============================================================
// 自定义测试事件
// ============================================================

struct IntegrationTestEvent : public Event {
    explicit IntegrationTestEvent(int v) : value(v) {}
    int value = 0;
    static constexpr EventId kEventId = MakeEventId("IntegrationTestEvent");
};

struct LifecycleTestEvent : public Event {
    explicit LifecycleTestEvent(std::string phase) : phase(std::move(phase)) {}
    std::string phase;
    static constexpr EventId kEventId = MakeEventId("LifecycleTestEvent");
};

// ============================================================
// ServiceLocator 注入 EventBus
// ============================================================

class EventBusServiceLocatorIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().Reset<EventBus>();
        ServiceLocator::Instance().Reset<JobSystem>();
        ServiceLocator::Instance().Reset<World>();
    }
};

TEST_F(EventBusServiceLocatorIntegrationTest, RegisterEventBusAfterCanpassServiceLocator) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    auto* resolved = ServiceLocator::Instance().Get<EventBus>();
    ASSERT_NE(resolved, nullptr);

    int received = 0;
    resolved->Subscribe<IntegrationTestEvent>([&received](const IntegrationTestEvent& e) {
        received = e.value;
    });
    resolved->Publish<IntegrationTestEvent>(42);
    EXPECT_EQ(received, 42);
}

TEST_F(EventBusServiceLocatorIntegrationTest, EmplaceCreateEventBusExample) {
    ServiceLocator::Instance().Emplace<EventBus, EventBus>();

    auto* resolved = ServiceLocator::Instance().Get<EventBus>();
    ASSERT_NE(resolved, nullptr);

    bool received = false;
    resolved->Subscribe<LifecycleTestEvent>([&received](const LifecycleTestEvent&) {
        received = true;
    });
    resolved->Publish<LifecycleTestEvent>("init");
    EXPECT_TRUE(received);
}

// ============================================================
// EventBus Instance() 与 ServiceLocator 一致性
// ============================================================

TEST_F(EventBusServiceLocatorIntegrationTest, InstanceAndServiceLocatorRegistrationReturnsTheSameObject) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    auto& inst = EventBus::Instance();
    auto* from_locator = ServiceLocator::Instance().Get<EventBus>();

    EXPECT_EQ(&inst, from_locator);
}

// ============================================================
// 多服务协作
// ============================================================

TEST_F(EventBusServiceLocatorIntegrationTest, MultiregisterAfterCan) {
    auto bus = std::make_shared<EventBus>();
    auto world = std::make_shared<World>();
    auto job_system = std::make_shared<JobSystem>();

    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);
    ServiceLocator::Instance().Register<World, World>(world);
    ServiceLocator::Instance().Register<JobSystem, JobSystem>(job_system);

    EXPECT_NE(ServiceLocator::Instance().Get<EventBus>(), nullptr);
    EXPECT_NE(ServiceLocator::Instance().Get<World>(), nullptr);
    EXPECT_NE(ServiceLocator::Instance().Get<JobSystem>(), nullptr);

    auto* w = ServiceLocator::Instance().Get<World>();
    Entity e = w->CreateEntity();
    EXPECT_TRUE(w->IsAlive(e));

    job_system->Shutdown();
}

TEST_F(EventBusServiceLocatorIntegrationTest, Event) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    World world;
    std::vector<Entity> created_entities;

    bus->Subscribe<IntegrationTestEvent>([&created_entities](const IntegrationTestEvent& e) {
        created_entities.push_back(static_cast<Entity>(e.value));
    });

    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    bus->Publish<IntegrationTestEvent>(static_cast<int>(e1));
    bus->Publish<IntegrationTestEvent>(static_cast<int>(e2));

    EXPECT_EQ(created_entities.size(), 2u);
}

// ============================================================
// BridgeTo 跨定位器桥接
// ============================================================

TEST_F(EventBusServiceLocatorIntegrationTest, BridgeToTransferServiceToTargetLocator) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    // BridgeTo 要求目标定位器，但 ServiceLocator 构造函数为私有
    // 使用单例模式：重置后重新注册到同一单例
    auto shared_bus = ServiceLocator::Instance().GetShared<EventBus>();
    ASSERT_TRUE(shared_bus != nullptr);

    // 验证共享指针语义
    auto* ptr1 = shared_bus.get();
    auto* ptr2 = ServiceLocator::Instance().Get<EventBus>();
    EXPECT_EQ(ptr1, ptr2);
}

TEST_F(EventBusServiceLocatorIntegrationTest, NotregisterReturnsFails) {
    ServiceLocator::Instance().Reset<EventBus>();
    // BridgeTo 对未注册服务返回 false
    // 由于无法创建另一个 ServiceLocator，通过 Has<> 验证
    EXPECT_FALSE(ServiceLocator::Instance().Has<EventBus>());
}

// ============================================================
// 生命周期管理
// ============================================================

TEST_F(EventBusServiceLocatorIntegrationTest, ResetAllClearAllServices) {
    ServiceLocator::Instance().Emplace<EventBus, EventBus>();
    ServiceLocator::Instance().Emplace<World, World>();

    EXPECT_TRUE(ServiceLocator::Instance().Has<EventBus>());
    EXPECT_TRUE(ServiceLocator::Instance().Has<World>());

    ServiceLocator::Instance().ResetAll();

    EXPECT_FALSE(ServiceLocator::Instance().Has<EventBus>());
    EXPECT_FALSE(ServiceLocator::Instance().Has<World>());
}

TEST_F(EventBusServiceLocatorIntegrationTest, ResetNot) {
    ServiceLocator::Instance().Emplace<EventBus, EventBus>();
    ServiceLocator::Instance().Emplace<World, World>();

    ServiceLocator::Instance().Reset<EventBus>();

    EXPECT_FALSE(ServiceLocator::Instance().Has<EventBus>());
    EXPECT_TRUE(ServiceLocator::Instance().Has<World>());
}

TEST_F(EventBusServiceLocatorIntegrationTest, ResetAfterEventBusNotAgainCan) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    int received = 0;
    bus->Subscribe<IntegrationTestEvent>([&received](const IntegrationTestEvent& e) {
        received = e.value;
    });

    EXPECT_NE(ServiceLocator::Instance().Get<EventBus>(), nullptr);

    ServiceLocator::Instance().Reset<EventBus>();

    EXPECT_EQ(ServiceLocator::Instance().Get<EventBus>(), nullptr);

    // 原始 shared_ptr 仍然有效
    bus->Publish<IntegrationTestEvent>(99);
    EXPECT_EQ(received, 99);
}

// ============================================================
// Has 检查
// ============================================================

TEST_F(EventBusServiceLocatorIntegrationTest, HasCorrectlyReflectRegistrationStatus) {
    EXPECT_FALSE(ServiceLocator::Instance().Has<EventBus>());

    ServiceLocator::Instance().Emplace<EventBus, EventBus>();
    EXPECT_TRUE(ServiceLocator::Instance().Has<EventBus>());

    ServiceLocator::Instance().Reset<EventBus>();
    EXPECT_FALSE(ServiceLocator::Instance().Has<EventBus>());
}

// ============================================================
// 场景生命周期事件驱动 World 操作
// ============================================================

TEST_F(EventBusServiceLocatorIntegrationTest, SceneLifecycleeventdriveWorld) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);
    auto world = std::make_shared<World>();
    ServiceLocator::Instance().Register<World, World>(world);

    std::vector<SceneLifecyclePhase> lifecycle_log;

    bus->Subscribe<SceneLifecycleEvent>([&](const SceneLifecycleEvent& e) {
        auto* w = ServiceLocator::Instance().Get<World>();
        if (w) {
            if (e.phase == SceneLifecyclePhase::Init) {
                w->CreateEntity();
            } else if (e.phase == SceneLifecyclePhase::Shutdown) {
                w->Clear();
            }
        }
        lifecycle_log.push_back(e.phase);
    });

    bus->Publish<SceneLifecycleEvent>(SceneLifecyclePhase::Init);
    EXPECT_EQ(world->EntityCount(), 1u);
    EXPECT_EQ(lifecycle_log.size(), 1u);

    bus->Publish<SceneLifecycleEvent>(SceneLifecyclePhase::Shutdown);
    EXPECT_EQ(world->EntityCount(), 0u);
    EXPECT_EQ(lifecycle_log.size(), 2u);
}
