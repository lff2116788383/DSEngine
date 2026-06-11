/**
 * @file lua_binding_net.cpp
 * @brief Lua 绑定：游戏网络传输 (dse.net) — GameNetworkingSockets 后端
 *
 * 仅在 DSE_NET_ENABLED 构建下编译为实体；否则为空 TU（零回归）。
 * 底层只依赖 engine/net 的扁平 C ABI（net_c_api.h），不直接 include GNS 头。
 *
 * 定位：dse.net 是游戏专用 UDP 可靠/非可靠传输（客户端↔服务端 / 状态同步），
 *       与跟 REST 服务通信的 dse.http 互补；它不能用来连 HTTPS API。
 *
 * 进程内单一传输实例（一个游戏一般只需一个网络上下文）。事件经回调在 poll()
 * 所在线程（主线程/脚本线程）同步触发，单线程安全。
 *
 * API：
 *   dse.net.init([log_debug])             -> bool      创建并初始化后端（幂等）
 *   dse.net.shutdown()                                关闭并销毁
 *   dse.net.available()                   -> bool      是否编译进真实后端
 *   dse.net.listen(port)                  -> bool      监听端口（服务端）
 *   dse.net.connect(host, port)           -> conn      连接（0=失败）
 *   dse.net.close(conn [,reason])
 *   dse.net.configure_lanes(conn, priorities [,weights]) -> bool
 *   dse.net.send(conn, data [,mode] [,lane]) -> bool   mode: "reliable"|"unreliable"|0|1
 *   dse.net.flush(conn)
 *   dse.net.poll()                                    每帧泵：派发事件回调（引擎 Tick 亦自动调用）
 *   dse.net.get_quality(conn)             -> table|nil { ping_ms, packet_loss, ... }
 *   dse.net.on(event, fn)                             事件回调，event:
 *        "connecting" -> fn(conn, host, port)
 *        "connected"  -> fn(conn)
 *        "closed"     -> fn(conn, reason)
 *        "message"    -> fn(conn, data, lane)   data 为二进制安全字符串
 *
 *   常量：dse.net.RELIABLE / UNRELIABLE，
 *         dse.net.CLOSE_NORMAL / CLOSE_BY_PEER / CLOSE_PROBLEM / CLOSE_REJECTED
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"

#ifdef DSE_NET_ENABLED

#include "engine/net/net_c_api.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace dse::runtime::lua_binding {
namespace {

// 进程内单一网络上下文。回调引用与 poll() 在同一线程使用，单线程安全。
struct NetState {
    lua_State*         L           = nullptr;
    dse_net_transport* transport   = nullptr;
    int                cb_connecting = LUA_NOREF;
    int                cb_connected  = LUA_NOREF;
    int                cb_closed     = LUA_NOREF;
    int                cb_message    = LUA_NOREF;
};
NetState g_net;

void Unref(int& ref) {
    if (g_net.L && ref != LUA_NOREF && ref != LUA_REFNIL) {
        luaL_unref(g_net.L, LUA_REGISTRYINDEX, ref);
    }
    ref = LUA_NOREF;
}

// 取出回调并 pcall（nargs 个参数已压栈在调用前由调用方准备）；失败则吞错打印。
void CallRef(int ref, int nargs_pusher(lua_State*), const char* what) {
    lua_State* L = g_net.L;
    if (!L || ref == LUA_NOREF || ref == LUA_REFNIL) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    int nargs = nargs_pusher(L);
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        std::fprintf(stderr, "[dse.net] %s callback error: %s\n", what, msg ? msg : "?");
        lua_pop(L, 1);
    }
}

// ── C ABI 事件回调（在 dse_net_poll 内同步触发）──
// 经 thread-local 传递当前事件参数给 pusher（避免捕获）。
struct EvArgs {
    dse_net_conn conn = 0;
    const char*  host = nullptr;
    uint16_t     port = 0;
    uint32_t     reason = 0;
    const void*  data = nullptr;
    size_t       len = 0;
    uint16_t     lane = 0;
};
thread_local EvArgs g_ev;

int PushConnecting(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(g_ev.conn));
    lua_pushstring(L, g_ev.host ? g_ev.host : "");
    lua_pushinteger(L, g_ev.port);
    return 3;
}
int PushConnected(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(g_ev.conn));
    return 1;
}
int PushClosed(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(g_ev.conn));
    lua_pushinteger(L, static_cast<lua_Integer>(g_ev.reason));
    return 2;
}
int PushMessage(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(g_ev.conn));
    lua_pushlstring(L, static_cast<const char*>(g_ev.data), g_ev.len);
    lua_pushinteger(L, g_ev.lane);
    return 3;
}

void Cb_connecting(void*, dse_net_conn c, const char* host, uint16_t port) {
    g_ev = EvArgs{}; g_ev.conn = c; g_ev.host = host; g_ev.port = port;
    CallRef(g_net.cb_connecting, PushConnecting, "connecting");
}
void Cb_connected(void*, dse_net_conn c) {
    g_ev = EvArgs{}; g_ev.conn = c;
    CallRef(g_net.cb_connected, PushConnected, "connected");
}
void Cb_closed(void*, dse_net_conn c, uint32_t reason) {
    g_ev = EvArgs{}; g_ev.conn = c; g_ev.reason = reason;
    CallRef(g_net.cb_closed, PushClosed, "closed");
}
void Cb_message(void*, dse_net_conn c, const void* data, size_t len, uint16_t lane) {
    g_ev = EvArgs{}; g_ev.conn = c; g_ev.data = data; g_ev.len = len; g_ev.lane = lane;
    CallRef(g_net.cb_message, PushMessage, "message");
}

void DoPoll() {
    if (!g_net.transport) return;
    dse_net_callbacks cbs{};
    cbs.on_connecting = Cb_connecting;
    cbs.on_connected  = Cb_connected;
    cbs.on_closed     = Cb_closed;
    cbs.on_message    = Cb_message;
    dse_net_poll(g_net.transport, &cbs, nullptr);
}

// ── Lua 接口 ──
int L_NetInit(lua_State* L) {
    if (!g_net.transport) {
        g_net.transport = dse_net_create();
        int log_debug = lua_toboolean(L, 1);
        if (!dse_net_init(g_net.transport, log_debug)) {
            dse_net_destroy(g_net.transport);
            g_net.transport = nullptr;
            lua_pushboolean(L, 0);
            return 1;
        }
    }
    lua_pushboolean(L, 1);
    return 1;
}

int L_NetShutdown(lua_State* L) {
    if (g_net.transport) {
        dse_net_destroy(g_net.transport);
        g_net.transport = nullptr;
    }
    Unref(g_net.cb_connecting);
    Unref(g_net.cb_connected);
    Unref(g_net.cb_closed);
    Unref(g_net.cb_message);
    (void)L;
    return 0;
}

int L_NetAvailable(lua_State* L) {
    lua_pushboolean(L, 1);  // 编译进真实后端即可用
    return 1;
}

dse_net_transport* RequireT(lua_State* L) {
    if (!g_net.transport) luaL_error(L, "dse.net: 未初始化，请先调用 dse.net.init()");
    return g_net.transport;
}

int L_NetListen(lua_State* L) {
    int port = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_net_listen(RequireT(L), static_cast<uint16_t>(port)) ? 1 : 0);
    return 1;
}

int L_NetConnect(lua_State* L) {
    const char* host = luaL_checkstring(L, 1);
    int port = static_cast<int>(luaL_checkinteger(L, 2));
    dse_net_conn c = dse_net_connect(RequireT(L), host, static_cast<uint16_t>(port));
    lua_pushinteger(L, static_cast<lua_Integer>(c));
    return 1;
}

int L_NetClose(lua_State* L) {
    dse_net_conn c = static_cast<dse_net_conn>(luaL_checkinteger(L, 1));
    uint32_t reason = static_cast<uint32_t>(luaL_optinteger(L, 2, DSE_NET_CLOSE_NORMAL));
    dse_net_close(RequireT(L), c, reason);
    return 0;
}

int L_NetConfigureLanes(lua_State* L) {
    dse_net_conn c = static_cast<dse_net_conn>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = static_cast<int>(lua_rawlen(L, 2));
    if (n <= 0) { lua_pushboolean(L, 0); return 1; }

    // priorities（必填）
    int* pri = static_cast<int*>(std::malloc(sizeof(int) * n));
    if (!pri) return luaL_error(L, "configure_lanes: out of memory");
    for (int i = 0; i < n; ++i) {
        lua_rawgeti(L, 2, i + 1);
        pri[i] = static_cast<int>(luaL_optinteger(L, -1, 0));
        lua_pop(L, 1);
    }
    // weights（可选）
    uint16_t* wts = nullptr;
    if (lua_istable(L, 3)) {
        wts = static_cast<uint16_t*>(std::malloc(sizeof(uint16_t) * n));
        if (!wts) { std::free(pri); return luaL_error(L, "configure_lanes: out of memory"); }
        for (int i = 0; i < n; ++i) {
            lua_rawgeti(L, 3, i + 1);
            wts[i] = static_cast<uint16_t>(luaL_optinteger(L, -1, 1));
            lua_pop(L, 1);
        }
    }
    int ok = dse_net_configure_lanes(RequireT(L), c, n, pri, wts);
    std::free(pri);
    std::free(wts);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

uint32_t ParseMode(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TSTRING) {
        const char* s = lua_tostring(L, idx);
        if (std::strcmp(s, "unreliable") == 0) return DSE_NET_UNRELIABLE;
        return DSE_NET_RELIABLE;
    }
    if (lua_isnumber(L, idx)) return static_cast<uint32_t>(lua_tointeger(L, idx));
    return DSE_NET_RELIABLE;
}

int L_NetSend(lua_State* L) {
    dse_net_conn c = static_cast<dse_net_conn>(luaL_checkinteger(L, 1));
    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);
    uint32_t mode = ParseMode(L, 3);
    uint16_t lane = static_cast<uint16_t>(luaL_optinteger(L, 4, 0));
    int ok = dse_net_send(RequireT(L), c, data, len, mode, lane);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int L_NetFlush(lua_State* L) {
    dse_net_conn c = static_cast<dse_net_conn>(luaL_checkinteger(L, 1));
    dse_net_flush(RequireT(L), c);
    return 0;
}

int L_NetPoll(lua_State* L) {
    (void)L;
    DoPoll();
    return 0;
}

int L_NetGetQuality(lua_State* L) {
    dse_net_conn c = static_cast<dse_net_conn>(luaL_checkinteger(L, 1));
    dse_net_quality q;
    if (!dse_net_get_quality(RequireT(L), c, &q)) { lua_pushnil(L); return 1; }
    lua_createtable(L, 0, 5);
    lua_pushnumber(L, q.ping_ms);            lua_setfield(L, -2, "ping_ms");
    lua_pushnumber(L, q.packet_loss);        lua_setfield(L, -2, "packet_loss");
    lua_pushnumber(L, q.out_bytes_per_sec);  lua_setfield(L, -2, "out_bytes_per_sec");
    lua_pushnumber(L, q.in_bytes_per_sec);   lua_setfield(L, -2, "in_bytes_per_sec");
    lua_pushinteger(L, q.pending_reliable);  lua_setfield(L, -2, "pending_reliable");
    return 1;
}

int L_NetOn(lua_State* L) {
    const char* ev = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    int* slot = nullptr;
    if      (std::strcmp(ev, "connecting") == 0) slot = &g_net.cb_connecting;
    else if (std::strcmp(ev, "connected")  == 0) slot = &g_net.cb_connected;
    else if (std::strcmp(ev, "closed")     == 0) slot = &g_net.cb_closed;
    else if (std::strcmp(ev, "message")    == 0) slot = &g_net.cb_message;
    else return luaL_error(L, "dse.net.on: 未知事件 '%s'（connecting/connected/closed/message）", ev);

    Unref(*slot);
    lua_pushvalue(L, 2);
    *slot = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

} // anonymous namespace

void RegisterNetBindings(lua_State* L) {
    g_net.L = L;
    lua_newtable(L);
    const luaL_Reg funcs[] = {
        {"init",            L_NetInit},
        {"shutdown",        L_NetShutdown},
        {"available",       L_NetAvailable},
        {"listen",          L_NetListen},
        {"connect",         L_NetConnect},
        {"close",           L_NetClose},
        {"configure_lanes", L_NetConfigureLanes},
        {"send",            L_NetSend},
        {"flush",           L_NetFlush},
        {"poll",            L_NetPoll},
        {"get_quality",     L_NetGetQuality},
        {"on",              L_NetOn},
        {nullptr, nullptr}
    };
    luaL_setfuncs(L, funcs, 0);

    // 常量
    lua_pushinteger(L, DSE_NET_RELIABLE);        lua_setfield(L, -2, "RELIABLE");
    lua_pushinteger(L, DSE_NET_UNRELIABLE);      lua_setfield(L, -2, "UNRELIABLE");
    lua_pushinteger(L, DSE_NET_CLOSE_NORMAL);    lua_setfield(L, -2, "CLOSE_NORMAL");
    lua_pushinteger(L, DSE_NET_CLOSE_BY_PEER);   lua_setfield(L, -2, "CLOSE_BY_PEER");
    lua_pushinteger(L, DSE_NET_CLOSE_PROBLEM);   lua_setfield(L, -2, "CLOSE_PROBLEM");
    lua_pushinteger(L, DSE_NET_CLOSE_REJECTED);  lua_setfield(L, -2, "CLOSE_REJECTED");
}

void PumpNet(lua_State* /*L*/) {
    DoPoll();
}

} // namespace dse::runtime::lua_binding

#endif // DSE_NET_ENABLED
