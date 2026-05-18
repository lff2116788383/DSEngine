#include "editor_chat_panel.h"
#include "editor_chat_protocol.h"
#include "editor_control_server.h"
#include "editor_console_panel.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>

#include <imgui.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "engine/runtime/engine_app.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

namespace dse::editor {

// ─── Construction / Destruction ─────────────────────────────────────────────

ChatPanel::ChatPanel() {
    messages_.push_back({ChatRole::System,
        "AI Chat ready. Type a message to interact with the scene.\n"
        "Example: \"Create a red cube at position 0,5,0\""});
}

ChatPanel::~ChatPanel() {
    StopBridge();
}

void ChatPanel::SetBridgePath(const std::string& path) {
    bridge_path_ = path;
}

// ─── Bridge subprocess management ───────────────────────────────────────────

void ChatPanel::StartBridge() {
    if (bridge_running_) return;

    if (bridge_path_.empty() || !std::filesystem::exists(bridge_path_)) {
        messages_.push_back({ChatRole::System,
            "Error: ai_chat_bridge.py not found. Expected at: " + bridge_path_});
        return;
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdin_read = nullptr, stdin_write = nullptr;
    HANDLE stdout_read = nullptr, stdout_write = nullptr;

    CreatePipe(&stdin_read, &stdin_write, &sa, 0);
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

    CreatePipe(&stdout_read, &stdout_write, &sa, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::string cmd = "python \"" + bridge_path_ + "\"";
    std::string work_dir = std::filesystem::path(bridge_path_).parent_path().string();

    if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, work_dir.c_str(), &si, &pi)) {
        messages_.push_back({ChatRole::System,
            "Error: Failed to start ai_chat_bridge.py (error " +
            std::to_string(GetLastError()) + ")"});
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        return;
    }

    CloseHandle(pi.hThread);
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);

    proc_handle_ = pi.hProcess;
    stdin_write_ = stdin_write;
    stdout_read_ = stdout_read;
#else
    int in_pipe[2], out_pipe[2];
    pipe(in_pipe);
    pipe(out_pipe);

    pid_t pid = fork();
    if (pid == 0) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]);
        close(out_pipe[1]);

        std::string dir = std::filesystem::path(bridge_path_).parent_path().string();
        chdir(dir.c_str());
        execlp("python3", "python3", bridge_path_.c_str(), nullptr);
        execlp("python", "python", bridge_path_.c_str(), nullptr);
        _exit(1);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    stdin_fd_ = in_pipe[1];
    stdout_fd_ = out_pipe[0];
    bridge_pid_ = pid;

    // Set stdout non-blocking
    fcntl(stdout_fd_, F_SETFL, fcntl(stdout_fd_, F_GETFL) | O_NONBLOCK);
#endif

    bridge_running_ = true;

    // Reader thread: reads lines from subprocess stdout
    reader_thread_ = std::thread([this]() {
        std::string line_buf;
        char ch;
        while (bridge_running_) {
#ifdef _WIN32
            DWORD bytes_read = 0;
            BOOL ok = ReadFile(stdout_read_, &ch, 1, &bytes_read, nullptr);
            if (!ok || bytes_read == 0) {
                bridge_running_ = false;
                break;
            }
#else
            ssize_t n = read(stdout_fd_, &ch, 1);
            if (n <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                bridge_running_ = false;
                break;
            }
#endif
            if (ch == '\n') {
                if (!line_buf.empty()) {
                    std::lock_guard<std::mutex> lock(output_mutex_);
                    pending_output_.push_back(std::move(line_buf));
                    line_buf.clear();
                }
            } else {
                line_buf += ch;
            }
        }
    });

    EditorLog(LogLevel::Info, "[ChatPanel] AI bridge started.");
}

void ChatPanel::StopBridge() {
    if (!bridge_running_) return;
    bridge_running_ = false;

#ifdef _WIN32
    if (proc_handle_) {
        TerminateProcess(proc_handle_, 0);
        WaitForSingleObject(proc_handle_, 2000);
        CloseHandle(proc_handle_);
        proc_handle_ = nullptr;
    }
    if (stdin_write_) { CloseHandle(stdin_write_); stdin_write_ = nullptr; }
    if (stdout_read_) { CloseHandle(stdout_read_); stdout_read_ = nullptr; }
#else
    if (bridge_pid_ > 0) {
        kill(bridge_pid_, SIGTERM);
        waitpid(bridge_pid_, nullptr, 0);
        bridge_pid_ = 0;
    }
    if (stdin_fd_ >= 0) { close(stdin_fd_); stdin_fd_ = -1; }
    if (stdout_fd_ >= 0) { close(stdout_fd_); stdout_fd_ = -1; }
#endif

    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
}

// ─── Send message to bridge subprocess ──────────────────────────────────────

void ChatPanel::SendToBridge(const std::string& text) {
    if (!bridge_running_) {
        StartBridge();
        if (!bridge_running_) return;
    }

    // Protocol: send JSON line to subprocess stdin
    // {"type": "user_message", "content": "..."}
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& alloc = doc.GetAllocator();
    doc.AddMember("type", "user_message", alloc);
    doc.AddMember("content", rapidjson::Value(text.c_str(), alloc), alloc);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);

    std::string line = std::string(buf.GetString()) + "\n";

#ifdef _WIN32
    DWORD written;
    WriteFile(stdin_write_, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
#else
    write(stdin_fd_, line.c_str(), line.size());
#endif

    waiting_for_response_ = true;
}

// ─── Execute tool call locally ──────────────────────────────────────────────

void ChatPanel::ExecuteToolCall(const std::string& tool_name, const std::string& args_json,
                                 const std::string& call_id,
                                 ControlServer& server, dse::runtime::EngineInstance& engine) {
    // Parse tool arguments
    rapidjson::Document params;
    if (!args_json.empty()) {
        params.Parse(args_json.c_str());
        if (params.HasParseError()) {
            params.SetObject();
        }
    } else {
        params.SetObject();
    }

    // Execute tool directly via ControlServer::DispatchTool (same process, no WebSocket)
    auto response = server.DispatchTool(tool_name, params, engine);

    // Serialize result to JSON string
    std::string result_json;
    if (response.is_error) {
        // Serialize error as proper JSON to avoid injection
        rapidjson::Document err_doc(rapidjson::kObjectType);
        err_doc.AddMember("error",
            rapidjson::Value(response.error_message.c_str(), err_doc.GetAllocator()),
            err_doc.GetAllocator());
        rapidjson::StringBuffer err_buf;
        rapidjson::Writer<rapidjson::StringBuffer> err_w(err_buf);
        err_doc.Accept(err_w);
        result_json = err_buf.GetString();
        messages_.push_back({ChatRole::ToolResult, tool_name + " -> Error: " + response.error_message});
    } else {
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> w(sb);
        if (response.result.IsNull() && !response.result.IsObject()) {
            w.StartObject();
            w.Key("success"); w.Bool(true);
            w.EndObject();
        } else {
            response.result.Accept(w);
        }
        result_json = sb.GetString();
        messages_.push_back({ChatRole::ToolResult, tool_name + " -> OK"});
    }
    scroll_to_bottom_ = true;

    // Send tool result back to bridge
    rapidjson::Document resp(rapidjson::kObjectType);
    auto& alloc = resp.GetAllocator();
    resp.AddMember("type", "tool_result", alloc);
    resp.AddMember("call_id", rapidjson::Value(call_id.c_str(), alloc), alloc);
    resp.AddMember("result", rapidjson::Value(result_json.c_str(), alloc), alloc);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    resp.Accept(writer);

    std::string line = std::string(buf.GetString()) + "\n";

#ifdef _WIN32
    DWORD written;
    WriteFile(stdin_write_, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
#else
    write(stdin_fd_, line.c_str(), line.size());
#endif
}

// ─── ImGui Draw ─────────────────────────────────────────────────────────────

void ChatPanel::Draw(ControlServer& server, dse::runtime::EngineInstance& engine) {
    // Process pending output from bridge
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        while (!pending_output_.empty()) {
            std::string line = std::move(pending_output_.front());
            pending_output_.pop_front();

            auto bmsg = ParseBridgeMessage(line);

            if (!bmsg.valid) {
                messages_.push_back({ChatRole::System, "[bridge] " + line});
                continue;
            }

            switch (bmsg.type) {
            case BridgeMessageType::AssistantMessage:
                messages_.push_back({ChatRole::Assistant, bmsg.content});
                waiting_for_response_ = false;
                scroll_to_bottom_ = true;
                break;
            case BridgeMessageType::ToolCall:
                messages_.push_back({ChatRole::System, "Calling: " + bmsg.tool_name});
                scroll_to_bottom_ = true;
                ExecuteToolCall(bmsg.tool_name, bmsg.tool_args, bmsg.call_id, server, engine);
                break;
            case BridgeMessageType::Error:
                messages_.push_back({ChatRole::System, "Error: " + bmsg.content});
                waiting_for_response_ = false;
                scroll_to_bottom_ = true;
                break;
            case BridgeMessageType::Status:
                messages_.push_back({ChatRole::System, bmsg.content});
                scroll_to_bottom_ = true;
                break;
            default:
                messages_.push_back({ChatRole::System, "[bridge] " + line});
                break;
            }
        }
    }

    // ─── Messages area ──────────────────────────────────────────────────────
    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ChatMessages", ImVec2(0, -footer_height), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& msg : messages_) {
        ImVec4 color;
        const char* prefix;
        switch (msg.role) {
            case ChatRole::User:
                color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
                prefix = "You: ";
                break;
            case ChatRole::Assistant:
                color = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
                prefix = "AI: ";
                break;
            case ChatRole::ToolResult:
                color = ImVec4(0.8f, 0.8f, 0.4f, 1.0f);
                prefix = "Tool: ";
                break;
            default:
                color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                prefix = "";
                break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s%s", prefix, msg.content.c_str());
        ImGui::PopStyleColor();
    }

    if (waiting_for_response_) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Thinking...");
    }

    if (scroll_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
    }

    ImGui::EndChild();

    // ─── Input area ─────────────────────────────────────────────────────────
    ImGui::Separator();

    bool send = false;
    ImGui::PushItemWidth(-60);
    if (ImGui::InputText("##chat_input", input_buf_, sizeof(input_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        send = true;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Send", ImVec2(50, 0)) || send) {
        std::string text(input_buf_);
        if (!text.empty() && !waiting_for_response_) {
            messages_.push_back({ChatRole::User, text});
            SendToBridge(text);
            input_buf_[0] = '\0';
            scroll_to_bottom_ = true;
        }
    }
}

} // namespace dse::editor
