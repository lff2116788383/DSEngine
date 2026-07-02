#include "editor_agent_panel.h"
#include "editor_agent_protocol.h"
#include "editor_control_server.h"
#include "editor_console_panel.h"
#include "editor_ai_config.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <algorithm>

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

AgentPanel::AgentPanel() {
    messages_.push_back({MessageRole::System,
        "AI Agent ready (v2.0). Type a message to interact with the scene.\n"
        "Simple requests execute directly. Complex tasks go through plan/approve/execute."});
}

AgentPanel::~AgentPanel() {
    StopBridge();
}

void AgentPanel::SetBridgePath(const std::string& path) {
    bridge_path_ = path;
}

void AgentPanel::SetMentionResolver(std::function<std::string(const std::string&)> resolver) {
    mention_resolver_ = std::move(resolver);
}

void AgentPanel::SetCurrentAgent(const std::string& agent_id) {
    current_agent_id_ = agent_id;
}

// ─── @mention 解析 ──────────────────────────────────────────────────────────

std::string AgentPanel::ResolveMentions(const std::string& text,
                                         const std::function<std::string(const std::string&)>& resolver) {
    if (!resolver) return {};

    std::string context_block;
    std::unordered_set<std::string> seen;
    size_t pos = 0;
    while ((pos = text.find('@', pos)) != std::string::npos) {
        ++pos;
        size_t end = pos;
        while (end < text.size() && (std::isalnum((unsigned char)text[end]) ||
               text[end] == '_' || text[end] == ':' || text[end] == '/'))
            ++end;
        if (end > pos) {
            std::string token = text.substr(pos - 1, end - pos + 1);
            if (seen.insert(token).second) {
                std::string resolved = resolver(token);
                if (!resolved.empty()) {
                    context_block += resolved;
                    if (context_block.back() != '\n') context_block += '\n';
                }
            }
        }
        pos = end;
    }
    return context_block;
}

void AgentPanel::ClearHistory() {
    messages_.clear();
    messages_.push_back({MessageRole::System, "Conversation cleared."});
    task_plan_.clear();
    plan_awaiting_approval_ = false;
    agent_executing_ = false;
    checkpoint_path_.clear();

    if (bridge_running_) {
        std::string line = BuildAgentClearHistory();
#ifdef _WIN32
        DWORD written;
        WriteFile(stdin_write_, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
#else
        write(stdin_fd_, line.c_str(), line.size());
#endif
    }
}

// ─── History Persistence ────────────────────────────────────────────────────

void AgentPanel::SaveHistory(const std::string& path) {
    if (path.empty() || messages_.empty()) return;

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    rapidjson::Document doc(rapidjson::kArrayType);
    auto& alloc = doc.GetAllocator();

    for (const auto& msg : messages_) {
        if (msg.role == MessageRole::ToolResult) continue;
        rapidjson::Value obj(rapidjson::kObjectType);
        const char* role_str =
            msg.role == MessageRole::User      ? "user"      :
            msg.role == MessageRole::Assistant ? "assistant" : "system";
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

void AgentPanel::LoadHistory(const std::string& path) {
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

        MessageRole role = MessageRole::System;
        if (role_str == "user")          role = MessageRole::User;
        else if (role_str == "assistant") role = MessageRole::Assistant;

        messages_.push_back({role, content_s, agent_id});
    }
    scroll_to_bottom_ = true;
}

// ─── Bridge subprocess management ───────────────────────────────────────────

void AgentPanel::StartBridge() {
    if (bridge_running_) return;

    if (bridge_path_.empty() || !std::filesystem::exists(bridge_path_)) {
        messages_.push_back({MessageRole::System,
            "Error: agent_bridge.py not found. Expected at: " + bridge_path_});
        return;
    }

    // Apply AIConfigManager settings as environment variables
    {
        auto& cfg = AIConfigManager::Instance().GetConfig();
        if (!cfg.providers.empty()) {
            const auto& p = cfg.providers[
                std::clamp(cfg.current_provider_index, 0, (int)cfg.providers.size() - 1)];
#ifdef _WIN32
            if (!p.api_key.empty())
                SetEnvironmentVariableA("OPENAI_API_KEY", p.api_key.c_str());
            if (!p.base_url.empty())
                SetEnvironmentVariableA("OPENAI_BASE_URL", p.base_url.c_str());
            if (!p.model.empty())
                SetEnvironmentVariableA("OPENAI_MODEL", p.model.c_str());
            if (!p.proxy_url.empty())
                SetEnvironmentVariableA("HTTP_PROXY", p.proxy_url.c_str());
            SetEnvironmentVariableA("OPENAI_TEMPERATURE",
                std::to_string(p.temperature).c_str());
            SetEnvironmentVariableA("OPENAI_MAX_TOKENS",
                std::to_string(p.max_tokens).c_str());
            SetEnvironmentVariableA("OPENAI_TIMEOUT_MS",
                std::to_string(p.timeout_ms).c_str());
            if (!p.image_model.empty())
                SetEnvironmentVariableA("OPENAI_IMAGE_MODEL", p.image_model.c_str());
#else
            if (!p.api_key.empty())
                setenv("OPENAI_API_KEY", p.api_key.c_str(), 1);
            if (!p.base_url.empty())
                setenv("OPENAI_BASE_URL", p.base_url.c_str(), 1);
            if (!p.model.empty())
                setenv("OPENAI_MODEL", p.model.c_str(), 1);
            if (!p.proxy_url.empty())
                setenv("HTTP_PROXY", p.proxy_url.c_str(), 1);
            setenv("OPENAI_TEMPERATURE",
                std::to_string(p.temperature).c_str(), 1);
            setenv("OPENAI_MAX_TOKENS",
                std::to_string(p.max_tokens).c_str(), 1);
            setenv("OPENAI_TIMEOUT_MS",
                std::to_string(p.timeout_ms).c_str(), 1);
            if (!p.image_model.empty())
                setenv("OPENAI_IMAGE_MODEL", p.image_model.c_str(), 1);
#endif
        }
        if (!cfg.default_agent.empty())
            current_agent_id_ = cfg.default_agent;
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
        messages_.push_back({MessageRole::System,
            "Error: Failed to start agent_bridge.py (error " +
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

    fcntl(stdout_fd_, F_SETFL, fcntl(stdout_fd_, F_GETFL) | O_NONBLOCK);
#endif

    bridge_running_ = true;
    bridge_crashed_ = false;

    reader_thread_ = std::thread([this]() {
        std::string line_buf;
        char ch;
        while (bridge_running_) {
#ifdef _WIN32
            DWORD bytes_read = 0;
            BOOL ok = ReadFile(stdout_read_, &ch, 1, &bytes_read, nullptr);
            if (!ok || bytes_read == 0) {
                if (bridge_running_) {
                    bridge_crashed_ = true;
                    std::lock_guard<std::mutex> lock(output_mutex_);
                    pending_output_.push_back(
                        R"({"type":"status","message":"[Agent Bridge] Process exited unexpectedly."})");
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
                        R"({"type":"status","message":"[Agent Bridge] Process exited unexpectedly."})");
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

    EditorLog(LogLevel::Info, "[AgentPanel] Agent bridge started.");
}

void AgentPanel::StopBridge() {
    const bool was_running = bridge_running_;
    bridge_running_ = false;

    if (was_running) {
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
    }

    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
}

// ─── Send message to bridge ─────────────────────────────────────────────────

void AgentPanel::SendToBridge(const std::string& json_line) {
    if (!bridge_running_) {
        StartBridge();
        if (!bridge_running_) return;
    }

#ifdef _WIN32
    DWORD written;
    WriteFile(stdin_write_, json_line.c_str(), static_cast<DWORD>(json_line.size()), &written, nullptr);
#else
    write(stdin_fd_, json_line.c_str(), json_line.size());
#endif
}

void AgentPanel::CancelGeneration() {
    if (!bridge_running_ || !waiting_for_response_) return;
    SendToBridge(BuildAgentCancel());
}

// ─── Execute tool call locally ──────────────────────────────────────────────

void AgentPanel::ExecuteToolCall(const std::string& tool_name, const std::string& args_json,
                                  const std::string& call_id,
                                  ControlServer& server, dse::runtime::EngineInstance& engine) {
    rapidjson::Document params;
    if (!args_json.empty()) {
        params.Parse(args_json.c_str());
        if (params.HasParseError()) {
            params.SetObject();
        }
    } else {
        params.SetObject();
    }

    auto response = server.DispatchTool(tool_name, params, engine);

    std::string result_json;
    if (response.is_error) {
        rapidjson::Document err_doc(rapidjson::kObjectType);
        err_doc.AddMember("error",
            rapidjson::Value(response.error_message.c_str(), err_doc.GetAllocator()),
            err_doc.GetAllocator());
        rapidjson::StringBuffer err_buf;
        rapidjson::Writer<rapidjson::StringBuffer> err_w(err_buf);
        err_doc.Accept(err_w);
        result_json = err_buf.GetString();
        messages_.push_back({MessageRole::ToolResult, tool_name + " -> Error: " + response.error_message});
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
        messages_.push_back({MessageRole::ToolResult, tool_name + " -> OK"});
    }
    scroll_to_bottom_ = true;

    // Send tool result back to bridge
    std::string line = BuildAgentToolResult(call_id, result_json);
    SendToBridge(line);
}

// ─── Agent Operations ───────────────────────────────────────────────────────

void AgentPanel::ApprovePlan() {
    plan_awaiting_approval_ = false;
    agent_executing_ = true;
    messages_.push_back({MessageRole::System, "Plan approved. Executing..."});
    SendToBridge(BuildAgentApprove("approved"));
}

void AgentPanel::RejectPlan() {
    plan_awaiting_approval_ = false;
    task_plan_.clear();
    messages_.push_back({MessageRole::System, "Plan rejected."});
    SendToBridge(BuildAgentApprove("rejected"));
}

void AgentPanel::RollbackToCheckpoint() {
    if (!checkpoint_path_.empty()) {
        messages_.push_back({MessageRole::System, "Rolling back to checkpoint..."});
        SendToBridge(BuildAgentRollback());
    }
}

// ─── Process Bridge Output ──────────────────────────────────────────────────

void AgentPanel::ProcessBridgeOutput(ControlServer& server, dse::runtime::EngineInstance& engine) {
    struct ToolCallEntry { std::string name, args, call_id; };
    std::vector<ToolCallEntry> pending_tool_calls;

    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        while (!pending_output_.empty()) {
            std::string line = std::move(pending_output_.front());
            pending_output_.pop_front();

            auto bmsg = ParseAgentMessage(line);

            if (!bmsg.valid) {
                messages_.push_back({MessageRole::System, "[bridge] " + line});
                continue;
            }

            switch (bmsg.type) {
            // ── Original Chat message types ──
            case AgentMessageType::AssistantMessage:
                messages_.push_back({MessageRole::Assistant, bmsg.content, current_agent_id_});
                messages_.back().start_time = std::chrono::steady_clock::now();
                messages_.back().end_time = std::chrono::steady_clock::now();
                waiting_for_response_ = false;
                scroll_to_bottom_ = true;
                break;

            case AgentMessageType::StreamStart:
                if (messages_.empty() || messages_.back().role != MessageRole::Assistant) {
                    messages_.push_back({MessageRole::Assistant, "", current_agent_id_});
                    messages_.back().start_time = std::chrono::steady_clock::now();
                    messages_.back().last_update = std::chrono::steady_clock::now();
                }
                messages_.back().is_streaming = true;
                break;

            case AgentMessageType::StreamChunk:
                if (!messages_.empty() && messages_.back().role == MessageRole::Assistant) {
                    messages_.back().content += bmsg.content;
                    messages_.back().last_update = std::chrono::steady_clock::now();
                }
                scroll_to_bottom_ = true;
                break;

            case AgentMessageType::StreamEnd:
                if (!messages_.empty()) {
                    messages_.back().is_streaming = false;
                    messages_.back().end_time = std::chrono::steady_clock::now();
                }
                waiting_for_response_ = false;
                break;

            case AgentMessageType::ToolCall:
                messages_.push_back({MessageRole::System, "Calling: " + bmsg.tool_name});
                scroll_to_bottom_ = true;
                pending_tool_calls.push_back({bmsg.tool_name, bmsg.tool_args, bmsg.call_id});
                break;

            case AgentMessageType::Error:
                messages_.push_back({MessageRole::System, "Error: " + bmsg.content});
                waiting_for_response_ = false;
                agent_executing_ = false;
                scroll_to_bottom_ = true;
                break;

            case AgentMessageType::Status:
                messages_.push_back({MessageRole::System, bmsg.content});
                scroll_to_bottom_ = true;
                break;

            case AgentMessageType::TokenUsage:
                total_input_tokens_  += bmsg.input_tokens;
                total_output_tokens_ += bmsg.output_tokens;
                break;

            case AgentMessageType::CancelAck:
                waiting_for_response_ = false;
                agent_executing_ = false;
                if (!messages_.empty() && messages_.back().is_streaming)
                    messages_.back().is_streaming = false;
                break;

            // ── Agent-specific message types ──
            case AgentMessageType::AgentPlan:
                task_plan_ = bmsg.plan;
                plan_awaiting_approval_ = true;
                messages_.push_back({MessageRole::System,
                    "Task plan ready (" + std::to_string(bmsg.task_count) + " tasks). "
                    "Review and approve below."});
                scroll_to_bottom_ = true;
                break;

            case AgentMessageType::AgentTaskStatus:
                for (auto& t : task_plan_) {
                    if (t.id == bmsg.task_id) {
                        t.status = bmsg.task_status;
                        if (!bmsg.task_result.empty()) t.result = bmsg.task_result;
                        if (!bmsg.task_error.empty())  t.error  = bmsg.task_error;
                        if (bmsg.task_retry > 0)       t.retry_count = bmsg.task_retry;
                        if (!bmsg.content.empty())     t.verification = bmsg.content;
                        break;
                    }
                }
                break;

            case AgentMessageType::AgentToolCall:
                // UI-only notification for Agent tool calls (already handled by ToolCall above)
                break;

            case AgentMessageType::AgentComplete:
                agent_executing_ = false;
                waiting_for_response_ = false;
                {
                    std::string summary = "Agent completed: " +
                        std::to_string(bmsg.completed_tasks) + "/" +
                        std::to_string(bmsg.total_tasks) + " tasks done";
                    if (bmsg.failed_tasks > 0)
                        summary += ", " + std::to_string(bmsg.failed_tasks) + " failed";
                    summary += " (" + std::to_string(bmsg.total_tokens_used) + " tokens)";
                    messages_.push_back({MessageRole::System, summary});
                }
                scroll_to_bottom_ = true;
                break;

            case AgentMessageType::AgentCheckpointCreated:
                checkpoint_path_ = bmsg.checkpoint_path;
                messages_.push_back({MessageRole::System, bmsg.content});
                scroll_to_bottom_ = true;
                break;

            case AgentMessageType::AgentSafetyBlocked:
                messages_.push_back({MessageRole::System,
                    "Safety blocked: " + bmsg.tool_name + " - " + bmsg.safety_reason});
                scroll_to_bottom_ = true;
                break;

            default:
                messages_.push_back({MessageRole::System, "[bridge] " + line});
                break;
            }
        }
    }

    for (const auto& tc : pending_tool_calls)
        ExecuteToolCall(tc.name, tc.args, tc.call_id, server, engine);
}

// ─── Markdown Renderer (reused from ChatPanel) ──────────────────────────────

static void RenderInline(const std::string& line, const ImVec4& base_color) {
    const ImVec4 code_color  = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);
    const ImVec4 bold_color  = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    size_t pos = 0;
    bool first = true;
    while (pos < line.size()) {
        if (line[pos] == '`') {
            size_t end = line.find('`', pos + 1);
            if (end != std::string::npos) {
                if (!first) ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Text, code_color);
                ImGui::TextUnformatted(line.substr(pos + 1, end - pos - 1).c_str());
                ImGui::PopStyleColor();
                first = false;
                pos = end + 1;
                continue;
            }
        }
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

static void RenderMarkdown(const std::string& text, const ImVec4& base_color) {
    std::istringstream stream(text);
    std::string line;
    bool in_code_block = false;

    while (std::getline(stream, line)) {
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
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, base_color);
            ImGui::TextUnformatted("\xE2\x80\xA2 ");
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);
            RenderInline(line.substr(2), base_color);
        } else if (line.empty()) {
            ImGui::Spacing();
        } else {
            RenderInline(line, base_color);
        }
    }

    if (in_code_block) {
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

// ─── Task Panel (Agent 任务视图) ────────────────────────────────────────────

void AgentPanel::DrawTaskPanel() {
    if (task_plan_.empty()) return;

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f, 0.95f, 1.0f, 1.0f), "Task Plan");

    for (int i = 0; i < static_cast<int>(task_plan_.size()); ++i) {
        const auto& task = task_plan_[i];

        ImVec4 status_color;
        const char* status_icon;
        if (task.status == "done") {
            status_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            status_icon = "[OK]";
        } else if (task.status == "running") {
            status_color = ImVec4(1.0f, 0.9f, 0.3f, 1.0f);
            status_icon = "[..]";
        } else if (task.status == "failed") {
            status_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            status_icon = "[XX]";
        } else if (task.status == "retrying") {
            status_color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
            status_icon = "[RT]";
        } else if (task.status == "skipped") {
            status_color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            status_icon = "[--]";
        } else {
            status_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            status_icon = "[  ]";
        }

        ImGui::PushStyleColor(ImGuiCol_Text, status_color);
        ImGui::Text("%s %s: %s", status_icon, task.id.c_str(), task.title.c_str());
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered() && !task.description.empty()) {
            ImGui::SetTooltip("%s\nSpecialist: %s\n%s",
                task.description.c_str(),
                task.specialist.c_str(),
                task.error.empty() ? "" : ("Error: " + task.error).c_str());
        }
    }

    // Approval / Rollback buttons
    if (plan_awaiting_approval_) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Approve Plan", ImVec2(120, 0))) {
            ApprovePlan();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Reject", ImVec2(70, 0))) {
            RejectPlan();
        }
        ImGui::PopStyleColor();
    }

    if (!checkpoint_path_.empty() && !agent_executing_) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.1f, 1.0f));
        if (ImGui::Button("Rollback", ImVec2(70, 0))) {
            RollbackToCheckpoint();
        }
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
}

// ─── ImGui Draw (Main Entry) ────────────────────────────────────────────────

void AgentPanel::Draw(ControlServer& server, dse::runtime::EngineInstance& engine) {
    // Limit message history
    static constexpr int kMaxMessages = 500;
    if (static_cast<int>(messages_.size()) > kMaxMessages) {
        auto first_non_system = std::find_if(messages_.begin() + 1, messages_.end(),
            [](const PanelMessage& m){ return m.role != MessageRole::System; });
        if (first_non_system != messages_.end())
            messages_.erase(first_non_system);
    }

    ProcessBridgeOutput(server, engine);

    // ── Task panel (Agent tasks) ──
    DrawTaskPanel();

    // ── Messages area ──
    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("AgentMessages", ImVec2(0, -footer_height), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    int resend_idx = -1;
    int edit_click_idx = -1;

    for (int mi = 0; mi < static_cast<int>(messages_.size()); ++mi) {
        const auto& msg = messages_[mi];
        ImVec4 color;
        const char* prefix;
        switch (msg.role) {
            case MessageRole::User:
                color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
                prefix = "You: ";
                break;
            case MessageRole::Assistant:
                color = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
                prefix = "AI: ";
                break;
            case MessageRole::ToolResult:
                color = ImVec4(0.8f, 0.8f, 0.4f, 1.0f);
                prefix = "Tool: ";
                break;
            default:
                color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                prefix = "";
                break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(prefix);
        ImGui::PopStyleColor();
        if (prefix[0] != '\0') ImGui::SameLine(0, 0);

        if (msg.role == MessageRole::Assistant) {
            RenderMarkdown(msg.content, color);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", msg.content.c_str());
            ImGui::PopStyleColor();
        }

        // Hover actions for User messages
        if (msg.role == MessageRole::User && !waiting_for_response_ && ImGui::IsItemHovered()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 0.6f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 1));
            ImGui::PushID(mi * 100 + 1);
            if (ImGui::SmallButton("Resend")) resend_idx = mi;
            ImGui::PopID();
            ImGui::SameLine(0, 4);
            ImGui::PushID(mi * 100 + 2);
            if (ImGui::SmallButton("Edit")) edit_click_idx = mi;
            ImGui::PopID();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        // Response time
        if (msg.role == MessageRole::Assistant && !msg.is_streaming &&
            msg.start_time.time_since_epoch().count()) {
            float ms = msg.response_time_ms();
            ImGui::SameLine();
            ImGui::TextDisabled("  %.0fms", ms);
        }

        ImGui::Spacing();
    }

    // Act on resend / edit clicks
    if (resend_idx >= 0 && resend_idx < static_cast<int>(messages_.size())) {
        std::string original_text = messages_[resend_idx].content;
        messages_.erase(messages_.begin() + resend_idx, messages_.end());
        messages_.push_back({MessageRole::User, original_text, current_agent_id_});
        std::string context = ResolveMentions(original_text, mention_resolver_);
        std::string send_text = context.empty() ? original_text : context + "\n" + original_text;
        SendToBridge(BuildAgentUserMessage(send_text, force_agent_mode_));
        waiting_for_response_ = true;
        scroll_to_bottom_ = true;
        if (!history_path_.empty()) SaveHistory(history_path_);
    }
    if (edit_click_idx >= 0 && edit_click_idx < static_cast<int>(messages_.size())) {
        const std::string& original = messages_[edit_click_idx].content;
        std::strncpy(input_buf_, original.c_str(), sizeof(input_buf_) - 1);
        input_buf_[sizeof(input_buf_) - 1] = '\0';
        edit_msg_idx_ = edit_click_idx;
        focus_input_next_frame_ = true;
    }

    if (waiting_for_response_) {
        if (agent_executing_)
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Agent executing...");
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Thinking...");
    }

    if (scroll_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
    }

    ImGui::EndChild();

    // ── Input area ──
    ImGui::Separator();

    // Agent selector + mode toggle
    ImGui::SetNextItemWidth(100);
    if (ImGui::BeginCombo("##agent_combo", current_agent_id_.c_str())) {
        const char* agents[] = {"general", "scene_architect", "script_writer", "asset_manager"};
        for (const char* a : agents) {
            if (ImGui::Selectable(a, current_agent_id_ == a))
                SetCurrentAgent(a);
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Specialist");

    ImGui::SameLine();

    // Force Agent mode checkbox
    ImGui::Checkbox("Agent", &force_agent_mode_);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Force Agent mode (plan/approve/execute)\nfor complex multi-step tasks");

    ImGui::SameLine();

    // Input text
    bool send = false;
    float btn_reserve = 110.0f;
    float input_w = ImGui::GetContentRegionAvail().x - btn_reserve;
    if (input_w < 80.0f) input_w = 80.0f;
    ImGui::PushItemWidth(input_w);
    if (focus_input_next_frame_) {
        ImGui::SetKeyboardFocusHere();
        focus_input_next_frame_ = false;
    }
    if (ImGui::InputText("##agent_input", input_buf_, sizeof(input_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        send = true;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (waiting_for_response_) {
        if (ImGui::Button("Stop", ImVec2(50, 0))) {
            CancelGeneration();
        }
    } else if (bridge_crashed_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.1f, 1.0f));
        if (ImGui::Button("Reconnect", ImVec2(90, 0))) {
            bridge_crashed_ = false;
            StartBridge();
        }
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button("Send", ImVec2(50, 0)) || send) {
            std::string text(input_buf_);
            if (!text.empty()) {
                if (edit_msg_idx_ >= 0 && edit_msg_idx_ < static_cast<int>(messages_.size())) {
                    messages_.erase(messages_.begin() + edit_msg_idx_, messages_.end());
                }
                edit_msg_idx_ = -1;

                std::string context = ResolveMentions(text, mention_resolver_);
                std::string send_text = context.empty() ? text : context + "\n" + text;

                messages_.push_back({MessageRole::User, text, current_agent_id_});
                SendToBridge(BuildAgentUserMessage(send_text, force_agent_mode_));
                waiting_for_response_ = true;
                input_buf_[0] = '\0';
                scroll_to_bottom_ = true;
                if (!history_path_.empty())
                    SaveHistory(history_path_);
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ClearHistory();
        total_input_tokens_  = 0;
        total_output_tokens_ = 0;
        edit_msg_idx_ = -1;
    }

    // Token statistics
    if (total_input_tokens_ > 0 || total_output_tokens_ > 0) {
        char token_buf[64];
        std::snprintf(token_buf, sizeof(token_buf), "in:%d out:%d",
                      total_input_tokens_, total_output_tokens_);
        float text_w = ImGui::CalcTextSize(token_buf).x;
        float avail   = ImGui::GetContentRegionAvail().x;
        if (avail > text_w + 8.0f)
            ImGui::SameLine(ImGui::GetCursorPosX() + avail - text_w);
        ImGui::TextDisabled("%s", token_buf);
    }
}

} // namespace dse::editor
