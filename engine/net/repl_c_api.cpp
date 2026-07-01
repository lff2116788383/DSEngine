/**
 * @file repl_c_api.cpp
 * @brief 复制层 C ABI 实现。
 */
#ifdef DSE_NET_ENABLED

#include "engine/net/repl_c_api.h"
#include "engine/net/replication/replication_server.h"
#include "engine/net/replication/replication_client.h"
#include "engine/net/rpc/rpc_registry.h"
#include "engine/net/net_transport.h"

#include <entt/entt.hpp>

using namespace dse::net;
using namespace dse::net::repl;
using namespace dse::net::rpc;

// ─── 内部包装结构 ────────────────────────────────────────────────────────

struct dse_repl_server_s {
    ReplicationServer impl;
};

struct dse_repl_client_s {
    ReplicationClient impl;
};

// RPC userdata wrapper for C callbacks
struct CRpcBinding {
    dse_rpc_handler_fn handler;
    dse_rpc_validator_fn validator;
    void* userdata;
};

// ─── 服务器 API ──────────────────────────────────────────────────────────

extern "C" {

dse_repl_server dse_repl_server_create(void) {
    return new dse_repl_server_s{};
}

void dse_repl_server_destroy(dse_repl_server srv) {
    delete srv;
}

int dse_repl_server_init(dse_repl_server srv, void* transport_ptr, void* registry_ptr) {
    if (!srv || !transport_ptr || !registry_ptr) return 0;
    srv->impl.Init(static_cast<INetTransport*>(transport_ptr),
                   static_cast<entt::registry*>(registry_ptr));
    return 1;
}

uint32_t dse_repl_server_mark(dse_repl_server srv, uint32_t entity, uint32_t owner_conn) {
    if (!srv) return 0;
    return srv->impl.MarkReplicated(static_cast<entt::entity>(entity),
                                     static_cast<ConnectionId>(owner_conn));
}

void dse_repl_server_set_owner(dse_repl_server srv, uint32_t entity, uint32_t owner_conn) {
    if (!srv) return;
    srv->impl.SetOwner(static_cast<entt::entity>(entity),
                       static_cast<ConnectionId>(owner_conn));
}

void dse_repl_server_unreplicate(dse_repl_server srv, uint32_t entity) {
    if (!srv) return;
    srv->impl.Unreplicate(static_cast<entt::entity>(entity));
}

void dse_repl_server_tick(dse_repl_server srv) {
    if (!srv) return;
    srv->impl.Tick();
}

void dse_repl_server_set_aoi(dse_repl_server srv, int policy, float radius) {
    if (!srv) return;
    srv->impl.SetAoiPolicy(static_cast<AoiPolicy>(policy), radius);
}

uint32_t dse_repl_server_client_count(dse_repl_server srv) {
    if (!srv) return 0;
    return static_cast<uint32_t>(srv->impl.ClientCount());
}

uint32_t dse_repl_server_seq(dse_repl_server srv) {
    if (!srv) return 0;
    return srv->impl.CurrentSeq();
}

// ─── 客户端 API ──────────────────────────────────────────────────────────

dse_repl_client dse_repl_client_create(void) {
    return new dse_repl_client_s{};
}

void dse_repl_client_destroy(dse_repl_client cli) {
    delete cli;
}

int dse_repl_client_init(dse_repl_client cli, void* transport_ptr, void* registry_ptr) {
    if (!cli || !transport_ptr || !registry_ptr) return 0;
    cli->impl.Init(static_cast<INetTransport*>(transport_ptr),
                   static_cast<entt::registry*>(registry_ptr));
    return 1;
}

void dse_repl_client_send_move(dse_repl_client cli, uint32_t net_id, float dx, float dy, float dz) {
    if (!cli) return;
    cli->impl.SendMove(static_cast<NetId>(net_id), dx, dy, dz);
}

int dse_repl_client_connected(dse_repl_client cli) {
    if (!cli) return 0;
    return cli->impl.IsConnected() ? 1 : 0;
}

uint32_t dse_repl_client_mirror_count(dse_repl_client cli) {
    if (!cli) return 0;
    return static_cast<uint32_t>(cli->impl.MirrorCount());
}

uint32_t dse_repl_client_to_entity(dse_repl_client cli, uint32_t net_id) {
    if (!cli) return 0xFFFFFFFF;
    entt::entity e = cli->impl.ToEntity(static_cast<NetId>(net_id));
    return e == entt::null ? 0xFFFFFFFF : static_cast<uint32_t>(e);
}

// ─── RPC API ─────────────────────────────────────────────────────────────

uint16_t dse_rpc_server_register(dse_repl_server srv, const char* name, int target,
                                  dse_rpc_handler_fn handler, dse_rpc_validator_fn validator,
                                  void* userdata) {
    if (!srv || !name || !handler) return 0xFFFF;

    auto* binding = new CRpcBinding{handler, validator, userdata};

    RpcHandler rpc_handler = [binding](const RpcContext& ctx) -> bool {
        return binding->handler(ctx.sender, ctx.target, ctx.payload,
                               ctx.payload_size, binding->userdata) != 0;
    };

    RpcValidator rpc_validator = nullptr;
    if (validator) {
        rpc_validator = [binding](ConnectionId sender, NetId target_id) -> bool {
            return binding->validator(sender, target_id, binding->userdata) != 0;
        };
    }

    return srv->impl.Rpc().Register(name, static_cast<RpcTarget>(target),
                                     std::move(rpc_handler), std::move(rpc_validator));
}

uint16_t dse_rpc_client_register(dse_repl_client cli, const char* name, int target,
                                  dse_rpc_handler_fn handler, void* userdata) {
    if (!cli || !name || !handler) return 0xFFFF;

    auto* binding = new CRpcBinding{handler, nullptr, userdata};

    RpcHandler rpc_handler = [binding](const RpcContext& ctx) -> bool {
        return binding->handler(ctx.sender, ctx.target, ctx.payload,
                               ctx.payload_size, binding->userdata) != 0;
    };

    return cli->impl.Rpc().Register(name, static_cast<RpcTarget>(target),
                                     std::move(rpc_handler));
}

int dse_rpc_client_send(dse_repl_client cli, uint16_t rpc_id, uint32_t target_net_id,
                         const void* payload, size_t payload_size) {
    if (!cli) return 0;
    return cli->impl.SendRpc(static_cast<RpcId>(rpc_id),
                              static_cast<NetId>(target_net_id),
                              payload, payload_size) ? 1 : 0;
}

void dse_rpc_server_broadcast(dse_repl_server srv, uint16_t rpc_id, uint32_t target_net_id,
                               const void* payload, size_t payload_size) {
    if (!srv) return;
    srv->impl.Rpc().Broadcast(nullptr, srv->impl.Clients(), static_cast<RpcId>(rpc_id),
                               static_cast<NetId>(target_net_id), payload, payload_size);
}

} // extern "C"

#endif // DSE_NET_ENABLED
