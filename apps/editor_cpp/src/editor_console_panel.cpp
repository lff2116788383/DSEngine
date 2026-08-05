#include "editor_console_panel.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_locale.h"
#include "editor_external_editor.h"
#include "editor_panel_registry.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/callback_sink.h>

#include "engine/base/debug.h"

namespace dse::editor {

namespace {

struct LogEntry {
    LogLevel level;
    std::string category;
    std::string message;
    std::string timestamp;
};

constexpr int kMaxLogEntries = 4000;

std::deque<LogEntry>& GetLogBuffer() {
    static std::deque<LogEntry> buffer;
    return buffer;
}

std::mutex& GetLogMutex() {
    static std::mutex mtx;
    return mtx;
}

std::set<std::string>& GetKnownCategories() {
    static std::set<std::string> cats = {"General", "Render", "Physics", "Audio",
        "Assets", "Script", "Editor", "Network", "Core", "ECS", "UI"};
    return cats;
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

char* GetCategoryFilter() {
    static char buf[64] = "";
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

ImVec4 GetCategoryColor(const std::string& cat) {
    if (cat == "Render")  return ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
    if (cat == "Physics") return ImVec4(0.3f, 0.9f, 0.4f, 1.0f);
    if (cat == "Audio")   return ImVec4(0.9f, 0.6f, 0.9f, 1.0f);
    if (cat == "Script")  return ImVec4(1.0f, 0.7f, 0.3f, 1.0f);
    if (cat == "Assets")  return ImVec4(0.6f, 0.8f, 0.6f, 1.0f);
    if (cat == "Network") return ImVec4(0.5f, 0.8f, 0.9f, 1.0f);
    if (cat == "Core")    return ImVec4(0.8f, 0.8f, 0.5f, 1.0f);
    if (cat == "ECS")     return ImVec4(0.7f, 0.5f, 0.8f, 1.0f);
    if (cat == "Editor")  return ImVec4(0.5f, 0.7f, 0.9f, 1.0f);
    if (cat == "UI")      return ImVec4(0.9f, 0.5f, 0.6f, 1.0f);
    return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
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

LogLevel EngineToEditorLevel(dse::debug::LogLevel level) {
    switch (level) {
        case dse::debug::LogLevel::Trace:
        case dse::debug::LogLevel::Debug:
        case dse::debug::LogLevel::Info:
            return LogLevel::Info;
        case dse::debug::LogLevel::Warn:
            return LogLevel::Warning;
        case dse::debug::LogLevel::Error:
        case dse::debug::LogLevel::Fatal:
            return LogLevel::Error;
        default:
            return LogLevel::Info;
    }
}

bool ShouldShowEntry(const LogEntry& entry) {
    if (entry.level == LogLevel::Info && !GetShowInfo()) return false;
    if (entry.level == LogLevel::Warning && !GetShowWarning()) return false;
    if (entry.level == LogLevel::Error && !GetShowError()) return false;

    const char* cat_filter = GetCategoryFilter();
    if (cat_filter[0] != '\0') {
        if (entry.category != cat_filter) {
            return false;
        }
    }

    const char* filter = GetFilterBuf();
    if (filter[0] != '\0') {
        if (entry.message.find(filter) == std::string::npos &&
            entry.category.find(filter) == std::string::npos) {
            return false;
        }
    }
    return true;
}

bool TryOpenSourceFromLog(const std::string& message) {
    static const std::regex path_line_regex(
        R"(([A-Za-z]:[\\\x2f][\w\\\x2f.\-]+\.\w+)[:\(](\d+))",
        std::regex::optimize);

    std::smatch match;
    if (!std::regex_search(message, match, path_line_regex)) {
        return false;
    }

    std::string file_path = match[1].str();
    std::string line_str = match[2].str();
    std::replace(file_path.begin(), file_path.end(), '/', '\\');

    if (file_path.size() < 2 || file_path[1] != ':') {
        wchar_t cwd[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, cwd);
        std::filesystem::path full = std::filesystem::path(cwd) / file_path;
        if (std::filesystem::exists(full)) {
            file_path = full.string();
        }
    }

    int line_num = 0;
    // 有意的解析回退：无行号时保持 0，不视为错误。
    try { line_num = std::stoi(line_str); } catch (...) {}
    return OpenInExternalEditor(file_path, line_num);
}

void AddLogEntry(LogLevel level, const char* category, const std::string& message, const std::string& timestamp) {
    auto& buffer = GetLogBuffer();
    buffer.push_back({level, category ? category : "General", message, timestamp});
    if (category) {
        GetKnownCategories().insert(category);
    }
    while (static_cast<int>(buffer.size()) > kMaxLogEntries) {
        buffer.pop_front();
    }
}

} // namespace

void EditorLog(LogLevel level, const std::string& message) {
    EditorLogCat(level, "Editor", message);
}

void EditorLogCat(LogLevel level, const char* category, const std::string& message) {
    std::lock_guard<std::mutex> lock(GetLogMutex());
    AddLogEntry(level, category, message, CurrentTimestamp());
}

void InstallEditorLogSink() {
    dse::debug::SetLogCallback([](dse::debug::LogLevel level, const char* category,
                                   const std::string& message, const std::string& timestamp) {
        LogLevel editor_level = EngineToEditorLevel(level);
        std::lock_guard<std::mutex> lock(GetLogMutex());
        std::string ts = timestamp.size() > 11 ? timestamp.substr(11) : timestamp;
        AddLogEntry(editor_level, category, message, ts);
    });

    auto callback = [](const spdlog::details::log_msg& msg) {
        LogLevel level = SpdlogLevelToEditorLevel(msg.level);
        std::string text(msg.payload.data(), msg.payload.size());
        std::lock_guard<std::mutex> lock(GetLogMutex());
        AddLogEntry(level, "ThirdParty", text, CurrentTimestamp());
    };

    auto sink = std::make_shared<spdlog::sinks::callback_sink_mt>(callback);
    auto default_logger = spdlog::default_logger();
    if (default_logger) {
        default_logger->sinks().push_back(sink);
    }

    EditorLogCat(LogLevel::Info, "Editor", "Console initialized (engine callback + spdlog sink installed).");
}

std::string ExportConsoleLogs(const std::string& directory) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(directory, ec);

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &time_t_now);
#else
    localtime_r(&time_t_now, &local_tm);
#endif
    char fname[64];
    std::snprintf(fname, sizeof(fname), "console_export_%04d%02d%02d_%02d%02d%02d.log",
        local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday,
        local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);

    std::string path = (fs::path(directory) / fname).string();
    std::ofstream out(path, std::ios::out);
    if (!out.is_open()) return "";

    std::lock_guard<std::mutex> lock(GetLogMutex());
    for (const auto& entry : GetLogBuffer()) {
        out << "[" << entry.timestamp << "] " << GetLevelTag(entry.level)
            << "[" << entry.category << "] " << entry.message << "\n";
    }
    out.flush();
    return path;
}

void DrawConsolePanelImpl() {
    ImGui::Begin("Console", PanelRegistry::Get().GetCurrentPanelOpen());
    PanelRegistry::Get().DrawMaximizeRestoreButton();

    if (ImGui::Button(T("Clear"))) {
        std::lock_guard<std::mutex> lock(GetLogMutex());
        GetLogBuffer().clear();
    }
    ImGui::SameLine();

    if (ImGui::Button(MDI_ICON_EXPORT " " "Export")) {
        std::string path = ExportConsoleLogs();
        if (!path.empty()) {
            EditorLogCat(LogLevel::Info, "Editor", "Logs exported to: " + path);
        } else {
            EditorLogCat(LogLevel::Error, "Editor", "Failed to export logs");
        }
    }
    ImGui::SameLine();

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

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

    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::BeginCombo("##cat_filter", GetCategoryFilter()[0] ? GetCategoryFilter() : "All Categories")) {
        if (ImGui::Selectable("All Categories", GetCategoryFilter()[0] == '\0')) {
            GetCategoryFilter()[0] = '\0';
        }
        for (const auto& cat : GetKnownCategories()) {
            bool selected = (cat == GetCategoryFilter());
            if (ImGui::Selectable(cat.c_str(), selected)) {
                std::strncpy(GetCategoryFilter(), cat.c_str(), 63);
                GetCategoryFilter()[63] = '\0';
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##filter", T("Filter..."), GetFilterBuf(), 128);
    ImGui::SameLine();
    ImGui::Checkbox(T("Auto-scroll"), &GetAutoScroll());

    ImGui::Separator();

    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard<std::mutex> lock(GetLogMutex());
        ImGuiListClipper clipper;
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
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::TextDisabled("[%s]", entry.timestamp.c_str());

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, GetCategoryColor(entry.category));
                ImGui::Text("[%s]", entry.category.c_str());
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(entry.level));
                ImGui::TextUnformatted(entry.message.c_str());
                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (!TryOpenSourceFromLog(entry.message)) {
                        std::string clipboard_text = "[" + entry.timestamp + "] " + GetLevelTag(entry.level)
                            + "[" + entry.category + "] " + entry.message;
                        ImGui::SetClipboardText(clipboard_text.c_str());
                    }
                }
            }
        }
    }

    if (GetAutoScroll() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    if (ImGui::BeginPopupContextWindow("ConsoleContextMenu")) {
        if (ImGui::MenuItem(T("Copy All"))) {
            std::lock_guard<std::mutex> lock(GetLogMutex());
            std::string all_text;
            for (const auto& entry : GetLogBuffer()) {
                all_text += "[" + entry.timestamp + "] " + GetLevelTag(entry.level)
                    + "[" + entry.category + "] " + entry.message + "\n";
            }
            ImGui::SetClipboardText(all_text.c_str());
        }
        if (ImGui::MenuItem(T("Export to File"))) {
            std::string path = ExportConsoleLogs();
            if (!path.empty()) {
                EditorLogCat(LogLevel::Info, "Editor", "Logs exported to: " + path);
            }
        }
        ImGui::Separator();
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
