#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <sstream>
#include <unordered_set>

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
    
    /// 设置 @mention 上下文解析器
    /// resolver(mention_token) 返回对应的上下文字符串，空串表示不处理
    void SetMentionResolver(std::function<std::string(const std::string&)> resolver);
    
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
    std::atomic<bool> bridge_crashed_{false}; // bridge 意外退出
    std::string history_path_;          // 自动保存路径
    std::function<std::string(const std::string&)> mention_resolver_; // @mention 解析器
    int edit_msg_idx_ = -1;             // 正在编辑的 User 消息索引（-1 = 无）
    bool focus_input_next_frame_ = false; // Edit 点击后下一帧要自动聚焦 InputText
    int total_input_tokens_  = 0;       // 累计 input tokens
    int total_output_tokens_ = 0;       // 累计 output tokens

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
