#include "editor_control_server.h"

#include <iostream>
#include <sstream>

#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXNetSystem.h>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>

namespace dse::editor {

// ─── JSON-RPC 2.0 error codes ──────────────────────────────────────────────

static constexpr int kParseError      = -32700;
static constexpr int kInvalidRequest  = -32600;
static constexpr int kMethodNotFound  = -32601;
static constexpr int kInvalidParams   = -32602;
static constexpr int kInternalError   = -32603;

// ─── ControlServer ──────────────────────────────────────────────────────────

ControlServer::ControlServer() {
    ix::initNetSystem();
}

ControlServer::~ControlServer() {
    Stop();
    ix::uninitNetSystem();
}

void ControlServer::RegisterTool(const std::string& method, ToolHandler handler) {
    tools_[method] = std::move(handler);
}

bool ControlServer::Start(int port) {
    if (running_) return true;

    server_ = std::make_unique<ix::WebSocketServer>(port, "127.0.0.1");

    server_->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> state,
               ix::WebSocket& ws,
               const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                std::cerr << "[ControlServer] Client connected: " << state->getRemoteIp() << std::endl;
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                std::cerr << "[ControlServer] Client disconnected" << std::endl;
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                // 解析 JSON-RPC 请求
                rapidjson::Document doc;
                doc.Parse(msg->str.c_str());

                if (doc.HasParseError()) {
                    auto resp = MakeError("null", kParseError, "JSON parse error");
                    SendResponse(reinterpret_cast<void*>(&ws), SerializeResponse(resp));
                    return;
                }

                if (!doc.IsObject() ||
                    !doc.HasMember("method") || !doc["method"].IsString()) {
                    std::string id = "null";
                    if (doc.HasMember("id")) {
                        if (doc["id"].IsString()) id = "\"" + std::string(doc["id"].GetString()) + "\"";
                        else if (doc["id"].IsInt()) id = std::to_string(doc["id"].GetInt());
                    }
                    auto resp = MakeError(id, kInvalidRequest, "Invalid JSON-RPC request");
                    SendResponse(reinterpret_cast<void*>(&ws), SerializeResponse(resp));
                    return;
                }

                JsonRpcRequest request;
                if (doc.HasMember("id")) {
                    if (doc["id"].IsString()) request.id = doc["id"].GetString();
                    else if (doc["id"].IsInt()) request.id = std::to_string(doc["id"].GetInt());
                }
                request.method = doc["method"].GetString();
                if (doc.HasMember("params") && doc["params"].IsObject()) {
                    request.params.CopyFrom(doc["params"], request.params.GetAllocator());
                } else {
                    request.params.SetObject();
                }
                request.connection = reinterpret_cast<void*>(&ws);

                EnqueueRequest(std::move(request));
            }
        });

    auto res = server_->listen();
    if (!res.first) {
        std::cerr << "[ControlServer] Failed to listen on port " << port
                  << ": " << res.second << std::endl;
        server_.reset();
        return false;
    }

    server_->start();
    port_ = port;
    running_ = true;
    std::cerr << "[ControlServer] Listening on ws://127.0.0.1:" << port << std::endl;
    return true;
}

void ControlServer::Poll(dse::runtime::EngineInstance& engine) {
    if (!running_) return;

    // 取出所有排队请求
    std::vector<JsonRpcRequest> requests;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        requests.swap(pending_requests_);
    }

    for (auto& req : requests) {
        JsonRpcResponse response;
        response.id = req.id;

        auto it = tools_.find(req.method);
        if (it == tools_.end()) {
            response = MakeError(req.id, kMethodNotFound,
                "Method not found: " + req.method);
        } else {
            try {
                response = it->second(req.params, engine);
                response.id = req.id;
            } catch (const std::exception& e) {
                response = MakeError(req.id, kInternalError,
                    std::string("Internal error: ") + e.what());
            }
        }

        SendResponse(req.connection, SerializeResponse(response));
    }
}

void ControlServer::Stop() {
    if (!running_) return;

    if (server_) {
        server_->stop();
        server_.reset();
    }
    running_ = false;
    std::cerr << "[ControlServer] Stopped" << std::endl;
}

int ControlServer::GetClientCount() const {
    if (!server_) return 0;
    return static_cast<int>(server_->getClients().size());
}

void ControlServer::EnqueueRequest(JsonRpcRequest request) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_requests_.push_back(std::move(request));
}

void ControlServer::SendResponse(void* connection, const std::string& json) {
    if (!connection) return;
    auto* ws = reinterpret_cast<ix::WebSocket*>(connection);
    ws->send(json);
}

std::string ControlServer::SerializeResponse(const JsonRpcResponse& response) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

    writer.StartObject();
    writer.Key("jsonrpc");
    writer.String("2.0");

    // id
    writer.Key("id");
    if (response.id.empty() || response.id == "null") {
        writer.Null();
    } else {
        // 尝试解析为整数
        char* end = nullptr;
        long val = std::strtol(response.id.c_str(), &end, 10);
        if (end != response.id.c_str() && *end == '\0') {
            writer.Int(static_cast<int>(val));
        } else {
            writer.String(response.id.c_str());
        }
    }

    if (response.is_error) {
        writer.Key("error");
        writer.StartObject();
        writer.Key("code");
        writer.Int(response.error_code);
        writer.Key("message");
        writer.String(response.error_message.c_str());
        writer.EndObject();
    } else {
        writer.Key("result");
        if (response.result.IsNull() && !response.result.IsObject()) {
            // 无结果时返回空对象
            writer.StartObject();
            writer.Key("success");
            writer.Bool(true);
            writer.EndObject();
        } else {
            response.result.Accept(writer);
        }
    }

    writer.EndObject();
    return buf.GetString();
}

JsonRpcResponse ControlServer::DispatchTool(const std::string& method,
                                              const rapidjson::Document& params,
                                              dse::runtime::EngineInstance& engine) {
    auto it = tools_.find(method);
    if (it == tools_.end()) {
        return MakeError("local", kMethodNotFound, "Method not found: " + method);
    }
    try {
        auto resp = it->second(params, engine);
        resp.id = "local";
        return resp;
    } catch (const std::exception& e) {
        return MakeError("local", kInternalError, std::string("Internal error: ") + e.what());
    }
}

JsonRpcResponse ControlServer::MakeError(const std::string& id, int code, const std::string& msg) {
    JsonRpcResponse resp;
    resp.id = id;
    resp.is_error = true;
    resp.error_code = code;
    resp.error_message = msg;
    return resp;
}

} // namespace dse::editor
