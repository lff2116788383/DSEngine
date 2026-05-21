#include "editor_chat_panel.h"
#include "editor_chat_protocol.h"
#include "editor_control_server.h"
#include "editor_console_panel.h"
#include "editor_ai_config.h"

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

void ChatPanel::SetMentionResolver(std::function<std::string(const std::string&)> resolver) {
    mention_resolver_ = std::move(resolver);
}

// ─── @mention 解析：提取 @token 并拼接上下文前缀 ────────────────────────────
static std::string ResolveMentions(const std::string& text,
                                    const std::function<std::string(const std::string&)>& resolver) {
    if (!resolver) return {};

    // 扫描所有 @word 和 @word:arg 格式的 token
    std::string context_block;
    size_t pos = 0;
    while ((pos = text.find('@', pos)) != std::string::npos) {
        ++pos;
        size_t end = pos;
        while (end < text.size() && (std::isalnum((unsigned char)text[end]) || text[end] == '_' || text[end] == ':' || text[end] == '/'))
            ++end;
        if (end > pos) {
            std::string token = text.substr(pos - 1, end - pos + 1); // include '@'
            std::string resolved = resolver(token);
            if (!resolved.empty()) {
                context_block += resolved;
                if (context_block.back() != '\n') context_block += '\n';
            }
        }
        pos = end;
    }
    return context_block;
}

void ChatPanel::SetCurrentAgent(const std::string& agent_id) {
    current_agent_id_ = agent_id;
}

void ChatPanel::ClearHistory() {
    messages_.clear();
    messages_.push_back({ChatRole::System,
        "Conversation cleared."});
}

// ─── Bridge subprocess management ───────────────────────────────────────────

void ChatPanel::StartBridge() {
    if (bridge_running_) return;

    if (bridge_path_.empty() || !std::filesystem::exists(bridge_path_)) {
        messages_.push_back({ChatRole::System,
            "Error: ai_chat_bridge.py not found. Expected at: " + bridge_path_});
        return;
    }

    // Apply AIConfigManager settings as environment variables
    {
        auto& cfg = AIConfigManager::Instance().GetConfig();
        if (!cfg.providers.empty()) {
            const auto& p = cfg.providers[
                std::clamp(cfg.current_provider_index, 0, (int)cfg.providers.size() - 1)];
            if (!p.api_key.empty())
                SetEnvironmentVariableA("OPENAI_API_KEY", p.api_key.c_str());
            if (!p.base_url.empty())
                SetEnvironmentVariableA("OPENAI_BASE_URL", p.base_url.c_str());
            if (!p.model.empty())
                SetEnvironmentVariableA("OPENAI_MODEL", p.model.c_str());
            if (!p.proxy_url.empty())
                SetEnvironmentVariableA("HTTP_PROXY", p.proxy_url.c_str());
        }
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
    bridge_crashed_ = false;

    // Reader thread: reads lines from subprocess stdout
    reader_thread_ = std::thread([this]() {
        std::string line_buf;
        char ch;
        while (bridge_running_) {
#ifdef _WIN32
            DWORD bytes_read = 0;
            BOOL ok = ReadFile(stdout_read_, &ch, 1, &bytes_read, nullptr);
            if (!ok || bytes_read == 0) {
                if (bridge_running_) {
                    // Unexpected exit: flag crash so UI can show reconnect prompt
                    bridge_crashed_ = true;
                    std::lock_guard<std::mutex> lock(output_mutex_);
                    pending_output_.push_back(
                        R"({"type":"status","message":"[Bridge] 进程意外退出，请点击重连。"})");
                }
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
                if (bridge_running_) {
                    bridge_crashed_ = true;
                    std::lock_guard<std::mutex> lock(output_mutex_);
                    pending_output_.push_back(
                        R"({"type":"status","message":"[Bridge] 进程意外退出，请点击重连。"})");
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

    // Use the protocol helper with agent_id
    std::string line = BuildUserMessage(text, current_agent_id_);

#ifdef _WIN32
    DWORD written;
    WriteFile(stdin_write_, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
#else
    write(stdin_fd_, line.c_str(), line.size());
#endif

    waiting_for_response_ = true;
}

void ChatPanel::CancelGeneration() {
    if (!bridge_running_ || !waiting_for_response_) return;

    std::string line = BuildCancelMessage();

#ifdef _WIN32
    DWORD written;
    WriteFile(stdin_write_, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
#else
    write(stdin_fd_, line.c_str(), line.size());
#endif
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

// ─── History Persistence ─────────────────────────────────────────────────────

void ChatPanel::SaveHistory(const std::string& path) {
    if (path.empty() || messages_.empty()) return;

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    rapidjson::Document doc(rapidjson::kArrayType);
    auto& alloc = doc.GetAllocator();

    for (const auto& msg : messages_) {
        if (msg.role == ChatRole::ToolResult) continue; // skip tool echoes
        rapidjson::Value obj(rapidjson::kObjectType);
        const char* role_str =
            msg.role == ChatRole::User      ? "user"      :
            msg.role == ChatRole::Assistant ? "assistant" : "system";
        obj.AddMember("role",     rapidjson::Value(role_str, alloc), alloc);
        obj.AddMember("content",  rapidjson::Value(msg.content.c_str(), alloc), alloc);
        obj.AddMember("agent_id", rapidjson::Value(msg.agent_id.c_str(), alloc), alloc);
        doc.PushBack(obj, alloc);
    }

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);

    std::ofstream f(path);
    if (f) f << buf.GetString();
}

void ChatPanel::LoadHistory(const std::string& path) {
    history_path_ = path;
    if (path.empty() || !std::filesystem::exists(path)) return;

    std::ifstream f(path);
    if (!f) return;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError() || !doc.IsArray()) return;

    messages_.clear();
    for (const auto& obj : doc.GetArray()) {
        if (!obj.IsObject()) continue;
        std::string role_str  = obj.HasMember("role")     && obj["role"].IsString()     ? obj["role"].GetString()     : "system";
        std::string content_s = obj.HasMember("content")  && obj["content"].IsString()  ? obj["content"].GetString()  : "";
        std::string agent_id  = obj.HasMember("agent_id") && obj["agent_id"].IsString() ? obj["agent_id"].GetString() : "";

        ChatRole role = ChatRole::System;
        if (role_str == "user")      role = ChatRole::User;
        else if (role_str == "assistant") role = ChatRole::Assistant;

        messages_.push_back({role, content_s, agent_id});
    }
    scroll_to_bottom_ = true;
}

// ─── Markdown Renderer ───────────────────────────────────────────────────────

// Render a single line that may contain inline **bold** and `code` spans.
static void RenderInline(const std::string& line, const ImVec4& base_color) {
    const ImVec4 code_color  = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);
    const ImVec4 bold_color  = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    size_t pos = 0;
    bool first = true;
    while (pos < line.size()) {
        // Try to match `code`
        if (line[pos] == '`') {
            size_t end = line.find('`', pos + 1);
            if (end != std::string::npos) {
                if (!first) ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Text, base_color);
                // print preceding is handled via slices
                ImGui::PopStyleColor();
                if (!first) ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Text, code_color);
                ImGui::TextUnformatted(line.substr(pos + 1, end - pos - 1).c_str());
                ImGui::PopStyleColor();
                first = false;
                pos = end + 1;
                continue;
            }
        }
        // Try to match **bold**
        if (pos + 1 < line.size() && line[pos] == '*' && line[pos+1] == '*') {
            size_t end = line.find("**", pos + 2);
            if (end != std::string::npos) {
                if (!first) ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Text, bold_color);
                ImGui::TextUnformatted(line.substr(pos + 2, end - pos - 2).c_str());
                ImGui::PopStyleColor();
                first = false;
                pos = end + 2;
                continue;
            }
        }
        // Accumulate plain text up to next special char
        size_t next = line.find_first_of("`*", pos);
        std::string chunk = line.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        if (!chunk.empty()) {
            if (!first) ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, base_color);
            ImGui::TextUnformatted(chunk.c_str());
            ImGui::PopStyleColor();
            first = false;
        }
        if (next == std::string::npos) break;
        pos = next;
        // If we get here the special char didn't form a valid span — emit it literally
        if (!first) ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, base_color);
        ImGui::TextUnformatted(line.substr(pos, 1).c_str());
        ImGui::PopStyleColor();
        first = false;
        ++pos;
    }
    if (first) {
        ImGui::PushStyleColor(ImGuiCol_Text, base_color);
        ImGui::TextUnformatted("");
        ImGui::PopStyleColor();
    }
}

// Render markdown text block. base_color applies to non-special text.
static void RenderMarkdown(const std::string& text, const ImVec4& base_color) {
    std::istringstream stream(text);
    std::string line;
    bool in_code_block = false;

    while (std::getline(stream, line)) {
        // Code fence
        if (line.rfind("```", 0) == 0) {
            if (!in_code_block) {
                in_code_block = true;
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                ImGui::BeginChild(("##cb" + std::to_string(ImGui::GetCursorScreenPos().y)).c_str(),
                                  ImVec2(ImGui::GetContentRegionAvail().x, 0.0f),
                                  true, ImGuiWindowFlags_NoScrollbar);
            } else {
                in_code_block = false;
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
            continue;
        }

        if (in_code_block) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.8f, 0.6f, 1.0f));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
            continue;
        }

        // H1 ## H2 ### H3
        if (line.rfind("### ", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
            ImGui::TextUnformatted(line.substr(4).c_str());
            ImGui::PopStyleColor();
        } else if (line.rfind("## ", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.95f, 1.0f, 1.0f));
            ImGui::Separator();
            ImGui::TextUnformatted(line.substr(3).c_str());
            ImGui::PopStyleColor();
        } else if (line.rfind("# ", 0) == 0) {
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::TextUnformatted(line.substr(2).c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
        } else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) {
            // Bullet list item
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, base_color);
            ImGui::TextUnformatted("\xE2\x80\xA2 "); // • UTF-8
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);
            RenderInline(line.substr(2), base_color);
        } else if (line.empty()) {
            ImGui::Spacing();
        } else {
            RenderInline(line, base_color);
        }
    }

    // Unterminated code block cleanup
    if (in_code_block) {
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

// ─── ImGui Draw ─────────────────────────────────────────────────────────────

void ChatPanel::Draw(ControlServer& server, dse::runtime::EngineInstance& engine) {
    // Tool calls collected from bridge output, executed after releasing the mutex
    struct ToolCallEntry { std::string name, args, call_id; };
    std::vector<ToolCallEntry> pending_tool_calls;

    // Process pending output from bridge (hold mutex only while draining queue)
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
                messages_.push_back({ChatRole::Assistant, bmsg.content, current_agent_id_});
                messages_.back().start_time = std::chrono::steady_clock::now();
                messages_.back().end_time = std::chrono::steady_clock::now();
                waiting_for_response_ = false;
                scroll_to_bottom_ = true;
                break;
            case BridgeMessageType::StreamStart:
                if (messages_.empty() || messages_.back().role != ChatRole::Assistant) {
                    messages_.push_back({ChatRole::Assistant, "", current_agent_id_});
                    messages_.back().start_time = std::chrono::steady_clock::now();
                    messages_.back().last_update = std::chrono::steady_clock::now();
                }
                messages_.back().is_streaming = true;
                break;
            case BridgeMessageType::StreamChunk:
                if (!messages_.empty() && messages_.back().role == ChatRole::Assistant) {
                    messages_.back().content += bmsg.content;
                    messages_.back().last_update = std::chrono::steady_clock::now();
                }
                scroll_to_bottom_ = true;
                break;
            case BridgeMessageType::StreamEnd:
                if (!messages_.empty()) {
                    messages_.back().is_streaming = false;
                    messages_.back().end_time = std::chrono::steady_clock::now();
                }
                waiting_for_response_ = false;
                break;
            case BridgeMessageType::ToolCall:
                messages_.push_back({ChatRole::System, "Calling: " + bmsg.tool_name});
                scroll_to_bottom_ = true;
                pending_tool_calls.push_back({bmsg.tool_name, bmsg.tool_args, bmsg.call_id});
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
            case BridgeMessageType::TokenUsage:
                {
                    std::string token_info = "Tokens: " + std::to_string(bmsg.input_tokens) +
                                           " in, " + std::to_string(bmsg.output_tokens) + " out";
                    messages_.push_back({ChatRole::System, token_info});
                    scroll_to_bottom_ = true;
                }
                break;
            case BridgeMessageType::CancelAck:
                waiting_for_response_ = false;
                if (!messages_.empty() && messages_.back().is_streaming)
                    messages_.back().is_streaming = false;
                break;
            default:
                messages_.push_back({ChatRole::System, "[bridge] " + line});
                break;
            }
        }
    } // mutex released here

    // Execute tool calls outside the mutex so reader thread isn't blocked
    for (const auto& tc : pending_tool_calls)
        ExecuteToolCall(tc.name, tc.args, tc.call_id, server, engine);

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
        // Prefix label
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(prefix);
        ImGui::PopStyleColor();
        if (prefix[0] != '\0') ImGui::SameLine(0, 0);

        // Render content: Markdown for Assistant, plain wrapped for User/System/Tool
        if (msg.role == ChatRole::Assistant) {
            RenderMarkdown(msg.content, color);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", msg.content.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
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

    // Agent selector
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("Agent", current_agent_id_.c_str())) {
        if (ImGui::Selectable("general", current_agent_id_ == "general")) {
            SetCurrentAgent("general");
        }
        if (ImGui::Selectable("scene_architect", current_agent_id_ == "scene_architect")) {
            SetCurrentAgent("scene_architect");
        }
        if (ImGui::Selectable("script_writer", current_agent_id_ == "script_writer")) {
            SetCurrentAgent("script_writer");
        }
        if (ImGui::Selectable("asset_manager", current_agent_id_ == "asset_manager")) {
            SetCurrentAgent("asset_manager");
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    bool send = false;
    ImGui::PushItemWidth(-100);
    if (ImGui::InputText("##chat_input", input_buf_, sizeof(input_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        send = true;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    
    if (waiting_for_response_) {
        // Show Stop button during generation
        if (ImGui::Button("Stop", ImVec2(50, 0))) {
            CancelGeneration();
        }
    } else if (bridge_crashed_) {
        // Bridge crashed: show reconnect button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.1f, 1.0f));
        if (ImGui::Button("Reconnect", ImVec2(90, 0))) {
            bridge_crashed_ = false;
            StartBridge();
        }
        ImGui::PopStyleColor();
    } else {
        // Normal: Send button
        if (ImGui::Button("Send", ImVec2(50, 0)) || send) {
            std::string text(input_buf_);
            if (!text.empty()) {
                // Resolve @mentions and prepend context
                std::string context = ResolveMentions(text, mention_resolver_);
                std::string send_text = context.empty() ? text : context + "\n用户问题：" + text;

                messages_.push_back({ChatRole::User, text, current_agent_id_});
                SendToBridge(send_text);
                input_buf_[0] = '\0';
                scroll_to_bottom_ = true;
                // Auto-save history after each user message
                if (!history_path_.empty())
                    SaveHistory(history_path_);
            }
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ClearHistory();
    }
}

} // namespace dse::editor
