/**
 * @file job_system.h
 * @brief 引擎作业系统，提供异步任务执行能力
 *
 * 改进点：
 * - 从纯静态单例改为实例化类，可通过 ServiceLocator 管理
 * - 保留静态便捷接口，内部委托到 ServiceLocator 获取实例
 * - 支持 EngineInstance 统一管理生命周期
 */

#ifndef DSE_CORE_JOB_SYSTEM_H
#define DSE_CORE_JOB_SYSTEM_H

#include <functional>
#include <future>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace dse {
namespace core {

class ServiceLocator;

/**
 * @class JobSystem
 * @brief 作业系统，提供基于线程池的异步任务执行能力
 *
 * 生命周期：
 * - 由 EngineInstance 通过 ServiceLocator 创建和销毁
 * - 旧静态接口 Init/Shutdown 保留兼容，内部委托到 ServiceLocator 管理的实例
 *
 * @example
 * // 新用法（推荐）
 * auto* job_sys = ServiceLocator::Instance().Get<JobSystem>();
 * job_sys->Execute(my_task);
 *
 * // 旧用法（兼容）
 * JobSystem::InitStatic();
 * JobSystem::ExecuteStatic(my_task);
 * JobSystem::ShutdownStatic();
 */
class JobSystem {
public:
    JobSystem() = default;
    ~JobSystem();

    // 禁止拷贝和移动
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    /**
     * @brief 初始化线程池
     */
    void Init();

    /**
     * @brief 关闭线程池，等待所有任务完成
     */
    void Shutdown();

    /**
     * @brief 提交异步任务到线程池
     * @param job 待执行的任务函数
     *
     * @note 若线程池未初始化或已关闭，任务将在调用线程同步执行
     */
    void Execute(const std::function<void()>& job);

    // --- 旧静态接口（兼容过渡，标记为 deprecated） ---

    /**
     * @brief 初始化 JobSystem（兼容旧接口）
     * @deprecated 使用 ServiceLocator 管理 JobSystem 生命周期
     */
    static void InitStatic();

    /**
     * @brief 关闭 JobSystem（兼容旧接口）
     * @deprecated 使用 ServiceLocator 管理 JobSystem 生命周期
     */
    static void ShutdownStatic();

    /**
     * @brief 提交异步任务（兼容旧接口）
     * @deprecated 通过 ServiceLocator 获取实例后调用 Execute
     */
    static void ExecuteStatic(const std::function<void()>& job);

private:
    /// 线程池工作线程，仅在 is_initialized_ 为 true 时存在。
    std::vector<std::thread> workers_;
    /// 待执行任务队列，由 queue_mutex_ 保护。
    std::queue<std::function<void()>> job_queue_;
    /// 保护线程池状态与任务队列的一致性。
    std::mutex queue_mutex_;
    /// 用于唤醒等待中的工作线程。
    std::condition_variable condition_;
    /// 标记线程池是否正在关闭，避免关闭过程中继续接收异步任务。
    bool is_stopping_ = false;
    /// 标记线程池是否已成功初始化，作为 Execute 的异步分发前置条件。
    bool is_initialized_ = false;
};

// 旧接口兼容宏：保持调用方式不变
// 后续版本将移除这些兼容别名
#define JobSystem_Init()       dse::core::JobSystem::InitStatic()
#define JobSystem_Shutdown()   dse::core::JobSystem::ShutdownStatic()
#define JobSystem_Execute(j)   dse::core::JobSystem::ExecuteStatic(j)

} // namespace core
} // namespace dse

#endif // DSE_CORE_JOB_SYSTEM_H

