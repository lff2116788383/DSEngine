#ifndef DSE_CORE_EVENT_BUS_H
#define DSE_CORE_EVENT_BUS_H

#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <string>
#include <cstdint>
#include <utility>

namespace dse {
namespace core {

class Event {
public:
    virtual ~Event() = default;
};

struct UiClickEvent : public Event {
    explicit UiClickEvent(std::uint32_t entity_id) : entity(entity_id) {}
    std::uint32_t entity = 0;
};

struct ResourceLoadedEvent : public Event {
    ResourceLoadedEvent(std::string resource_path, bool loaded)
        : path(std::move(resource_path)), success(loaded) {}
    std::string path;
    bool success = false;
};

enum class SceneLifecyclePhase {
    Init,
    Shutdown
};

struct SceneLifecycleEvent : public Event {
    explicit SceneLifecycleEvent(SceneLifecyclePhase lifecycle_phase) : phase(lifecycle_phase) {}
    SceneLifecyclePhase phase = SceneLifecyclePhase::Init;
};

class IEventCallback {
public:
    virtual ~IEventCallback() = default;
    virtual void Call(Event* event) = 0;
};

template<typename TEvent>
class EventCallback : public IEventCallback {
public:
    using CallbackFunc = std::function<void(const TEvent&)>;

    explicit EventCallback(CallbackFunc callback) : callback_(std::move(callback)) {}

    void Call(Event* event) override {
        callback_(*static_cast<TEvent*>(event));
    }

private:
    CallbackFunc callback_;
};

struct SubscriptionHandle {
    std::type_index type = std::type_index(typeid(void));
    std::size_t id = 0;
    bool valid = false;
};

class EventBus {
public:
    static EventBus& Instance() {
        static EventBus instance;
        return instance;
    }

    template<typename TEvent>
    SubscriptionHandle Subscribe(std::function<void(const TEvent&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto type_idx = std::type_index(typeid(TEvent));
        auto id = next_subscription_id_++;
        callbacks_[type_idx].push_back({
            id,
            std::make_shared<EventCallback<TEvent>>(std::move(callback))
        });
        return {type_idx, id, true};
    }

    void Unsubscribe(const SubscriptionHandle& handle) {
        if (!handle.valid) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = callbacks_.find(handle.type);
        if (it == callbacks_.end()) {
            return;
        }
        auto& entries = it->second;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const CallbackEntry& entry) {
            return entry.id == handle.id;
        }), entries.end());
        if (entries.empty()) {
            callbacks_.erase(it);
        }
    }

    template<typename TEvent, typename... Args>
    void Publish(Args&&... args) {
        TEvent event(std::forward<Args>(args)...);
        PublishEvent(&event, std::type_index(typeid(TEvent)));
    }

private:
    struct CallbackEntry {
        std::size_t id = 0;
        std::shared_ptr<IEventCallback> callback;
    };

    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    void PublishEvent(Event* event, std::type_index type_idx) {
        std::vector<std::shared_ptr<IEventCallback>> call_list;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = callbacks_.find(type_idx);
            if (it == callbacks_.end()) {
                return;
            }
            call_list.reserve(it->second.size());
            for (auto& entry : it->second) {
                call_list.push_back(entry.callback);
            }
        }
        for (auto& callback : call_list) {
            callback->Call(event);
        }
    }

    std::unordered_map<std::type_index, std::vector<CallbackEntry>> callbacks_;
    std::atomic<std::size_t> next_subscription_id_{1};
    std::mutex mutex_;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_EVENT_BUS_H
