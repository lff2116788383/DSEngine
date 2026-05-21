#include "editor_git_panel.h"
#include "editor_icons.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace dse::editor {

namespace {

enum class GitFileStatus { Modified, Added, Deleted, Renamed, Untracked };

struct GitFileEntry {
    GitFileStatus status;
    std::string path;
    bool staged;
};

struct GitState {
    std::string branch_name = "(unknown)";
    std::string remote_branch;
    int ahead = 0;
    int behind = 0;
    std::vector<GitFileEntry> files;
    bool valid = false;
    std::chrono::steady_clock::time_point last_refresh;
    std::mutex mtx;
    char commit_message[512] = "";
};

GitState& GetState() {
    static GitState state;
    return state;
}

/// Execute a command and capture stdout
std::string ExecCommand(const char* cmd) {
    std::string result;
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE read_pipe, write_pipe;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) return result;
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    char cmd_buf[1024];
    snprintf(cmd_buf, sizeof(cmd_buf), "cmd /c %s", cmd);

    if (CreateProcessA(nullptr, cmd_buf, nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(write_pipe);
        write_pipe = nullptr;

        char buf[4096];
        DWORD bytes_read;
        while (ReadFile(read_pipe, buf, sizeof(buf) - 1, &bytes_read, nullptr) && bytes_read > 0) {
            buf[bytes_read] = '\0';
            result += buf;
        }

        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    if (write_pipe) CloseHandle(write_pipe);
    CloseHandle(read_pipe);
    return result;
}

void ParseGitStatus(const std::string& output, GitState& state) {
    state.files.clear();
    std::istringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.size() < 4) continue;
        char x = line[0], y = line[1];
        std::string path = line.substr(3);

        // Trim trailing whitespace
        while (!path.empty() && (path.back() == '\r' || path.back() == '\n' || path.back() == ' '))
            path.pop_back();

        GitFileEntry entry;
        entry.path = path;
        entry.staged = (x != ' ' && x != '?');

        if (x == '?' || y == '?') entry.status = GitFileStatus::Untracked;
        else if (x == 'A' || y == 'A') entry.status = GitFileStatus::Added;
        else if (x == 'D' || y == 'D') entry.status = GitFileStatus::Deleted;
        else if (x == 'R' || y == 'R') entry.status = GitFileStatus::Renamed;
        else entry.status = GitFileStatus::Modified;

        state.files.push_back(entry);
    }
}

const char* StatusIcon(GitFileStatus s) {
    switch (s) {
        case GitFileStatus::Modified:  return MDI_ICON_FILE;
        case GitFileStatus::Added:     return MDI_ICON_PLUS;
        case GitFileStatus::Deleted:   return MDI_ICON_DELETE;
        case GitFileStatus::Renamed:   return MDI_ICON_FILE;
        case GitFileStatus::Untracked: return MDI_ICON_HELP;
    }
    return "?";
}

ImVec4 StatusColor(GitFileStatus s) {
    switch (s) {
        case GitFileStatus::Modified:  return ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
        case GitFileStatus::Added:     return ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
        case GitFileStatus::Deleted:   return ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
        case GitFileStatus::Renamed:   return ImVec4(0.3f, 0.7f, 0.9f, 1.0f);
        case GitFileStatus::Untracked: return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

const char* StatusLabel(GitFileStatus s) {
    switch (s) {
        case GitFileStatus::Modified:  return "M";
        case GitFileStatus::Added:     return "A";
        case GitFileStatus::Deleted:   return "D";
        case GitFileStatus::Renamed:   return "R";
        case GitFileStatus::Untracked: return "?";
    }
    return "?";
}

} // namespace

void RefreshGitStatus() {
    auto& state = GetState();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - state.last_refresh);
    if (elapsed.count() < 5 && state.valid) return;

    std::lock_guard<std::mutex> lock(state.mtx);
    state.last_refresh = now;

    // Branch name
    std::string branch = ExecCommand("git rev-parse --abbrev-ref HEAD 2>NUL");
    while (!branch.empty() && (branch.back() == '\r' || branch.back() == '\n'))
        branch.pop_back();
    if (!branch.empty()) {
        state.branch_name = branch;
        state.valid = true;
    }

    // Ahead/behind
    std::string ab = ExecCommand("git rev-list --left-right --count HEAD...@{u} 2>NUL");
    if (!ab.empty()) {
        sscanf(ab.c_str(), "%d %d", &state.ahead, &state.behind);
    }

    // Status
    std::string status = ExecCommand("git status --porcelain 2>NUL");
    ParseGitStatus(status, state);
}

const char* GetGitBranchName() {
    return GetState().branch_name.c_str();
}

void DrawGitPanel() {
    ImGui::Begin("Git");

    RefreshGitStatus();

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mtx);

    if (!state.valid) {
        ImGui::TextDisabled("Not a git repository or git is not installed.");
        ImGui::End();
        return;
    }

    // Branch info bar
    {
        ImGui::Text(MDI_ICON_SOURCE_BRANCH " %s", state.branch_name.c_str());
        if (state.ahead > 0 || state.behind > 0) {
            ImGui::SameLine();
            if (state.ahead > 0)
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), " +%d", state.ahead);
            if (state.behind > 0)
                ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), " -%d", state.behind);
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 80);
        if (ImGui::SmallButton("Refresh")) {
            state.last_refresh = {};
        }
    }

    ImGui::Separator();

    // Stats
    int staged = 0, modified = 0, untracked = 0;
    for (auto& f : state.files) {
        if (f.staged) staged++;
        else if (f.status == GitFileStatus::Untracked) untracked++;
        else modified++;
    }
    ImGui::Text("Changes: %d staged, %d modified, %d untracked",
                staged, modified, untracked);

    ImGui::Separator();

    // File list
    ImGui::BeginChild("GitFiles", ImVec2(0, ImGui::GetContentRegionAvail().y - 80.0f), true);
    {
        // Staged files
        if (staged > 0) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Staged Changes (%d)", staged);
            for (auto& f : state.files) {
                if (!f.staged) continue;
                ImGui::PushStyleColor(ImGuiCol_Text, StatusColor(f.status));
                ImGui::Text("  %s %s  %s", StatusIcon(f.status), StatusLabel(f.status), f.path.c_str());
                ImGui::PopStyleColor();

                // Right-click: unstage
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
                    std::string cmd = "git reset HEAD -- \"" + f.path + "\" 2>NUL";
                    ExecCommand(cmd.c_str());
                    state.last_refresh = {};
                }
            }
            ImGui::Separator();
        }

        // Unstaged changes
        if (modified > 0 || untracked > 0) {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Changes (%d)", modified + untracked);
            for (auto& f : state.files) {
                if (f.staged) continue;
                ImGui::PushStyleColor(ImGuiCol_Text, StatusColor(f.status));
                ImGui::Text("  %s %s  %s", StatusIcon(f.status), StatusLabel(f.status), f.path.c_str());
                ImGui::PopStyleColor();

                // Right-click: stage
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
                    std::string cmd = "git add \"" + f.path + "\" 2>NUL";
                    ExecCommand(cmd.c_str());
                    state.last_refresh = {};
                }
            }
        }

        if (state.files.empty()) {
            ImGui::TextDisabled("Working tree clean");
        }
    }
    ImGui::EndChild();

    // Commit area
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextMultiline("##commit_msg", state.commit_message, sizeof(state.commit_message),
                               ImVec2(-1, 40));
    if (ImGui::Button("Commit", ImVec2(80, 0))) {
        if (std::strlen(state.commit_message) > 0) {
            std::string cmd = "git commit -m \"";
            cmd += state.commit_message;
            cmd += "\" 2>NUL";
            ExecCommand(cmd.c_str());
            state.commit_message[0] = '\0';
            state.last_refresh = {};
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stage All", ImVec2(80, 0))) {
        ExecCommand("git add -A 2>NUL");
        state.last_refresh = {};
    }
    ImGui::SameLine();
    if (ImGui::Button("Pull", ImVec2(50, 0))) {
        ExecCommand("git pull 2>NUL");
        state.last_refresh = {};
    }
    ImGui::SameLine();
    if (ImGui::Button("Push", ImVec2(50, 0))) {
        ExecCommand("git push 2>NUL");
        state.last_refresh = {};
    }

    ImGui::End();
}

} // namespace dse::editor
