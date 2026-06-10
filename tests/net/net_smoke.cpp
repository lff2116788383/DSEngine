// DSEngine 网络层 — 回环冒烟测试。
// 单进程内 Listen + Connect 到 127.0.0.1，握手后发送 可靠 + 非可靠 各一条，
// 验证服务端经真实 UDP 回环收到内容。退出码 0=通过，非 0=失败。
#include "engine/net/net_transport.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

using namespace dse::net;

namespace {
constexpr uint16_t kPort = 27300;
const char* kReliable   = "RELIABLE_HELLO";
const char* kUnreliable = "UNRELIABLE_PING";

class SmokeListener final : public INetListener {
public:
    int  connected = 0;
    bool got_reliable = false;
    bool got_unreliable = false;

    void OnConnecting(ConnectionId c) override { std::printf("[smoke] connecting conn=%u\n", c); }
    void OnConnected(ConnectionId c) override {
        ++connected;
        std::printf("[smoke] connected conn=%u (total=%d)\n", c, connected);
    }
    void OnClosed(ConnectionId c, CloseReason r) override {
        std::printf("[smoke] closed conn=%u reason=%u\n", c, static_cast<unsigned>(r));
    }
    void OnMessage(ConnectionId c, const MessageView& m) override {
        std::string s(static_cast<const char*>(m.data), m.size);
        std::printf("[smoke] recv conn=%u len=%zu '%s'\n", c, m.size, s.c_str());
        if (s == kReliable)   got_reliable = true;
        if (s == kUnreliable) got_unreliable = true;
    }
};

bool pump_until(INetTransport* t, SmokeListener& l, bool (*done)(SmokeListener&), int max_ms) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        t->Poll(l);
        if (done(l)) return true;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > max_ms) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
} // namespace

int main() {
    INetTransport* t = CreateGnsTransport();
    NetConfig cfg; cfg.log_debug = false;
    if (!t->Init(cfg)) { std::printf("[smoke] FAIL Init\n"); return 1; }

    if (!t->Listen(kPort)) { std::printf("[smoke] FAIL Listen\n"); t->Shutdown(); return 2; }

    Address addr{"127.0.0.1", kPort};
    ConnectionId client = t->Connect(addr);
    if (client == kInvalidConnection) { std::printf("[smoke] FAIL Connect\n"); t->Shutdown(); return 3; }

    SmokeListener l;
    if (!pump_until(t, l, [](SmokeListener& s){ return s.connected >= 2; }, 5000)) {
        std::printf("[smoke] FAIL handshake (connected=%d)\n", l.connected);
        t->Shutdown(); return 4;
    }

    t->Send(client, kReliable,   std::strlen(kReliable),   SendMode::Reliable);
    t->Send(client, kUnreliable, std::strlen(kUnreliable), SendMode::Unreliable);

    if (!pump_until(t, l, [](SmokeListener& s){ return s.got_reliable && s.got_unreliable; }, 3000)) {
        std::printf("[smoke] FAIL recv (reliable=%d unreliable=%d)\n",
                    l.got_reliable, l.got_unreliable);
        t->Shutdown(); return 5;
    }

    ConnQuality q;
    if (t->GetQuality(client, q)) {
        std::printf("[smoke] quality ping=%.1fms loss=%.3f out=%.0fB/s in=%.0fB/s\n",
                    q.ping_ms, q.packet_loss, q.out_bytes_per_sec, q.in_bytes_per_sec);
    }

    t->Shutdown();
    delete t;
    std::printf("[smoke] PASS\n");
    return 0;
}
