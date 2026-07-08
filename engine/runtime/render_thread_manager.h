/**
 * @file render_thread_manager.h
 * @brief Manages the render thread lifecycle and frame synchronization.
 *        Extracted from FramePipeline to separate thread management concerns.
 */

#ifndef DSE_RENDER_THREAD_MANAGER_H
#define DSE_RENDER_THREAD_MANAGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

/**
 * @class RenderThreadManager
 * @brief Encapsulates render thread creation, synchronization, and teardown.
 *
 * The main thread signals a new frame via SignalNewFrame(); the render thread
 * wakes, invokes the registered render callback, then notifies completion.
 * WaitForComplete() blocks the main thread until the render thread finishes.
 */
class RenderThreadManager {
public:
    /// @param render_callback Called on the render thread each frame.
    /// @param on_thread_start Called once when the render thread starts (e.g. to acquire GL context).
    /// @param on_thread_stop  Called once when the render thread exits (e.g. to release GL context).
    /// @param on_main_acquire Called on the main thread after stopping (e.g. to re-acquire GL context).
    /// @param on_main_release Called on the main thread before starting (e.g. to release GL context).
    RenderThreadManager(
        std::function<void()> render_callback,
        std::function<void()> on_thread_start = {},
        std::function<void()> on_thread_stop = {},
        std::function<void()> on_main_acquire = {},
        std::function<void()> on_main_release = {});

    ~RenderThreadManager();

    // Non-copyable, non-movable (owns a std::thread)
    RenderThreadManager(const RenderThreadManager&) = delete;
    RenderThreadManager& operator=(const RenderThreadManager&) = delete;

    /// Start the render thread. No-op if already running.
    void Start();

    /// Signal the render thread to stop and join it. No-op if not running.
    void Stop();

    /// @brief Wake the render thread to process a new frame.
    /// Must be called from the main thread after preparing the frame data.
    void SignalNewFrame();

    /// @brief Block until the render thread has finished the current frame.
    /// Must be called from the main thread.
    void WaitForComplete();

    /// @return True if the render thread is currently running.
    bool IsActive() const { return active_.load(); }

private:
    void ThreadFunc();

    std::function<void()> render_callback_;
    std::function<void()> on_thread_start_;
    std::function<void()> on_thread_stop_;
    std::function<void()> on_main_acquire_;
    std::function<void()> on_main_release_;

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable render_cv_;   ///< render thread waits for new frame
    std::condition_variable main_cv_;     ///< main thread waits for render done
    bool frame_pending_ = false;
    bool frame_done_ = true;
    bool exit_ = false;
    std::atomic<bool> active_{false};
};

#endif // DSE_RENDER_THREAD_MANAGER_H
