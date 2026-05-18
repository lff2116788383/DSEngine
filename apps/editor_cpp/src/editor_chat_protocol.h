#pragma once

#include <string>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace dse::editor {

// ─── Bridge 协议消息类型 ────────────────────────────────────────────────────

enum class BridgeMessageType {
    AssistantMessage,
    ToolCall,
    Error,
    Status,
    Unknown
};

struct BridgeMessage {
    BridgeMessageType type = BridgeMessageType::Unknown;
    std::string content;      // assistant_message / error / status
    std::string tool_name;    // tool_call
    std::string tool_args;    // tool_call
    std::string call_id;      // tool_call
    std::string raw;          // 原始文本（解析失败时使用）
    bool valid = false;       // JSON 解析是否成功
};

// ─── 纯函数：解析 bridge stdout 的 JSON-line ───────────────────────────────

inline BridgeMessage ParseBridgeMessage(const std::string& line) {
    BridgeMessage msg;
    msg.raw = line;

    rapidjson::Document doc;
    doc.Parse(line.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        msg.type = BridgeMessageType::Unknown;
        msg.content = line;
        return msg;
    }

    msg.valid = true;

    std::string type_str;
    if (doc.HasMember("type") && doc["type"].IsString())
        type_str = doc["type"].GetString();

    if (type_str == "assistant_message") {
        msg.type = BridgeMessageType::AssistantMessage;
        if (doc.HasMember("content") && doc["content"].IsString())
            msg.content = doc["content"].GetString();
    }
    else if (type_str == "tool_call") {
        msg.type = BridgeMessageType::ToolCall;
        if (doc.HasMember("name") && doc["name"].IsString())
            msg.tool_name = doc["name"].GetString();
        if (doc.HasMember("arguments") && doc["arguments"].IsString())
            msg.tool_args = doc["arguments"].GetString();
        if (doc.HasMember("call_id") && doc["call_id"].IsString())
            msg.call_id = doc["call_id"].GetString();
    }
    else if (type_str == "error") {
        msg.type = BridgeMessageType::Error;
        if (doc.HasMember("message") && doc["message"].IsString())
            msg.content = doc["message"].GetString();
    }
    else if (type_str == "status") {
        msg.type = BridgeMessageType::Status;
        if (doc.HasMember("message") && doc["message"].IsString())
            msg.content = doc["message"].GetString();
    }
    else {
        msg.type = BridgeMessageType::Unknown;
    }

    return msg;
}

// ─── 纯函数：构建发送给 bridge 的 user_message JSON-line ───────────────────

inline std::string BuildUserMessage(const std::string& content) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& alloc = doc.GetAllocator();
    doc.AddMember("type", "user_message", alloc);
    doc.AddMember("content", rapidjson::Value(content.c_str(), alloc), alloc);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString()) + "\n";
}

// ─── 纯函数：构建 tool_result JSON-line ────────────────────────────────────

inline std::string BuildToolResult(const std::string& call_id,
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

} // namespace dse::editor
