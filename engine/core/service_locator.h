/**
 * @file service_locator.h
 * @brief 服务定位器 - 替代全局单例的依赖管理容器
 *
 * 核心原则：
 * 1. 所有核心服务通过 ServiceLocator 注册和获取
 * 2. 服务生命周期由 EngineInstance 统一管理
 * 3. 支持测试时注入 Mock 实现
 *
 * @example
 * // 注册服务
 * ServiceLocator::Instance().Register<JobSystem>(std::make_shared<JobSystem>());
 *
 * // 获取服务
 * auto* job_sys = ServiceLocator::Instance().Get<JobSystem>();
 *
 * // 测试时注入 Mock
 * ServiceLocator::Instance().Register<IJobSystem>(mock_job_system);
 */

#ifndef DSE_CORE_SERVICE_LOCATOR_H
#define DSE_CORE_SERVICE_LOCATOR_H

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <cassert>
#include <mutex>
#include <utility>

namespace dse {
namespace core {

/**
 * @class ServiceLocator
 * @brief 全局服务容器，管理引擎核心服务的注册、获取和生命周期
 *
 * 设计约束：
 * - ServiceLocator 自身为单例，但其管理的服务不再是单例
 * - 服务通过接口类型注册，支持运行时替换（便于测试和热重载）
 * - 线程安全：注册和获取操作受互斥锁保护
 */
class ServiceLocator {
public:
    /**
     * @brief 获取 ServiceLocator 单例
     * @return ServiceLocator 实例引用
     */
    static ServiceLocator& Instance() {
        static ServiceLocator instance;
        return instance;
    }

    /**
     * @brief 注册服务实例
     * @tparam TInterface 服务接口类型（用于查找）
     * @tparam TImpl 服务实现类型
     * @param service 服务实例的共享指针
     */
    template<typename TInterface, typename TImpl>
    void Register(std::shared_ptr<TImpl> service) {
        static_assert(std::is_base_of_v<TInterface, TImpl> || std::is_same_v<TInterface, TImpl>,
            "TImpl must derive from or be the same as TInterface");
        std::lock_guard<std::mutex> lock(mutex_);
        services_[std::type_index(typeid(TInterface))] = service;
    }

    /**
     * @brief 将当前 locator 中某个服务桥接注册到另一个 locator
     * @tparam TInterface 服务接口类型
     * @param target 目标 locator
     * @return 成功桥接返回 true，未注册返回 false
     */
    template<typename TInterface>
    bool BridgeTo(ServiceLocator& target) const {
        auto service = GetShared<TInterface>();
        if (!service) {
            return false;
        }
        target.Register<TInterface, TInterface>(std::move(service));
        return true;
    }

    /**
     * @brief 便捷注册：直接构造服务实例
     * @tparam TInterface 服务接口类型
     * @tparam TImpl 服务实现类型
     * @tparam Args 构造参数类型
     * @param args 传递给 TImpl 构造函数的参数
     */
    template<typename TInterface, typename TImpl, typename... Args>
    void Emplace(Args&&... args) {
        Register<TInterface, TImpl>(std::make_shared<TImpl>(std::forward<Args>(args)...));
    }

    /**
     * @brief 获取服务实例（原始指针）
     * @tparam TInterface 服务接口类型
     * @return 服务实例指针，未注册时返回 nullptr
     */
    template<typename TInterface>
    TInterface* Get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(std::type_index(typeid(TInterface)));
        if (it == services_.end()) {
            return nullptr;
        }
        auto ptr = std::static_pointer_cast<TInterface>(it->second);
        return ptr.get();
    }

    /**
     * @brief 获取服务实例（共享指针）
     * @tparam TInterface 服务接口类型
     * @return 服务实例的共享指针，未注册时返回 nullptr
     */
    template<typename TInterface>
    std::shared_ptr<TInterface> GetShared() const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(std::type_index(typeid(TInterface)));
        if (it == services_.end()) {
            return nullptr;
        }
        return std::static_pointer_cast<TInterface>(it->second);
    }

    /**
     * @brief 检查服务是否已注册
     * @tparam TInterface 服务接口类型
     * @return 已注册返回 true
     */
    template<typename TInterface>
    bool Has() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.find(std::type_index(typeid(TInterface))) != services_.end();
    }

    /**
     * @brief 重置指定服务（用于测试隔离）
     * @tparam TInterface 服务接口类型
     */
    template<typename TInterface>
    void Reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.erase(std::type_index(typeid(TInterface)));
    }

    /**
     * @brief 重置所有服务（用于 EngineInstance Shutdown）
     */
    void ResetAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.clear();
    }

private:
    ServiceLocator() = default;
    ~ServiceLocator() = default;
    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;

    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
    mutable std::mutex mutex_;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_SERVICE_LOCATOR_H
