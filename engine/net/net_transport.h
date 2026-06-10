#pragma once
// DSEngine 网络层 — 传输抽象接口（纯虚，无后端依赖）。
// 后端实现（当前为 GameNetworkingSockets）在 engine/net/backends/ 下，
// 通过 CreateGnsTransport() 工厂创建；上层可在不改动调用方的情况下替换后端。
#include "net_types.h"

namespace dse::net {

// 事件回调接口。上层实现并在 Poll() 中被回调。
class INetListener {
public:
    virtual ~INetListener() = default;
    // 服务端：收到新连接（已自动 Accept）。客户端：本地发起连接进入握手。
    virtual void OnConnecting(ConnectionId /*conn*/) {}
    // 连接就绪，可收发。
    virtual void OnConnected(ConnectionId /*conn*/) {}
    // 连接关闭/异常。
    virtual void OnClosed(ConnectionId /*conn*/, CloseReason /*reason*/) {}
    // 收到一条消息。
    virtual void OnMessage(ConnectionId /*conn*/, const MessageView& /*msg*/) {}
};

// 配置（预留；当前仅占位，后续可加密钥/超时/带宽上限等）。
struct NetConfig {
    bool log_debug = false;
};

class INetTransport {
public:
    virtual ~INetTransport() = default;

    // ── 生命周期 ──
    virtual bool Init(const NetConfig& cfg) = 0;
    virtual void Shutdown() = 0;

    // ── 服务端 / 客户端 ──
    // 监听本地端口；返回是否成功。新连接通过 INetListener 回调暴露。
    virtual bool Listen(uint16_t port) = 0;
    // 连接到远端；返回连接句柄（kInvalidConnection 表示发起失败）。
    virtual ConnectionId Connect(const Address& addr) = 0;
    // 关闭某连接。
    virtual void Close(ConnectionId conn, CloseReason reason = CloseReason::Normal) = 0;

    // ── 收发 ──
    // 发送一条消息（消息级；reliable/unreliable 由 mode 决定）。
    virtual bool Send(ConnectionId conn, const void* data, size_t len, SendMode mode) = 0;

    // ── 每帧泵：派发连接事件 + 收消息（回调到 listener）──
    virtual void Poll(INetListener& listener) = 0;

    // ── 诊断 ──
    virtual bool GetQuality(ConnectionId conn, ConnQuality& out) = 0;
};

// 工厂：创建 GameNetworkingSockets 后端实例（调用方负责 delete）。
INetTransport* CreateGnsTransport();

} // namespace dse::net
