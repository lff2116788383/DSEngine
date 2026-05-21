#pragma once

#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace dse::runtime {
class EngineInstance;
}

namespace dse::editor {

class ControlServer;

// ─── Chat 消息 ──────────────────────────────────────────────────────────────

enum class ChatRole { User, Assistant, System, ToolResult };

struct ChatMessage {
    ChatRole role;
    std::string content;
    std::string agent_id;
    bool is_streaming = false;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::chrono::steady_clock::time_point last_update;
    
    float response_time_ms() const {
        auto end = end_time.time_since_epoch().count() ? end_time : std::chrono::steady_clock::now();
        return std::chrono::duration<float, std::milli>(end - start_time).count();
    }
};

// ─── ChatPanel ──────────────────────────────────────────────────────────────

/// 编辑器内建 AI Chat 面板。
/// 通过 Python 子进程 (ai_chat_bridge.py) 调用 LLM API，
/// Tool 调用在 C++ 侧直接执行（同进程函数调用，不走 WebSocket）。
class ChatPanel {
public:
    ChatPanel();
    ~ChatPanel();

    ChatPanel(const ChatPanel&) = delete;
    ChatPanel& operator=(const ChatPanel&) = delete;

    /// 设置 Python bridge 脚本路径
    void SetBridgePath(const std::string& path);
    
    /// 设置当前 Agent ID
    void SetCurrentAgent(const std::string& agent_id);
    
    /// 清空对话历史
    void ClearHistory();
    
    /// 保存对话历史到 JSON 文件
    void SaveHistory(const std::string& path);
    
    /// 从 JSON 文件加载对话历史
    void LoadHistory(const std::string& path);

    /// 每帧调用：绘制 ImGui UI + 处理子进程 I/O
    void Draw(ControlServer& server, dse::runtime::EngineInstance& engine);

private:
    void SendToBridge(const std::string& text);
    void CancelGeneration();
    void StartBridge();
    void StopBridge();
    void ExecuteToolCall(const std::string& tool_name, const std::string& args_json,
                         const std::string& call_id,
                         ControlServer& server, dse::runtime::EngineInstance& engine);

    // UI 状态
    char input_buf_[1024] = {};
    std::vector<ChatMessage> messages_;
    std::string current_agent_id_ = "general";
    bool scroll_to_bottom_ = false;
    bool waiting_for_response_ = false;
    bool bridge_crashed_ = false;       // bridge 意外退出
    std::string history_path_;          // 自动保存路径

    // Python 子进程
    std::string bridge_path_;
    std::atomic<bool> bridge_running_{false};
    std::thread reader_thread_;
    std::mutex output_mutex_;
    std::deque<std::string> pending_output_;  // 子进程 stdout 行

#ifdef _WIN32
    void* proc_handle_ = nullptr;
    void* stdin_write_ = nullptr;
    void* stdout_read_ = nullptr;
#else
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    pid_t bridge_pid_ = 0;
#endif
};

} // namespace dse::editor
