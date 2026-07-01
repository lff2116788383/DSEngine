/**
 * @file replication_server.h
 * @brief 服务器权威复制：把权威世界的组件状态同步给客户端。
 *
 * 功能：
 *   - 协议版本握手（版本不匹配拒绝连接）
 *   - spawn / despawn（可靠）
 *   - 全量快照 + 增量 delta（不可靠，带序号 + baseline ACK）
 *   - AOI 兴趣裁剪（距离/全局）
 *   - RPC 派发（Server RPC 权限校验）
 *   - 输入限流与权威校验
 *
 * 用法（dedicated / listen server）：
 *   ReplicationServer srv;
 *   srv.Init(transport, &world.registry());
 *   srv.SetAoiPolicy(AoiPolicy::Distance, 200.0f);
 *   srv.MarkReplicated(entity);
 *   每帧：transport->Poll(srv); srv.Tick();
 */
#ifndef DSE_NET_REPLICATION_REPLICATION_SERVER_H
#define DSE_NET_REPLICATION_REPLICATION_SERVER_H

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "engine/net/net_transport.h"
#include "engine/net/replication/repl_protocol.h"
#include "engine/net/replication/repl_aoi.h"
#include "engine/net/rpc/rpc_registry.h"

namespace dse::net::repl {

/// 每连接状态（baseline、ACK、握手等）。
struct ClientState {
    ConnectionId conn = kInvalidConnection;
    bool         handshake_done = false;
    uint32_t     last_acked_seq = 0;        ///< 客户端已确认的快照序号
    std::unordered_map<NetId, TransformComponent> baseline; ///< 该连接的 baseline 状态
    std::unordered_set<NetId> known_entities; ///< 该连接已知的实体集
};

class ReplicationServer final : public INetListener {
public:
    void Init(INetTransport* transport, entt::registry* world);

    /// 把实体纳入复制：分配 NetId，并向已连接客户端立即广播 spawn。
    NetId MarkReplicated(entt::entity e, ConnectionId owner = kInvalidConnection);
    void SetOwner(entt::entity e, ConnectionId owner);
    void Unreplicate(entt::entity e);

    /// 每帧调用：发送增量快照（或全量 fallback）+ 处理 AOI 进出。
    void Tick();

    /// 设置 AOI 策略。
    void SetAoiPolicy(AoiPolicy policy, float radius = 0.0f) { aoi_.SetPolicy(policy, radius); }

    /// 获取 RPC 注册表（用于注册 Server/Multicast RPC）。
    dse::net::rpc::RpcRegistry& Rpc() { return rpc_; }

    size_t ClientCount() const { return clients_.size(); }
    const std::vector<ConnectionId>& Clients() const { return clients_; }
    NetId  NetIdOf(entt::entity e) const;

    /// 获取当前快照序号。
    uint32_t CurrentSeq() const { return seq_; }

    // ── INetListener ──
    void OnConnected(ConnectionId c) override;
    void OnClosed(ConnectionId c, CloseReason r) override;
    void OnMessage(ConnectionId c, const MessageView& m, LaneId lane) override;

private:
    void SendHandshakeAck(ConnectionId c, bool accepted);
    void SendSpawn(ConnectionId c, NetId id);
    void SendDespawn(ConnectionId c, NetId id);
    void SendSnapshotToClient(ClientState& cs);
    void SendDeltaToClient(ClientState& cs);
    void Broadcast(const void* data, size_t len, SendMode mode);
    void HandleHandshake(ConnectionId c, const uint8_t* data, size_t size);
    void HandleInputMove(ConnectionId c, const uint8_t* data, size_t size);
    void HandleSnapshotAck(ConnectionId c, const uint8_t* data, size_t size);
    void HandleRpcCall(ConnectionId c, const uint8_t* data, size_t size);

    INetTransport*  transport_ = nullptr;
    entt::registry* world_     = nullptr;

    std::vector<ConnectionId>                     clients_;
    std::unordered_map<ConnectionId, ClientState> client_states_;
    std::unordered_map<uint32_t, NetId>           ent2net_;
    std::unordered_map<NetId, entt::entity>       net2ent_;
    std::unordered_map<NetId, ConnectionId>       owner_;
    std::unordered_map<ConnectionId, uint32_t>    input_used_;
    NetId    next_id_ = 1;
    uint32_t seq_     = 0;  ///< 全局快照序号（单调递增）

    AoiManager aoi_;
    dse::net::rpc::RpcRegistry rpc_;

    static constexpr float    kMaxInputDelta    = 100.0f;
    static constexpr uint32_t kMaxInputsPerTick = 64;
};

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPLICATION_SERVER_H
