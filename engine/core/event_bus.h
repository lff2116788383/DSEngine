/**
 * @file event_bus.h
 * @brief 核心事件总线系统，提供跨模块、类型安全的发布-订阅机制
 *
 * 改进点：
 * - 使用 EventId (编译期 FNV-1a 哈希) 替代 std::type_index，确保跨 DLL 安全
 * - 模板接口保持不变，Subscribe/Publish 用法无需改动
 */

#ifndef DSE_CORE_EVENT_BUS_H
#define DSE_CORE_EVENT_BUS_H

#include "engine/core/event_id.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <string>
#include <cstdint>
#include <utility>
#include "engine/core/service_locator.h"

namespace dse {
namespace core {

class ServiceLocator;

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

    static constexpr EventId kEventId = events::kUiClick;
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

    static constexpr EventId kEventId = events::kResourceLoaded;
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

    static constexpr EventId kEventId = events::kSceneLifecycle;
};

/**
 * @struct SubSceneLoadedEvent
 * @brief 子场景加载完成事件
 */
struct SubSceneLoadedEvent : public Event {
    explicit SubSceneLoadedEvent(const std::string& scene_path) : path(scene_path) {}
    std::string path;
    static constexpr EventId kEventId = events::kSubSceneLoaded;
};

/**
 * @struct SubSceneUnloadedEvent
 * @brief 子场景卸载完成事件
 */
struct SubSceneUnloadedEvent : public Event {
    explicit SubSceneUnloadedEvent(const std::string& scene_path) : path(scene_path) {}
    std::string path;
    static constexpr EventId kEventId = events::kSubSceneUnloaded;
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

/// 事件类型特征：获取 TEvent 对应的 EventId
template<typename TEvent>
struct EventTraits {
    static constexpr EventId GetId() {
        // 优先使用事件类内定义的 kEventId 常量
        if constexpr (requires { TEvent::kEventId; }) {
            return TEvent::kEventId;
        } else {
            // 回退：基于类型名的编译期哈希（不应走到此路径，新事件应定义 kEventId）
            return MakeEventId(__FUNCSIG__);
        }
    }
};

struct SubscriptionHandle {
    EventId event_id = 0;     ///< 事件类型 ID（跨 DLL 安全）
    std::size_t id = 0;       ///< 订阅唯一序号
    bool valid = false;
};

/**
 * @class EventBus
 * @brief 事件总线，负责事件的分发和订阅管理
 *
 * 生命周期管理：
 * - 推荐通过 ServiceLocator 获取：ServiceLocator::Instance().Get<EventBus>()
 * - Instance() 保留作为兼容过渡，委托到 ServiceLocator
 * - 由 EngineInstance 统一管理初始化和销毁
 *
 * @example
 * // 新用法（推荐）
 * auto* bus = ServiceLocator::Instance().Get<EventBus>();
 * bus->Publish<UiClickEvent>(entity_id);
 *
 * // 旧用法（兼容）
 * EventBus::Instance().Publish<UiClickEvent>(entity_id);
 */
class EventBus {
public:
    EventBus() = default;
    explicit EventBus(ServiceLocator* owner_locator) : owner_locator_(owner_locator) {}
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /**
     * @brief 获取 EventBus 实例（兼容过渡，委托到 ServiceLocator）
     * @return EventBus 实例引用
     * @deprecated 通过 ServiceLocator::Instance().Get<EventBus>() 获取
     */
    static EventBus& Instance() {
        auto& locator = ServiceLocator::Instance();
        auto* existing = locator.Get<EventBus>();
        if (existing) {
            existing->SetOwnerLocator(&locator);
            return *existing;
        }

        auto created = std::make_shared<EventBus>(&locator);
        locator.Register<EventBus, EventBus>(created);
        return *created;
    }

    void SetOwnerLocator(ServiceLocator* owner_locator) {
        owner_locator_ = owner_locator;
    }

    ServiceLocator* owner_locator() const {
        return owner_locator_;
    }

    template<typename TEvent>
    /**
     * @brief 订阅特定类型的事件
     * @param callback 事件触发时的回调函数
     * @return 订阅句柄，用于后续取消订阅
     */
    SubscriptionHandle Subscribe(std::function<void(const TEvent&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        EventId eid = EventTraits<TEvent>::GetId();
        auto id = next_subscription_id_++;
        callbacks_[eid].push_back({
            id,
            std::make_shared<EventCallback<TEvent>>(std::move(callback))
        });
        return {eid, id, true};
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
        auto it = callbacks_.find(handle.event_id);
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
        PublishEvent(&event, EventTraits<TEvent>::GetId());
    }

private:
    struct CallbackEntry {
        std::size_t id = 0;
        std::shared_ptr<IEventCallback> callback;
    };

    /**
     * @brief 按事件 ID 查找订阅者并派发事件
     * @param event 事件基类指针
     * @param event_id 事件类型 ID
     */
    void PublishEvent(Event* event, EventId event_id) {
        std::vector<std::shared_ptr<IEventCallback>> call_list;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = callbacks_.find(event_id);
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

    std::unordered_map<EventId, std::vector<CallbackEntry>> callbacks_;
    std::atomic<std::size_t> next_subscription_id_{1};
    std::mutex mutex_;
    ServiceLocator* owner_locator_ = nullptr;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_EVENT_BUS_H
