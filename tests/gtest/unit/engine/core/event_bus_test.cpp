/**
 * @file event_bus_test.cpp
 * @brief EventBus 单元测试
 *
 * 覆盖场景：
 * - 事件订阅与发布基本流程
 * - 自定义 EventId (kEventId) 确保跨 DLL 安全
 * - 取消订阅后不再收到事件
 * - 多订阅者独立接收
 * - 通过 ServiceLocator 注入获取 EventBus
 * - 内置事件类型 (UiClickEvent / ResourceLoadedEvent / SceneLifecycleEvent)
 */

#include <gtest/gtest.h>
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/service_locator.h"
#include <string>
#include <vector>

using namespace dse::core;

// ============================================================
// 自定义测试事件
// ============================================================

/// 测试用简单事件，携带整数值
struct TestIntEvent : public Event {
    explicit TestIntEvent(int v) : value(v) {}
    int value = 0;
    static constexpr EventId kEventId = MakeEventId("TestIntEvent");
};

/// 测试用字符串事件
struct TestStringEvent : public Event {
    explicit TestStringEvent(std::string s) : msg(std::move(s)) {}
    std::string msg;
    static constexpr EventId kEventId = MakeEventId("TestStringEvent");
};

// ============================================================
// EventBus 基本订阅/发布
// ============================================================

class EventBusTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().Reset<EventBus>();
    }
};

// 测试 事件总线：之后Publishesreceiveevent
TEST_F(EventBusTest, AfterPublishesreceiveevent) {
    EventBus bus;
    int received = 0;
    bus.Subscribe<TestIntEvent>([&received](const TestIntEvent& e) {
        received = e.value;
    });
    bus.Publish<TestIntEvent>(42);
    EXPECT_EQ(received, 42);
}

// 测试 事件总线：Notevent发布不崩溃
TEST_F(EventBusTest, NoteventPublishesDoesNotCrash) {
    EventBus bus;
    bus.Publish<TestIntEvent>(99);
    SUCCEED();
}

// 测试 事件总线：Multireceiveevent
TEST_F(EventBusTest, Multireceiveevent) {
    EventBus bus;
    int count_a = 0;
    int count_b = 0;
    bus.Subscribe<TestIntEvent>([&count_a](const TestIntEvent&) { ++count_a; });
    bus.Subscribe<TestIntEvent>([&count_b](const TestIntEvent&) { ++count_b; });
    bus.Publish<TestIntEvent>(1);
    EXPECT_EQ(count_a, 1);
    EXPECT_EQ(count_b, 1);
}

// 测试 事件总线：不同事件类型执行不Interfere带Each Other
TEST_F(EventBusTest, DifferentEventTypesDoNotInterfereWithEachOther) {
    EventBus bus;
    int int_received = 0;
    std::string str_received;
    bus.Subscribe<TestIntEvent>([&int_received](const TestIntEvent& e) {
        int_received = e.value;
    });
    bus.Subscribe<TestStringEvent>([&str_received](const TestStringEvent& e) {
        str_received = e.msg;
    });

    bus.Publish<TestIntEvent>(7);
    bus.Publish<TestStringEvent>("hello");

    EXPECT_EQ(int_received, 7);
    EXPECT_EQ(str_received, "hello");
}

// ============================================================
// 取消订阅
// ============================================================

// 测试 事件总线：之后不Againreceiveevent
TEST_F(EventBusTest, AfterNotAgainreceiveevent) {
    EventBus bus;
    int count = 0;
    auto handle = bus.Subscribe<TestIntEvent>([&count](const TestIntEvent&) { ++count; });
    bus.Publish<TestIntEvent>(1);
    EXPECT_EQ(count, 1);

    bus.Unsubscribe(handle);
    bus.Publish<TestIntEvent>(2);
    EXPECT_EQ(count, 1); // 不应再增加
}

// 测试 事件总线：无效句柄不崩溃
TEST_F(EventBusTest, InvalidHandleDoesNotCrash) {
    EventBus bus;
    SubscriptionHandle invalid{0, 0, false};
    bus.Unsubscribe(invalid);
    SUCCEED();
}

// 测试 事件总线：单个不
TEST_F(EventBusTest, OneNot) {
    EventBus bus;
    int count_a = 0;
    int count_b = 0;
    auto handle_a = bus.Subscribe<TestIntEvent>([&count_a](const TestIntEvent&) { ++count_a; });
    bus.Subscribe<TestIntEvent>([&count_b](const TestIntEvent&) { ++count_b; });

    bus.Unsubscribe(handle_a);
    bus.Publish<TestIntEvent>(1);

    EXPECT_EQ(count_a, 0);
    EXPECT_EQ(count_b, 1);
}

// ============================================================
// 内置事件类型
// ============================================================

// 测试 事件总线：UI点击事件发布且接收
TEST_F(EventBusTest, UiClickEventPublishAndReceive) {
    EventBus bus;
    std::uint32_t received_entity = 0;
    bus.Subscribe<UiClickEvent>([&received_entity](const UiClickEvent& e) {
        received_entity = e.entity;
    });
    bus.Publish<UiClickEvent>(12345u);
    EXPECT_EQ(received_entity, 12345u);
}

// 测试 事件总线：资源已加载事件发布且接收
TEST_F(EventBusTest, ResourceLoadedEventPublishAndReceive) {
    EventBus bus;
    std::string received_path;
    bool received_success = false;
    bus.Subscribe<ResourceLoadedEvent>([&](const ResourceLoadedEvent& e) {
        received_path = e.path;
        received_success = e.success;
    });
    bus.Publish<ResourceLoadedEvent>("textures/hero.png", true);
    EXPECT_EQ(received_path, "textures/hero.png");
    EXPECT_TRUE(received_success);
}

// 测试 事件总线：场景生命周期事件发布且接收
TEST_F(EventBusTest, SceneLifecycleEventPublishAndReceive) {
    EventBus bus;
    SceneLifecyclePhase received = SceneLifecyclePhase::Init;
    bus.Subscribe<SceneLifecycleEvent>([&received](const SceneLifecycleEvent& e) {
        received = e.phase;
    });
    bus.Publish<SceneLifecycleEvent>(SceneLifecyclePhase::Shutdown);
    EXPECT_EQ(received, SceneLifecyclePhase::Shutdown);
}

// ============================================================
// EventTraits 反射机制
// ============================================================

// 测试 事件总线：事件Traitsusek事件ID
TEST_F(EventBusTest, EventTraitsusekEventId) {
    // 有 kEventId 的事件应使用其常量
    constexpr EventId ui_click_id = EventTraits<UiClickEvent>::GetId();
    EXPECT_EQ(ui_click_id, UiClickEvent::kEventId);

    constexpr EventId res_loaded_id = EventTraits<ResourceLoadedEvent>::GetId();
    EXPECT_EQ(res_loaded_id, ResourceLoadedEvent::kEventId);
}

// ============================================================
// ServiceLocator 注入获取
// ============================================================

// 测试 事件总线：通道服务定位器注入获取
TEST_F(EventBusTest, PassServiceLocatorInjectsAcquire) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    auto* resolved = ServiceLocator::Instance().Get<EventBus>();
    ASSERT_NE(resolved, nullptr);

    int received = 0;
    resolved->Subscribe<TestIntEvent>([&received](const TestIntEvent& e) {
        received = e.value;
    });
    resolved->Publish<TestIntEvent>(99);
    EXPECT_EQ(received, 99);

    ServiceLocator::Instance().Reset<EventBus>();
}

// 测试 事件总线：实例Compatible Interfaces Work Properly
TEST_F(EventBusTest, InstanceCompatibleInterfacesWorkProperly) {
    // EventBus::Instance() 应能正常工作（兼容接口）
    EventBus& inst = EventBus::Instance();
    int received = 0;
    inst.Subscribe<TestIntEvent>([&received](const TestIntEvent& e) {
        received = e.value;
    });
    inst.Publish<TestIntEvent>(77);
    EXPECT_EQ(received, 77);

    // 清理：取消订阅避免影响其他测试
    // 注意：此处不手动 Unsubscribe，因为每次测试 TearDown 会 Reset ServiceLocator
}

// ============================================================
// OwnerLocator 测试
// ============================================================

class EventBusOwnerLocatorTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().Reset<EventBus>();
    }
};

// 测试 事件总线所有者定位器：默认当所有者定位器为空
TEST_F(EventBusOwnerLocatorTest, DefaultWhenOwnerLocatorIsEmpty) {
    EventBus bus;
    EXPECT_EQ(bus.owner_locator(), nullptr);
}

// 测试 事件总线所有者定位器：实例记录整体Situation服务定位器
TEST_F(EventBusOwnerLocatorTest, InstanceRecordTheWholeSituationServiceLocator) {
    auto& bus = EventBus::Instance();
    EXPECT_EQ(bus.owner_locator(), &ServiceLocator::Instance());
}

// ============================================================
// EventTraits 反射机制 - kEventId 一致性验证
// ============================================================

// 测试 事件Traits：Useevent Insidek事件ID
TEST(EventTraitsTest, UseeventInsidekEventId) {
    EventId id = EventTraits<UiClickEvent>::GetId();
    EXPECT_EQ(id, UiClickEvent::kEventId);
    EXPECT_EQ(id, events::kUiClick);
}

// 测试 事件Traits：资源已加载Eventk事件ID
TEST(EventTraitsTest, ResourceLoadedEventkEventId) {
    EventId id = EventTraits<ResourceLoadedEvent>::GetId();
    EXPECT_EQ(id, ResourceLoadedEvent::kEventId);
    EXPECT_EQ(id, events::kResourceLoaded);
}
