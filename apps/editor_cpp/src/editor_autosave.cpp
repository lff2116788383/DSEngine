#include "editor_autosave.h"
#include "editor_autosave_core.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>
#include <vector>

#include "imgui.h"

#include "editor_scene_io.h"
#include "editor_scene_tabs.h"
#include "editor_settings.h"
#include "editor_toolbar.h"
#include "editor_project.h"
#include "editor_console_panel.h"

#if defined(_WIN32)
#include <ShlObj.h>
#include <iostream>
#endif

namespace dse::editor {

AutoSaveManager& AutoSaveManager::Get() {
    static AutoSaveManager instance;
    return instance;
}

std::string AutoSaveManager::GetAutoSaveDir() const {
    auto& proj_mgr = ProjectManager::Get();
    if (proj_mgr.HasOpenProject()) {
        auto dir = proj_mgr.GetProjectRoot() / ".editor" / "autosave";
        return dir.string();
    }

#if defined(_WIN32)
    wchar_t* appdata = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appdata))) {
        std::filesystem::path dir = std::filesystem::path(appdata) / "DSEngine" / "autosave";
        CoTaskMemFree(appdata);
        return dir.string();
    }
#endif
    return (std::filesystem::current_path() / ".editor" / "autosave").string();
}

std::string AutoSaveManager::GetAutoSavePath() const {
    auto& tab_mgr = SceneTabManager::Get();
    std::string file_name = MakeAutoSaveFileName(tab_mgr.GetActiveDisplayName());

    std::filesystem::path dir(GetAutoSaveDir());
    return (dir / file_name).string();
}

bool AutoSaveManager::CheckRecovery() {
    recovery_pending_ = false;
    recovery_path_.clear();
    recovery_files_.clear();
    recovery_index_ = 0;

    // 枚举「所有」可恢复文档（不是只取第一个），以便逐个呈现/恢复/丢弃。
    recovery_files_ = CollectRecoveryFiles(GetAutoSaveDir());
    if (recovery_files_.empty()) return false;

    recovery_path_ = recovery_files_.front();
    recovery_pending_ = true;
    EditorLog(LogLevel::Warning,
              "Auto-save recovery: " + std::to_string(recovery_files_.size()) +
                  " recoverable document(s) found; first: " + recovery_path_);
    return true;
}

bool AutoSaveManager::DrawRecoveryDialog(entt::registry& registry) {
    if (!recovery_pending_) return false;

    ImGui::OpenPopup("AutoSave Recovery");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0));

    // 处理完当前项后推进到下一个可恢复文档；列表清空时结束恢复流程。
    auto advance = [this]() {
        ++recovery_index_;
        if (recovery_index_ < recovery_files_.size()) {
            recovery_path_ = recovery_files_[recovery_index_];
        } else {
            recovery_pending_ = false;
        }
        ImGui::CloseCurrentPopup();
    };

    if (ImGui::BeginPopupModal("AutoSave Recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "Detected auto-save file(s) from a previous session. "
            "This may indicate the editor exited unexpectedly.");
        if (recovery_files_.size() > 1) {
            ImGui::Text("Document %zu of %zu",
                        recovery_index_ + 1, recovery_files_.size());
        }
        ImGui::Spacing();

        // Show file info（best-effort：取文件时间失败不应让对话框抛异常）
        std::error_code info_ec;
        if (std::filesystem::exists(recovery_path_, info_ec) && !info_ec) {
            auto ftime = std::filesystem::last_write_time(recovery_path_, info_ec);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm_buf{};
#if defined(_WIN32)
            localtime_s(&tm_buf, &tt);
#else
            localtime_r(&tt, &tm_buf);
#endif
            char time_str[64];
            std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);
            ImGui::Text("File: %s", std::filesystem::path(recovery_path_).filename().string().c_str());
            ImGui::Text("Last modified: %s", time_str);
        }

        // 加载前逐文档校验：拦下半截/损坏（非法 JSON、缺 entities 数组）的恢复
        // 候选，避免让 LoadScene 抛异常，并给出可读诊断。
        RecoveryValidation validation;
        {
            std::ifstream in(recovery_path_, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
            validation = ValidateRecoveryScene(content);
        }
        if (validation.loadable) {
            ImGui::Text("Contents: %s", validation.message.c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "Cannot recover: %s", validation.message.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!validation.loadable) ImGui::BeginDisabled();
        if (ImGui::Button("Recover", ImVec2(120, 0))) {
            // 恢复文件可能因上次崩溃写到一半而损坏：加载失败时记录日志并保持
            // 编辑器存活，不让损坏场景把编辑器一起带崩。
            try {
                LoadScene(registry, recovery_path_);
                auto& tab_mgr = SceneTabManager::Get();
                tab_mgr.MarkDirty();
                EditorLog(LogLevel::Info, "Recovered scene from auto-save: " + recovery_path_);
            } catch (const std::exception& e) {
                EditorLog(LogLevel::Error,
                    std::string("Auto-save recovery failed (corrupt file?): ") + e.what());
            }
            advance();
        }
        if (!validation.loadable) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            std::error_code ec;
            std::filesystem::remove(recovery_path_, ec);
            EditorLog(LogLevel::Info, "Discarded auto-save file: " + recovery_path_);
            advance();
        }
        ImGui::EndPopup();
    }
    return recovery_pending_;
}

void AutoSaveManager::Tick(entt::registry& registry) {
    try {
    EditorSettings settings = LoadEditorSettings();
    auto& tab_mgr = SceneTabManager::Get();
    double now = ImGui::GetTime();

    // 多文档：任一打开的场景页签为脏即触发自动保存（不再只看当前页签）。
    AutoSaveDecision decision = DecideAutoSave(
        IsEditorInPlayMode(),
        settings.auto_save_enabled,
        tab_mgr.IsAnyTabDirty(),
        last_save_time_,
        now,
        static_cast<double>(settings.auto_save_interval_sec));

    if (decision == AutoSaveDecision::Skip) return;
    if (decision == AutoSaveDecision::InitTimer) {
        last_save_time_ = now;
        return;
    }

    // Perform auto-save
    std::string dir = GetAutoSaveDir();
    // 自动保存失败（磁盘满/只读/路径无效）不应让正常编辑崩溃：失败则记录日志
    // 并推迟到下个间隔再试。
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        EditorLog(LogLevel::Error, "Auto-save failed to create dir: " + ec.message());
        last_save_time_ = now;
        return;
    }

    // 把每个脏页签作为一次「全有或全无」的多文档事务写盘：全部先写到各自 .tmp，
    // 只有都成功才逐个 rename 提交。任一失败则回滚、保留既有自动保存文件不被
    // 半截内容覆盖，恢复时不会读到截断/损坏的场景。
    auto dirty_tabs = tab_mgr.CollectDirtyTabsForAutoSave(registry);
    std::vector<AutoSaveItem> items;
    items.reserve(dirty_tabs.size());
    for (const auto& info : dirty_tabs) {
        std::string path =
            (std::filesystem::path(dir) / MakeAutoSaveFileName(info.display_name)).string();
        entt::registry* reg = info.registry;
        items.push_back({path, [reg](const std::string& tmp) {
            SaveScene(*reg, tmp);
            return true;
        }});
    }
    if (items.empty()) {  // 已无脏页签（例如刚被手动保存），仅重置计时。
        last_save_time_ = now;
        return;
    }

    std::error_code write_ec;
    if (!AtomicWriteAll(items, write_ec)) {
        EditorLog(LogLevel::Error,
                  "Auto-save failed (atomic multi-document): " + write_ec.message());
        last_save_time_ = now;
        return;
    }
    last_save_time_ = now;
    has_auto_saved_ = true;

    // Format time string for status bar
    auto now_tp = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now_tp);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    last_save_time_str_ = buf;

    EditorLog(LogLevel::Info,
              "Auto-saved " + std::to_string(items.size()) + " dirty document(s)");

    } catch (const std::exception& e) {
        std::cerr << "[AutoSave::Tick] Exception: " << e.what() << std::endl;
        return;
    } catch (...) {
        std::cerr << "[AutoSave::Tick] Unknown exception" << std::endl;
        return;
    }
}

void AutoSaveManager::OnManualSave() {
    std::string path = GetAutoSavePath();
    std::error_code ec;
    std::filesystem::remove(path, ec);
    last_save_time_ = ImGui::GetTime();
    has_auto_saved_ = false;
}

void AutoSaveManager::OnExit() {
    std::string path = GetAutoSavePath();
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace dse::editor
