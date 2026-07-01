/**
 * @file replication_client.h
 * @brief 客户端镜像复制：协议握手、spawn/despawn、全量/增量快照、RPC、ACK。
 *
 * 用法（纯客户端进程）：
 *   ReplicationClient cli;
 *   cli.Init(transport, &mirror.registry());
 *   ConnectionId s = transport->Connect(addr);
 *   每帧：transport->Poll(cli);
 *
 * 客户端不拥有权威：本地修改不直接生效，只能经 SendMove/RPC 请求服务器。
 */
#ifndef DSE_NET_REPLICATION_REPLICATION_CLIENT_H
#define DSE_NET_REPLICATION_REPLICATION_CLIENT_H

#include <cstdint>
#include <functional>
#include <unordered_map>

#include <entt/entt.hpp>

#include "engine/net/net_transport.h"
#include "engine/net/replication/repl_protocol.h"
#include "engine/net/rpc/rpc_registry.h"

namespace dse::net::repl {

class ReplicationClient final : public INetListener {
public:
    void Init(INetTransport* transport, entt::registry* mirror);

    /// 向服务器发送一次移动输入请求。
    void SendMove(NetId target, float dx, float dy, float dz);

    /// 发送 RPC 到服务器。
    bool SendRpc(RpcId rpc_id, NetId target, const void* payload = nullptr, size_t len = 0);
    bool SendRpcByName(const std::string& name, NetId target, const void* payload = nullptr, size_t len = 0);

    /// 获取 RPC 注册表（用于注册 Client RPC handler）。
    dse::net::rpc::RpcRegistry& Rpc() { return rpc_; }

    ConnectionId  ServerConn() const { return server_conn_; }
    entt::entity  ToEntity(NetId id) const;
    size_t        MirrorCount() const { return net2ent_.size(); }
    bool          IsConnected() const { return handshake_done_; }
    uint32_t      LastReceivedSeq() const { return last_received_seq_; }

    // ── INetListener ──
    void OnConnected(ConnectionId c) override;
    void OnClosed(ConnectionId c, CloseReason r) override;
    void OnMessage(ConnectionId c, const MessageView& m, LaneId lane) override;

private:
    void SendHandshake();
    void SendSnapshotAck();
    void HandleHandshakeAck(const uint8_t* data, size_t size);
    void HandleSpawn(const uint8_t* data, size_t size);
    void HandleDespawn(const uint8_t* data, size_t size);
    void HandleSnapshot(const uint8_t* data, size_t size);
    void HandleDeltaSnapshot(const uint8_t* data, size_t size);
    void HandleRpcCall(const uint8_t* data, size_t size);

    INetTransport*  transport_   = nullptr;
    entt::registry* mirror_      = nullptr;
    ConnectionId    server_conn_ = kInvalidConnection;
    bool            handshake_done_ = false;
    uint32_t        last_received_seq_ = 0;
    std::unordered_map<NetId, entt::entity> net2ent_;

    dse::net::rpc::RpcRegistry rpc_;
};

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPLICATION_CLIENT_H
