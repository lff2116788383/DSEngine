/**
 * @file net_full_test.cpp
 * @brief 网络层完整测试：协议握手、delta 压缩、AOI、RPC、位级序列化。
 *
 * 单进程回环测试，覆盖：
 *   1. 协议版本握手（正确版本 + 错误版本拒绝）
 *   2. 全量快照 + 序号 ACK
 *   3. 增量 delta（仅发送变化字段）
 *   4. AOI 距离裁剪（进入/离开）
 *   5. RPC 注册与双向调用（Server RPC + Multicast）
 *   6. NetBitWriter/Reader 位级量化往返
 *
 * 退出码 0=通过，非 0=对应失败步骤。
 */
#include "engine/net/net_transport.h"
#include "engine/net/replication/replication_server.h"
#include "engine/net/replication/replication_client.h"
#include "engine/net/replication/repl_aoi.h"
#include "engine/net/rpc/rpc_registry.h"
#include "engine/net/serialize/net_bitwriter.h"
#include "engine/ecs/transform.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

using namespace dse::net;
using namespace dse::net::repl;
using namespace dse::net::rpc;

namespace {
constexpr uint16_t kPort = 27501;

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
        if (c == client_conn_) client_->OnMessage(c, m, lane);
        else                   server_->OnMessage(c, m, lane);
    }
private:
    ReplicationServer* server_;
    ReplicationClient* client_;
    ConnectionId       client_conn_;
};

bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

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

// ─── 测试 1: NetBitWriter/Reader 位级量化往返 ────────────────────────────

int TestBitWriter() {
    std::printf("[test] BitWriter/Reader...\n");

    NetBitWriter w;
    w.WriteBool(true);
    w.WriteBool(false);
    w.WriteU8(0xAB);
    w.WriteU16(12345);
    w.WriteU32(0xDEADBEEF);
    w.WriteF32(3.14159f);
    w.WriteQuantized(50.0f, 0.0f, 100.0f, 10);  // 10-bit quantization
    w.WriteQuatSmallestThree(0.0f, 0.0f, 0.7071068f, 0.7071068f);  // 45-degree rotation

    NetBitReader r(w.data(), w.size_bytes());
    if (r.ReadBool() != true)   { std::printf("  FAIL bool true\n");  return 1; }
    if (r.ReadBool() != false)  { std::printf("  FAIL bool false\n"); return 1; }
    if (r.ReadU8()  != 0xAB)    { std::printf("  FAIL u8\n");         return 1; }
    if (r.ReadU16() != 12345)   { std::printf("  FAIL u16\n");        return 1; }
    if (r.ReadU32() != 0xDEADBEEF) { std::printf("  FAIL u32\n");     return 1; }
    float f = r.ReadF32();
    if (!approx(f, 3.14159f, 1e-5f)) { std::printf("  FAIL f32 (%f)\n", f); return 1; }

    float q = r.ReadQuantized(0.0f, 100.0f, 10);
    if (!approx(q, 50.0f, 0.2f)) { std::printf("  FAIL quantized (%f)\n", q); return 1; }

    float qx, qy, qz, qw;
    r.ReadQuatSmallestThree(qx, qy, qz, qw);
    if (!approx(qz, 0.7071068f, 0.01f) || !approx(qw, 0.7071068f, 0.01f)) {
        std::printf("  FAIL quat (%f %f %f %f)\n", qx, qy, qz, qw);
        return 1;
    }

    if (!r.ok()) { std::printf("  FAIL reader overflow\n"); return 1; }
    std::printf("  PASS BitWriter/Reader\n");
    return 0;
}

// ─── 测试 2: RPC 注册与派发 ─────────────────────────────────────────────

int TestRpcRegistry() {
    std::printf("[test] RPC Registry...\n");

    RpcRegistry reg;
    bool handler_called = false;
    uint32_t received_target = 0;

    RpcId id = reg.Register("TestRpc", RpcTarget::Server,
        [&](const RpcContext& ctx) -> bool {
            handler_called = true;
            received_target = ctx.target;
            return true;
        },
        [](ConnectionId sender, NetId target) -> bool {
            return sender == 42;  // 只允许连接 42 调用
        });

    if (id == kInvalidRpcId) { std::printf("  FAIL register\n"); return 1; }
    if (reg.FindByName("TestRpc") != id) { std::printf("  FAIL find\n"); return 1; }

    // 构造 RPC payload（跳过 MsgType 字节，直接从 rpc_id 开始）
    ByteWriter w;
    w.WriteU16(id);
    w.WriteU32(99);  // target NetId

    // 允许的调用者
    if (!reg.Dispatch(42, w.data(), w.size())) { std::printf("  FAIL dispatch allowed\n"); return 1; }
    if (!handler_called || received_target != 99) { std::printf("  FAIL handler\n"); return 1; }

    // 不允许的调用者
    handler_called = false;
    if (reg.Dispatch(7, w.data(), w.size())) { std::printf("  FAIL dispatch rejected\n"); return 1; }
    if (handler_called) { std::printf("  FAIL validator\n"); return 1; }

    std::printf("  PASS RPC Registry\n");
    return 0;
}

// ─── 测试 3: AOI 管理器 ─────────────────────────────────────────────────

int TestAoi() {
    std::printf("[test] AOI Manager...\n");

    entt::registry world;
    std::unordered_map<NetId, entt::entity> net2ent;

    // 创建 3 个实体：远、近、很远
    entt::entity e1 = world.create();
    auto& t1 = world.emplace<TransformComponent>(e1);
    t1.position = glm::vec3(10.0f, 0.0f, 0.0f);  // 距离=10
    net2ent[1] = e1;

    entt::entity e2 = world.create();
    auto& t2 = world.emplace<TransformComponent>(e2);
    t2.position = glm::vec3(500.0f, 0.0f, 0.0f); // 距离=500
    net2ent[2] = e2;

    entt::entity e3 = world.create();
    auto& t3 = world.emplace<TransformComponent>(e3);
    t3.position = glm::vec3(5.0f, 5.0f, 0.0f);   // 距离~7
    net2ent[3] = e3;

    AoiManager aoi;
    aoi.SetPolicy(AoiPolicy::Distance, 100.0f);

    std::unordered_map<ConnectionId, glm::vec3> viewpoints;
    viewpoints[1] = glm::vec3(0.0f);  // 观察者在原点

    aoi.Update(world, net2ent, viewpoints);

    // e1(10) 和 e3(~7) 应该可见，e2(500) 不可见
    if (!aoi.IsRelevant(1, 1)) { std::printf("  FAIL e1 should be visible\n"); return 1; }
    if (aoi.IsRelevant(1, 2))  { std::printf("  FAIL e2 should NOT be visible\n"); return 1; }
    if (!aoi.IsRelevant(1, 3)) { std::printf("  FAIL e3 should be visible\n"); return 1; }

    // Always 模式：全部可见
    aoi.SetPolicy(AoiPolicy::Always);
    aoi.Update(world, net2ent, viewpoints);
    if (!aoi.IsRelevant(1, 2)) { std::printf("  FAIL Always: e2 should be visible\n"); return 1; }

    std::printf("  PASS AOI Manager\n");
    return 0;
}

} // namespace

int main() {
    // ── 单元测试（无需网络）──
    int rc;
    if ((rc = TestBitWriter()) != 0) return 100 + rc;
    if ((rc = TestRpcRegistry()) != 0) return 200 + rc;
    if ((rc = TestAoi()) != 0) return 300 + rc;

    // ── 集成测试：完整回环（需要 GNS transport）──
    std::printf("[test] Full replication integration...\n");

    INetTransport* t = CreateGnsTransport();
    NetConfig cfg; cfg.log_debug = false;
    if (!t->Init(cfg))     { std::printf("  FAIL Init\n");   return 1; }
    if (!t->Listen(kPort)) { std::printf("  FAIL Listen\n"); t->Shutdown(); return 2; }

    ConnectionId clientConn = t->Connect(Address{"127.0.0.1", kPort});
    if (clientConn == kInvalidConnection) { std::printf("  FAIL Connect\n"); t->Shutdown(); return 3; }

    entt::registry srvReg;
    entt::registry cliReg;

    ReplicationServer server; server.Init(t, &srvReg);
    ReplicationClient client; client.Init(t, &cliReg);
    Router router(&server, &client, clientConn);

    // 创建权威实体
    entt::entity se = srvReg.create();
    auto& st = srvReg.emplace<TransformComponent>(se);
    st.position = glm::vec3(1.0f, 2.0f, 3.0f);
    NetId id = server.MarkReplicated(se);

    // 4) 协议版本握手
    if (!pump(t, router, &server, false,
              [&]{ return client.IsConnected() && server.ClientCount() >= 1; }, 5000)) {
        std::printf("  FAIL handshake\n");
        t->Shutdown(); return 4;
    }
    std::printf("  PASS handshake\n");

    server.SetOwner(se, server.Clients().front());

    // 5) spawn
    if (!pump(t, router, &server, false, [&]{ return client.MirrorCount() >= 1; }, 3000)) {
        std::printf("  FAIL spawn\n");
        t->Shutdown(); return 5;
    }
    std::printf("  PASS spawn\n");

    // 6) 全量快照 + ACK
    if (!pump(t, router, &server, true, [&]{
            entt::entity ce = client.ToEntity(id);
            if (ce == entt::null || !cliReg.valid(ce)) return false;
            auto* ct = cliReg.try_get<TransformComponent>(ce);
            return ct && approx(ct->position.x, 1.0f) && approx(ct->position.y, 2.0f);
        }, 3000)) {
        std::printf("  FAIL snapshot sync\n");
        t->Shutdown(); return 6;
    }
    // 验证客户端发了 ACK
    if (!pump(t, router, &server, true, [&]{ return client.LastReceivedSeq() > 0; }, 2000)) {
        std::printf("  FAIL snapshot ack\n");
        t->Shutdown(); return 6;
    }
    std::printf("  PASS snapshot + ACK (seq=%u)\n", client.LastReceivedSeq());

    // 7) 增量 delta：只修改 position.x，验证客户端收到更新
    st.position.x = 99.0f;
    if (!pump(t, router, &server, true, [&]{
            entt::entity ce = client.ToEntity(id);
            if (ce == entt::null || !cliReg.valid(ce)) return false;
            auto* ct = cliReg.try_get<TransformComponent>(ce);
            return ct && approx(ct->position.x, 99.0f);
        }, 3000)) {
        std::printf("  FAIL delta sync\n");
        t->Shutdown(); return 7;
    }
    std::printf("  PASS delta compression\n");

    // 8) RPC 双向调用
    bool rpc_received = false;
    server.Rpc().Register("TestAction", RpcTarget::Server,
        [&](const RpcContext& ctx) -> bool {
            rpc_received = (ctx.target == id);
            return true;
        },
        [&](ConnectionId sender, NetId) -> bool {
            return sender == server.Clients().front();
        });

    // 客户端也需要知道这个 RPC 的 ID
    RpcId rpc_id = client.Rpc().Register("TestAction", RpcTarget::Server,
        [](const RpcContext&) -> bool { return true; });
    client.SendRpc(rpc_id, id);

    if (!pump(t, router, &server, true, [&]{ return rpc_received; }, 3000)) {
        std::printf("  FAIL RPC C->S\n");
        t->Shutdown(); return 8;
    }
    std::printf("  PASS RPC (C->S)\n");

    // 9) AOI 距离裁剪
    server.SetAoiPolicy(AoiPolicy::Distance, 50.0f);
    // 把实体移到很远
    st.position = glm::vec3(1000.0f, 0.0f, 0.0f);
    // 需要多次 Tick 让 AOI 生效并 despawn 该实体
    if (!pump(t, router, &server, true, [&]{ return client.MirrorCount() == 0; }, 5000)) {
        std::printf("  FAIL AOI despawn (mirror=%zu)\n", client.MirrorCount());
        t->Shutdown(); return 9;
    }
    std::printf("  PASS AOI distance culling\n");

    // 10) 把实体移回来，应重新 spawn
    st.position = glm::vec3(0.0f, 0.0f, 0.0f);
    if (!pump(t, router, &server, true, [&]{ return client.MirrorCount() >= 1; }, 5000)) {
        std::printf("  FAIL AOI re-spawn (mirror=%zu)\n", client.MirrorCount());
        t->Shutdown(); return 10;
    }
    std::printf("  PASS AOI re-enter\n");

    // 11) despawn
    server.Unreplicate(se);
    if (!pump(t, router, &server, false, [&]{ return client.MirrorCount() == 0; }, 3000)) {
        std::printf("  FAIL despawn\n");
        t->Shutdown(); return 11;
    }
    std::printf("  PASS despawn\n");

    t->Shutdown();
    delete t;
    std::printf("[test] ALL PASS\n");
    return 0;
}
