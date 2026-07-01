/**
 * @file rpc_registry.h
 * @brief 通用 RPC 注册表 — 注册/调用/派发远程过程调用。
 *
 * 设计：
 *   - 每个 RPC 有唯一 RpcId + 名称 + 目标类型(Server/Client/Multicast)
 *   - 注册时提供编解码函数指针（序列化/反序列化 payload）
 *   - 调用端：序列化参数 → 经 transport 发送 RpcCall 消息
 *   - 接收端：按 rpc_id 查表 → 反序列化 → 调用 handler
 *   - Server RPC 必须校验调用者权限（属主检查）
 *
 * 用法：
 *   RpcRegistry rpc;
 *   rpc.Register("RequestFire", RpcTarget::Server, handler, validator);
 *   // 客户端调用：
 *   rpc.CallServer(transport, conn, "RequestFire", netId, payload...);
 *   // 服务端接收（在 OnMessage 中）：
 *   rpc.Dispatch(sender, msg_data, msg_size, context);
 */
#ifndef DSE_NET_RPC_RPC_REGISTRY_H
#define DSE_NET_RPC_RPC_REGISTRY_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/net/net_transport.h"
#include "engine/net/replication/repl_protocol.h"
#include "engine/net/serialize/byte_stream.h"

namespace dse::net::rpc {

using namespace dse::net::repl;

/// RPC 上下文：handler 收到调用时可用的信息。
struct RpcContext {
    ConnectionId sender  = kInvalidConnection;  ///< 发送方连接
    NetId        target  = kInvalidNetId;       ///< 目标实体
    const void*  payload = nullptr;             ///< 参数 payload 数据
    size_t       payload_size = 0;              ///< payload 字节数
};

/// RPC handler 回调类型。返回 true 表示处理成功。
using RpcHandler = std::function<bool(const RpcContext& ctx)>;

/// RPC 权限校验回调（Server RPC）：校验 sender 是否有权对 target 调用此 RPC。
using RpcValidator = std::function<bool(ConnectionId sender, NetId target)>;

/// 注册条目。
struct RpcEntry {
    RpcId        id       = kInvalidRpcId;
    std::string  name;
    RpcTarget    target   = RpcTarget::Server;
    RpcHandler   handler;
    RpcValidator validator;  ///< 仅 Server RPC 使用
    SendMode     mode     = SendMode::Reliable;
};

class RpcRegistry {
public:
    /// 注册一个 RPC。返回分配的 RpcId。
    RpcId Register(const std::string& name, RpcTarget target,
                   RpcHandler handler, RpcValidator validator = nullptr,
                   SendMode mode = SendMode::Reliable) {
        RpcId id = next_id_++;
        RpcEntry entry;
        entry.id = id;
        entry.name = name;
        entry.target = target;
        entry.handler = std::move(handler);
        entry.validator = std::move(validator);
        entry.mode = mode;
        entries_[id] = std::move(entry);
        name_to_id_[name] = id;
        return id;
    }

    /// 按名称查找 RpcId。
    RpcId FindByName(const std::string& name) const {
        auto it = name_to_id_.find(name);
        return it != name_to_id_.end() ? it->second : kInvalidRpcId;
    }

    /// 获取注册条目。
    const RpcEntry* GetEntry(RpcId id) const {
        auto it = entries_.find(id);
        return it != entries_.end() ? &it->second : nullptr;
    }

    /// 发送 RPC 调用（编码并通过 transport 发送）。
    bool Send(INetTransport* transport, ConnectionId conn,
              RpcId rpc_id, NetId target_net_id,
              const void* payload, size_t payload_size) {
        auto it = entries_.find(rpc_id);
        if (it == entries_.end()) return false;

        ByteWriter w;
        w.WriteU8(static_cast<uint8_t>(MsgType::RpcCall));
        w.WriteU16(rpc_id);
        w.WriteU32(target_net_id);
        if (payload && payload_size > 0) {
            w.WriteBytes(payload, payload_size);
        }
        return transport->Send(conn, w.data(), w.size(), it->second.mode);
    }

    /// 便捷发送（按名称）。
    bool SendByName(INetTransport* transport, ConnectionId conn,
                    const std::string& name, NetId target_net_id,
                    const void* payload = nullptr, size_t payload_size = 0) {
        RpcId id = FindByName(name);
        if (id == kInvalidRpcId) return false;
        return Send(transport, conn, id, target_net_id, payload, payload_size);
    }

    /// 广播 RPC 到多个连接（Server→Client Multicast）。
    void Broadcast(INetTransport* transport, const std::vector<ConnectionId>& conns,
                   RpcId rpc_id, NetId target_net_id,
                   const void* payload, size_t payload_size) {
        auto it = entries_.find(rpc_id);
        if (it == entries_.end()) return;

        ByteWriter w;
        w.WriteU8(static_cast<uint8_t>(MsgType::RpcCall));
        w.WriteU16(rpc_id);
        w.WriteU32(target_net_id);
        if (payload && payload_size > 0) {
            w.WriteBytes(payload, payload_size);
        }
        for (ConnectionId c : conns) {
            transport->Send(c, w.data(), w.size(), it->second.mode);
        }
    }

    /// 派发收到的 RPC 消息。在 OnMessage 中调用。
    /// data 指向 MsgType::RpcCall 之后的内容（即从 rpc_id 开始）。
    bool Dispatch(ConnectionId sender, const void* data, size_t size) {
        if (size < 6) return false;  // 至少 u16 rpc_id + u32 target
        ByteReader r(data, size);
        RpcId rpc_id = static_cast<RpcId>(r.ReadU16());
        NetId target = r.ReadU32();
        if (!r.ok()) return false;

        auto it = entries_.find(rpc_id);
        if (it == entries_.end()) return false;

        const auto& entry = it->second;

        // Server RPC 权限校验
        if (entry.target == RpcTarget::Server && entry.validator) {
            if (!entry.validator(sender, target)) return false;
        }

        RpcContext ctx;
        ctx.sender = sender;
        ctx.target = target;
        ctx.payload = static_cast<const uint8_t*>(data) + 6;
        ctx.payload_size = size > 6 ? size - 6 : 0;

        return entry.handler ? entry.handler(ctx) : false;
    }

    size_t Count() const { return entries_.size(); }
    void Clear() { entries_.clear(); name_to_id_.clear(); next_id_ = 1; }

private:
    std::unordered_map<RpcId, RpcEntry> entries_;
    std::unordered_map<std::string, RpcId> name_to_id_;
    RpcId next_id_ = 1;
};

} // namespace dse::net::rpc

#endif // DSE_NET_RPC_RPC_REGISTRY_H
