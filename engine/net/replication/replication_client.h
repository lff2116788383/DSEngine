/**
 * @file replication_client.h
 * @brief 客户端镜像复制（MVP）：把服务器发来的 spawn/despawn/快照应用到本地镜像 registry。
 *
 * 用法（纯客户端进程）：
 *   ReplicationClient cli;
 *   cli.Init(transport, &mirror.registry());
 *   ConnectionId s = transport->Connect(addr);   // cli 在 OnConnected 里记录 server 连接
 *   每帧：transport->Poll(cli); （需要时）cli.SendMove(...)
 *
 * 客户端不拥有权威：本地修改不直接生效，只能经 SendMove 请求服务器，由服务器回传快照。
 */
#ifndef DSE_NET_REPLICATION_REPLICATION_CLIENT_H
#define DSE_NET_REPLICATION_REPLICATION_CLIENT_H

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

#include "engine/net/net_transport.h"
#include "engine/net/replication/repl_protocol.h"

namespace dse::net::repl {

class ReplicationClient final : public INetListener {
public:
    /// 绑定传输与本地镜像 registry。两者生命周期需覆盖本对象。
    void Init(INetTransport* transport, entt::registry* mirror);

    /// 向服务器发送一次移动输入请求（可靠，C→S）。target 为要移动的实体 NetId。
    void SendMove(NetId target, float dx, float dy, float dz);

    ConnectionId  ServerConn() const { return server_conn_; }
    entt::entity  ToEntity(NetId id) const;        ///< 未知返回 entt::null
    size_t        MirrorCount() const { return net2ent_.size(); }

    // ── INetListener ──
    void OnConnected(ConnectionId c) override;
    void OnClosed(ConnectionId c, CloseReason r) override;
    void OnMessage(ConnectionId c, const MessageView& m, LaneId lane) override;

private:
    INetTransport*  transport_   = nullptr;
    entt::registry* mirror_      = nullptr;
    ConnectionId    server_conn_ = kInvalidConnection;
    std::unordered_map<NetId, entt::entity> net2ent_;
};

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPLICATION_CLIENT_H
