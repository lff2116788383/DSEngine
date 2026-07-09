/**
* @file job_system.h
* @brief 引擎作业系统，提供异步任务执行、优先级调度与任务依赖能力
*
* 能力概览：
* - 基于线程池的异步任务执行（工作窃取调度）
* - 三级优先级（High/Normal/Low），高优先级任务优先出队
* - JobHandle 支持等待完成与任务依赖链
* - 每 worker 本地双端队列 + 真正的工作窃取，减少锁竞争
* - ParallelFor 使用原子计数 barrier，避免逐批 promise 开销
* - 调用者 helping：Wait/ParallelFor 阻塞时主动执行队列任务
* - 热路径零堆分配（JobEntry 池化）
* - Worker 忙/闲时间统计
* - 通过 ServiceLocator 管理生命周期
*/

#ifndef DSE_CORE_JOB_SYSTEM_H
#define DSE_CORE_JOB_SYSTEM_H

#include <functional>
#include <vector>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <chrono>
#include "engine/core/dse_export.h"
#include <queue>
#include <unordered_set>

namespace dse {
namespace core {

class ServiceLocator;

/// 任务优先级
enum class JobPriority : uint8_t {
    Low    = 0,  ///< 后台加载、资源预热等非关键任务
    Normal = 1,  ///< 常规异步任务（默认）
    High   = 2,  ///< 帧内关键路径任务（渲染提交、物理同步等）
};

// ── 内部实现详情 ──────────────────────────────────────────
namespace detail {

/// 内部任务条目（池化，零堆分配热路径）
struct JobEntry {
    std::function<void()> task;           ///< 任务函数
    JobPriority priority = JobPriority::Normal; ///< 优先级

    /// 依赖计数：>0 表示仍在等待依赖完成
    std::atomic<int> pending_deps{0};

    /// 完成标志：true 表示已执行完毕
    std::atomic<bool> done{false};

    /// 引用计数：handle 持有 + 执行持有 + 依赖引用
    std::atomic<int> refcount{1};

    /// 依赖此任务的后续任务列表（在 deps_mutex_ 下操作）
    std::vector<JobEntry*> dependents;

    /// 引用计数管理
    void AddRef() { refcount.fetch_add(1, std::memory_order_relaxed); }
};

} // namespace detail

/// 任务句柄，用于等待完成和声明依赖关系
class JobHandle {
public:
    JobHandle() = default;
    explicit JobHandle(detail::JobEntry* entry) : entry_(entry) {}

    /// 兼容旧接口：返回内部指针的整数表示
    uint64_t id() const { return reinterpret_cast<uint64_t>(entry_); }
    bool is_valid() const { return entry_ != nullptr; }
    bool operator==(const JobHandle& other) const { return entry_ == other.entry_; }
    bool operator!=(const JobHandle& other) const { return entry_ != other.entry_; }
    explicit operator bool() const { return is_valid(); }

    /// 内部访问（供 JobSystem 使用）
    detail::JobEntry* entry() const { return entry_; }

private:
    detail::JobEntry* entry_ = nullptr;
};

/// JobHandle 哈希，用于 unordered_map/unordered_set
struct JobHandleHash {
    size_t operator()(const JobHandle& h) const noexcept {
        return std::hash<uint64_t>{}(h.id());
    }
};

/// Worker 忙/闲时间统计
struct WorkerStats {
    double busy_time_ms = 0.0;   ///< 累计执行任务时长
    double idle_time_ms = 0.0;   ///< 累计等待时长
    uint64_t jobs_executed = 0;  ///< 执行的任务总数

    double utilization() const {
        double total = busy_time_ms + idle_time_ms;
        return total > 0.0 ? busy_time_ms / total : 0.0;
    }
};

/**
* @class JobSystem
* @brief 作业系统，提供基于线程池的异步任务执行、工作窃取调度与依赖管理
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
    * @note 等待期间调用线程会主动执行队列中的任务（调用者 helping）
    */
    void Wait(JobHandle handle);

    /**
    * @brief 并行 for 循环：将 [begin, end) 范围按 batch_size 分片提交到线程池
    * @param begin 起始索引
    * @param end 终止索引（不含）
    * @param batch_size 每个任务处理的元素数量
    * @param func 回调 func(size_t index)，对每个元素调用
    * @param priority 任务优先级
    *
    * @note 阻塞直到所有分片完成。若线程池未初始化，退化为串行执行。
    * @note 使用原子计数 barrier 取代逐批 promise，等待期间调用者 helping。
    */
    void ParallelFor(size_t begin, size_t end, size_t batch_size,
                     const std::function<void(size_t)>& func,
                     JobPriority priority = JobPriority::High);

    /**
    * @brief 获取 worker 统计信息
    * @return 每 worker 的忙/闲时间统计
    */
    const std::vector<WorkerStats>& GetWorkerStats() const { return worker_stats_; }

private:
    /// 内部任务条目（别名）
    using JobEntry = detail::JobEntry;

    /// 工作窃取：每 worker 本地队列
    struct alignas(64) WorkStealingQueue {
        std::mutex mutex;
        std::deque<JobEntry*> queue;
    };

    /// 优先级队列比较器：High 优先出队
    struct JobEntryPtrCompare {
        bool operator()(const JobEntry* a, const JobEntry* b) const {
            return static_cast<uint8_t>(a->priority) < static_cast<uint8_t>(b->priority);
        }
    };

    /// 全局优先级队列（跨线程/优先级溢出入口）
    std::priority_queue<JobEntry*, std::vector<JobEntry*>, JobEntryPtrCompare> global_queue_;
    std::mutex global_queue_mutex_;

    /// 每 worker 本地队列
    std::vector<std::unique_ptr<WorkStealingQueue>> local_queues_;

    /// 线程池工作线程
    std::vector<std::thread> workers_;

    /// 依赖管理互斥锁（仅保护 dependents 列表操作，不阻塞 job 执行）
    std::mutex deps_mutex_;

    /// 唤醒等待中的工作线程
    std::condition_variable wake_cv_;
    std::mutex wake_mutex_;
    std::atomic<bool> wake_pending_{false};

    /// 标记线程池是否正在关闭
    std::atomic<bool> is_stopping_{false};
    /// 标记线程池是否已成功初始化
    std::atomic<bool> is_initialized_{false};

    /// 所有已分配的 JobEntry（Shutdown 时统一释放）
    std::vector<JobEntry*> live_entries_;
    std::mutex live_entries_mutex_;

    /// Worker 统计
    std::vector<WorkerStats> worker_stats_;
    std::mutex stats_mutex_;

    // ── 内部方法 ──

    /// 线程池工作线程主循环
    void WorkerThread(int index);

    /// 将任务推入队列（优先推入调用线程本地队列）
    void Enqueue(JobEntry* entry);

    /// 从队列中取一个任务（本地 → 全局 → 窃取）
    JobEntry* TryPopAny(int worker_index);

    /// 尝试从全局队列取任务
    JobEntry* TryPopGlobal();

    /// 尝试从指定本地队列窃取任务
    JobEntry* TrySteal(WorkStealingQueue& src);

    /// 分配 JobEntry（new + 注册到 live_entries_）
    JobEntry* AcquireEntry();

    /// 清理 JobEntry task（推迟到 Shutdown 统一释放内存）
    void ReleaseEntry(JobEntry* entry);

    /// 任务完成后处理：通知依赖、设置完成标志
    void CompleteJob(JobEntry* entry);

    /// 调用者 helping：执行一个任务，返回是否执行了
    bool TryExecuteOne(int worker_index);

    /// 获取当前线程的 worker 索引（-1 表示非 worker 线程）
    static int GetCurrentWorkerIndex();
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_JOB_SYSTEM_H
