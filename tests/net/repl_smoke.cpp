// DSEngine 复制层 — 回环冒烟测试（MVP）。
// 单进程内用一个传输实例（GNS 进程级回调，故同进程仅一个 transport，见 §10 D-5）
// 同时驱动 ReplicationServer 与 ReplicationClient：服务器把权威实体的 Transform
// 复制给客户端镜像，验证 spawn / 全量快照 / 输入 RPC / despawn 全链路。
// 退出码 0=通过，非 0=对应失败步骤。
#include "engine/net/net_transport.h"
#include "engine/net/replication/replication_server.h"
#include "engine/net/replication/replication_client.h"
#include "engine/ecs/transform.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

using namespace dse::net;
using dse::net::repl::ReplicationServer;
using dse::net::repl::ReplicationClient;
using dse::net::repl::NetId;

namespace {
constexpr uint16_t kPort = 27500;

// 单传输回环：按连接 id 把事件路由给 server 或 client。
// 约定：client 自己发起的连接 id == clientConn；其它连接均视为服务器侧的客户端连接。
class Router final : public INetListener {
public:
    Router(ReplicationServer* s, ReplicationClient* c, ConnectionId clientConn)
        : server_(s), client_(c), client_conn_(clientConn) {}

    void OnConnected(ConnectionId c) override {
        if (c == client_conn_) client_->OnConnected(c);
        else                   server_->OnConnected(c);
    }
    void OnClosed(ConnectionId c, CloseReason r) override {
        if (c == client_conn_) client_->OnClosed(c, r);
        else                   server_->OnClosed(c, r);
    }
    void OnMessage(ConnectionId c, const MessageView& m, LaneId lane) override {
        if (c == client_conn_) client_->OnMessage(c, m, lane);  // server→client 流量
        else                   server_->OnMessage(c, m, lane);  // client→server 流量
    }
private:
    ReplicationServer* server_;
    ReplicationClient* client_;
    ConnectionId       client_conn_;
};

bool approx(float a, float b) { return std::fabs(a - b) < 1e-3f; }

// 反复 Poll（可选每轮 tick 服务器）直到 pred 成立或超时。
template <class Pred>
bool pump(INetTransport* t, Router& r, ReplicationServer* srv, bool tick, Pred pred, int max_ms) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (tick && srv) srv->Tick();
        t->Poll(r);
        if (pred()) return true;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > max_ms) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
} // namespace

int main() {
    INetTransport* t = CreateGnsTransport();
    NetConfig cfg; cfg.log_debug = false;
    if (!t->Init(cfg))      { std::printf("[repl] FAIL Init\n");    return 1; }
    if (!t->Listen(kPort))  { std::printf("[repl] FAIL Listen\n");  t->Shutdown(); return 2; }

    ConnectionId clientConn = t->Connect(Address{"127.0.0.1", kPort});
    if (clientConn == kInvalidConnection) { std::printf("[repl] FAIL Connect\n"); t->Shutdown(); return 3; }

    entt::registry srvReg;   // 权威世界
    entt::registry cliReg;   // 客户端镜像

    ReplicationServer server; server.Init(t, &srvReg);
    ReplicationClient client; client.Init(t, &cliReg);
    Router router(&server, &client, clientConn);

    // 权威实体：位置 (1,2,3)，纳入复制（此时尚未握手，spawn 将在 OnConnected 时补发）。
    entt::entity se = srvReg.create();
    auto& st = srvReg.emplace<TransformComponent>(se);
    st.position = glm::vec3(1.0f, 2.0f, 3.0f);
    NetId id = server.MarkReplicated(se);

    // 1) 握手
    if (!pump(t, router, &server, false,
              [&]{ return client.ServerConn() != kInvalidConnection && server.ClientCount() >= 1; }, 5000)) {
        std::printf("[repl] FAIL handshake (clientConn=%u srvClients=%zu)\n",
                    client.ServerConn(), server.ClientCount());
        t->Shutdown(); return 4;
    }

    // 握手后把权威实体的属主设为该客户端连接（服务器侧 accepted 连接），
    // 这样后续来自该连接的 InputMove 才会通过服务器权威校验。
    server.SetOwner(se, server.Clients().front());

    // 2) spawn 到达：客户端镜像出现该实体
    if (!pump(t, router, &server, false, [&]{ return client.MirrorCount() >= 1; }, 3000)) {
        std::printf("[repl] FAIL spawn (mirror=%zu)\n", client.MirrorCount());
        t->Shutdown(); return 5;
    }

    // 3) 全量快照：镜像实体 Transform 与权威一致
    if (!pump(t, router, &server, true, [&]{
            entt::entity ce = client.ToEntity(id);
            if (ce == entt::null || !cliReg.valid(ce)) return false;
            auto* ct = cliReg.try_get<TransformComponent>(ce);
            return ct && approx(ct->position.x, 1.0f) && approx(ct->position.y, 2.0f) && approx(ct->position.z, 3.0f);
        }, 3000)) {
        std::printf("[repl] FAIL snapshot sync\n");
        t->Shutdown(); return 6;
    }

    // 4) 所有权校验（负例）：把属主改成一个不存在的连接，客户端移动应被服务器拒绝。
    server.SetOwner(se, 999999u);
    client.SendMove(id, 5.0f, 0.0f, 0.0f);
    pump(t, router, &server, true, []{ return false; }, 400); // 跑满 400ms 让(被拒)消息到达
    if (!approx(srvReg.get<TransformComponent>(se).position.x, 1.0f)) {
        std::printf("[repl] FAIL ownership enforcement (non-owner move applied)\n");
        t->Shutdown(); return 7;
    }
    server.SetOwner(se, server.Clients().front()); // 恢复正确属主

    // 5) 输入 RPC（C→S）：属主客户端请求 +10 x，服务器权威应用后经快照回传
    client.SendMove(id, 10.0f, 0.0f, 0.0f);
    if (!pump(t, router, &server, true, [&]{
            entt::entity ce = client.ToEntity(id);
            if (ce == entt::null || !cliReg.valid(ce)) return false;
            auto* ct = cliReg.try_get<TransformComponent>(ce);
            return ct && approx(ct->position.x, 11.0f);
        }, 3000)) {
        std::printf("[repl] FAIL input rpc (server-authoritative move)\n");
        t->Shutdown(); return 8;
    }
    // 服务器权威状态也应已更新
    if (!approx(srvReg.get<TransformComponent>(se).position.x, 11.0f)) {
        std::printf("[repl] FAIL server authoritative state not updated\n");
        t->Shutdown(); return 9;
    }

    // 6) despawn：镜像实体被移除
    server.Unreplicate(se);
    if (!pump(t, router, &server, false, [&]{ return client.MirrorCount() == 0; }, 3000)) {
        std::printf("[repl] FAIL despawn (mirror=%zu)\n", client.MirrorCount());
        t->Shutdown(); return 10;
    }

    t->Shutdown();
    delete t;
    std::printf("[repl] PASS\n");
    return 0;
}
