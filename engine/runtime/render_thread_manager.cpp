/**
 * @file render_thread_manager.cpp
 * @brief RenderThreadManager implementation.
 */

#include "engine/runtime/render_thread_manager.h"
#include "engine/base/debug.h"

RenderThreadManager::RenderThreadManager(
    std::function<void()> render_callback,
    std::function<void()> on_thread_start,
    std::function<void()> on_thread_stop,
    std::function<void()> on_main_acquire,
    std::function<void()> on_main_release)
    : render_callback_(std::move(render_callback))
    , on_thread_start_(std::move(on_thread_start))
    , on_thread_stop_(std::move(on_thread_stop))
    , on_main_acquire_(std::move(on_main_acquire))
    , on_main_release_(std::move(on_main_release)) {}

RenderThreadManager::~RenderThreadManager() {
    Stop();
}

void RenderThreadManager::Start() {
    if (active_.load()) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        exit_ = false;
        frame_pending_ = false;
        frame_done_ = true;
    }

    // Main thread releases GL context before render thread takes ownership
    if (on_main_release_) {
        on_main_release_();
    }

    thread_ = std::thread(&RenderThreadManager::ThreadFunc, this);
    active_.store(true);
    DEBUG_LOG_INFO("[RenderThread] Started");
}

void RenderThreadManager::Stop() {
    if (!active_.load()) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        exit_ = true;
        frame_pending_ = true;  // wake the thread so it checks the exit flag
    }
    render_cv_.notify_one();

    if (thread_.joinable()) {
        thread_.join();
    }
    active_.store(false);

    // Main thread re-acquires GL context
    if (on_main_acquire_) {
        on_main_acquire_();
    }
    DEBUG_LOG_INFO("[RenderThread] Stopped");
}

void RenderThreadManager::SignalNewFrame() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_pending_ = true;
        frame_done_ = false;
    }
    render_cv_.notify_one();
}

void RenderThreadManager::WaitForComplete() {
    std::unique_lock<std::mutex> lock(mutex_);
    main_cv_.wait(lock, [this] { return frame_done_; });
}

void RenderThreadManager::ThreadFunc() {
    // Render thread acquires GL/Vulkan/DX11 context
    if (on_thread_start_) {
        on_thread_start_();
    }

    while (true) {
        // Wait for main thread to signal a new frame
        {
            std::unique_lock<std::mutex> lock(mutex_);
            render_cv_.wait(lock, [this] { return frame_pending_ || exit_; });
            if (exit_) break;
            frame_pending_ = false;
        }

        if (render_callback_) {
            render_callback_();
        }

        // Notify main thread that rendering is complete
        {
            std::lock_guard<std::mutex> lock(mutex_);
            frame_done_ = true;
        }
        main_cv_.notify_one();
    }

    // Render thread releases context
    if (on_thread_stop_) {
        on_thread_stop_();
    }
}
