#include "editor_console_panel.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_locale.h"
#include "editor_external_editor.h"

#include <deque>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <regex>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/callback_sink.h>

namespace dse::editor {

namespace {

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string timestamp;
};

constexpr int kMaxLogEntries = 2000;

std::deque<LogEntry>& GetLogBuffer() {
    static std::deque<LogEntry> buffer;
    return buffer;
}

std::mutex& GetLogMutex() {
    static std::mutex mtx;
    return mtx;
}

bool& GetAutoScroll() {
    static bool auto_scroll = true;
    return auto_scroll;
}

bool& GetShowInfo() {
    static bool show = true;
    return show;
}

bool& GetShowWarning() {
    static bool show = true;
    return show;
}

bool& GetShowError() {
    static bool show = true;
    return show;
}

char* GetFilterBuf() {
    static char buf[128] = "";
    return buf;
}

std::string CurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &time_t_now);
#else
    localtime_r(&time_t_now, &local_tm);
#endif

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
        local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec,
        static_cast<int>(ms.count()));
    return buf;
}

ImVec4 GetLevelColor(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        case LogLevel::Warning: return ImVec4(1.0f, 0.85f, 0.0f, 1.0f);
        case LogLevel::Error:   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

const char* GetLevelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return "[Info]   ";
        case LogLevel::Warning: return "[Warn]   ";
        case LogLevel::Error:   return "[Error]  ";
    }
    return "[???]    ";
}

const char* GetLevelIcon(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return MDI_ICON_INFORMATION;
        case LogLevel::Warning: return MDI_ICON_ALERT;
        case LogLevel::Error:   return MDI_ICON_CLOSE_CIRCLE;
    }
    return "?";
}

LogLevel SpdlogLevelToEditorLevel(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace:
        case spdlog::level::debug:
        case spdlog::level::info:
            return LogLevel::Info;
        case spdlog::level::warn:
            return LogLevel::Warning;
        case spdlog::level::err:
        case spdlog::level::critical:
            return LogLevel::Error;
        default:
            return LogLevel::Info;
    }
}

bool ShouldShowEntry(const LogEntry& entry) {
    if (entry.level == LogLevel::Info && !GetShowInfo()) return false;
    if (entry.level == LogLevel::Warning && !GetShowWarning()) return false;
    if (entry.level == LogLevel::Error && !GetShowError()) return false;

    const char* filter = GetFilterBuf();
    if (filter[0] != '\0') {
        if (entry.message.find(filter) == std::string::npos) {
            return false;
        }
    }
    return true;
}

/// Try to extract a file path + line number from a log message and open in external editor.
/// Supports patterns: "path/file.ext:123", "path\file.ext(123)", "path/file.ext line 123"
/// Returns true if a file was found and open was attempted.
bool TryOpenSourceFromLog(const std::string& message) {
    // Pattern 1: file.ext:line (common in gcc/clang/spdlog output)
    // Pattern 2: file.ext(line) (MSVC style)
    // Regex matches paths with extensions like .cpp, .h, .lua, .py, .txt
    static const std::regex path_line_regex(
        R"(([A-Za-z]:[\\/][\w\\/.\-]+\.\w+|[\w./\\\-]+\.\w+)[:\(](\d+))",
        std::regex::optimize);

    std::smatch match;
    if (!std::regex_search(message, match, path_line_regex)) {
        return false;
    }

    std::string file_path = match[1].str();
    std::string line_str = match[2].str();

    // Normalize path separators
    std::replace(file_path.begin(), file_path.end(), '/', '\\');

    // If relative path, try to resolve against project root
    if (file_path.size() < 2 || file_path[1] != ':') {
        // Try current working directory or a known project root
        wchar_t cwd[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, cwd);
        std::filesystem::path full = std::filesystem::path(cwd) / file_path;
        if (std::filesystem::exists(full)) {
            file_path = full.string();
        }
    }

    // Open in user-configured external editor (Preferences → External Script Editor)
    int line_num = 0;
    try { line_num = std::stoi(line_str); } catch (...) {}
    return OpenInExternalEditor(file_path, line_num);
}

} // namespace

void EditorLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(GetLogMutex());
    auto& buffer = GetLogBuffer();
    buffer.push_back({level, message, CurrentTimestamp()});
    while (static_cast<int>(buffer.size()) > kMaxLogEntries) {
        buffer.pop_front();
    }
}

void InstallEditorLogSink() {
    auto callback = [](const spdlog::details::log_msg& msg) {
        LogLevel level = SpdlogLevelToEditorLevel(msg.level);
        std::string text(msg.payload.data(), msg.payload.size());
        EditorLog(level, text);
    };

    auto sink = std::make_shared<spdlog::sinks::callback_sink_mt>(callback);
    auto default_logger = spdlog::default_logger();
    if (default_logger) {
        default_logger->sinks().push_back(sink);
    }

    EditorLog(LogLevel::Info, "Editor Console initialized.");
}

void DrawConsolePanelImpl() {
    ImGui::Begin("Console");

    // Toolbar row: Clear, filter toggles, search
    if (ImGui::Button(T("Clear"))) {
        std::lock_guard<std::mutex> lock(GetLogMutex());
        GetLogBuffer().clear();
    }
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Level filter toggle buttons
    auto toggle_button = [](const char* label, bool& enabled, const ImVec4& active_color) {
        if (enabled) {
            ImGui::PushStyleColor(ImGuiCol_Button, active_color);
        }
        if (ImGui::Button(label)) {
            enabled = !enabled;
        }
        if (enabled) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
    };

    {
        // Count entries by level
        int info_count = 0, warn_count = 0, error_count = 0;
        {
            std::lock_guard<std::mutex> lock(GetLogMutex());
            for (const auto& entry : GetLogBuffer()) {
                switch (entry.level) {
                    case LogLevel::Info: info_count++; break;
                    case LogLevel::Warning: warn_count++; break;
                    case LogLevel::Error: error_count++; break;
                }
            }
        }

        char label_buf[64];
        std::snprintf(label_buf, sizeof(label_buf), "%s %d###info_toggle", MDI_ICON_INFORMATION, info_count);
        toggle_button(label_buf, GetShowInfo(), ImVec4(0.2f, 0.4f, 0.7f, 1.0f));

        std::snprintf(label_buf, sizeof(label_buf), "%s %d###warn_toggle", MDI_ICON_ALERT, warn_count);
        toggle_button(label_buf, GetShowWarning(), ImVec4(0.7f, 0.6f, 0.1f, 1.0f));

        std::snprintf(label_buf, sizeof(label_buf), "%s %d###error_toggle", MDI_ICON_CLOSE_CIRCLE, error_count);
        toggle_button(label_buf, GetShowError(), ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    }

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##filter", T("Filter..."), GetFilterBuf(), 128);
    ImGui::SameLine();
    ImGui::Checkbox(T("Auto-scroll"), &GetAutoScroll());

    ImGui::Separator();

    // Log entries
    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard<std::mutex> lock(GetLogMutex());
        ImGuiListClipper clipper;
        // Build visible entries indices
        static std::vector<int> visible_indices;
        visible_indices.clear();
        const auto& buffer = GetLogBuffer();
        for (int i = 0; i < static_cast<int>(buffer.size()); i++) {
            if (ShouldShowEntry(buffer[i])) {
                visible_indices.push_back(i);
            }
        }

        clipper.Begin(static_cast<int>(visible_indices.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                const auto& entry = buffer[visible_indices[row]];
                ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(entry.level));
                ImGui::TextUnformatted(GetLevelIcon(entry.level));
                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", entry.timestamp.c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted(entry.message.c_str());
                ImGui::PopStyleColor();

                // Double-click: jump to source if file path found, else copy to clipboard
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (!TryOpenSourceFromLog(entry.message)) {
                        std::string clipboard_text = "[" + entry.timestamp + "] " + GetLevelTag(entry.level) + entry.message;
                        ImGui::SetClipboardText(clipboard_text.c_str());
                    }
                }
            }
        }
    }

    if (GetAutoScroll() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextWindow("ConsoleContextMenu")) {
        if (ImGui::MenuItem(T("Copy All"))) {
            std::lock_guard<std::mutex> lock(GetLogMutex());
            std::string all_text;
            for (const auto& entry : GetLogBuffer()) {
                all_text += "[" + entry.timestamp + "] " + GetLevelTag(entry.level) + entry.message + "\n";
            }
            ImGui::SetClipboardText(all_text.c_str());
        }
        if (ImGui::MenuItem(T("Clear"))) {
            std::lock_guard<std::mutex> lock(GetLogMutex());
            GetLogBuffer().clear();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace dse::editor
