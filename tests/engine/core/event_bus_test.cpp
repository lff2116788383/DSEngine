#include "catch/catch.hpp"
#include "engine/core/event_bus.h"

using dse::core::EventBus;
using dse::core::UiClickEvent;
using dse::core::ResourceLoadedEvent;

// 正向测试：订阅后发布事件应触发回调并携带正确载荷。
TEST_CASE("Given_SubscribedHandler_When_PublishEvent_Then_CallbackReceivesExpectedPayload", "[engine][unit][event_bus]") {
    auto& bus = EventBus::Instance();
    int received_entity = -1;
    auto handle = bus.Subscribe<UiClickEvent>([&](const UiClickEvent& e) {
        received_entity = static_cast<int>(e.entity);
    });

    bus.Publish<UiClickEvent>(42u);
    REQUIRE(received_entity == 42);

    bus.Unsubscribe(handle);
}

// 边界测试：取消订阅后再次发布事件不应再触发已移除回调。
TEST_CASE("Given_UnsubscribedHandler_When_PublishEvent_Then_CallbackIsNotInvoked", "[engine][unit][event_bus]") {
    auto& bus = EventBus::Instance();
    int invoke_count = 0;
    auto handle = bus.Subscribe<ResourceLoadedEvent>([&](const ResourceLoadedEvent&) {
        ++invoke_count;
    });
    bus.Publish<ResourceLoadedEvent>("data/a.png", true);
    REQUIRE(invoke_count == 1);

    bus.Unsubscribe(handle);
    bus.Publish<ResourceLoadedEvent>("data/b.png", true);
    REQUIRE(invoke_count == 1);
}

// 反向测试：无效句柄取消订阅不应导致崩溃，也不应影响后续事件分发。
TEST_CASE("Given_InvalidHandle_When_Unsubscribe_Then_NoSideEffectsOnDispatch", "[engine][unit][event_bus]") {
    auto& bus = EventBus::Instance();
    const dse::core::SubscriptionHandle invalid_handle{};
    bus.Unsubscribe(invalid_handle);

    int invoke_count = 0;
    auto handle = bus.Subscribe<UiClickEvent>([&](const UiClickEvent&) {
        ++invoke_count;
    });
    bus.Publish<UiClickEvent>(7u);
    REQUIRE(invoke_count == 1);

    bus.Unsubscribe(handle);
}
