#ifndef DSE_CORE_JOB_SYSTEM_H
#define DSE_CORE_JOB_SYSTEM_H

#include <functional>
#include <future>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace core {

class JobSystem {
public:
    static void Init();
    static void Shutdown();

    // Submit a job to be executed asynchronously
    static void Execute(const std::function<void()>& job);

private:
    static std::vector<std::thread> workers_;
    static std::queue<std::function<void()>> job_queue_;
    static std::mutex queue_mutex_;
    static std::condition_variable condition_;
    static bool stop_;
};

} // namespace core

#endif // DSE_CORE_JOB_SYSTEM_H
