#include "editor_autosave.h"

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
    std::string scene_name = tab_mgr.GetActiveDisplayName();
    if (scene_name.empty()) scene_name = "Untitled";

    // Sanitize filename
    for (char& c : scene_name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }

    std::filesystem::path dir(GetAutoSaveDir());
    return (dir / (scene_name + ".autosave.dscene")).string();
}

bool AutoSaveManager::CheckRecovery() {
    recovery_pending_ = false;
    recovery_path_.clear();

    std::string autosave_dir = GetAutoSaveDir();
    // 用 error_code 变体：自动保存目录不可访问/迭代中被删除时降级为「无可恢复」，
    // 不让文件系统异常在编辑器启动期抛出导致崩溃。
    std::error_code ec;
    if (!std::filesystem::exists(autosave_dir, ec) || ec) return false;

    for (std::filesystem::directory_iterator it(autosave_dir, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        const auto& entry = *it;
        if (entry.path().extension() == ".dscene" &&
            entry.path().stem().string().find(".autosave") != std::string::npos) {
            recovery_path_ = entry.path().string();
            recovery_pending_ = true;
            EditorLog(LogLevel::Warning, "Auto-save recovery file found: " + recovery_path_);
            return true;
        }
    }
    return false;
}

bool AutoSaveManager::DrawRecoveryDialog(entt::registry& registry) {
    if (!recovery_pending_) return false;

    ImGui::OpenPopup("AutoSave Recovery");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0));

    if (ImGui::BeginPopupModal("AutoSave Recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "Detected an auto-save file from a previous session. "
            "This may indicate the editor exited unexpectedly.");
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
            recovery_pending_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            std::error_code ec;
            std::filesystem::remove(recovery_path_, ec);
            EditorLog(LogLevel::Info, "Discarded auto-save file");
            recovery_pending_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return recovery_pending_;
}

void AutoSaveManager::Tick(entt::registry& registry) {
    if (IsEditorInPlayMode()) return;

    EditorSettings settings = LoadEditorSettings();
    if (!settings.auto_save_enabled) return;

    auto& tab_mgr = SceneTabManager::Get();
    if (!tab_mgr.GetActiveTab().dirty) return;

    double now = ImGui::GetTime();
    double interval = static_cast<double>(settings.auto_save_interval_sec);
    if (interval < 10.0) interval = 10.0;

    if (last_save_time_ == 0.0) {
        last_save_time_ = now;
        return;
    }

    if ((now - last_save_time_) < interval) return;

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
    try {
        SaveScene(registry, path);
    } catch (const std::exception& e) {
        EditorLog(LogLevel::Error, std::string("Auto-save failed: ") + e.what());
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
