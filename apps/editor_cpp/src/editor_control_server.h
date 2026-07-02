#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <memory>
#include <unordered_map>

#include <rapidjson/document.h>

namespace ix {
class WebSocketServer;
}

namespace dse::runtime {
class EngineInstance;
}

namespace dse::editor {

// ─── JSON-RPC 请求/响应 ─────────────────────────────────────────────────────

struct JsonRpcRequest {
    std::string id;
    std::string method;
    rapidjson::Document params;  // 移动语义，不拷贝
    void* connection = nullptr;  // 用于路由响应（ix::WebSocket*）
};

struct JsonRpcResponse {
    std::string id;
    bool is_error = false;
    int error_code = 0;
    std::string error_message;
    rapidjson::Document result;  // 成功时的结果
};

// ─── Tool handler 签名 ─────────────────────────────────────────────────────

/// Tool handler: 接收 params, engine, 返回 response
using ToolHandler = std::function<JsonRpcResponse(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine)>;

// ─── ControlServer ──────────────────────────────────────────────────────────

/// WebSocket JSON-RPC 服务器。
/// WS 回调线程负责接收消息并入队；主线程通过 Poll() 处理请求。
class ControlServer {
public:
    ControlServer();
    ~ControlServer();

    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;

    /// 注册 Tool handler（在 Start 之前调用）
    void RegisterTool(const std::string& method, ToolHandler handler);

    /// 启动 WebSocket 服务器，监听指定端口（默认 9527）
    bool Start(int port = 9527);

    /// 在主线程调用，处理排队的 JSON-RPC 请求
    void Poll(dse::runtime::EngineInstance& engine);

    /// 停止服务器
    void Stop();

    bool IsRunning() const { return running_; }
    int GetPort() const { return port_; }
    int GetClientCount() const;

    /// 本地调用 Tool handler（供 ChatPanel 等同进程组件使用，不走 WebSocket）
    JsonRpcResponse DispatchTool(const std::string& method,
                                  const rapidjson::Document& params,
                                  dse::runtime::EngineInstance& engine);

private:
    void EnqueueRequest(JsonRpcRequest request);
    void SendResponse(void* connection, const std::string& json);
    std::string SerializeResponse(const JsonRpcResponse& response);
    JsonRpcResponse MakeError(const std::string& id, int code, const std::string& msg);

    std::unique_ptr<ix::WebSocketServer> server_;
    int port_ = 0;
    bool running_ = false;

    // Tool 注册表
    std::unordered_map<std::string, ToolHandler> tools_;

    // 线程安全请求队列（WS 线程入队，主线程出队）
    std::mutex queue_mutex_;
    std::vector<JsonRpcRequest> pending_requests_;
};

// ─── 注册内建 Tools ─────────────────────────────────────────────────────────

/// 注册所有内建 Tool handler 到 server
void RegisterBuiltinTools(ControlServer& server);

/// 注入 Gizmo 指针（EditorApp 初始化时调用，供 dsengine_gizmo_set_mode 工具使用）
void SetGizmoPointers(int* operation, int* mode);

} // namespace dse::editor
