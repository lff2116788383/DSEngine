#include "editor_autosave.h"
#include "editor_autosave_core.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>

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
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

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

    AutoSaveDecision decision = DecideAutoSave(
        IsEditorInPlayMode(),
        settings.auto_save_enabled,
        tab_mgr.GetActiveTab().dirty,
        last_save_time_,
        now,
        static_cast<double>(settings.auto_save_interval_sec));

    if (decision == AutoSaveDecision::Skip) return;
    if (decision == AutoSaveDecision::InitTimer) {
        last_save_time_ = now;
        return;
    }

    // Perform auto-save
    std::string path = GetAutoSavePath();
    // 自动保存失败（磁盘满/只读/路径无效）不应让正常编辑崩溃：失败则记录日志
    // 并推迟到下个间隔再试。
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    if (ec) {
        EditorLog(LogLevel::Error, "Auto-save failed to create dir: " + ec.message());
        last_save_time_ = now;
        return;
    }
    // 原子写：先写到 <path>.tmp，成功后 rename 覆盖。写到一半崩溃只会留下临时
    // 文件，已有的自动保存文件保持完整，恢复时不会读到截断/损坏的场景。
    std::error_code write_ec;
    bool wrote = AtomicWriteFile(
        path,
        [&](const std::string& tmp) {
            SaveScene(registry, tmp);
            return true;
        },
        write_ec);
    if (!wrote) {
        EditorLog(LogLevel::Error,
                  "Auto-save failed (atomic write): " + write_ec.message());
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

    EditorLog(LogLevel::Info, "Auto-saved scene: " + path);

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
