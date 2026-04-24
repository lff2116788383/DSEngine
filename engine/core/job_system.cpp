/**
 * @file job_system.cpp
 * @brief 引擎作业系统实现
 */

#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include <algorithm>

namespace dse {
namespace core {

// ============================================================
// 实例方法
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
    }

    for (int i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] {
                        return is_stopping_ || !job_queue_.empty();
                    });

                    if (is_stopping_ && job_queue_.empty()) {
                        return;
                    }

                    job = std::move(job_queue_.front());
                    job_queue_.pop();
                }

                job();
            }
        });
    }
}


void JobSystem::Shutdown() {
    std::vector<std::thread> workers_to_join;
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
        while (!job_queue_.empty()) {
            job_queue_.pop();
        }
        is_stopping_ = false;
    }
}

void JobSystem::Execute(const std::function<void()>& job) {
    if (!job) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!is_initialized_ || is_stopping_) {
            // 线程池未就绪或正在关闭时，回退为同步执行，避免悬挂任务。
        } else {
            job_queue_.push(job);
            condition_.notify_one();
            return;
        }
    }

    job();
}

// ============================================================
// 旧静态接口（兼容过渡，委托到 ServiceLocator 中的实例）
// ============================================================

void JobSystem::InitStatic() {
    auto& locator = ServiceLocator::Instance();
    auto* existing = locator.Get<JobSystem>();
    if (!existing) {
        auto instance = std::make_shared<JobSystem>();
        instance->Init();
        locator.Register<JobSystem, JobSystem>(instance);
    }
}

void JobSystem::ShutdownStatic() {
    auto& locator = ServiceLocator::Instance();
    auto shared = locator.GetShared<JobSystem>();
    if (shared) {
        shared->Shutdown();
        locator.Reset<JobSystem>();
    }
}

void JobSystem::ExecuteStatic(const std::function<void()>& job) {
    auto* instance = ServiceLocator::Instance().Get<JobSystem>();
    if (instance) {
        instance->Execute(job);
    } else {
        // 无实例时同步执行，确保功能不中断
        if (job) {
            job();
        }
    }
}

} // namespace core
} // namespace dse

