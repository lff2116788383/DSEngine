/**
* @file job_system.cpp
* @brief 引擎作业系统实现 - 优先级调度 + 依赖管理 + 工作窃取
*/

#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include "engine/core/memory/memory.h"
#include "engine/core/memory/linear_allocator.h"
#include <algorithm>

namespace dse {
namespace core {

// ============================================================
// 析构与生命周期
// ============================================================

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Init() {
    int num_threads = 0;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (is_initialized_) {
            return;
        }

        is_stopping_ = false;
        is_initialized_ = true;
        num_threads = static_cast<int>(std::max(1u, std::thread::hardware_concurrency() - 1));
        workers_.reserve(static_cast<size_t>(num_threads));
        local_queues_.reserve(static_cast<size_t>(num_threads));
        for (int i = 0; i < num_threads; ++i) {
            local_queues_.push_back(new WorkerLocalQueue());
        }
    }

    for (int i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&JobSystem::WorkerThread, this, i);
    }
}

void JobSystem::Shutdown() {
    std::vector<std::thread> workers_to_join;
    std::vector<WorkerLocalQueue*> queues_to_delete;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!is_initialized_) {
            is_stopping_ = false;
            return;
        }

        is_stopping_ = true;
        is_initialized_ = false;
        workers_to_join.swap(workers_);
    }

    condition_.notify_all();

    for (std::thread& worker : workers_to_join) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queues_to_delete.swap(local_queues_);
        // 清理全局队列
        while (!job_queue_.empty()) {
            job_queue_.pop();
        }
        // 清理依赖和信号
        for (auto& [id, promise] : completion_signals_) {
            if (promise) {
                try { promise->set_value(); } catch (...) {}
                delete promise;
            }
        }
        completion_signals_.clear();
        pending_dependents_.clear();
        pending_jobs_.clear();
        completed_jobs_.clear();
        is_stopping_ = false;
    }

    // 必须在线程全部退出后再释放本地队列；WorkerThread 和 StealJob 会无锁读取 local_queues_。
    for (auto* q : queues_to_delete) {
        delete q;
    }
}

// ============================================================
// 兼容接口
// ============================================================

void JobSystem::Execute(const std::function<void()>& job) {
    Submit(job, JobPriority::Normal);
}

// ============================================================
// Submit - 带优先级
// ============================================================

JobHandle JobSystem::Submit(const std::function<void()>& job,
                             JobPriority priority) {
    if (!job) {
        return JobHandle();
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!is_initialized_ || is_stopping_) {
            // 回退为同步执行
        } else {
            uint64_t id = next_job_id_.fetch_add(1);
            auto* completion = new std::promise<void>();
            completion_signals_[id] = completion;

            JobEntry entry;
            entry.task = job;
            entry.priority = priority;
            entry.job_id = id;
            entry.completion = completion;
            entry.owns_completion = false; // 所有权在 completion_signals_ 中
            entry.pending_deps = nullptr;
            entry.owns_pending = false;

            job_queue_.push(std::move(entry));
            condition_.notify_one();
            return JobHandle(id);
        }
    }

    // 同步执行
    job();
    return JobHandle();
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

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!is_initialized_ || is_stopping_) {
            // 回退为同步执行
        } else {
            uint64_t id = next_job_id_.fetch_add(1);
            auto* completion = new std::promise<void>();
            completion_signals_[id] = completion;

            // 计算未完成的依赖
            int pending_count = 0;
            std::vector<uint64_t> unmet_deps;
            for (const auto& dep : dependencies) {
                if (!dep.is_valid()) continue;
                if (completed_jobs_.count(dep.id()) > 0) continue;
                unmet_deps.push_back(dep.id());
                ++pending_count;
            }

            if (pending_count == 0) {
                // 所有依赖已满足，直接入队
                JobEntry entry;
                entry.task = job;
                entry.priority = priority;
                entry.job_id = id;
                entry.completion = completion;
                entry.owns_completion = false;
                entry.pending_deps = nullptr;
                entry.owns_pending = false;

                job_queue_.push(std::move(entry));
                condition_.notify_one();
            } else {
                // 存在未满足的依赖，注册等待
                auto* pending = new std::atomic<int>(pending_count);
                JobEntry entry;
                entry.task = job;
                entry.priority = priority;
                entry.job_id = id;
                entry.completion = completion;
                entry.owns_completion = false;
                entry.pending_deps = pending;
                entry.owns_pending = false;

                // 注册依赖映射：当依赖完成时通知此任务
                for (uint64_t dep_id : unmet_deps) {
                    pending_dependents_[dep_id].push_back(id);
                }

                // 暂存到等待区（不进入全局队列，依赖满足后推入）
                pending_jobs_[id] = std::move(entry);
            }

            return JobHandle(id);
        }
    }

    // 同步执行
    job();
    return JobHandle();
}

// ============================================================
// Wait - 等待任务完成
// ============================================================

void JobSystem::Wait(JobHandle handle) {
    if (!handle.is_valid()) return;

    std::future<void> future;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        auto it = completion_signals_.find(handle.id());
        if (it == completion_signals_.end() || !it->second) {
            // 已完成或不存在
            return;
        }
        future = it->second->get_future();
    }

    // 在锁外等待，避免死锁
    if (future.valid()) {
        future.wait();
    }
}

// ============================================================
// 内部：依赖检查与通知
// ============================================================

bool JobSystem::CheckDependencies(const JobEntry& entry) {
    if (!entry.pending_deps) return true;
    return entry.pending_deps->load(std::memory_order_acquire) == 0;
}

void JobSystem::NotifyDependents(uint64_t completed_job_id) {
    // 注意：此函数在 queue_mutex_ 保护下调用
    auto it = pending_dependents_.find(completed_job_id);
    if (it == pending_dependents_.end()) return;

    for (uint64_t dependent_id : it->second) {
        auto dep_it = pending_jobs_.find(dependent_id);
        if (dep_it == pending_jobs_.end()) continue;

        auto& entry = dep_it->second;
        if (entry.pending_deps) {
            int remaining = entry.pending_deps->fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (remaining == 0) {
                // 所有依赖满足，推入就绪队列
                job_queue_.push(std::move(entry));
                pending_jobs_.erase(dep_it);
                condition_.notify_one();
            }
        }
    }

    pending_dependents_.erase(it);
}

// ============================================================
// 内部：工作窃取
// ============================================================

bool JobSystem::StealJob(WorkerLocalQueue& src, JobEntry& out_job) {
    std::lock_guard<std::mutex> lock(src.mutex);
    if (src.queue.empty()) return false;

    // 从队尾窃取（与拥有者从队首取操作方向相反，减少竞争）
    out_job = std::move(src.queue.back());
    src.queue.pop_back();
    return true;
}

// ============================================================
// 工作线程主循环
// ============================================================

void JobSystem::WorkerThread(int index) {
    WorkerLocalQueue* my_queue = nullptr;
    std::vector<WorkerLocalQueue*> queue_snapshot;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (index < 0 || index >= static_cast<int>(local_queues_.size())) {
            return;
        }
        my_queue = local_queues_[index];
        queue_snapshot = local_queues_;
    }

    while (true) {
        JobEntry entry;
        bool got_job = false;

        // 1. 先检查本地队列
        if (my_queue) {
            std::lock_guard<std::mutex> lock(my_queue->mutex);
            if (!my_queue->queue.empty()) {
                entry = std::move(my_queue->queue.front());
                my_queue->queue.pop_front();
                got_job = true;
            }
        }

        // 2. 再检查全局队列
        if (!got_job) {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (is_stopping_ && job_queue_.empty()) {
                return;
            }

            if (!job_queue_.empty()) {
                entry = std::move(const_cast<JobEntry&>(job_queue_.top()));
                job_queue_.pop();
                got_job = true;
            }
        }

        // 3. 尝试窃取其他线程的任务。使用启动时快照，避免 Shutdown 修改 local_queues_ 时并发读写 vector。
        if (!got_job) {
            for (int i = 0; i < static_cast<int>(queue_snapshot.size()); ++i) {
                if (i == index || queue_snapshot[i] == nullptr) continue;
                if (StealJob(*queue_snapshot[i], entry)) {
                    got_job = true;
                    break;
                }
            }
        }

        // 4. 无任务则等待
        if (!got_job) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] {
                return is_stopping_ || !job_queue_.empty();
            });
            continue;
        }

        // 执行任务
        if (entry.task) {
            entry.task();
            // 任务可使用每线程 scratch 做瞬时分配；任务结束统一复位（零争用）。
            Memory::ThreadScratch().Reset();
        }

        // 通知完成
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            uint64_t job_id = entry.job_id;

            // 通知依赖此任务的其他任务
            NotifyDependents(job_id);

            // 标记完成
            completed_jobs_.insert(job_id);

            // 设置完成信号
            auto sig_it = completion_signals_.find(job_id);
            if (sig_it != completion_signals_.end() && sig_it->second) {
                try { sig_it->second->set_value(); } catch (...) {}
                delete sig_it->second;
                completion_signals_.erase(sig_it);
            }

            // 清理已完成的依赖跟踪数据（简化版：每次执行后清理过大的集合）
            if (completed_jobs_.size() > 10000) {
                completed_jobs_.clear();
            }
        }

        // 清理 entry 持有的资源
        if (entry.owns_pending && entry.pending_deps) {
            delete entry.pending_deps;
        }
        if (entry.owns_completion && entry.completion) {
            try { entry.completion->set_value(); } catch (...) {}
            delete entry.completion;
        }
    }
}

} // namespace core
} // namespace dse
