#include "engine/net/replication/replication_server.h"

#include "engine/net/replication/repl_transform_codec.h"
#include "engine/net/serialize/byte_stream.h"

#include <cmath>
#include <cstring>

namespace dse::net::repl {

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

    // 已连接且握手完成的客户端立即收到 spawn
    for (auto& [conn, cs] : client_states_) {
        if (cs.handshake_done) {
            SendSpawn(conn, id);
            cs.known_entities.insert(id);
        }
    }
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

    for (auto& [conn, cs] : client_states_) {
        if (cs.handshake_done && cs.known_entities.count(id)) {
            SendDespawn(conn, id);
            cs.known_entities.erase(id);
            cs.baseline.erase(id);
        }
    }

    ent2net_.erase(it);
    net2ent_.erase(id);
    owner_.erase(id);
}

void ReplicationServer::Tick() {
    input_used_.clear();
    if (!transport_ || !world_ || client_states_.empty() || net2ent_.empty()) return;

    ++seq_;

    // 计算 AOI 视点：每个连接的 owner 实体位置
    std::unordered_map<ConnectionId, glm::vec3> viewpoints;
    for (auto& [conn, cs] : client_states_) {
        if (!cs.handshake_done) continue;
        // 默认视点为原点；如果该连接拥有实体，则用第一个 owned 实体的位置
        glm::vec3 vp(0.0f);
        for (auto& [id, ow] : owner_) {
            if (ow == conn) {
                auto ent_it = net2ent_.find(id);
                if (ent_it != net2ent_.end() && world_->valid(ent_it->second)) {
                    const auto* t = world_->try_get<TransformComponent>(ent_it->second);
                    if (t) vp = t->position;
                }
                break;
            }
        }
        viewpoints[conn] = vp;
    }

    // 更新 AOI
    aoi_.Update(*world_, net2ent_, viewpoints);

    // 对每个已握手的客户端：处理 AOI 进出 + 发快照
    for (auto& [conn, cs] : client_states_) {
        if (!cs.handshake_done) continue;

        const auto& relevant = aoi_.GetRelevantSet(conn);

        // AOI 进入：spawn 新实体
        for (NetId id : relevant) {
            if (cs.known_entities.find(id) == cs.known_entities.end()) {
                SendSpawn(conn, id);
                cs.known_entities.insert(id);
            }
        }

        // AOI 离开：despawn 不再相关的实体
        std::vector<NetId> to_remove;
        for (NetId id : cs.known_entities) {
            if (relevant.find(id) == relevant.end()) {
                to_remove.push_back(id);
            }
        }
        for (NetId id : to_remove) {
            SendDespawn(conn, id);
            cs.known_entities.erase(id);
            cs.baseline.erase(id);
        }

        // 发送增量或全量快照
        if (cs.last_acked_seq > 0 && !cs.baseline.empty()) {
            SendDeltaToClient(cs);
        } else {
            SendSnapshotToClient(cs);
        }
    }
}

void ReplicationServer::SendSnapshotToClient(ClientState& cs) {
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::Snapshot));
    w.WriteU32(seq_);
    const size_t count_pos = w.size();
    uint16_t count = 0;
    w.WriteU16(0);

    for (NetId id : cs.known_entities) {
        auto ent_it = net2ent_.find(id);
        if (ent_it == net2ent_.end() || !world_->valid(ent_it->second)) continue;
        const auto* t = world_->try_get<TransformComponent>(ent_it->second);
        if (!t) continue;
        w.WriteU32(id);
        WriteTransform(w, *t);
        cs.baseline[id] = *t;
        ++count;
    }

    uint8_t* bytes = const_cast<uint8_t*>(w.data());
    bytes[count_pos]     = static_cast<uint8_t>(count & 0xFF);
    bytes[count_pos + 1] = static_cast<uint8_t>((count >> 8) & 0xFF);

    if (count == 0) return;
    transport_->Send(cs.conn, w.data(), w.size(), SendMode::Unreliable);
}

void ReplicationServer::SendDeltaToClient(ClientState& cs) {
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::DeltaSnapshot));
    w.WriteU32(seq_);
    w.WriteU32(cs.last_acked_seq);
    const size_t count_pos = w.size();
    uint16_t count = 0;
    w.WriteU16(0);

    constexpr float kPosEpsilon = 0.001f;
    constexpr float kRotEpsilon = 0.0001f;
    constexpr float kScaleEpsilon = 0.001f;

    for (NetId id : cs.known_entities) {
        auto ent_it = net2ent_.find(id);
        if (ent_it == net2ent_.end() || !world_->valid(ent_it->second)) continue;
        const auto* t = world_->try_get<TransformComponent>(ent_it->second);
        if (!t) continue;

        uint8_t flags = 0;
        auto base_it = cs.baseline.find(id);
        if (base_it == cs.baseline.end()) {
            flags = kDeltaPosition | kDeltaRotation | kDeltaScale;
        } else {
            const auto& b = base_it->second;
            if (std::abs(t->position.x - b.position.x) > kPosEpsilon ||
                std::abs(t->position.y - b.position.y) > kPosEpsilon ||
                std::abs(t->position.z - b.position.z) > kPosEpsilon) {
                flags |= kDeltaPosition;
            }
            if (std::abs(t->rotation.x - b.rotation.x) > kRotEpsilon ||
                std::abs(t->rotation.y - b.rotation.y) > kRotEpsilon ||
                std::abs(t->rotation.z - b.rotation.z) > kRotEpsilon ||
                std::abs(t->rotation.w - b.rotation.w) > kRotEpsilon) {
                flags |= kDeltaRotation;
            }
            if (std::abs(t->scale.x - b.scale.x) > kScaleEpsilon ||
                std::abs(t->scale.y - b.scale.y) > kScaleEpsilon ||
                std::abs(t->scale.z - b.scale.z) > kScaleEpsilon) {
                flags |= kDeltaScale;
            }
        }

        if (flags == 0) continue;  // 无变化，跳过

        w.WriteU32(id);
        w.WriteU8(flags);
        if (flags & kDeltaPosition) {
            w.WriteF32(t->position.x); w.WriteF32(t->position.y); w.WriteF32(t->position.z);
        }
        if (flags & kDeltaRotation) {
            w.WriteF32(t->rotation.x); w.WriteF32(t->rotation.y);
            w.WriteF32(t->rotation.z); w.WriteF32(t->rotation.w);
        }
        if (flags & kDeltaScale) {
            w.WriteF32(t->scale.x); w.WriteF32(t->scale.y); w.WriteF32(t->scale.z);
        }
        cs.baseline[id] = *t;
        ++count;
    }

    uint8_t* bytes = const_cast<uint8_t*>(w.data());
    bytes[count_pos]     = static_cast<uint8_t>(count & 0xFF);
    bytes[count_pos + 1] = static_cast<uint8_t>((count >> 8) & 0xFF);

    if (count == 0) return;
    transport_->Send(cs.conn, w.data(), w.size(), SendMode::Unreliable);
}

void ReplicationServer::OnConnected(ConnectionId c) {
    clients_.push_back(c);
    ClientState cs;
    cs.conn = c;
    client_states_[c] = std::move(cs);
    // 等待客户端发送 Handshake 消息
}

void ReplicationServer::OnClosed(ConnectionId c, CloseReason) {
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        if (*it == c) { clients_.erase(it); break; }
    }
    client_states_.erase(c);
    input_used_.erase(c);
    for (auto it = owner_.begin(); it != owner_.end();) {
        if (it->second == c) it = owner_.erase(it);
        else                 ++it;
    }
}

void ReplicationServer::OnMessage(ConnectionId c, const MessageView& m, LaneId) {
    if (!world_ || m.size == 0) return;
    const auto* data = static_cast<const uint8_t*>(m.data);
    const auto type = static_cast<MsgType>(data[0]);

    switch (type) {
    case MsgType::Handshake:
        HandleHandshake(c, data + 1, m.size - 1);
        break;
    case MsgType::InputMove:
        HandleInputMove(c, data + 1, m.size - 1);
        break;
    case MsgType::SnapshotAck:
        HandleSnapshotAck(c, data + 1, m.size - 1);
        break;
    case MsgType::RpcCall:
        HandleRpcCall(c, data + 1, m.size - 1);
        break;
    default:
        break;
    }
}

void ReplicationServer::HandleHandshake(ConnectionId c, const uint8_t* data, size_t size) {
    if (size < 2) return;
    ByteReader r(data, size);
    uint16_t client_version = r.ReadU16();
    if (!r.ok()) return;

    bool accepted = (client_version == kProtocolVersion);
    SendHandshakeAck(c, accepted);

    if (accepted) {
        auto it = client_states_.find(c);
        if (it != client_states_.end()) {
            it->second.handshake_done = true;
            // 发送当前所有被复制实体的 spawn
            for (auto& [id, e] : net2ent_) {
                if (world_->valid(e)) {
                    SendSpawn(c, id);
                    it->second.known_entities.insert(id);
                }
            }
        }
    } else {
        transport_->Close(c, CloseReason::Rejected);
    }
}

void ReplicationServer::HandleInputMove(ConnectionId c, const uint8_t* data, size_t size) {
    if (size < 16) return;  // u32 + 3×f32
    ByteReader r(data, size);
    const NetId id = r.ReadU32();
    const float dx = r.ReadF32();
    const float dy = r.ReadF32();
    const float dz = r.ReadF32();
    if (!r.ok()) return;

    auto cs_it = client_states_.find(c);
    if (cs_it == client_states_.end() || !cs_it->second.handshake_done) return;

    if (++input_used_[c] > kMaxInputsPerTick) return;

    auto it = net2ent_.find(id);
    if (it == net2ent_.end() || !world_->valid(it->second)) return;
    auto own = owner_.find(id);
    if (own == owner_.end() || own->second != c) return;
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

void ReplicationServer::HandleSnapshotAck(ConnectionId c, const uint8_t* data, size_t size) {
    if (size < 4) return;
    ByteReader r(data, size);
    uint32_t acked_seq = r.ReadU32();
    if (!r.ok()) return;

    auto it = client_states_.find(c);
    if (it != client_states_.end()) {
        if (acked_seq > it->second.last_acked_seq) {
            it->second.last_acked_seq = acked_seq;
        }
    }
}

void ReplicationServer::HandleRpcCall(ConnectionId c, const uint8_t* data, size_t size) {
    auto cs_it = client_states_.find(c);
    if (cs_it == client_states_.end() || !cs_it->second.handshake_done) return;
    rpc_.Dispatch(c, data, size);
}

void ReplicationServer::SendHandshakeAck(ConnectionId c, bool accepted) {
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::HandshakeAck));
    w.WriteU16(kProtocolVersion);
    w.WriteU8(accepted ? 1 : 0);
    transport_->Send(c, w.data(), w.size(), SendMode::Reliable);
}

void ReplicationServer::SendSpawn(ConnectionId c, NetId id) {
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::Spawn));
    w.WriteU32(id);
    w.WriteU32(kDefaultArchetype);
    transport_->Send(c, w.data(), w.size(), SendMode::Reliable);
}

void ReplicationServer::SendDespawn(ConnectionId c, NetId id) {
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::Despawn));
    w.WriteU32(id);
    transport_->Send(c, w.data(), w.size(), SendMode::Reliable);
}

void ReplicationServer::Broadcast(const void* data, size_t len, SendMode mode) {
    for (auto& [conn, cs] : client_states_) {
        if (cs.handshake_done) transport_->Send(conn, data, len, mode);
    }
}

} // namespace dse::net::repl
