/**
 * @file replication_server.h
 * @brief 服务器权威复制（MVP）：把权威世界的 TransformComponent 同步给所有客户端。
 *
 * 用法（dedicated / listen server）：
 *   ReplicationServer srv;
 *   srv.Init(transport, &world.registry());
 *   srv.MarkReplicated(entity);            // 纳入复制并广播 spawn
 *   每帧：transport->Poll(srv); srv.Tick();// 收输入/连接事件 + 发快照
 *
 * ReplicationServer 本身就是 INetTransport 的监听器（INetListener）：在专用服务器
 * 进程中它是唯一监听者，直接 transport->Poll(srv) 即可；回环测试用一个路由监听器
 * 把客户端连接的事件分发给 ReplicationClient（见 tests/net/repl_smoke.cpp）。
 */
#ifndef DSE_NET_REPLICATION_REPLICATION_SERVER_H
#define DSE_NET_REPLICATION_REPLICATION_SERVER_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

#include "engine/net/net_transport.h"
#include "engine/net/replication/repl_protocol.h"

namespace dse::net::repl {

class ReplicationServer final : public INetListener {
public:
    /// 绑定传输与权威 registry。两者生命周期需覆盖本对象。
    void Init(INetTransport* transport, entt::registry* world);

    /// 把实体纳入复制：分配 NetId，并向已连接客户端立即广播 spawn。
    /// 实体需带 TransformComponent（否则快照里跳过）。返回分配的 NetId。
    /// owner 为该实体的属主连接（默认无属主——仅服务器可改其状态，拒绝任何客户端输入）。
    NetId MarkReplicated(entt::entity e, ConnectionId owner = kInvalidConnection);

    /// 设置/变更实体属主连接（服务器权威：只有属主连接的 InputMove 会被接受）。
    void SetOwner(entt::entity e, ConnectionId owner);

    /// 移出复制：向所有客户端广播 despawn 并清除映射（不销毁权威实体）。
    void Unreplicate(entt::entity e);

    /// 向所有客户端发送一次全量快照（不可靠）。通常每帧调用一次。
    /// 同时重置每连接的输入预算（限流窗口）。
    void Tick();

    size_t ClientCount() const { return clients_.size(); }
    const std::vector<ConnectionId>& Clients() const { return clients_; }
    NetId  NetIdOf(entt::entity e) const;

    // ── INetListener ──
    void OnConnected(ConnectionId c) override;
    void OnClosed(ConnectionId c, CloseReason r) override;
    void OnMessage(ConnectionId c, const MessageView& m, LaneId lane) override;

private:
    void SendSpawn(ConnectionId c, NetId id);
    void Broadcast(const void* data, size_t len, SendMode mode);

    INetTransport*  transport_ = nullptr;
    entt::registry* world_     = nullptr;

    std::vector<ConnectionId>                clients_;
    std::unordered_map<uint32_t, NetId>      ent2net_;     // key = uint32(entt::entity)
    std::unordered_map<NetId, entt::entity>  net2ent_;
    std::unordered_map<NetId, ConnectionId>  owner_;       // NetId → 属主连接（kInvalidConnection=仅服务器）
    std::unordered_map<ConnectionId, uint32_t> input_used_;// 每连接本 tick 已处理输入数（限流）
    NetId next_id_ = 1;
};

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPLICATION_SERVER_H
