#ifndef DSE_CORE_EVENT_BUS_H
#define DSE_CORE_EVENT_BUS_H

#include <entt/entt.hpp>

namespace core {

class EventBus {
public:
    static entt::dispatcher& Instance() {
        static entt::dispatcher dispatcher;
        return dispatcher;
    }
};

} // namespace core

#endif // DSE_CORE_EVENT_BUS_H
