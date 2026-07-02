#pragma once

#include <string>
#include <vector>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace dse::editor {

// ─── Agent Bridge 协议消息类型 (v2.0, 超集于原 ChatPanel 协议) ──────────────

enum class AgentMessageType {
    // ── 原有 Chat 消息类型 (完整保留) ──
    AssistantMessage,
    ToolCall,
    Error,
    Status,
    StreamStart,
    StreamChunk,
    StreamEnd,
    TokenUsage,
    CancelAck,

    // ── Agent 新增消息类型 ──
    AgentPlan,              // 任务规划完成，等待审批
    AgentTaskStatus,        // 子任务状态变更
    AgentToolCall,          // Agent 工具调用通知 (仅 UI 显示用)
    AgentComplete,          // Agent 执行完成报告
    AgentCheckpointCreated, // 场景快照已创建
    AgentSafetyBlocked,     // 安全策略拦截

    Unknown
};

// ─── Agent 子任务 ───────────────────────────────────────────────────────────

struct AgentTask {
    std::string id;
    std::string title;
    std::string description;
    std::string specialist;
    std::string status;         // "pending" | "running" | "done" | "failed" | "retrying" | "skipped"
    std::string result;
    std::string error;
    std::vector<std::string> dependencies;
    int retry_count = 0;
    int estimated_tools = 0;
    std::string verification;   // "deterministic" | "llm" | ""
};

// ─── Bridge 消息结构 ────────────────────────────────────────────────────────

struct AgentBridgeMessage {
    AgentMessageType type = AgentMessageType::Unknown;

    // Chat fields (原有)
    std::string content;
    std::string tool_name;
    std::string tool_args;
    std::string call_id;
    std::string raw;
    bool valid = false;
    int chunk_id = 0;
    bool is_last = false;
    int input_tokens = 0;
    int output_tokens = 0;
    std::string model;

    // Agent fields (新增)
    std::vector<AgentTask> plan;
    int task_count = 0;
    int estimated_tools_total = 0;
    std::string task_id;
    std::string task_status;
    std::string task_result;
    std::string task_error;
    int task_retry = 0;
    std::string checkpoint_path;
    std::string safety_reason;
    std::string goal;
    int total_tasks = 0;
    int completed_tasks = 0;
    int failed_tasks = 0;
    int total_tokens_used = 0;
};

// ─── 解析 Bridge stdout JSON-line ───────────────────────────────────────────

inline AgentBridgeMessage ParseAgentMessage(const std::string& line) {
    AgentBridgeMessage msg;
    msg.raw = line;

    rapidjson::Document doc;
    doc.Parse(line.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        msg.type = AgentMessageType::Unknown;
        msg.content = line;
        return msg;
    }

    msg.valid = true;

    std::string type_str;
    if (doc.HasMember("type") && doc["type"].IsString())
        type_str = doc["type"].GetString();

    // ── 原有 Chat 消息类型 ──
    if (type_str == "assistant_message") {
        msg.type = AgentMessageType::AssistantMessage;
        if (doc.HasMember("content") && doc["content"].IsString())
            msg.content = doc["content"].GetString();
    }
    else if (type_str == "tool_call") {
        msg.type = AgentMessageType::ToolCall;
        if (doc.HasMember("name") && doc["name"].IsString())
            msg.tool_name = doc["name"].GetString();
        if (doc.HasMember("arguments") && doc["arguments"].IsString())
            msg.tool_args = doc["arguments"].GetString();
        if (doc.HasMember("call_id") && doc["call_id"].IsString())
            msg.call_id = doc["call_id"].GetString();
    }
    else if (type_str == "error") {
        msg.type = AgentMessageType::Error;
        if (doc.HasMember("message") && doc["message"].IsString())
            msg.content = doc["message"].GetString();
    }
    else if (type_str == "status") {
        msg.type = AgentMessageType::Status;
        if (doc.HasMember("message") && doc["message"].IsString())
            msg.content = doc["message"].GetString();
    }
    else if (type_str == "stream_start") {
        msg.type = AgentMessageType::StreamStart;
    }
    else if (type_str == "stream_chunk") {
        msg.type = AgentMessageType::StreamChunk;
        if (doc.HasMember("content") && doc["content"].IsString())
            msg.content = doc["content"].GetString();
        if (doc.HasMember("chunk_id") && doc["chunk_id"].IsInt())
            msg.chunk_id = doc["chunk_id"].GetInt();
        if (doc.HasMember("is_last") && doc["is_last"].IsBool())
            msg.is_last = doc["is_last"].GetBool();
    }
    else if (type_str == "stream_end") {
        msg.type = AgentMessageType::StreamEnd;
        if (doc.HasMember("chunk_id") && doc["chunk_id"].IsInt())
            msg.chunk_id = doc["chunk_id"].GetInt();
    }
    else if (type_str == "token_usage") {
        msg.type = AgentMessageType::TokenUsage;
        if (doc.HasMember("input_tokens") && doc["input_tokens"].IsInt())
            msg.input_tokens = doc["input_tokens"].GetInt();
        if (doc.HasMember("output_tokens") && doc["output_tokens"].IsInt())
            msg.output_tokens = doc["output_tokens"].GetInt();
        if (doc.HasMember("model") && doc["model"].IsString())
            msg.model = doc["model"].GetString();
    }
    else if (type_str == "cancel_ack") {
        msg.type = AgentMessageType::CancelAck;
    }
    // ── Agent 新增消息类型 ──
    else if (type_str == "agent_plan") {
        msg.type = AgentMessageType::AgentPlan;
        if (doc.HasMember("task_count") && doc["task_count"].IsInt())
            msg.task_count = doc["task_count"].GetInt();
        if (doc.HasMember("estimated_tools") && doc["estimated_tools"].IsInt())
            msg.estimated_tools_total = doc["estimated_tools"].GetInt();
        if (doc.HasMember("plan") && doc["plan"].IsArray()) {
            for (const auto& item : doc["plan"].GetArray()) {
                AgentTask task;
                if (item.HasMember("id") && item["id"].IsString())
                    task.id = item["id"].GetString();
                if (item.HasMember("title") && item["title"].IsString())
                    task.title = item["title"].GetString();
                if (item.HasMember("description") && item["description"].IsString())
                    task.description = item["description"].GetString();
                if (item.HasMember("specialist") && item["specialist"].IsString())
                    task.specialist = item["specialist"].GetString();
                if (item.HasMember("estimated_tools") && item["estimated_tools"].IsInt())
                    task.estimated_tools = item["estimated_tools"].GetInt();
                if (item.HasMember("dependencies") && item["dependencies"].IsArray()) {
                    for (const auto& dep : item["dependencies"].GetArray()) {
                        if (dep.IsString())
                            task.dependencies.push_back(dep.GetString());
                    }
                }
                task.status = "pending";
                msg.plan.push_back(std::move(task));
            }
        }
    }
    else if (type_str == "agent_task_status") {
        msg.type = AgentMessageType::AgentTaskStatus;
        if (doc.HasMember("task_id") && doc["task_id"].IsString())
            msg.task_id = doc["task_id"].GetString();
        if (doc.HasMember("status") && doc["status"].IsString())
            msg.task_status = doc["status"].GetString();
        if (doc.HasMember("result") && doc["result"].IsString())
            msg.task_result = doc["result"].GetString();
        if (doc.HasMember("error") && doc["error"].IsString())
            msg.task_error = doc["error"].GetString();
        if (doc.HasMember("retry") && doc["retry"].IsInt())
            msg.task_retry = doc["retry"].GetInt();
        if (doc.HasMember("verification") && doc["verification"].IsString())
            msg.content = doc["verification"].GetString();
    }
    else if (type_str == "agent_tool_call") {
        msg.type = AgentMessageType::AgentToolCall;
        if (doc.HasMember("task_id") && doc["task_id"].IsString())
            msg.task_id = doc["task_id"].GetString();
        if (doc.HasMember("tool") && doc["tool"].IsString())
            msg.tool_name = doc["tool"].GetString();
    }
    else if (type_str == "agent_complete") {
        msg.type = AgentMessageType::AgentComplete;
        if (doc.HasMember("checkpoint_path") && doc["checkpoint_path"].IsString())
            msg.checkpoint_path = doc["checkpoint_path"].GetString();
        if (doc.HasMember("summary") && doc["summary"].IsObject()) {
            const auto& s = doc["summary"];
            if (s.HasMember("goal") && s["goal"].IsString())
                msg.goal = s["goal"].GetString();
            if (s.HasMember("total_tasks") && s["total_tasks"].IsInt())
                msg.total_tasks = s["total_tasks"].GetInt();
            if (s.HasMember("completed") && s["completed"].IsInt())
                msg.completed_tasks = s["completed"].GetInt();
            if (s.HasMember("failed") && s["failed"].IsInt())
                msg.failed_tasks = s["failed"].GetInt();
            if (s.HasMember("total_tokens") && s["total_tokens"].IsInt())
                msg.total_tokens_used = s["total_tokens"].GetInt();
        }
    }
    else if (type_str == "agent_checkpoint_created") {
        msg.type = AgentMessageType::AgentCheckpointCreated;
        if (doc.HasMember("path") && doc["path"].IsString())
            msg.checkpoint_path = doc["path"].GetString();
        if (doc.HasMember("message") && doc["message"].IsString())
            msg.content = doc["message"].GetString();
    }
    else if (type_str == "agent_safety_blocked") {
        msg.type = AgentMessageType::AgentSafetyBlocked;
        if (doc.HasMember("tool") && doc["tool"].IsString())
            msg.tool_name = doc["tool"].GetString();
        if (doc.HasMember("reason") && doc["reason"].IsString())
            msg.safety_reason = doc["reason"].GetString();
    }
    else {
        msg.type = AgentMessageType::Unknown;
    }

    return msg;
}

// ─── 构建发送给 bridge 的消息 ───────────────────────────────────────────────

inline std::string BuildAgentUserMessage(const std::string& content,
                                          bool force_agent_mode = false) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& alloc = doc.GetAllocator();
    doc.AddMember("type", "user_message", alloc);
    doc.AddMember("content", rapidjson::Value(content.c_str(), alloc), alloc);
    doc.AddMember("force_agent_mode", force_agent_mode, alloc);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString()) + "\n";
}

inline std::string BuildAgentToolResult(const std::string& call_id,
                                         const std::string& result_json) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& alloc = doc.GetAllocator();
    doc.AddMember("type", "tool_result", alloc);
    doc.AddMember("call_id", rapidjson::Value(call_id.c_str(), alloc), alloc);
    doc.AddMember("result", rapidjson::Value(result_json.c_str(), alloc), alloc);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString()) + "\n";
}

inline std::string BuildAgentApprove(const std::string& status,
                                      const std::string& feedback = "") {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& alloc = doc.GetAllocator();
    doc.AddMember("type", "agent_approve", alloc);
    doc.AddMember("status", rapidjson::Value(status.c_str(), alloc), alloc);
    if (!feedback.empty())
        doc.AddMember("feedback", rapidjson::Value(feedback.c_str(), alloc), alloc);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString()) + "\n";
}

inline std::string BuildAgentRollback() {
    rapidjson::Document doc(rapidjson::kObjectType);
    doc.AddMember("type", "agent_rollback", doc.GetAllocator());

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString()) + "\n";
}

inline std::string BuildAgentCancel() {
    rapidjson::Document doc(rapidjson::kObjectType);
    doc.AddMember("type", "cancel", doc.GetAllocator());

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString()) + "\n";
}

inline std::string BuildAgentClearHistory() {
    rapidjson::Document doc(rapidjson::kObjectType);
    doc.AddMember("type", "clear_history", doc.GetAllocator());

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString()) + "\n";
}

} // namespace dse::editor
