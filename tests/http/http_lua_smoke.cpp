/**
 * @file http_lua_smoke.cpp
 * @brief 无头验证 Lua 绑定 dse.http：在裸 lua_State 上注册 Phase1 API，
 *        用脚本发起 dse.http.get/post + on_done 回调，驱动 PumpHttp 触发回调，
 *        校验回调在同一 Lua 上下文执行且拿到正确响应。不依赖窗口/RHI。
 *
 * 退出码：0 = Lua 回调成功拿到本地服务器响应；非 0 = 失败。
 */
#include "engine/scripting/lua/bindings/lua_binding_registry.h"
#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/http/http_client.h"

#include "ixwebsocket/IXHttpServer.h"
#include "ixwebsocket/IXNetSystem.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono_literals;

int main() {
    ix::initNetSystem();

    const int port = 8732;
    ix::HttpServer server(port, "127.0.0.1");
    server.setOnConnectionCallback(
        [](ix::HttpRequestPtr req, std::shared_ptr<ix::ConnectionState>) -> ix::HttpResponsePtr {
            ix::WebSocketHttpHeaders headers;
            headers["Content-Type"] = "application/json";
            std::string body = (req->method == "POST")
                ? std::string("{\"echo\":") + req->body + "}"
                : std::string("{\"msg\":\"hello-lua\"}");
            return std::make_shared<ix::HttpResponse>(
                200, "OK", ix::HttpErrorCode::Ok, headers, body);
        });
    if (!server.listenAndStart()) {
        std::fprintf(stderr, "[lua-smoke] server failed to start\n");
        return 1;
    }

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    dse::runtime::lua_binding::RegisterPhase1LuaApi(L);

    const std::string script =
        "_G.done = false\n"
        "_G.ok = false\n"
        "_G.body = ''\n"
        "assert(dse.http, 'dse.http missing')\n"
        "assert(dse.http.available(), 'http backend not available')\n"
        "local url = 'http://127.0.0.1:" + std::to_string(port) + "/chat'\n"
        "dse.http.request{\n"
        "  url = url,\n"
        "  method = 'POST',\n"
        "  headers = { ['Content-Type'] = 'application/json', ['X-Test'] = '1' },\n"
        "  body = '{\\\"q\\\":42}',\n"
        "  on_done = function(resp)\n"
        "    _G.done = true\n"
        "    _G.ok = resp.ok\n"
        "    _G.body = resp.body\n"
        "    _G.status = resp.status\n"
        "  end\n"
        "}\n";

    if (luaL_dostring(L, script.c_str()) != LUA_OK) {
        std::fprintf(stderr, "[lua-smoke] script error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 2;
    }

    // 驱动主线程泵，直到回调置 done=true（或超时）
    bool done = false;
    auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline) {
        dse::runtime::lua_binding::PumpHttp(L);
        lua_getglobal(L, "done");
        done = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (done) break;
        std::this_thread::sleep_for(20ms);
    }

    bool ok = false;
    std::string body;
    int status = 0;
    if (done) {
        lua_getglobal(L, "ok");     ok = lua_toboolean(L, -1) != 0;          lua_pop(L, 1);
        lua_getglobal(L, "status"); status = (int)lua_tointeger(L, -1);      lua_pop(L, 1);
        lua_getglobal(L, "body");   body = lua_tostring(L, -1) ? lua_tostring(L, -1) : ""; lua_pop(L, 1);
    }

    dse::http::HttpClient::Instance().Wait();
    dse::http::HttpClient::Instance().Poll();
    lua_close(L);
    server.stop();
    ix::uninitNetSystem();

    std::fprintf(stderr, "[lua-smoke] done=%d ok=%d status=%d body=\"%s\"\n",
                 done, ok, status, body.c_str());

    const bool pass = done && ok && status == 200 &&
                      body.find("\"echo\":{\"q\":42}") != std::string::npos;
    if (pass) {
        std::fprintf(stderr, "HTTP_LUA_SMOKE_PASS\n");
        return 0;
    }
    std::fprintf(stderr, "HTTP_LUA_SMOKE_FAIL\n");
    return 3;
}
