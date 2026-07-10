/**
 * @file lua_binding_http.cpp
 * @brief Lua 绑定补全：dse.http.request{...on_done} 高层回调式请求 + PumpHttp 每帧泵。
 *
 * 与自动生成的 lua_binding_free_http.gen.cpp 互补：后者暴露低层轮询式 C ABI
 * (update/available/poll/http_get_response)，本文件提供脚本层最常用的
 * 「一次请求 + on_done 回调」范式，直接绑定引擎 HttpClient C++ 接口（二进制安全、
 * 含完整响应头），并定义引擎 Tick 每帧调用的 PumpHttp（触发已完成请求回调）。
 *
 * 仅在 DSE_ENABLE_HTTP 构建下编译为实体；否则为空 TU（OFF 构建零回归）。
 *
 * API：
 *   dse.http.request{
 *     url = "http://...",           -- 必填，含 scheme
 *     method = "GET"|"POST"|...,    -- 默认 GET
 *     headers = { ["K"] = "V", ... },
 *     body = "...",
 *     timeout = 30,                 -- 秒
 *     verify_peer = true,           -- https 是否校验证书
 *     ca_file = "...",              -- 自定义 CA bundle（可选）
 *     on_done = function(resp) ... end,
 *   } -> request_id
 *   回调参数 resp: { ok=bool, status=int, body=string, error=string, headers={...} }
 *
 * 回调在调用 PumpHttp 的线程（主线程/脚本线程）同步触发，单线程安全。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"

#ifdef DSE_ENABLE_HTTP

#include "engine/http/http_client.h"

extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <cstdio>
#include <string>

namespace dse::runtime::lua_binding {
namespace {

// 回调触发时使用的 Lua 上下文（与 PumpHttp/request 所在线程一致，单线程安全）。
lua_State* g_http_L = nullptr;

// 把 Response 压成 Lua 表 { ok, status, body, error, headers }。
void PushResponse(lua_State* L, const dse::http::Response& resp) {
    lua_createtable(L, 0, 5);
    lua_pushboolean(L, resp.ok() ? 1 : 0);
    lua_setfield(L, -2, "ok");
    lua_pushinteger(L, resp.status);
    lua_setfield(L, -2, "status");
    lua_pushlstring(L, resp.body.data(), resp.body.size());
    lua_setfield(L, -2, "body");
    lua_pushlstring(L, resp.error.data(), resp.error.size());
    lua_setfield(L, -2, "error");
    lua_createtable(L, 0, static_cast<int>(resp.headers.size()));
    for (const auto& h : resp.headers) {
        lua_pushlstring(L, h.second.data(), h.second.size());
        lua_setfield(L, -2, h.first.c_str());
    }
    lua_setfield(L, -2, "headers");
}

// 从 request 表读取可选字符串字段。
std::string OptStringField(lua_State* L, int t, const char* key, const char* def) {
    std::string out = def ? def : "";
    lua_getfield(L, t, key);
    if (lua_isstring(L, -1)) {
        size_t n = 0;
        const char* s = lua_tolstring(L, -1, &n);
        out.assign(s, n);
    }
    lua_pop(L, 1);
    return out;
}

int L_HttpRequest(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    g_http_L = L;

    dse::http::Request req;
    req.url = OptStringField(L, 1, "url", "");
    if (req.url.empty()) {
        return luaL_error(L, "dse.http.request: 'url' 必填（需含 http:// 或 https://）");
    }
    req.method = OptStringField(L, 1, "method", "GET");
    req.body   = OptStringField(L, 1, "body", "");

    lua_getfield(L, 1, "timeout");
    if (lua_isnumber(L, -1)) req.timeout_sec = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "verify_peer");
    if (!lua_isnil(L, -1)) req.verify_peer = (lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);

    {
        std::string ca = OptStringField(L, 1, "ca_file", "");
        if (!ca.empty()) req.ca_file = std::move(ca);
    }

    // headers 子表 { ["K"] = "V", ... }
    lua_getfield(L, 1, "headers");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_type(L, -2) == LUA_TSTRING && lua_isstring(L, -1)) {
                size_t kn = 0, vn = 0;
                const char* k = lua_tolstring(L, -2, &kn);
                const char* v = lua_tolstring(L, -1, &vn);
                req.headers.emplace_back(std::string(k, kn), std::string(v, vn));
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    // on_done 回调（可选）：ref 进注册表，回调触发或请求失败后 unref。
    int cb_ref = LUA_NOREF;
    lua_getfield(L, 1, "on_done");
    if (lua_isfunction(L, -1)) {
        cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }

    dse::http::RequestId id = dse::http::HttpClient::Instance().Send(
        req, [cb_ref](const dse::http::Response& resp) {
            lua_State* L2 = g_http_L;
            if (!L2) return;
            if (cb_ref == LUA_NOREF || cb_ref == LUA_REFNIL) return;
            lua_rawgeti(L2, LUA_REGISTRYINDEX, cb_ref);
            if (lua_isfunction(L2, -1)) {
                PushResponse(L2, resp);
                if (lua_pcall(L2, 1, 0, 0) != LUA_OK) {
                    const char* m = lua_tostring(L2, -1);
                    std::fprintf(stderr, "[dse.http] on_done 回调错误: %s\n", m ? m : "?");
                    lua_pop(L2, 1);
                }
            } else {
                lua_pop(L2, 1);
            }
            luaL_unref(L2, LUA_REGISTRYINDEX, cb_ref);
        });

    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

} // anonymous namespace

// 把 request 追加到已有的 dse.http 表（由 lua_binding_free_http.gen.cpp 先行创建）。
void RegisterHttpRequestBinding(lua_State* L) {
    g_http_L = L;
    lua_getglobal(L, "dse");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    lua_getfield(L, -1, "http");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "http");
    }
    lua_pushcfunction(L, L_HttpRequest);
    lua_setfield(L, -2, "request");
    lua_pop(L, 2); // http, dse
}

// 引擎 Tick 每帧调用：在调用线程触发所有已完成 HTTP 请求的回调。
void PumpHttp(lua_State* L) {
    g_http_L = L;
    if (dse::http::HttpClient::Available()) {
        dse::http::HttpClient::Instance().Poll();
    }
}

} // namespace dse::runtime::lua_binding

#endif // DSE_ENABLE_HTTP
