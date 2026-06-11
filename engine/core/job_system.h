/**
* @file job_system.h
* @brief 引擎作业系统，提供异步任务执行、优先级调度与任务依赖能力
*
* 能力概览：
* - 基于线程池的异步任务执行
* - 三级优先级（High/Normal/Low），高优先级任务优先出队
* - JobHandle 支持等待完成与任务依赖链
* - 每线程本地队列 + 工作窃取，减少竞争提升吞吐
* - 通过 ServiceLocator 管理生命周期
*/

#ifndef DSE_CORE_JOB_SYSTEM_H
#define DSE_CORE_JOB_SYSTEM_H

#include <functional>
#include <future>
#include <vector>
#include <thread>
#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include "engine/core/dse_export.h"
#include "engine/core/object_pool.h"

namespace dse {
namespace core {

class ServiceLocator;

/// 任务优先级
enum class JobPriority : uint8_t {
    Low    = 0,  ///< 后台加载、资源预热等非关键任务
    Normal = 1,  ///< 常规异步任务（默认）
    High   = 2,  ///< 帧内关键路径任务（渲染提交、物理同步等）
};

/// 任务句柄，用于等待完成和声明依赖关系
class JobHandle {
public:
    JobHandle() = default;
    explicit JobHandle(uint64_t id) : id_(id) {}

    uint64_t id() const { return id_; }
    bool is_valid() const { return id_ != 0; }
    bool operator==(const JobHandle& other) const { return id_ == other.id_; }
    bool operator!=(const JobHandle& other) const { return id_ != other.id_; }
    explicit operator bool() const { return is_valid(); }

private:
    uint64_t id_ = 0;
};

/// JobHandle 哈希，用于 unordered_map/unordered_set
struct JobHandleHash {
    size_t operator()(const JobHandle& h) const noexcept {
        return std::hash<uint64_t>{}(h.id());
    }
};

/**
* @class JobSystem
* @brief 作业系统，提供基于线程池的异步任务执行、优先级调度与依赖管理
*
* 生命周期：
* - 由 EngineInstance 通过 ServiceLocator 创建和销毁
* - 通过 ServiceLocator::Instance().Get<JobSystem>() 获取实例
*
* @example
* // 基本用法
* auto* job_sys = ServiceLocator::Instance().Get<JobSystem>();
* job_sys->Execute(my_task);
*
* @example
* // 带优先级
* auto handle = job_sys->Submit(my_task, JobPriority::High);
*
* @example
* // 带依赖
* auto physics = job_sys->Submit(physics_task, JobPriority::High);
* auto render  = job_sys->SubmitWithDependency(render_task, {physics}, JobPriority::Normal);
* job_sys->Wait(render);
*/
class DSE_EXPORT JobSystem {
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
    * @brief 提交异步任务到线程池（兼容旧接口，Normal 优先级）
    * @param job 待执行的任务函数
    *
    * @note 若线程池未初始化或已关闭，任务将在调用线程同步执行
    */
    void Execute(const std::function<void()>& job);

    /**
    * @brief 提交带优先级的异步任务
    * @param job 待执行的任务函数
    * @param priority 任务优先级
    * @return JobHandle 任务句柄，可用于 Wait / SubmitWithDependency
    *
    * @note 若线程池未初始化或已关闭，任务将同步执行并返回无效句柄
    */
    JobHandle Submit(const std::function<void()>& job,
                     JobPriority priority = JobPriority::Normal);

    /**
    * @brief 提交带依赖的异步任务，所有依赖完成后才执行
    * @param job 待执行的任务函数
    * @param dependencies 依赖的任务句柄列表
    * @param priority 任务优先级
    * @return JobHandle 任务句柄
    *
    * @note 若任一依赖无效（如已回收），该依赖视为已满足
    */
    JobHandle SubmitWithDependency(const std::function<void()>& job,
                                    const std::vector<JobHandle>& dependencies,
                                    JobPriority priority = JobPriority::Normal);

    /**
    * @brief 等待指定任务完成
    * @param handle 要等待的任务句柄
    *
    * @note 若句柄无效或任务已完成，立即返回；若任务不存在，也立即返回
    */
    void Wait(JobHandle handle);

private:
    /// 内部任务描述
    struct JobEntry {
        std::function<void()> task;           ///< 任务函数
        JobPriority priority = JobPriority::Normal; ///< 优先级
        uint64_t job_id = 0;                  ///< 全局唯一 ID
        std::vector<uint64_t> dependencies;   ///< 依赖的 job id 列表
        std::atomic<int>* pending_deps = nullptr; ///< 待完成依赖计数（0 时可执行）
        std::promise<void>* completion = nullptr; ///< 完成信号
        bool owns_pending = false;            ///< 是否持有 pending_deps 所有权
        bool owns_completion = false;         ///< 是否持有 completion 所有权
    };

    /// 优先级队列比较器：High 优先出队
    struct JobEntryCompare {
        bool operator()(const JobEntry& a, const JobEntry& b) const {
            return static_cast<uint8_t>(a.priority) < static_cast<uint8_t>(b.priority);
        }
    };

    /// 工作窃取：每线程本地队列
    struct WorkerLocalQueue {
        std::mutex mutex;
        std::deque<JobEntry> queue;
    };

    /// 尝试从指定本地队列窃取任务
    bool StealJob(WorkerLocalQueue& src, JobEntry& out_job);

    /// 检查任务依赖是否已满足
    bool CheckDependencies(const JobEntry& entry);

    /// 当依赖完成时通知等待中的任务
    void NotifyDependents(uint64_t completed_job_id);

    /// 线程池工作线程主循环
    void WorkerThread(int index);

    /// 全局优先级队列
    std::priority_queue<JobEntry, std::vector<JobEntry>, JobEntryCompare> job_queue_;
    /// 线程池工作线程
    std::vector<std::thread> workers_;
    /// 每线程本地队列（工作窃取）
    std::vector<WorkerLocalQueue*> local_queues_;
    /// 保护全局队列与状态
    std::mutex queue_mutex_;
    /// 用于唤醒等待中的工作线程
    std::condition_variable condition_;
    /// 标记线程池是否正在关闭
    bool is_stopping_ = false;
    /// 标记线程池是否已成功初始化
    bool is_initialized_ = false;
    /// 全局递增任务 ID
    std::atomic<uint64_t> next_job_id_{1};
    /// 等待依赖完成的任务（job_id → 待检查列表）
    std::unordered_map<uint64_t, std::vector<uint64_t>> pending_dependents_;
    /// 等待依赖满足的任务暂存区（job_id → JobEntry）
    std::unordered_map<uint64_t, JobEntry> pending_jobs_;
    /// 任务完成信号（job_id → promise）
    std::unordered_map<uint64_t, std::promise<void>*> completion_signals_;
    /// 已完成任务集合（简化依赖检查，定期清理）
    std::unordered_set<uint64_t> completed_jobs_;
    /// 完成信号的固定大小对象池（示范用：替代每次 new/delete promise；
    /// 所有 Acquire/Release 均在 queue_mutex_ 保护下进行）。
    ObjectPool<std::promise<void>> promise_pool_;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_JOB_SYSTEM_H

