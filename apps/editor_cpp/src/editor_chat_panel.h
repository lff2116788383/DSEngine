#pragma once

#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>

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
    bool is_streaming = false;  // 正在流式接收中
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

    /// 每帧调用：绘制 ImGui UI + 处理子进程 I/O
    void Draw(ControlServer& server, dse::runtime::EngineInstance& engine);

private:
    void SendToBridge(const std::string& text);
    void StartBridge();
    void StopBridge();
    void ExecuteToolCall(const std::string& tool_name, const std::string& args_json,
                         const std::string& call_id,
                         ControlServer& server, dse::runtime::EngineInstance& engine);

    // UI 状态
    char input_buf_[1024] = {};
    std::vector<ChatMessage> messages_;
    bool scroll_to_bottom_ = false;
    bool waiting_for_response_ = false;

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
