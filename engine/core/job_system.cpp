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
    // 末尾额外一个槽位记录"调用者 helping"（主/外部线程在 Wait/ParallelFor 中帮忙执行的任务）
    worker_stats_.resize(static_cast<size_t>(num_threads) + 1);

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

    // 释放 JobEntry 池内存（block 存储由 unique_ptr 持有，清空即释放）
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        free_list_head_ = nullptr;
        pool_blocks_.clear();
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
    JobEntry* entry = nullptr;
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (!free_list_head_) {
            // freelist 空：分配一整块并串入 freelist（一次分配，摊薄热路径开销）
            auto block = std::make_unique<JobEntry[]>(kPoolBlockSize);
            JobEntry* raw = block.get();
            pool_blocks_.push_back(std::move(block));
            for (size_t i = 0; i < kPoolBlockSize; ++i) {
                raw[i].pool_next = free_list_head_;
                free_list_head_ = &raw[i];
            }
        }
        entry = free_list_head_;
        free_list_head_ = entry->pool_next;
    }

    // 重置状态（该 entry 目前只有本线程可见，relaxed 即可；generation 保留自增值）
    entry->pool_next = nullptr;
    entry->pending_deps.store(0, std::memory_order_relaxed);
    entry->done.store(false, std::memory_order_relaxed);
    entry->refcount.store(0, std::memory_order_relaxed);
    entry->dependents.clear();
    entry->task = nullptr;
    entry->priority = JobPriority::Normal;
    return entry;
}

void JobSystem::RecycleEntry(JobEntry* entry) {
    // 归零复核与回收必须在同一把 pool_mutex_ 下完成，才能与并发 ResolvePin 的
    // re-pin 互斥（否则可能"回收后又被 pin"，造成一个条目被两个任务同时使用）。
    std::lock_guard<std::mutex> lock(pool_mutex_);
    if (entry->refcount.load(std::memory_order_acquire) != 0) {
        // 归零后又被 ResolvePin 重新 pin，放弃回收；待该 pin 释放时再判定。
        return;
    }
    // 自增 generation：使任何仍指向本任务的 JobHandle 在 ResolvePin 时失效。
    entry->generation.fetch_add(1, std::memory_order_release);
    // 清理 task（释放 lambda 捕获）与 dependents
    entry->task = nullptr;
    entry->dependents.clear();

    entry->pool_next = free_list_head_;
    free_list_head_ = entry;
}

void JobSystem::ReleaseRef(JobEntry* entry) {
    if (!entry) return;
    if (entry->refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        RecycleEntry(entry);
    }
}

JobSystem::JobEntry* JobSystem::ResolvePin(const JobHandle& handle) {
    JobEntry* entry = handle.entry();
    if (!entry) return nullptr;
    std::lock_guard<std::mutex> lock(pool_mutex_);
    // 代次不匹配 → 任务早已完成并被回收（或复用为其他任务）
    if (entry->generation.load(std::memory_order_acquire) != handle.generation()) {
        return nullptr;
    }
    // 代次匹配：pin 住（在锁内自增，与 RecycleEntry 的归零复核互斥）
    entry->refcount.fetch_add(1, std::memory_order_acq_rel);
    return entry;
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
    // refcount = 1: 执行引用（JobHandle 是弱引用，不计数）
    entry->refcount.store(1, std::memory_order_relaxed);
    const uint32_t gen = entry->generation.load(std::memory_order_relaxed);

    Enqueue(entry);

    return JobHandle(entry, gen);
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
    // refcount = 1: 执行引用（每登记一个未满足依赖再 AddRef 一次；JobHandle 弱引用不计数）
    entry->refcount.store(1, std::memory_order_relaxed);
    const uint32_t gen = entry->generation.load(std::memory_order_relaxed);

    // 先安全 pin 住所有仍存活的依赖（代次匹配才 pin；已回收/失效的依赖视为已满足）。
    // pin 期间依赖不会被回收，从而可在 deps_mutex_ 下安全读取其 done 并登记。
    std::vector<JobEntry*> pinned_deps;
    pinned_deps.reserve(dependencies.size());
    for (const auto& dep : dependencies) {
        if (JobEntry* de = ResolvePin(dep)) {
            pinned_deps.push_back(de);
        }
    }

    // 在 deps_mutex_ 保护下设置依赖关系（与 CompleteJob 的通知操作互斥）
    int unmet = 0;
    {
        std::lock_guard<std::mutex> lock(deps_mutex_);

        // 第一遍：计算未满足的依赖数
        for (JobEntry* de : pinned_deps) {
            if (de->done.load(std::memory_order_acquire)) continue;
            ++unmet;
        }

        // 设置 pending_deps（在添加到 dependents 之前，但都在锁内）
        entry->pending_deps.store(unmet, std::memory_order_release);

        // 第二遍：添加到各依赖的 dependents 列表
        for (JobEntry* de : pinned_deps) {
            if (de->done.load(std::memory_order_acquire)) continue;
            de->dependents.push_back(entry);
            // 增加被依赖者对 entry 的引用（CompleteJob 通知时会释放）
            entry->AddRef();
        }
    }

    if (unmet == 0) {
        // 所有依赖已满足，直接入队
        Enqueue(entry);
    }

    // 释放对依赖的临时 pin
    for (JobEntry* de : pinned_deps) {
        ReleaseRef(de);
    }

    return JobHandle(entry, gen);
}

// ============================================================
// Wait - 等待任务完成（调用者 helping）
// ============================================================

void JobSystem::Wait(JobHandle handle) {
    // 安全 pin：代次不匹配说明任务早已完成并被回收 → 直接返回。
    // pin 住可保证等待期间该条目不会被回收/复用。
    JobEntry* entry = ResolvePin(handle);
    if (!entry) return;

    // 调用者 helping：在等待期间执行队列中的任务
    int my_index = GetCurrentWorkerIndex();

    while (!entry->done.load(std::memory_order_acquire)) {
        if (!TryExecuteOne(my_index)) {
            // 队列为空，短暂让出 CPU
            std::this_thread::yield();
        }
    }

    ReleaseRef(entry);
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
        // refcount = 1: 仅执行引用；barrier 由 remaining 原子计数完成，不占用条目引用
        entry->refcount.store(1, std::memory_order_relaxed);

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

    auto busy_start = std::chrono::high_resolution_clock::now();

    // 执行任务
    if (entry->task) {
        entry->task();
        // 任务可使用每线程 scratch 做瞬时分配；任务结束统一复位（零争用）。
        Memory::ThreadScratch().Reset();
    }

    CompleteJob(entry);

    // 记录 helping 执行统计：真实 worker 记到自身槽位；主/外部线程（index<0）记到末尾 helper 槽位。
    // 否则在 caller-helping 抢先执行完所有任务时，worker 统计会全为 0。
    auto busy_end = std::chrono::high_resolution_clock::now();
    double busy_ms = std::chrono::duration<double, std::milli>(busy_end - busy_start).count();
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        const int n = static_cast<int>(local_queues_.size());
        const int stat_index = (worker_index >= 0 && worker_index < n) ? worker_index : n;
        if (stat_index >= 0 && stat_index < static_cast<int>(worker_stats_.size())) {
            worker_stats_[stat_index].busy_time_ms += busy_ms;
            worker_stats_[stat_index].jobs_executed++;
        }
    }

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
        // 完成标志必须在 deps_mutex_ 内置位：SubmitWithDependency 在同一锁下
        // 读取 done 并决定是否登记为 dependent。若在锁外置 done，会出现竞态——
        // CompleteJob 处理完（空的）dependents 释放锁后、置 done 前，
        // SubmitWithDependency 读到 done==false 遂登记新 dependent，而该 dependent
        // 永不会被处理，pending_deps 永久 >0 → Wait 活锁。
        entry->done.store(true, std::memory_order_release);
        for (JobEntry* dependent : entry->dependents) {
            if (dependent->pending_deps.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                // 所有依赖满足，需要入队
                to_enqueue.push_back(dependent);
            }
            // 释放被依赖者对 dependent 的引用（dep-ref）。dependent 的执行引用此刻
            // 必然仍存在（尚未执行），故此处不会归零、不会触发回收 → 用裸 fetch_sub，
            // 避免在 deps_mutex_ 下嵌套 pool_mutex_。
            dependent->refcount.fetch_sub(1, std::memory_order_acq_rel);
        }
        entry->dependents.clear();
    }

    // 在锁外入队（避免 deps_mutex_ 与 queue mutex 嵌套）
    for (JobEntry* dep : to_enqueue) {
        Enqueue(dep);
    }

    // 释放执行引用：归零则回收进 freelist（在锁外，避免锁嵌套）。
    ReleaseRef(entry);
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
