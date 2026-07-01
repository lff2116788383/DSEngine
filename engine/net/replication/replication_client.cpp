#include "engine/net/replication/replication_client.h"

#include "engine/net/replication/repl_transform_codec.h"
#include "engine/net/serialize/byte_stream.h"

namespace dse::net::repl {

void ReplicationClient::Init(INetTransport* transport, entt::registry* mirror) {
    transport_ = transport;
    mirror_    = mirror;
}

entt::entity ReplicationClient::ToEntity(NetId id) const {
    auto it = net2ent_.find(id);
    return it == net2ent_.end() ? entt::null : it->second;
}

void ReplicationClient::SendMove(NetId target, float dx, float dy, float dz) {
    if (!transport_ || server_conn_ == kInvalidConnection || !handshake_done_) return;
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::InputMove));
    w.WriteU32(target);
    w.WriteF32(dx);
    w.WriteF32(dy);
    w.WriteF32(dz);
    transport_->Send(server_conn_, w.data(), w.size(), SendMode::Reliable);
}

bool ReplicationClient::SendRpc(RpcId rpc_id, NetId target, const void* payload, size_t len) {
    if (!transport_ || server_conn_ == kInvalidConnection || !handshake_done_) return false;
    return rpc_.Send(transport_, server_conn_, rpc_id, target, payload, len);
}

bool ReplicationClient::SendRpcByName(const std::string& name, NetId target, const void* payload, size_t len) {
    if (!transport_ || server_conn_ == kInvalidConnection || !handshake_done_) return false;
    return rpc_.SendByName(transport_, server_conn_, name, target, payload, len);
}

void ReplicationClient::OnConnected(ConnectionId c) {
    server_conn_ = c;
    SendHandshake();
}

void ReplicationClient::OnClosed(ConnectionId c, CloseReason) {
    if (c == server_conn_) {
        server_conn_ = kInvalidConnection;
        handshake_done_ = false;
    }
}

void ReplicationClient::OnMessage(ConnectionId, const MessageView& m, LaneId) {
    if (!mirror_ || m.size == 0) return;
    const auto* data = static_cast<const uint8_t*>(m.data);
    const auto type = static_cast<MsgType>(data[0]);

    switch (type) {
    case MsgType::HandshakeAck:
        HandleHandshakeAck(data + 1, m.size - 1);
        break;
    case MsgType::Spawn:
        HandleSpawn(data + 1, m.size - 1);
        break;
    case MsgType::Despawn:
        HandleDespawn(data + 1, m.size - 1);
        break;
    case MsgType::Snapshot:
        HandleSnapshot(data + 1, m.size - 1);
        break;
    case MsgType::DeltaSnapshot:
        HandleDeltaSnapshot(data + 1, m.size - 1);
        break;
    case MsgType::RpcCall:
        HandleRpcCall(data + 1, m.size - 1);
        break;
    default:
        break;
    }
}

void ReplicationClient::SendHandshake() {
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::Handshake));
    w.WriteU16(kProtocolVersion);
    transport_->Send(server_conn_, w.data(), w.size(), SendMode::Reliable);
}

void ReplicationClient::SendSnapshotAck() {
    if (!transport_ || server_conn_ == kInvalidConnection) return;
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::SnapshotAck));
    w.WriteU32(last_received_seq_);
    transport_->Send(server_conn_, w.data(), w.size(), SendMode::Reliable);
}

void ReplicationClient::HandleHandshakeAck(const uint8_t* data, size_t size) {
    if (size < 3) return;  // u16 version + u8 accepted
    ByteReader r(data, size);
    (void)r.ReadU16();  // server protocol version
    uint8_t accepted = r.ReadU8();
    if (!r.ok()) return;
    handshake_done_ = (accepted != 0);
}

void ReplicationClient::HandleSpawn(const uint8_t* data, size_t size) {
    if (size < 8) return;  // u32 netId + u32 archetype
    ByteReader r(data, size);
    const NetId id = r.ReadU32();
    (void)r.ReadU32();  // archetype
    if (!r.ok() || net2ent_.count(id)) return;
    entt::entity e = mirror_->create();
    mirror_->emplace<TransformComponent>(e);
    net2ent_[id] = e;
}

void ReplicationClient::HandleDespawn(const uint8_t* data, size_t size) {
    if (size < 4) return;
    ByteReader r(data, size);
    const NetId id = r.ReadU32();
    if (!r.ok()) return;
    auto it = net2ent_.find(id);
    if (it != net2ent_.end()) {
        if (mirror_->valid(it->second)) mirror_->destroy(it->second);
        net2ent_.erase(it);
    }
}

void ReplicationClient::HandleSnapshot(const uint8_t* data, size_t size) {
    if (size < 6) return;  // u32 seq + u16 count
    ByteReader r(data, size);
    uint32_t seq = r.ReadU32();
    const uint16_t count = r.ReadU16();
    if (!r.ok()) return;

    for (uint16_t i = 0; i < count; ++i) {
        const NetId id = r.ReadU32();
        TransformComponent tmp;
        ReadTransform(r, tmp);
        if (!r.ok()) return;
        auto it = net2ent_.find(id);
        if (it == net2ent_.end() || !mirror_->valid(it->second)) continue;
        if (auto* t = mirror_->try_get<TransformComponent>(it->second)) *t = tmp;
    }

    if (seq > last_received_seq_) {
        last_received_seq_ = seq;
        SendSnapshotAck();
    }
}

void ReplicationClient::HandleDeltaSnapshot(const uint8_t* data, size_t size) {
    if (size < 10) return;  // u32 seq + u32 baseline_seq + u16 count
    ByteReader r(data, size);
    uint32_t seq = r.ReadU32();
    (void)r.ReadU32();  // baseline_seq (server's reference)
    const uint16_t count = r.ReadU16();
    if (!r.ok()) return;

    for (uint16_t i = 0; i < count; ++i) {
        const NetId id = r.ReadU32();
        const uint8_t flags = r.ReadU8();
        if (!r.ok()) return;

        auto it = net2ent_.find(id);
        TransformComponent* t = nullptr;
        if (it != net2ent_.end() && mirror_->valid(it->second)) {
            t = mirror_->try_get<TransformComponent>(it->second);
        }

        if (flags & kDeltaPosition) {
            float px = r.ReadF32(), py = r.ReadF32(), pz = r.ReadF32();
            if (t) { t->position.x = px; t->position.y = py; t->position.z = pz; }
        }
        if (flags & kDeltaRotation) {
            float rx = r.ReadF32(), ry = r.ReadF32(), rz = r.ReadF32(), rw = r.ReadF32();
            if (t) { t->rotation.x = rx; t->rotation.y = ry; t->rotation.z = rz; t->rotation.w = rw; }
        }
        if (flags & kDeltaScale) {
            float sx = r.ReadF32(), sy = r.ReadF32(), sz = r.ReadF32();
            if (t) { t->scale.x = sx; t->scale.y = sy; t->scale.z = sz; }
        }
        if (t) t->dirty = true;
        if (!r.ok()) return;
    }

    if (seq > last_received_seq_) {
        last_received_seq_ = seq;
        SendSnapshotAck();
    }
}

void ReplicationClient::HandleRpcCall(const uint8_t* data, size_t size) {
    if (!handshake_done_) return;
    rpc_.Dispatch(server_conn_, data, size);
}

} // namespace dse::net::repl
