// DSEngine 网络层 — C ABI 实现。仅依赖 engine/net 抽象（不 include 任何 GNS 头）。
#include "engine/net/net_c_api.h"
#include "engine/net/net_transport.h"

#include <vector>

using namespace dse::net;

// 不透明句柄：包住后端实例。
struct dse_net_transport {
    INetTransport* impl = nullptr;
};

namespace {

// 把 INetListener 回调转发到 C 回调结构体。
class CApiListener final : public INetListener {
public:
    const dse_net_callbacks* cbs = nullptr;
    void* user = nullptr;

    void OnConnecting(ConnectionId c, const Address& peer) override {
        if (cbs && cbs->on_connecting)
            cbs->on_connecting(user, c, peer.host.c_str(), peer.port);
    }
    void OnConnected(ConnectionId c) override {
        if (cbs && cbs->on_connected) cbs->on_connected(user, c);
    }
    void OnClosed(ConnectionId c, CloseReason r) override {
        if (cbs && cbs->on_closed) cbs->on_closed(user, c, static_cast<uint32_t>(r));
    }
    void OnMessage(ConnectionId c, const MessageView& m, LaneId lane) override {
        if (cbs && cbs->on_message) cbs->on_message(user, c, m.data, m.size, lane);
    }
};

}  // namespace

extern "C" {

dse_net_transport* dse_net_create(void) {
    auto* t = new dse_net_transport();
    t->impl = CreateGnsTransport();
    return t;
}

void dse_net_destroy(dse_net_transport* t) {
    if (!t) return;
    if (t->impl) {
        t->impl->Shutdown();  // 幂等：未 Init/已 Shutdown 时安全
        delete t->impl;
    }
    delete t;
}

int dse_net_init(dse_net_transport* t, int log_debug) {
    if (!t || !t->impl) return 0;
    NetConfig cfg;
    cfg.log_debug = (log_debug != 0);
    return t->impl->Init(cfg) ? 1 : 0;
}

void dse_net_shutdown(dse_net_transport* t) {
    if (t && t->impl) t->impl->Shutdown();
}

int dse_net_listen(dse_net_transport* t, uint16_t port) {
    if (!t || !t->impl) return 0;
    return t->impl->Listen(port) ? 1 : 0;
}

dse_net_conn dse_net_connect(dse_net_transport* t, const char* host, uint16_t port) {
    if (!t || !t->impl || !host) return kInvalidConnection;
    Address a{host, port};
    return t->impl->Connect(a);
}

void dse_net_close(dse_net_transport* t, dse_net_conn conn, uint32_t reason) {
    if (t && t->impl) t->impl->Close(conn, static_cast<CloseReason>(reason));
}

int dse_net_configure_lanes(dse_net_transport* t, dse_net_conn conn,
                            int num_lanes, const int* priorities, const uint16_t* weights) {
    if (!t || !t->impl || num_lanes <= 0 || !priorities) return 0;
    LaneConfig cfg;
    cfg.priorities.assign(priorities, priorities + num_lanes);
    if (weights) cfg.weights.assign(weights, weights + num_lanes);
    return t->impl->ConfigureLanes(conn, cfg) ? 1 : 0;
}

int dse_net_send(dse_net_transport* t, dse_net_conn conn,
                 const void* data, size_t len, uint32_t mode, uint16_t lane) {
    if (!t || !t->impl) return 0;
    return t->impl->Send(conn, data, len, static_cast<SendMode>(mode),
                         static_cast<LaneId>(lane)) ? 1 : 0;
}

void dse_net_flush(dse_net_transport* t, dse_net_conn conn) {
    if (t && t->impl) t->impl->Flush(conn);
}

void dse_net_poll(dse_net_transport* t, const dse_net_callbacks* cbs, void* user) {
    if (!t || !t->impl) return;
    CApiListener l;
    l.cbs = cbs;
    l.user = user;
    t->impl->Poll(l);
}

int dse_net_get_quality(dse_net_transport* t, dse_net_conn conn, dse_net_quality* out) {
    if (!t || !t->impl || !out) return 0;
    ConnQuality q;
    if (!t->impl->GetQuality(conn, q)) return 0;
    out->ping_ms           = q.ping_ms;
    out->packet_loss       = q.packet_loss;
    out->out_bytes_per_sec = q.out_bytes_per_sec;
    out->in_bytes_per_sec  = q.in_bytes_per_sec;
    out->pending_reliable  = q.pending_reliable;
    return 1;
}

}  // extern "C"
