#include "engine/core/job_system.h"
#include <algorithm>

namespace core {

std::vector<std::thread> JobSystem::workers_;
std::queue<std::function<void()>> JobSystem::job_queue_;
std::mutex JobSystem::queue_mutex_;
std::condition_variable JobSystem::condition_;
bool JobSystem::stop_ = false;

void JobSystem::Init() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (!workers_.empty()) {
        return;
    }
    stop_ = false;
    lock.unlock();
    int num_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
    for (int i = 0; i < num_threads; ++i) {
        workers_.emplace_back([] {
            while (true) {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [] { return stop_ || !job_queue_.empty(); });
                    
                    if (stop_ && job_queue_.empty()) {
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
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (workers_.empty()) {
            stop_ = false;
            return;
        }
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        while (!job_queue_.empty()) {
            job_queue_.pop();
        }
        stop_ = false;
    }
}

void JobSystem::Execute(const std::function<void()>& job) {
    if (!job) {
        return;
    }
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (workers_.empty()) {
            lock.unlock();
            job();
            return;
        }
    }
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_) {
            lock.unlock();
            job();
            return;
        }
        job_queue_.push(job);
    }
    condition_.notify_one();
}

} // namespace core
