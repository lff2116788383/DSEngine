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
    if (!transport_ || server_conn_ == kInvalidConnection) return;
    ByteWriter w;
    w.WriteU8(static_cast<uint8_t>(MsgType::InputMove));
    w.WriteU32(target);
    w.WriteF32(dx);
    w.WriteF32(dy);
    w.WriteF32(dz);
    transport_->Send(server_conn_, w.data(), w.size(), SendMode::Reliable);
}

void ReplicationClient::OnConnected(ConnectionId c) {
    server_conn_ = c;
}

void ReplicationClient::OnClosed(ConnectionId c, CloseReason) {
    if (c == server_conn_) server_conn_ = kInvalidConnection;
}

void ReplicationClient::OnMessage(ConnectionId, const MessageView& m, LaneId) {
    if (!mirror_ || m.size == 0) return;
    ByteReader r(m.data, m.size);
    const auto type = static_cast<MsgType>(r.ReadU8());

    switch (type) {
    case MsgType::Spawn: {
        const NetId id = r.ReadU32();
        (void)r.ReadU32(); // archetype（MVP 忽略，统一建带 Transform 的镜像实体）
        if (!r.ok() || net2ent_.count(id)) return;
        entt::entity e = mirror_->create();
        mirror_->emplace<TransformComponent>(e);
        net2ent_[id] = e;
        break;
    }
    case MsgType::Despawn: {
        const NetId id = r.ReadU32();
        if (!r.ok()) return;
        auto it = net2ent_.find(id);
        if (it != net2ent_.end()) {
            if (mirror_->valid(it->second)) mirror_->destroy(it->second);
            net2ent_.erase(it);
        }
        break;
    }
    case MsgType::Snapshot: {
        const uint16_t count = r.ReadU16();
        for (uint16_t i = 0; i < count; ++i) {
            const NetId id = r.ReadU32();
            TransformComponent tmp;
            ReadTransform(r, tmp);
            if (!r.ok()) return;
            auto it = net2ent_.find(id);
            if (it == net2ent_.end() || !mirror_->valid(it->second)) continue;
            if (auto* t = mirror_->try_get<TransformComponent>(it->second)) *t = tmp;
        }
        break;
    }
    default:
        break;
    }
}

} // namespace dse::net::repl
