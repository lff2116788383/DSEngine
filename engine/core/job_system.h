/**
 * @file job_system.h
 * @brief 引擎核心模块，提供基础功能支持
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

namespace core {

/**
 * @class JobSystem
 * @brief JobSystem 类的核心功能实现
 */
class JobSystem {
public:
    /**
     * @brief 执行 Init 操作
     */
    static void Init();
    /**
     * @brief 执行 Shutdown 操作
     */
    static void Shutdown();

    // Submit a job to be executed asynchronously
    /**
     * @brief 执行 Execute 操作
     * @param std::function<void( 参数说明
     */
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
