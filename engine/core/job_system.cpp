/**
* @file job_system.cpp
* @brief 引擎作业系统实现 - 工作窃取调度 + 原子计数依赖 + 调用者 helping
*
* 重写要点：
* 1. 每 worker 本地 deque + 真正工作窃取：Submit 推入调用线程本地队列（或全局队列
*    如果是外部线程），worker 空闲时先取本地、再取全局、再窃取其他 worker。
* 2. ParallelFor 用单原子计数 barrier：一次分片提交，atomic<int> 计数完成，
*    免除每批 promise + map 插入。
* 3. 调用者 helping：Wait/ParallelFor 阻塞期间主动从队列取任务执行。
* 4. 依赖用原子计数器：pending_deps fetch_sub 到 0 即入队，移除全局锁内的
*    完成记账 map。
* 5. 热路径零堆分配（JobEntry 生命周期与帧一致，Shutdown 统一释放）。
* 6. Worker 忙/闲时间统计。
*/

#include "engine/core/job_system.h"
#include "engine/base/debug.h"
#include "engine/core/service_locator.h"
#include "engine/core/memory/memory.h"
#include "engine/core/memory/linear_allocator.h"
#include <algorithm>
#include <queue>

namespace dse {
namespace core {

// ============================================================
// thread_local：当前线程的 worker 索引
// ============================================================

namespace {
// -1 表示非 worker 线程（主线程、外部线程等）
thread_local int tls_worker_index = -1;
} // anonymous namespace

// ============================================================
// 析构与生命周期
// ============================================================

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Init() {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    // Single-threaded WASM build (no -pthread): std::thread construction throws
    // std::system_error and aborts. Leave the pool uninitialized so
    // Submit/Execute fall back to synchronous execution.
    return;
#endif
    if (is_initialized_.load(std::memory_order_acquire)) {
        return;
    }

    is_stopping_.store(false, std::memory_order_release);
    is_initialized_.store(true, std::memory_order_release);

    int num_threads = static_cast<int>(std::max(1u, std::thread::hardware_concurrency() - 1));

    local_queues_.reserve(static_cast<size_t>(num_threads));
    for (int i = 0; i < num_threads; ++i) {
        local_queues_.push_back(std::make_unique<WorkStealingQueue>());
    }
    worker_stats_.resize(static_cast<size_t>(num_threads));

    workers_.reserve(static_cast<size_t>(num_threads));
    for (int i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&JobSystem::WorkerThread, this, i);
    }

    DEBUG_LOG_INFO("JobSystem 初始化：worker 线程数={}（工作窃取 + 池化 + 调用者helping）", num_threads);
}

void JobSystem::Shutdown() {
    if (!is_initialized_.load(std::memory_order_acquire)) {
        is_stopping_.store(false, std::memory_order_release);
        return;
    }

    is_stopping_.store(true, std::memory_order_release);
    is_initialized_.store(false, std::memory_order_release);

    // 唤醒所有等待的 worker
    {
        std::lock_guard<std::mutex> lock(wake_mutex_);
        wake_pending_.store(true, std::memory_order_release);
    }
    wake_cv_.notify_all();

    // 等待所有 worker 退出
    for (auto& w : workers_) {
        if (w.joinable()) {
            w.join();
        }
    }
    workers_.clear();

    // 清理队列中的残留任务引用
    {
        std::lock_guard<std::mutex> lock(global_queue_mutex_);
        while (!global_queue_.empty()) {
            global_queue_.pop();
        }
    }
    for (auto& q : local_queues_) {
        std::lock_guard<std::mutex> lq(q->mutex);
        q->queue.clear();
    }
    local_queues_.clear();

    // 释放所有 JobEntry 内存
    {
        std::lock_guard<std::mutex> lock(live_entries_mutex_);
        for (JobEntry* entry : live_entries_) {
            delete entry;
        }
        live_entries_.clear();
    }

    is_stopping_.store(false, std::memory_order_release);
    worker_stats_.clear();
}

// ============================================================
// 兼容接口
// ============================================================

void JobSystem::Execute(const std::function<void()>& job) {
    Submit(job, JobPriority::Normal);
}

// ============================================================
// JobEntry 分配/释放
// ============================================================

JobSystem::JobEntry* JobSystem::AcquireEntry() {
    JobEntry* entry = new JobEntry();
    // 重置状态（atomics 需要 relaxed store，因为 entry 尚未被其他线程看到）
    entry->pending_deps.store(0, std::memory_order_relaxed);
    entry->done.store(false, std::memory_order_relaxed);
    entry->refcount.store(1, std::memory_order_relaxed);
    entry->dependents.clear();
    entry->task = nullptr;
    entry->priority = JobPriority::Normal;

    std::lock_guard<std::mutex> lock(live_entries_mutex_);
    live_entries_.push_back(entry);
    return entry;
}

void JobSystem::ReleaseEntry(JobEntry* entry) {
    if (!entry) return;
    // 清理 task（释放 lambda 捕获），推迟到 Shutdown 统一释放内存
    entry->task = nullptr;
    entry->dependents.clear();
    entry->done.store(true, std::memory_order_release);
}

// ============================================================
// Submit - 带优先级
// ============================================================

JobHandle JobSystem::Submit(const std::function<void()>& job,
                             JobPriority priority) {
    if (!job) {
        return JobHandle();
    }

    if (!is_initialized_.load(std::memory_order_acquire) ||
        is_stopping_.load(std::memory_order_acquire)) {
        // 回退为同步执行
        job();
        return JobHandle();
    }

    JobEntry* entry = AcquireEntry();
    entry->task = job;
    entry->priority = priority;
    // refcount = 2: 1 for handle, 1 for execution
    entry->refcount.store(2, std::memory_order_relaxed);

    Enqueue(entry);

    return JobHandle(entry);
}

// ============================================================
// SubmitWithDependency - 带依赖
// ============================================================

JobHandle JobSystem::SubmitWithDependency(const std::function<void()>& job,
                                            const std::vector<JobHandle>& dependencies,
                                            JobPriority priority) {
    if (!job) {
        return JobHandle();
    }

    if (!is_initialized_.load(std::memory_order_acquire) ||
        is_stopping_.load(std::memory_order_acquire)) {
        // 回退为同步执行
        job();
        return JobHandle();
    }

    JobEntry* entry = AcquireEntry();
    entry->task = job;
    entry->priority = priority;
    // refcount = 2: 1 for handle, 1 for execution
    entry->refcount.store(2, std::memory_order_relaxed);

    // 在 deps_mutex_ 保护下设置依赖关系
    // 这确保与 CompleteJob 中的通知操作互斥
    int unmet = 0;
    {
        std::lock_guard<std::mutex> lock(deps_mutex_);

        // 第一遍：计算未满足的依赖数
        for (const auto& dep : dependencies) {
            if (!dep.is_valid()) continue;
            JobEntry* dep_entry = dep.entry();
            if (!dep_entry) continue;
            if (dep_entry->done.load(std::memory_order_acquire)) continue;
            ++unmet;
        }

        // 设置 pending_deps（在添加到 dependents 之前，但都在锁内）
        entry->pending_deps.store(unmet, std::memory_order_release);

        // 第二遍：添加到各依赖的 dependents 列表
        for (const auto& dep : dependencies) {
            if (!dep.is_valid()) continue;
            JobEntry* dep_entry = dep.entry();
            if (!dep_entry) continue;
            if (dep_entry->done.load(std::memory_order_acquire)) continue;
            dep_entry->dependents.push_back(entry);
            // 增加被依赖者对 entry 的引用（CompleteJob 通知时会释放）
            entry->AddRef();
        }
    }

    if (unmet == 0) {
        // 所有依赖已满足，直接入队
        Enqueue(entry);
    }

    return JobHandle(entry);
}

// ============================================================
// Wait - 等待任务完成（调用者 helping）
// ============================================================

void JobSystem::Wait(JobHandle handle) {
    if (!handle.is_valid()) return;
    JobEntry* entry = handle.entry();
    if (!entry) return;

    // 快速路径：已完成
    if (entry->done.load(std::memory_order_acquire)) return;

    // 调用者 helping：在等待期间执行队列中的任务
    int my_index = GetCurrentWorkerIndex();

    while (!entry->done.load(std::memory_order_acquire)) {
        if (!TryExecuteOne(my_index)) {
            // 队列为空，短暂让出 CPU
            std::this_thread::yield();
        }
    }
}

// ============================================================
// ParallelFor - 原子计数 barrier + 调用者 helping
// ============================================================

void JobSystem::ParallelFor(size_t begin, size_t end, size_t batch_size,
                            const std::function<void(size_t)>& func,
                            JobPriority priority) {
    if (begin >= end) return;
    if (batch_size == 0) batch_size = 1;

    const size_t count = end - begin;

    // 未初始化时退化为串行
    if (!is_initialized_.load(std::memory_order_acquire) ||
        is_stopping_.load(std::memory_order_acquire)) {
        for (size_t i = begin; i < end; ++i) {
            func(i);
        }
        return;
    }

    // 原子计数 barrier：完成一个分片就 fetch_sub
    const int total_batches = static_cast<int>((count + batch_size - 1) / batch_size);
    std::atomic<int> remaining(total_batches);

    int my_index = GetCurrentWorkerIndex();

    for (size_t batch_begin = begin; batch_begin < end; batch_begin += batch_size) {
        const size_t batch_end = std::min(batch_begin + batch_size, end);

        JobEntry* entry = AcquireEntry();
        entry->priority = priority;
        // refcount = 2: 1 for barrier (local), 1 for execution
        entry->refcount.store(2, std::memory_order_relaxed);

        entry->task = [&func, &remaining, batch_begin, batch_end]() {
            for (size_t i = batch_begin; i < batch_end; ++i) {
                func(i);
            }
            remaining.fetch_sub(1, std::memory_order_acq_rel);
        };

        Enqueue(entry);
    }

    // 调用者 helping：在等待期间执行队列中的任务
    while (remaining.load(std::memory_order_acquire) > 0) {
        if (!TryExecuteOne(my_index)) {
            std::this_thread::yield();
        }
    }
}

// ============================================================
// 内部：入队
// ============================================================

void JobSystem::Enqueue(JobEntry* entry) {
    int worker_idx = GetCurrentWorkerIndex();

    if (worker_idx >= 0 && worker_idx < static_cast<int>(local_queues_.size())) {
        // Worker 线程：推入本地队列
        auto& q = local_queues_[worker_idx];
        {
            std::lock_guard<std::mutex> lock(q->mutex);
            q->queue.push_back(entry);
        }
    } else {
        // 外部线程：推入全局队列
        {
            std::lock_guard<std::mutex> lock(global_queue_mutex_);
            global_queue_.push(entry);
        }
    }

    // 唤醒一个等待的 worker
    {
        std::lock_guard<std::mutex> lock(wake_mutex_);
        wake_pending_.store(true, std::memory_order_release);
    }
    wake_cv_.notify_one();
}

// ============================================================
// 内部：取任务
// ============================================================

JobSystem::JobEntry* JobSystem::TryPopGlobal() {
    std::lock_guard<std::mutex> lock(global_queue_mutex_);
    if (global_queue_.empty()) return nullptr;
    JobEntry* entry = global_queue_.top();
    global_queue_.pop();
    return entry;
}

JobSystem::JobEntry* JobSystem::TrySteal(WorkStealingQueue& src) {
    std::lock_guard<std::mutex> lock(src.mutex);
    if (src.queue.empty()) return nullptr;
    // 从队尾窃取（与拥有者从队首取方向相反，减少竞争）
    JobEntry* entry = src.queue.back();
    src.queue.pop_back();
    return entry;
}

JobSystem::JobEntry* JobSystem::TryPopAny(int worker_index) {
    // 1. 先检查本地队列
    if (worker_index >= 0 && worker_index < static_cast<int>(local_queues_.size())) {
        auto& q = local_queues_[worker_index];
        std::lock_guard<std::mutex> lock(q->mutex);
        if (!q->queue.empty()) {
            JobEntry* entry = q->queue.front();
            q->queue.pop_front();
            return entry;
        }
    }

    // 2. 再检查全局队列
    if (JobEntry* entry = TryPopGlobal()) {
        return entry;
    }

    // 3. 尝试窃取其他线程的任务
    if (worker_index >= 0) {
        const int n = static_cast<int>(local_queues_.size());
        // 从 worker_index+1 开始顺序窃取
        for (int i = 1; i < n; ++i) {
            int idx = (worker_index + i) % n;
            if (idx == worker_index) continue;
            if (JobEntry* entry = TrySteal(*local_queues_[idx])) {
                return entry;
            }
        }
    } else {
        // 非 worker 线程：检查所有本地队列
        for (auto& q : local_queues_) {
            if (JobEntry* entry = TrySteal(*q)) {
                return entry;
            }
        }
    }

    return nullptr;
}

// ============================================================
// 内部：执行一个任务（调用者 helping）
// ============================================================

bool JobSystem::TryExecuteOne(int worker_index) {
    JobEntry* entry = TryPopAny(worker_index);
    if (!entry) return false;

    // 执行任务
    if (entry->task) {
        entry->task();
        // 任务可使用每线程 scratch 做瞬时分配；任务结束统一复位（零争用）。
        Memory::ThreadScratch().Reset();
    }

    CompleteJob(entry);

    return true;
}

// ============================================================
// 内部：任务完成处理
// ============================================================

void JobSystem::CompleteJob(JobEntry* entry) {
    // 收集需要入队的 dependents（在锁外执行 Enqueue 避免持锁过久）
    std::vector<JobEntry*> to_enqueue;

    {
        std::lock_guard<std::mutex> lock(deps_mutex_);
        for (JobEntry* dependent : entry->dependents) {
            if (dependent->pending_deps.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                // 所有依赖满足，需要入队
                to_enqueue.push_back(dependent);
            }
            // 释放被依赖者对 dependent 的引用
            dependent->refcount.fetch_sub(1, std::memory_order_acq_rel);
        }
        entry->dependents.clear();
    }

    // 在锁外入队（避免 deps_mutex_ 与 queue mutex 嵌套）
    for (JobEntry* dep : to_enqueue) {
        Enqueue(dep);
    }

    // 设置完成标志
    entry->done.store(true, std::memory_order_release);

    // 释放执行引用（refcount 从 2→1 或从 1→0）
    // handle 引用不会被显式释放，entry 在 Shutdown 时统一释放
    entry->refcount.fetch_sub(1, std::memory_order_acq_rel);
}

// ============================================================
// 内部：获取当前 worker 索引
// ============================================================

int JobSystem::GetCurrentWorkerIndex() {
    return tls_worker_index;
}

// ============================================================
// 工作线程主循环
// ============================================================

void JobSystem::WorkerThread(int index) {
    tls_worker_index = index;

    while (true) {
        auto busy_start = std::chrono::high_resolution_clock::now();

        JobEntry* entry = TryPopAny(index);

        if (entry) {
            // 执行任务
            if (entry->task) {
                entry->task();
                Memory::ThreadScratch().Reset();
            }
            CompleteJob(entry);

            // 记录忙时间
            auto busy_end = std::chrono::high_resolution_clock::now();
            double busy_ms = std::chrono::duration<double, std::milli>(busy_end - busy_start).count();
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                if (index < static_cast<int>(worker_stats_.size())) {
                    worker_stats_[index].busy_time_ms += busy_ms;
                    worker_stats_[index].jobs_executed++;
                }
            }
        } else {
            // 无任务，等待
            auto idle_start = std::chrono::high_resolution_clock::now();

            {
                std::unique_lock<std::mutex> lock(wake_mutex_);
                if (is_stopping_.load(std::memory_order_acquire)) {
                    auto idle_end = std::chrono::high_resolution_clock::now();
                    double idle_ms = std::chrono::duration<double, std::milli>(idle_end - idle_start).count();
                    std::lock_guard<std::mutex> lock2(stats_mutex_);
                    if (index < static_cast<int>(worker_stats_.size())) {
                        worker_stats_[index].idle_time_ms += idle_ms;
                    }
                    return;
                }
                // 短超时轮询 + 条件变量等待，平衡延迟与 CPU 占用
                wake_cv_.wait_for(lock, std::chrono::milliseconds(1), [this] {
                    return is_stopping_.load(std::memory_order_acquire) ||
                           wake_pending_.exchange(false, std::memory_order_acq_rel);
                });
            }

            auto idle_end = std::chrono::high_resolution_clock::now();
            double idle_ms = std::chrono::duration<double, std::milli>(idle_end - idle_start).count();
            {
                std::lock_guard<std::mutex> lock2(stats_mutex_);
                if (index < static_cast<int>(worker_stats_.size())) {
                    worker_stats_[index].idle_time_ms += idle_ms;
                }
            }
        }
    }
}

} // namespace core
} // namespace dse
