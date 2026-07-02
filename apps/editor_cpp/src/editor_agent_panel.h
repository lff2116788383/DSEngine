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

#include "editor_agent_protocol.h"

namespace dse::runtime {
class EngineInstance;
}

namespace dse::editor {

class ControlServer;

// ─── 聊天消息 ───────────────────────────────────────────────────────────────

enum class MessageRole { User, Assistant, System, ToolResult };

struct PanelMessage {
    MessageRole role;
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

// ─── Agent Panel (统一面板，替代旧 ChatPanel) ──────────────────────────────

/// 编辑器统一 AI 面板 (v2.0)。
/// 合并了原 ChatPanel 的全部功能 + Agent 任务规划/审批/执行/回滚。
/// 通过 Python 子进程 (agent_bridge.py) 调用 LLM API，
/// Tool 调用在 C++ 侧直接执行（同进程函数调用，不走 WebSocket）。
///
/// 简单请求: classify -> direct 快速路径（体验与原 Chat 一致）
/// 复杂任务: classify -> checkpoint -> plan -> approve -> execute -> verify -> summarize
class AgentPanel {
public:
    AgentPanel();
    ~AgentPanel();

    AgentPanel(const AgentPanel&) = delete;
    AgentPanel& operator=(const AgentPanel&) = delete;

    /// 设置 Python bridge 脚本路径 (agent_bridge.py)
    void SetBridgePath(const std::string& path);

    /// 设置当前 Agent/Specialist ID
    void SetCurrentAgent(const std::string& agent_id);

    /// 设置 @mention 上下文解析器
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
    // ── Bridge 子进程管理 ──
    void StartBridge();
    void StopBridge();
    void SendToBridge(const std::string& json_line);
    void CancelGeneration();

    // ── 工具调用执行 ──
    void ExecuteToolCall(const std::string& tool_name, const std::string& args_json,
                         const std::string& call_id,
                         ControlServer& server, dse::runtime::EngineInstance& engine);

    // ── Agent 操作 ──
    void ApprovePlan();
    void RejectPlan();
    void RollbackToCheckpoint();

    // ── ImGui 子区域绘制 ──
    void DrawTaskPanel();
    void DrawMessages(ControlServer& server, dse::runtime::EngineInstance& engine);
    void DrawInputArea();

    // ── 消息处理 ──
    void ProcessBridgeOutput(ControlServer& server, dse::runtime::EngineInstance& engine);

    // ── @mention 解析 ──
    static std::string ResolveMentions(const std::string& text,
                                        const std::function<std::string(const std::string&)>& resolver);

    // ── UI 状态 ──
    char input_buf_[1024] = {};
    std::vector<PanelMessage> messages_;
    std::string current_agent_id_ = "general";
    bool scroll_to_bottom_ = false;
    bool waiting_for_response_ = false;
    std::atomic<bool> bridge_crashed_{false};
    std::string history_path_;
    std::function<std::string(const std::string&)> mention_resolver_;
    int edit_msg_idx_ = -1;
    bool focus_input_next_frame_ = false;
    int total_input_tokens_ = 0;
    int total_output_tokens_ = 0;
    bool force_agent_mode_ = false;     // 用户强制 Agent 模式

    // ── Agent 状态 ──
    std::vector<AgentTask> task_plan_;   // 当前任务计划
    bool plan_awaiting_approval_ = false;
    bool agent_executing_ = false;
    std::string checkpoint_path_;        // 场景快照路径

    // ── Python 子进程 ──
    std::string bridge_path_;
    std::atomic<bool> bridge_running_{false};
    std::thread reader_thread_;
    std::mutex output_mutex_;
    std::deque<std::string> pending_output_;

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
