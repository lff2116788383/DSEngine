#include "editor_task_service.h"
#include "editor_panel_registry.h"

#include <algorithm>

#include "imgui.h"

namespace dse::editor {

// Internal task record. Owns its worker thread; guarded by the service mutex
// except for the atomics / log which the worker updates under log_mutex.
struct Task {
    uint64_t id = 0;
    std::string title;
    std::function<bool(TaskContext&)> body;
    std::function<void(bool)> on_complete;

    std::thread thread;
    std::atomic<bool> cancel{false};
    std::atomic<TaskState> state{TaskState::Pending};
    std::atomic<float> progress{-1.0f};

    std::mutex log_mutex;
    std::string message;
    std::vector<std::string> log;
    std::string fail_message;

    bool completed_dispatched = false; // on_complete already fired on UI thread
    bool joined = false;
};

// ── TaskContext ─────────────────────────────────────────────────────────────

void TaskContext::SetProgress(float fraction, std::string message) {
    if (!task_) return;
    task_->progress.store(fraction);
    if (!message.empty()) {
        std::lock_guard<std::mutex> lk(task_->log_mutex);
        task_->message = std::move(message);
    }
}

void TaskContext::Log(std::string line, bool is_error) {
    if (!task_) return;
    std::lock_guard<std::mutex> lk(task_->log_mutex);
    task_->log.emplace_back((is_error ? "[err] " : "") + std::move(line));
    // Bound memory: keep the last 2000 lines.
    if (task_->log.size() > 2000) {
        task_->log.erase(task_->log.begin(), task_->log.begin() + (task_->log.size() - 2000));
    }
}

void TaskContext::Fail(std::string message) {
    if (!task_) return;
    std::lock_guard<std::mutex> lk(task_->log_mutex);
    task_->fail_message = std::move(message);
}

// ── BackgroundTaskService ───────────────────────────────────────────────────

BackgroundTaskService& BackgroundTaskService::Get() {
    static BackgroundTaskService instance;
    return instance;
}

BackgroundTaskService::~BackgroundTaskService() {
    Shutdown();
}

uint64_t BackgroundTaskService::Submit(std::string title,
                                       std::function<bool(TaskContext&)> body,
                                       std::function<void(bool)> on_complete) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (shutting_down_ || !body) return 0;

    auto task = std::make_unique<Task>();
    Task* raw = task.get();
    raw->id = next_id_++;
    raw->title = std::move(title);
    raw->body = std::move(body);
    raw->on_complete = std::move(on_complete);
    raw->state.store(TaskState::Running);

    raw->thread = std::thread([raw]() {
        TaskContext ctx;
        ctx.task_ = raw;
        ctx.cancel_ = &raw->cancel;
        bool ok = false;
        try {
            ok = raw->body(ctx);
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(raw->log_mutex);
            raw->fail_message = std::string("exception: ") + e.what();
            ok = false;
        } catch (...) {
            std::lock_guard<std::mutex> lk(raw->log_mutex);
            raw->fail_message = "unknown exception";
            ok = false;
        }
        if (raw->cancel.load()) {
            raw->state.store(TaskState::Canceled);
        } else {
            raw->state.store(ok ? TaskState::Succeeded : TaskState::Failed);
        }
    });

    uint64_t id = raw->id;
    tasks_.push_back(std::move(task));
    return id;
}

void BackgroundTaskService::RequestCancel(uint64_t id) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& t : tasks_) {
        if (t->id == id) { t->cancel.store(true); break; }
    }
}

bool BackgroundTaskService::AnyRunning() {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& t : tasks_) {
        if (t->state.load() == TaskState::Running) return true;
    }
    return false;
}

std::vector<TaskView> BackgroundTaskService::Snapshot() {
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<TaskView> out;
    out.reserve(tasks_.size());
    for (auto& t : tasks_) {
        TaskView v;
        v.id = t->id;
        v.title = t->title;
        v.state = t->state.load();
        v.progress = t->progress.load();
        v.cancel_requested = t->cancel.load();
        {
            std::lock_guard<std::mutex> lg(t->log_mutex);
            v.message = t->message;
            size_t start = t->log.size() > 12 ? t->log.size() - 12 : 0;
            v.log_tail.assign(t->log.begin() + start, t->log.end());
        }
        out.push_back(std::move(v));
    }
    return out;
}

void BackgroundTaskService::Update() {
    // Fire completion callbacks and join finished worker threads on the UI thread.
    std::vector<std::pair<std::function<void(bool)>, bool>> callbacks;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& t : tasks_) {
            TaskState s = t->state.load();
            bool finished = (s == TaskState::Succeeded || s == TaskState::Failed || s == TaskState::Canceled);
            if (finished && !t->joined) {
                if (t->thread.joinable()) t->thread.join();
                t->joined = true;
            }
            if (finished && !t->completed_dispatched) {
                t->completed_dispatched = true;
                if (t->on_complete) callbacks.emplace_back(t->on_complete, s == TaskState::Succeeded);
            }
        }
    }
    for (auto& cb : callbacks) cb.first(cb.second);
}

void BackgroundTaskService::Shutdown() {
    std::vector<std::unique_ptr<Task>> to_join;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        shutting_down_ = true;
        for (auto& t : tasks_) t->cancel.store(true);
        to_join = std::move(tasks_);
        tasks_.clear();
    }
    // Join outside the lock so a worker touching the service can't deadlock.
    for (auto& t : to_join) {
        if (t->thread.joinable()) t->thread.join();
    }
}

void BackgroundTaskService::DrawPanel(bool* open) {
    if (open && !*open) return;
    Update();
    if (!ImGui::Begin("Background Tasks", open)) { ImGui::End(); return; }
    PanelRegistry::Get().DrawMaximizeRestoreButton();

    auto views = Snapshot();
    if (views.empty()) {
        ImGui::TextDisabled("No background tasks.");
        ImGui::End();
        return;
    }

    for (auto& v : views) {
        ImGui::PushID((int)v.id);
        const char* state_str =
            v.state == TaskState::Running   ? "Running"   :
            v.state == TaskState::Succeeded ? "Succeeded" :
            v.state == TaskState::Failed    ? "Failed"    :
            v.state == TaskState::Canceled  ? "Canceled"  : "Pending";
        ImGui::Text("%s  [%s]", v.title.c_str(), state_str);

        if (v.state == TaskState::Running) {
            if (v.progress >= 0.0f) ImGui::ProgressBar(v.progress, ImVec2(-1, 0));
            else ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(-1, 0), "working...");
            ImGui::SameLine();
            if (!v.cancel_requested) {
                if (ImGui::SmallButton("Cancel")) RequestCancel(v.id);
            } else {
                ImGui::TextDisabled("(canceling)");
            }
        }
        if (!v.message.empty()) ImGui::TextWrapped("%s", v.message.c_str());
        if (!v.log_tail.empty() && ImGui::TreeNode("Log")) {
            for (auto& line : v.log_tail) ImGui::TextUnformatted(line.c_str());
            ImGui::TreePop();
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::End();
}

} // namespace dse::editor
