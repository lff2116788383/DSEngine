/**
 * @file editor_task_service.h
 * @brief Unified editor background task service (P1-1).
 *
 * Runs long-running work (Git operations, Web/C# builds, imports) off the UI
 * thread with a single, shared progress/log/cancel model. All worker threads
 * are owned by the service and joined on Shutdown() — the editor never leaves a
 * detached thread that could touch destroyed state after exit.
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace dse::editor {

enum class TaskState { Pending, Running, Succeeded, Failed, Canceled };

/// Handed to a task body; the only channel it uses to report progress/log and
/// to observe cancellation. Thread-safe.
class TaskContext {
public:
    /// @param fraction 0..1 for determinate progress, negative for indeterminate.
    void SetProgress(float fraction, std::string message = {});
    void Log(std::string line, bool is_error = false);
    /// The task body must poll this (or pass CancelFlag() to RunProcess).
    bool IsCancelRequested() const { return cancel_->load(); }
    const std::atomic<bool>& CancelFlag() const { return *cancel_; }
    /// Mark the task as failed with a diagnostic (also settable by returning false).
    void Fail(std::string message);

private:
    friend class BackgroundTaskService;
    struct Task* task_ = nullptr;
    std::atomic<bool>* cancel_ = nullptr;
};

/// Immutable snapshot of a task for UI rendering.
struct TaskView {
    uint64_t id = 0;
    std::string title;
    TaskState state = TaskState::Pending;
    float progress = -1.0f;
    std::string message;
    std::vector<std::string> log_tail; // last N lines
    bool cancel_requested = false;
};

class BackgroundTaskService {
public:
    static BackgroundTaskService& Get();

    /// Submit work to run on a background thread. Returns a task id.
    /// @param body        returns true on success, false on failure.
    /// @param on_complete invoked on the UI thread from Update()/DrawPanel().
    uint64_t Submit(std::string title,
                    std::function<bool(TaskContext&)> body,
                    std::function<void(bool success)> on_complete = {});

    void RequestCancel(uint64_t id);
    bool AnyRunning();
    std::vector<TaskView> Snapshot();

    /// Pump completion callbacks on the UI thread; call once per frame.
    void Update();

    /// Cancel all tasks and join every worker thread. Safe to call multiple times.
    void Shutdown();

    /// ImGui panel: running/finished tasks with progress bars, log, cancel.
    void DrawPanel(bool* open);

    ~BackgroundTaskService();

private:
    BackgroundTaskService() = default;
    BackgroundTaskService(const BackgroundTaskService&) = delete;
    BackgroundTaskService& operator=(const BackgroundTaskService&) = delete;

    std::mutex mutex_;
    std::vector<std::unique_ptr<struct Task>> tasks_;
    uint64_t next_id_ = 1;
    bool shutting_down_ = false;
};

} // namespace dse::editor
