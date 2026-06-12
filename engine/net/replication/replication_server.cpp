#include "engine/net/replication/replication_server.h"

#include "engine/net/replication/repl_transform_codec.h"
#include "engine/net/serialize/byte_stream.h"

namespace dse::net::repl {

namespace {
constexpr float    kMaxInputDelta    = 100.0f; // 服务器侧的简单合理性上限（防异常输入）
constexpr uint32_t kMaxInputsPerTick = 64;     // 每连接每 tick 处理输入上限（防 flood）
} // namespace

void ReplicationServer::Init(INetTransport* transport, entt::registry* world) {
    transport_ = transport;
    world_     = world;
}

NetId ReplicationServer::NetIdOf(entt::entity e) const {
    auto it = ent2net_.find(static_cast<uint32_t>(e));
    return it == ent2net_.end() ? kInvalidNetId : it->second;
}

NetId ReplicationServer::MarkReplicated(entt::entity e, ConnectionId owner) {
    const uint32_t key = static_cast<uint32_t>(e);
    auto existing = ent2net_.find(key);
    if (existing != ent2net_.end()) {
        if (owner != kInvalidConnection) owner_[existing->second] = owner;
        return existing->second;
    }

    const NetId id = next_id_++;
    ent2net_[key]  = id;
    net2ent_[id]   = e;
    if (owner != kInvalidConnection) owner_[id] = owner;

    for (ConnectionId c : clients_) SendSpawn(c, id); // 已连客户端立即可见
    return id;
}

void ReplicationServer::SetOwner(entt::entity e, ConnectionId owner) {
    auto it = ent2net_.find(static_cast<uint32_t>(e));
    if (it == ent2net_.end()) return;
    if (owner == kInvalidConnection) owner_.erase(it->second);
    else                             owner_[it->second] = owner;
}

void ReplicationServer::Unreplicate(entt::entity e) {
    const uint32_t key = static_cast<uint32_t>(e);
    auto it = ent2net_.find(key);
    if (it == ent2net_.end()) return;
    const NetId id = it->second;

    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::Despawn));
    w.WriteU32(id);
    Broadcast(w.data(), w.size(), SendMode::Reliable);

    ent2net_.erase(it);
    net2ent_.erase(id);
    owner_.erase(id);
}

void ReplicationServer::Tick() {
    input_used_.clear(); // 重置每连接输入限流窗口
    if (!transport_ || !world_ || clients_.empty() || net2ent_.empty()) return;

    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::Snapshot));
    const size_t count_pos = w.size();
    uint16_t count = 0;
    w.WriteU16(0); // 占位，稍后回填

    for (auto& [id, e] : net2ent_) {
        if (!world_->valid(e)) continue;
        const auto* t = world_->try_get<TransformComponent>(e);
        if (!t) continue;
        w.WriteU32(id);
        WriteTransform(w, *t);
        ++count;
    }

    // 回填 count（小端，直接覆盖占位的 2 字节）。
    uint8_t* bytes = const_cast<uint8_t*>(w.data());
    bytes[count_pos]     = static_cast<uint8_t>(count & 0xFF);
    bytes[count_pos + 1] = static_cast<uint8_t>((count >> 8) & 0xFF);

    if (count == 0) return;
    Broadcast(w.data(), w.size(), SendMode::Unreliable);
}

void ReplicationServer::OnConnected(ConnectionId c) {
    clients_.push_back(c);
    for (auto& [id, e] : net2ent_) SendSpawn(c, id); // late-join：同步当前所有实体
}

void ReplicationServer::OnClosed(ConnectionId c, CloseReason) {
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        if (*it == c) { clients_.erase(it); break; }
    }
    input_used_.erase(c);
    // 清除该连接拥有的实体属主标记（实体本身去留由游戏逻辑决定）。
    for (auto it = owner_.begin(); it != owner_.end();) {
        if (it->second == c) it = owner_.erase(it);
        else                 ++it;
    }
}

void ReplicationServer::OnMessage(ConnectionId c, const MessageView& m, LaneId) {
    if (!world_ || m.size == 0) return;
    ByteReader r(m.data, m.size);
    const auto type = static_cast<MsgType>(r.ReadU8());
    if (type != MsgType::InputMove) return; // MVP 只处理输入

    const NetId id = r.ReadU32();
    const float dx = r.ReadF32();
    const float dy = r.ReadF32();
    const float dz = r.ReadF32();
    if (!r.ok()) return;

    // 限流：每连接每 tick 处理的输入数有上限，防止单连接 flood。
    if (++input_used_[c] > kMaxInputsPerTick) return;

    // 服务器权威校验链：①目标存在且存活 ②来源连接为该实体属主 ③输入幅度合理。
    auto it = net2ent_.find(id);
    if (it == net2ent_.end() || !world_->valid(it->second)) return;
    auto own = owner_.find(id);
    if (own == owner_.end() || own->second != c) return; // 非属主连接：拒绝（服务器权威）
    auto* t = world_->try_get<TransformComponent>(it->second);
    if (!t) return;
    if (dx < -kMaxInputDelta || dx > kMaxInputDelta ||
        dy < -kMaxInputDelta || dy > kMaxInputDelta ||
        dz < -kMaxInputDelta || dz > kMaxInputDelta) return;
    t->position.x += dx;
    t->position.y += dy;
    t->position.z += dz;
    t->dirty = true;
}

void ReplicationServer::SendSpawn(ConnectionId c, NetId id) {
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::Spawn));
    w.WriteU32(id);
    w.WriteU32(kDefaultArchetype);
    transport_->Send(c, w.data(), w.size(), SendMode::Reliable);
}

void ReplicationServer::Broadcast(const void* data, size_t len, SendMode mode) {
    for (ConnectionId c : clients_) transport_->Send(c, data, len, mode);
}

} // namespace dse::net::repl
