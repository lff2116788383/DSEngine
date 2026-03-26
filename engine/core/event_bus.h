/**
 * @file event_bus.h
 * @brief 核心事件总线系统，提供跨模块、类型安全的发布-订阅机制
 */

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

/**
 * @class Event
 * @brief 所有系统事件的基类，业务事件应继承此基类
 */
class Event {
public:
    virtual ~Event() = default;
};

struct UiClickEvent : public Event {
    /**
     * @brief UI点击事件构造函数
     * @param entity_id 被点击的 UI 实体 ID
     */
    explicit UiClickEvent(std::uint32_t entity_id) : entity(entity_id) {}
    std::uint32_t entity = 0;
};

struct ResourceLoadedEvent : public Event {
    /**
     * @brief 资源加载完毕事件构造函数
     * @param resource_path 资源路径
     * @param loaded 是否加载成功
     */
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
    /**
     * @brief 场景生命周期事件构造函数
     * @param lifecycle_phase 当前阶段(Init/Shutdown)
     */
    explicit SceneLifecycleEvent(SceneLifecyclePhase lifecycle_phase) : phase(lifecycle_phase) {}
    SceneLifecyclePhase phase = SceneLifecyclePhase::Init;
};

/**
 * @class IEventCallback
 * @brief 事件回调接口基类
 */
class IEventCallback {
public:
    virtual ~IEventCallback() = default;
    /**
     * @brief 执行具体的回调调用
     * @param event 触发的事件基类指针
     */
    virtual void Call(Event* event) = 0;
};

template<typename TEvent>
/**
 * @class EventCallback
 * @brief 类型安全的事件回调包装器
 */
class EventCallback : public IEventCallback {
public:
    using CallbackFunc = std::function<void(const TEvent&)>;

    explicit EventCallback(CallbackFunc callback) : callback_(std::move(callback)) {}

    /**
     * @brief 将基类指针安全转换为子类后触发实际的回调
     * @param event 事件基类指针
     */
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

/**
 * @class EventBus
 * @brief 全局事件总线，负责事件的分发和订阅管理
 */
class EventBus {
public:
    /**
     * @brief 获取 EventBus 单例
     * @return EventBus 实例引用
     */
    static EventBus& Instance() {
        static EventBus instance;
        return instance;
    }

    template<typename TEvent>
    /**
     * @brief 订阅特定类型的事件
     * @param callback 事件触发时的回调函数
     * @return 订阅句柄，用于后续取消订阅
     */
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

    /**
     * @brief 取消已订阅的事件
     * @param handle 订阅时返回的句柄
     */
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
    /**
     * @brief 触发并发布事件
     * @param args 传递给事件构造函数的参数
     */
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

    /**
     * @brief 执行 PublishEvent 操作
     * @param event 参数说明
     * @param type_idx 参数说明
     */
    void PublishEvent(Event* event, std::type_index type_idx) {
        std::vector<std::shared_ptr<IEventCallback>> call_list;
        {
    /**
     * @brief 执行 lock 操作
     * @param mutex_ 参数说明
     * @return std::lock_guard<std::mutex> 返回值说明
     */
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
