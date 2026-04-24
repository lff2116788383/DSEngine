/**
 * @file event_bus.cpp
 * @brief EventBus 实现 - Instance() 兼容接口委托到 ServiceLocator
 */

#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"
#include <stdexcept>

namespace dse {
namespace core {

EventBus& EventBus::Instance() {
    // 委托到 ServiceLocator；兼容入口不再隐式创建实例，避免绕开 EngineInstance 装配链。
    auto& locator = ServiceLocator::Instance();
    auto* existing = locator.Get<EventBus>();
    if (!existing) {
        throw std::runtime_error("EventBus::Instance() requires an EngineInstance-managed EventBus registration");
    }
    return *existing;
}

} // namespace core
} // namespace dse
