/**
 * @file event_id_test.cpp
 * @brief EventId 和 EventBus 的单元测试
 *
 * 覆盖场景：
 * - FNV-1a 编译期哈希的一致性
 * - EventId 常量跨编译单元稳定性
 * - EventBus 发布/订阅/取消订阅
 * - EventBus 跨 DLL 安全 EventId 替代 type_index
 */

#include <gtest/gtest.h>
#include "engine/core/event_id.h"
#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"

using namespace dse::core;

// ============================================================
// EventId 测试
// ============================================================

TEST(EventIdTest, Fnv1aHash一致性) {
    // 相同字符串必须产生相同哈希值
    constexpr EventId id1 = MakeEventId("TestEvent");
    constexpr EventId id2 = MakeEventId("TestEvent");
    EXPECT_EQ(id1, id2);
}

TEST(EventIdTest, Fnv1aHash不同字符串产生不同值) {
    constexpr EventId id1 = MakeEventId("EventA");
    constexpr EventId id2 = MakeEventId("EventB");
    EXPECT_NE(id1, id2);
}

TEST(EventIdTest, 空字符串哈希等于FNV偏移基值) {
    constexpr EventId empty_hash = MakeEventId("");
    constexpr EventId fnv_offset = 0xcbf29ce484222325ull;
    EXPECT_EQ(empty_hash, fnv_offset);
}

TEST(EventIdTest, 预定义事件ID非零) {
    EXPECT_NE(events::kUiClick, 0u);
    EXPECT_NE(events::kResourceLoaded, 0u);
    EXPECT_NE(events::kSceneLifecycle, 0u);
}

TEST(EventIdTest, 预定义事件ID互不相等) {
    EXPECT_NE(events::kUiClick, events::kResourceLoaded);
    EXPECT_NE(events::kUiClick, events::kSceneLifecycle);
    EXPECT_NE(events::kResourceLoaded, events::kSceneLifecycle);
}

TEST(EventIdTest, 编译期求值) {
    // 验证 MakeEventId 可以在编译期使用（constexpr）
    static_assert(MakeEventId("CompileTime") != 0, "MakeEventId must be constexpr");
    SUCCEED();
}

// ============================================================
// EventBus 测试
// ============================================================

class EventBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前重置 ServiceLocator 中的 EventBus
        ServiceLocator::Instance().Reset<EventBus>();
    }

    void TearDown() override {
        ServiceLocator::Instance().Reset<EventBus>();
    }
};

class EventBusOwnerLocatorTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().Reset<EventBus>();
    }
};

TEST_F(EventBusTest, 发布无订阅者不崩溃) {
    EventBus bus;
    // 无订阅者时发布事件不应崩溃
    bus.Publish<UiClickEvent>(42u);
    SUCCEED();
}

TEST_F(EventBusTest, 订阅并接收事件) {
    EventBus bus;
    std::uint32_t received_entity = 0;

    bus.Subscribe<UiClickEvent>([&received_entity](const UiClickEvent& e) {
        received_entity = e.entity;
    });

    bus.Publish<UiClickEvent>(123u);
    EXPECT_EQ(received_entity, 123u);
}

TEST_F(EventBusTest, 多个订阅者均收到事件) {
    EventBus bus;
    int count_a = 0;
    int count_b = 0;

    bus.Subscribe<UiClickEvent>([&count_a](const UiClickEvent&) { ++count_a; });
    bus.Subscribe<UiClickEvent>([&count_b](const UiClickEvent&) { ++count_b; });

    bus.Publish<UiClickEvent>(1u);
    EXPECT_EQ(count_a, 1);
    EXPECT_EQ(count_b, 1);
}

TEST_F(EventBusTest, 取消订阅后不再收到事件) {
    EventBus bus;
    int count = 0;

    auto handle = bus.Subscribe<UiClickEvent>([&count](const UiClickEvent&) { ++count; });
    bus.Publish<UiClickEvent>(1u);
    EXPECT_EQ(count, 1);

    bus.Unsubscribe(handle);
    bus.Publish<UiClickEvent>(2u);
    EXPECT_EQ(count, 1); // 取消后计数不变
}

TEST_F(EventBusTest, 不同事件类型互不干扰) {
    EventBus bus;
    std::uint32_t ui_entity = 0;
    bool resource_loaded = false;

    bus.Subscribe<UiClickEvent>([&ui_entity](const UiClickEvent& e) { ui_entity = e.entity; });
    bus.Subscribe<ResourceLoadedEvent>([&resource_loaded](const ResourceLoadedEvent& e) { resource_loaded = e.success; });

    bus.Publish<UiClickEvent>(999u);
    EXPECT_EQ(ui_entity, 999u);
    EXPECT_FALSE(resource_loaded); // UI 事件不应触发资源回调

    bus.Publish<ResourceLoadedEvent>("test.png", true);
    EXPECT_TRUE(resource_loaded);
}

TEST_F(EventBusTest, Instance委托到ServiceLocator) {
    // 通过 Instance() 获取的 EventBus 应该是同一个实例
    auto& bus1 = EventBus::Instance();
    auto& bus2 = EventBus::Instance();
    EXPECT_EQ(&bus1, &bus2);
}

TEST_F(EventBusTest, 无效句柄取消不崩溃) {
    EventBus bus;
    SubscriptionHandle invalid_handle{0, 0, false};
    bus.Unsubscribe(invalid_handle);
    SUCCEED();
}

TEST_F(EventBusOwnerLocatorTest, 默认构造时OwnerLocator为空) {
    EventBus bus;
    EXPECT_EQ(bus.owner_locator(), nullptr);
}

TEST_F(EventBusOwnerLocatorTest, Instance记录全局ServiceLocator) {
    auto& bus = EventBus::Instance();
    EXPECT_EQ(bus.owner_locator(), &ServiceLocator::Instance());
}

TEST_F(EventBusTest, SceneLifecycleEvent订阅发布) {
    EventBus bus;
    SceneLifecyclePhase received_phase = SceneLifecyclePhase::Init;

    bus.Subscribe<SceneLifecycleEvent>([&received_phase](const SceneLifecycleEvent& e) {
        received_phase = e.phase;
    });

    bus.Publish<SceneLifecycleEvent>(SceneLifecyclePhase::Shutdown);
    EXPECT_EQ(received_phase, SceneLifecyclePhase::Shutdown);
}

// ============================================================
// EventTraits 测试 - 验证 kEventId 优先回退机制
// ============================================================

TEST(EventTraitsTest, 使用事件类内kEventId) {
    EventId id = EventTraits<UiClickEvent>::GetId();
    EXPECT_EQ(id, UiClickEvent::kEventId);
    EXPECT_EQ(id, events::kUiClick);
}

TEST(EventTraitsTest, ResourceLoadedEvent的kEventId) {
    EventId id = EventTraits<ResourceLoadedEvent>::GetId();
    EXPECT_EQ(id, ResourceLoadedEvent::kEventId);
    EXPECT_EQ(id, events::kResourceLoaded);
}
