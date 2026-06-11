/**
 * @file net_lua_smoke.cpp
 * @brief 无头验证 Lua 绑定 dse.net：在裸 lua_State 上注册 Phase1 API，用脚本起
 *        server(listen)+client(connect) 到 127.0.0.1 回环，握手后客户端经 lane 1
 *        发可靠消息，驱动 PumpNet 派发事件回调，校验服务端在 Lua 上下文收到消息。
 *        全程只经 dse.net.* Lua 接口（底层走 dse_net C ABI → GNS），不依赖窗口/RHI。
 *
 * 退出码：0 = Lua 回调成功收到回环消息且 lane 正确；非 0 = 失败。
 */
#include "engine/scripting/lua/bindings/lua_binding_registry.h"
#include "engine/scripting/lua/bindings/lua_binding_modules.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using namespace std::chrono_literals;

int main() {
    const int port = 27411;

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    dse::runtime::lua_binding::RegisterPhase1LuaApi(L);

    const std::string script =
        "_G.connected = 0\n"
        "_G.got = false\n"
        "_G.recv = ''\n"
        "_G.recv_lane = -1\n"
        "assert(dse.net, 'dse.net missing')\n"
        "assert(dse.net.available(), 'net backend not available')\n"
        "assert(dse.net.init(false), 'net init failed')\n"
        "dse.net.on('connected', function(conn)\n"
        "  _G.connected = _G.connected + 1\n"
        "  if conn == _G.client then\n"
        "    dse.net.configure_lanes(conn, {0, 0}, {1, 1})\n"
        "    assert(dse.net.send(conn, 'NET_LUA_HELLO', 'reliable', 1), 'send failed')\n"
        "  end\n"
        "end)\n"
        "dse.net.on('message', function(conn, data, lane)\n"
        "  _G.got = true\n"
        "  _G.recv = data\n"
        "  _G.recv_lane = lane\n"
        "end)\n"
        "assert(dse.net.listen(" + std::to_string(port) + "), 'listen failed')\n"
        "_G.client = dse.net.connect('127.0.0.1', " + std::to_string(port) + ")\n"
        "assert(_G.client ~= 0, 'connect failed')\n";

    if (luaL_dostring(L, script.c_str()) != LUA_OK) {
        std::fprintf(stderr, "[net-lua-smoke] script error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 2;
    }

    // 驱动每帧泵，直到 message 回调置 got=true（或超时）
    bool got = false;
    auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline) {
        dse::runtime::lua_binding::PumpNet(L);
        lua_getglobal(L, "got");
        got = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (got) break;
        std::this_thread::sleep_for(5ms);
    }

    std::string recv;
    int recv_lane = -1, connected = 0;
    if (got) {
        lua_getglobal(L, "recv");      recv = lua_tostring(L, -1) ? lua_tostring(L, -1) : ""; lua_pop(L, 1);
        lua_getglobal(L, "recv_lane"); recv_lane = (int)lua_tointeger(L, -1); lua_pop(L, 1);
    }
    lua_getglobal(L, "connected"); connected = (int)lua_tointeger(L, -1); lua_pop(L, 1);

    // 干净关闭 GNS
    luaL_dostring(L, "dse.net.shutdown()");
    lua_close(L);

    std::fprintf(stderr, "[net-lua-smoke] connected=%d got=%d lane=%d recv=\"%s\"\n",
                 connected, got, recv_lane, recv.c_str());

    const bool pass = got && recv == "NET_LUA_HELLO" && recv_lane == 1;
    if (pass) {
        std::fprintf(stderr, "NET_LUA_SMOKE_PASS\n");
        return 0;
    }
    std::fprintf(stderr, "NET_LUA_SMOKE_FAIL\n");
    return 3;
}
