/**
 * @file lua_binding_repl.cpp
 * @brief Lua 绑定：复制层 (dse.repl) — 服务器/客户端复制 + RPC
 *
 * 仅在 DSE_NET_ENABLED 构建下编译。底层依赖 repl_c_api.h。
 *
 * API：
 *   -- 服务器
 *   dse.repl.server_create()                    -> userdata
 *   dse.repl.server_init(srv, transport, registry) -> bool
 *   dse.repl.server_mark(srv, entity [,owner])  -> net_id
 *   dse.repl.server_set_owner(srv, entity, owner)
 *   dse.repl.server_unreplicate(srv, entity)
 *   dse.repl.server_tick(srv)
 *   dse.repl.server_set_aoi(srv, policy, radius)  -- policy: "always"|"distance"
 *   dse.repl.server_client_count(srv)           -> int
 *   dse.repl.server_seq(srv)                    -> int
 *
 *   -- 客户端
 *   dse.repl.client_create()                    -> userdata
 *   dse.repl.client_init(cli, transport, registry) -> bool
 *   dse.repl.client_send_move(cli, net_id, dx, dy, dz)
 *   dse.repl.client_connected(cli)              -> bool
 *   dse.repl.client_mirror_count(cli)           -> int
 *   dse.repl.client_to_entity(cli, net_id)      -> entity_id | nil
 *
 *   -- RPC
 *   dse.repl.rpc_server_register(srv, name, target, handler) -> rpc_id
 *   dse.repl.rpc_client_register(cli, name, target, handler) -> rpc_id
 *   dse.repl.rpc_client_send(cli, rpc_id, target_net_id [,payload]) -> bool
 *   dse.repl.rpc_server_broadcast(srv, rpc_id, target_net_id [,payload])
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"

#ifdef DSE_NET_ENABLED

#include "engine/net/repl_c_api.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace dse::runtime::lua_binding {
namespace {

// ─── 辅助：从 Lua userdata 取句柄 ───────────────────────────────────────

static dse_repl_server ToServer(lua_State* L, int idx) {
    return *static_cast<dse_repl_server*>(luaL_checkudata(L, idx, "dse_repl_server"));
}

static dse_repl_client ToClient(lua_State* L, int idx) {
    return *static_cast<dse_repl_client*>(luaL_checkudata(L, idx, "dse_repl_client"));
}

// ─── 服务器 ──────────────────────────────────────────────────────────────

int L_ServerCreate(lua_State* L) {
    auto* ud = static_cast<dse_repl_server*>(lua_newuserdata(L, sizeof(dse_repl_server)));
    *ud = dse_repl_server_create();
    luaL_setmetatable(L, "dse_repl_server");
    return 1;
}

int L_ServerGC(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    if (srv) dse_repl_server_destroy(srv);
    return 0;
}

int L_ServerInit(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    void* transport = lua_touserdata(L, 2);
    void* registry  = lua_touserdata(L, 3);
    lua_pushboolean(L, dse_repl_server_init(srv, transport, registry));
    return 1;
}

int L_ServerMark(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t owner  = static_cast<uint32_t>(luaL_optinteger(L, 3, 0));
    uint32_t net_id = dse_repl_server_mark(srv, entity, owner);
    lua_pushinteger(L, net_id);
    return 1;
}

int L_ServerSetOwner(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t owner  = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    dse_repl_server_set_owner(srv, entity, owner);
    return 0;
}

int L_ServerUnreplicate(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_repl_server_unreplicate(srv, entity);
    return 0;
}

int L_ServerTick(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    dse_repl_server_tick(srv);
    return 0;
}

int L_ServerSetAoi(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    int policy = 0;
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char* s = lua_tostring(L, 2);
        if (std::strcmp(s, "distance") == 0) policy = 1;
    } else {
        policy = static_cast<int>(luaL_optinteger(L, 2, 0));
    }
    float radius = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    dse_repl_server_set_aoi(srv, policy, radius);
    return 0;
}

int L_ServerClientCount(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    lua_pushinteger(L, dse_repl_server_client_count(srv));
    return 1;
}

int L_ServerSeq(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    lua_pushinteger(L, dse_repl_server_seq(srv));
    return 1;
}

// ─── 客户端 ──────────────────────────────────────────────────────────────

int L_ClientCreate(lua_State* L) {
    auto* ud = static_cast<dse_repl_client*>(lua_newuserdata(L, sizeof(dse_repl_client)));
    *ud = dse_repl_client_create();
    luaL_setmetatable(L, "dse_repl_client");
    return 1;
}

int L_ClientGC(lua_State* L) {
    dse_repl_client cli = ToClient(L, 1);
    if (cli) dse_repl_client_destroy(cli);
    return 0;
}

int L_ClientInit(lua_State* L) {
    dse_repl_client cli = ToClient(L, 1);
    void* transport = lua_touserdata(L, 2);
    void* registry  = lua_touserdata(L, 3);
    lua_pushboolean(L, dse_repl_client_init(cli, transport, registry));
    return 1;
}

int L_ClientSendMove(lua_State* L) {
    dse_repl_client cli = ToClient(L, 1);
    uint32_t net_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    float dx = static_cast<float>(luaL_checknumber(L, 3));
    float dy = static_cast<float>(luaL_checknumber(L, 4));
    float dz = static_cast<float>(luaL_checknumber(L, 5));
    dse_repl_client_send_move(cli, net_id, dx, dy, dz);
    return 0;
}

int L_ClientConnected(lua_State* L) {
    dse_repl_client cli = ToClient(L, 1);
    lua_pushboolean(L, dse_repl_client_connected(cli));
    return 1;
}

int L_ClientMirrorCount(lua_State* L) {
    dse_repl_client cli = ToClient(L, 1);
    lua_pushinteger(L, dse_repl_client_mirror_count(cli));
    return 1;
}

int L_ClientToEntity(lua_State* L) {
    dse_repl_client cli = ToClient(L, 1);
    uint32_t net_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t ent = dse_repl_client_to_entity(cli, net_id);
    if (ent == 0xFFFFFFFF) lua_pushnil(L);
    else                   lua_pushinteger(L, ent);
    return 1;
}

// ─── RPC ─────────────────────────────────────────────────────────────────

struct LuaRpcBinding {
    lua_State* L;
    int handler_ref;
};

static int LuaRpcHandlerTrampoline(uint32_t sender, uint32_t target,
                                    const void* payload, size_t len, void* userdata) {
    auto* binding = static_cast<LuaRpcBinding*>(userdata);
    lua_State* L = binding->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, binding->handler_ref);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return 0; }
    lua_pushinteger(L, sender);
    lua_pushinteger(L, target);
    if (payload && len > 0)
        lua_pushlstring(L, static_cast<const char*>(payload), len);
    else
        lua_pushnil(L);
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        std::fprintf(stderr, "[dse.repl] RPC handler error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }
    int result = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return result;
}

int L_RpcServerRegister(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int target = 0;
    if (lua_type(L, 3) == LUA_TSTRING) {
        const char* s = lua_tostring(L, 3);
        if (std::strcmp(s, "client") == 0) target = 1;
        else if (std::strcmp(s, "multicast") == 0) target = 2;
    } else {
        target = static_cast<int>(luaL_optinteger(L, 3, 0));
    }
    luaL_checktype(L, 4, LUA_TFUNCTION);

    auto* binding = new LuaRpcBinding{L, LUA_NOREF};
    lua_pushvalue(L, 4);
    binding->handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    uint16_t rpc_id = dse_rpc_server_register(srv, name, target,
                                               LuaRpcHandlerTrampoline, nullptr, binding);
    lua_pushinteger(L, rpc_id);
    return 1;
}

int L_RpcClientRegister(lua_State* L) {
    dse_repl_client cli = ToClient(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int target = 1;  // Client by default
    if (lua_type(L, 3) == LUA_TSTRING) {
        const char* s = lua_tostring(L, 3);
        if (std::strcmp(s, "server") == 0) target = 0;
        else if (std::strcmp(s, "multicast") == 0) target = 2;
    } else {
        target = static_cast<int>(luaL_optinteger(L, 3, 1));
    }
    luaL_checktype(L, 4, LUA_TFUNCTION);

    auto* binding = new LuaRpcBinding{L, LUA_NOREF};
    lua_pushvalue(L, 4);
    binding->handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    uint16_t rpc_id = dse_rpc_client_register(cli, name, target,
                                               LuaRpcHandlerTrampoline, binding);
    lua_pushinteger(L, rpc_id);
    return 1;
}

int L_RpcClientSend(lua_State* L) {
    dse_repl_client cli = ToClient(L, 1);
    uint16_t rpc_id = static_cast<uint16_t>(luaL_checkinteger(L, 2));
    uint32_t target = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    size_t len = 0;
    const char* payload = nullptr;
    if (lua_isstring(L, 4)) payload = luaL_checklstring(L, 4, &len);
    lua_pushboolean(L, dse_rpc_client_send(cli, rpc_id, target, payload, len));
    return 1;
}

int L_RpcServerBroadcast(lua_State* L) {
    dse_repl_server srv = ToServer(L, 1);
    uint16_t rpc_id = static_cast<uint16_t>(luaL_checkinteger(L, 2));
    uint32_t target = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    size_t len = 0;
    const char* payload = nullptr;
    if (lua_isstring(L, 4)) payload = luaL_checklstring(L, 4, &len);
    dse_rpc_server_broadcast(srv, rpc_id, target, payload, len);
    return 0;
}

} // anonymous namespace

void RegisterReplBindings(lua_State* L) {
    // 服务器 metatable
    luaL_newmetatable(L, "dse_repl_server");
    lua_pushcfunction(L, L_ServerGC);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // 客户端 metatable
    luaL_newmetatable(L, "dse_repl_client");
    lua_pushcfunction(L, L_ClientGC);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // dse.repl 模块表
    lua_newtable(L);
    const luaL_Reg funcs[] = {
        // 服务器
        {"server_create",       L_ServerCreate},
        {"server_init",         L_ServerInit},
        {"server_mark",         L_ServerMark},
        {"server_set_owner",    L_ServerSetOwner},
        {"server_unreplicate",  L_ServerUnreplicate},
        {"server_tick",         L_ServerTick},
        {"server_set_aoi",      L_ServerSetAoi},
        {"server_client_count", L_ServerClientCount},
        {"server_seq",          L_ServerSeq},
        // 客户端
        {"client_create",       L_ClientCreate},
        {"client_init",         L_ClientInit},
        {"client_send_move",    L_ClientSendMove},
        {"client_connected",    L_ClientConnected},
        {"client_mirror_count", L_ClientMirrorCount},
        {"client_to_entity",    L_ClientToEntity},
        // RPC
        {"rpc_server_register",   L_RpcServerRegister},
        {"rpc_client_register",   L_RpcClientRegister},
        {"rpc_client_send",       L_RpcClientSend},
        {"rpc_server_broadcast",  L_RpcServerBroadcast},
        {nullptr, nullptr}
    };
    luaL_setfuncs(L, funcs, 0);

    // 常量
    lua_pushinteger(L, 0); lua_setfield(L, -2, "AOI_ALWAYS");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "AOI_DISTANCE");
    lua_pushinteger(L, 0); lua_setfield(L, -2, "RPC_SERVER");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "RPC_CLIENT");
    lua_pushinteger(L, 2); lua_setfield(L, -2, "RPC_MULTICAST");
}

} // namespace dse::runtime::lua_binding

#else // !DSE_NET_ENABLED

namespace dse::runtime::lua_binding {

void RegisterReplBindings(lua_State*) {
    // 无网络构建：不注册任何绑定
}

} // namespace dse::runtime::lua_binding

#endif // DSE_NET_ENABLED
