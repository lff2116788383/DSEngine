// DSEngine 网络层 — C ABI 回环冒烟测试。
// 全程只用 dse_net_* 扁平 C 接口（模拟 Lua/C# 宿主），单进程内 Listen+Connect 到
// 127.0.0.1，握手后经 C 回调收 reliable 消息。退出码 0=通过，非 0=失败。
#include "engine/net/net_c_api.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace {
constexpr uint16_t kPort = 27301;
const char* kHello = "CAPI_HELLO";

struct State {
    int  connected = 0;
    bool got_msg = false;
};

void on_connecting(void* u, dse_net_conn c, const char* host, uint16_t port) {
    std::printf("[capi] connecting conn=%u peer=%s:%u\n", c, host, port);
}
void on_connected(void* u, dse_net_conn c) {
    auto* s = static_cast<State*>(u);
    ++s->connected;
    std::printf("[capi] connected conn=%u (total=%d)\n", c, s->connected);
}
void on_closed(void* u, dse_net_conn c, uint32_t reason) {
    std::printf("[capi] closed conn=%u reason=%u\n", c, reason);
}
void on_message(void* u, dse_net_conn c, const void* data, size_t len, uint16_t lane) {
    auto* s = static_cast<State*>(u);
    std::string msg(static_cast<const char*>(data), len);
    std::printf("[capi] recv conn=%u len=%zu lane=%u '%s'\n", c, len, lane, msg.c_str());
    if (msg == kHello) s->got_msg = true;
}

template <typename Pred>
bool pump_until(dse_net_transport* t, const dse_net_callbacks* cbs, State* s, Pred done, int max_ms) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        dse_net_poll(t, cbs, s);
        if (done(*s)) return true;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > max_ms) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
} // namespace

int main() {
    dse_net_transport* t = dse_net_create();
    if (!dse_net_init(t, 0)) { std::printf("[capi] FAIL init\n"); dse_net_destroy(t); return 1; }

    if (!dse_net_listen(t, kPort)) { std::printf("[capi] FAIL listen\n"); dse_net_destroy(t); return 2; }

    dse_net_conn client = dse_net_connect(t, "127.0.0.1", kPort);
    if (client == 0) { std::printf("[capi] FAIL connect\n"); dse_net_destroy(t); return 3; }

    dse_net_callbacks cbs{};
    cbs.on_connecting = on_connecting;
    cbs.on_connected  = on_connected;
    cbs.on_closed     = on_closed;
    cbs.on_message    = on_message;

    State st;
    if (!pump_until(t, &cbs, &st, [](const State& s){ return s.connected >= 2; }, 5000)) {
        std::printf("[capi] FAIL handshake (connected=%d)\n", st.connected);
        dse_net_destroy(t); return 4;
    }

    dse_net_send(t, client, kHello, std::strlen(kHello), DSE_NET_RELIABLE, 0);
    if (!pump_until(t, &cbs, &st, [](const State& s){ return s.got_msg; }, 3000)) {
        std::printf("[capi] FAIL recv\n"); dse_net_destroy(t); return 5;
    }

    dse_net_quality q;
    if (dse_net_get_quality(t, client, &q)) {
        std::printf("[capi] quality ping=%.1fms loss=%.3f\n", q.ping_ms, q.packet_loss);
    }

    dse_net_destroy(t);
    std::printf("[capi] PASS\n");
    return 0;
}
