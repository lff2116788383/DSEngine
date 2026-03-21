#include "core/job_system.h"
#include <algorithm>

namespace core {

std::vector<std::thread> JobSystem::workers_;
std::queue<std::function<void()>> JobSystem::job_queue_;
std::mutex JobSystem::queue_mutex_;
std::condition_variable JobSystem::condition_;
bool JobSystem::stop_ = false;

void JobSystem::Init() {
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
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void JobSystem::Execute(const std::function<void()>& job) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        job_queue_.push(job);
    }
    condition_.notify_one();
}

} // namespace core
