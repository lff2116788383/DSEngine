/**
 * @file lua_binding_http.cpp
 * @brief Lua 绑定：通用异步 HTTP(S) 客户端 (dse.http)
 *
 * 仅在 DSE_ENABLE_HTTP 构建下编译为实体；否则为空 TU（零回归）。
 *
 * API（回调均在主线程/脚本线程触发，单线程安全）：
 *   dse.http.request(opts)  -- opts = {
 *                                url=string,                    -- 必填，含 scheme
 *                                method="GET"|"POST"|...,       -- 默认 GET
 *                                headers={ ["K"]="V", ... },    -- 可选请求头(map)
 *                                body=string,                   -- 可选请求体
 *                                timeout=number,                -- 可选秒，默认 30
 *                                verify_peer=bool,              -- https 是否校验证书，默认 true
 *                                ca_file=string,                -- 可选自定义 CA bundle
 *                                on_done=function(resp) end }   -- 完成回调
 *                            → request_id (number)
 *   dse.http.get(url, on_done)                 → request_id
 *   dse.http.post(url, body [,content_type], on_done) → request_id
 *   dse.http.update()                          -- 每帧调用，触发已完成回调（引擎 Tick 亦会自动调用）
 *   dse.http.available()                       → bool（是否编译进真实后端）
 *
 *   resp 表字段：{ id, status, body, error, ok (bool), headers={K=V,...} }
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"

#ifdef DSE_ENABLE_HTTP

#include "engine/http/http_client.h"

#include <cstdio>
#include <string>
#include <utility>

namespace dse::runtime::lua_binding {
namespace {

// 绑定所在的主 Lua 状态。HTTP 回调在 PumpHttp/update() 中（同一线程）触发，
// 因此直接用该状态访问注册表是安全的。
lua_State* g_lua = nullptr;

// 把 C++ Response 压成一个 Lua 表压栈。
void PushResponse(lua_State* L, const dse::http::Response& r) {
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, static_cast<lua_Integer>(r.id));   lua_setfield(L, -2, "id");
    lua_pushinteger(L, r.status);                          lua_setfield(L, -2, "status");
    lua_pushlstring(L, r.body.data(), r.body.size());      lua_setfield(L, -2, "body");
    lua_pushlstring(L, r.error.data(), r.error.size());    lua_setfield(L, -2, "error");
    lua_pushboolean(L, r.ok() ? 1 : 0);                    lua_setfield(L, -2, "ok");
    lua_createtable(L, 0, static_cast<int>(r.headers.size()));
    for (const auto& kv : r.headers) {
        lua_pushlstring(L, kv.second.data(), kv.second.size());
        lua_setfield(L, -2, kv.first.c_str());
    }
    lua_setfield(L, -2, "headers");
}

// 提交请求：cb_index 处为 Lua 回调函数（可为 nil）。返回 request id。
dse::http::RequestId Submit(lua_State* L, const dse::http::Request& req, int cb_index) {
    int ref = LUA_NOREF;
    if (cb_index != 0 && !lua_isnoneornil(L, cb_index)) {
        lua_pushvalue(L, cb_index);                 // 复制回调到栈顶
        ref = luaL_ref(L, LUA_REGISTRYINDEX);       // 弹出并取得引用
    }

    auto& client = dse::http::HttpClient::Instance();
    return client.Send(req, [ref](const dse::http::Response& resp) {
        // 在主线程（Poll）上下文执行
        lua_State* L2 = g_lua;
        if (!L2) return;
        if (ref == LUA_NOREF || ref == LUA_REFNIL) return;
        lua_rawgeti(L2, LUA_REGISTRYINDEX, ref);    // 取回调
        if (lua_isfunction(L2, -1)) {
            PushResponse(L2, resp);
            if (lua_pcall(L2, 1, 0, 0) != LUA_OK) {
                // 吞掉错误信息（打印到 stderr），避免破坏帧循环
                const char* msg = lua_tostring(L2, -1);
                std::fprintf(stderr, "[dse.http] callback error: %s\n", msg ? msg : "?");
                lua_pop(L2, 1);
            }
        } else {
            lua_pop(L2, 1);
        }
        luaL_unref(L2, LUA_REGISTRYINDEX, ref);
    });
}

// 从 opts 表的 headers 字段（map）填充到 req.headers。
void ReadHeaders(lua_State* L, int opts_index, dse::http::Request& req) {
    lua_getfield(L, opts_index, "headers");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            // key=-2, value=-1
            if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING) {
                size_t klen = 0, vlen = 0;
                const char* k = lua_tolstring(L, -2, &klen);
                const char* v = lua_tolstring(L, -1, &vlen);
                req.headers.emplace_back(std::string(k, klen), std::string(v, vlen));
            }
            lua_pop(L, 1); // 弹 value，保留 key 供 next
        }
    }
    lua_pop(L, 1); // 弹 headers 字段
}

int L_HttpRequest(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    dse::http::Request req;

    lua_getfield(L, 1, "url");
    req.url = luaL_optstring(L, -1, "");
    lua_pop(L, 1);
    if (req.url.empty()) return luaL_error(L, "dse.http.request: 'url' is required");

    lua_getfield(L, 1, "method");
    req.method = luaL_optstring(L, -1, "GET");
    lua_pop(L, 1);

    lua_getfield(L, 1, "body");
    if (lua_isstring(L, -1)) {
        size_t blen = 0;
        const char* b = lua_tolstring(L, -1, &blen);
        req.body.assign(b, blen);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "timeout");
    if (lua_isnumber(L, -1)) req.timeout_sec = static_cast<int>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "verify_peer");
    if (lua_isboolean(L, -1)) req.verify_peer = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getfield(L, 1, "ca_file");
    if (lua_isstring(L, -1)) req.ca_file = lua_tostring(L, -1);
    lua_pop(L, 1);

    ReadHeaders(L, 1, req);

    lua_getfield(L, 1, "on_done");
    const int cb_index = lua_gettop(L);   // on_done 现在在栈顶
    dse::http::RequestId id = Submit(L, req, cb_index);
    lua_pop(L, 1); // 弹 on_done

    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

int L_HttpGet(lua_State* L) {
    dse::http::Request req;
    req.url    = luaL_checkstring(L, 1);
    req.method = "GET";
    // 第二参数为回调（可选）
    dse::http::RequestId id = Submit(L, req, lua_isnoneornil(L, 2) ? 0 : 2);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

int L_HttpPost(lua_State* L) {
    dse::http::Request req;
    req.url    = luaL_checkstring(L, 1);
    req.method = "POST";
    size_t blen = 0;
    const char* b = luaL_optlstring(L, 2, "", &blen);
    req.body.assign(b, blen);

    int cb_index;
    if (lua_type(L, 3) == LUA_TSTRING) {
        req.headers.emplace_back("Content-Type", lua_tostring(L, 3));
        cb_index = lua_isnoneornil(L, 4) ? 0 : 4;
    } else {
        req.headers.emplace_back("Content-Type", "application/json");
        cb_index = lua_isnoneornil(L, 3) ? 0 : 3;
    }
    dse::http::RequestId id = Submit(L, req, cb_index);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

int L_HttpUpdate(lua_State* L) {
    int n = dse::http::HttpClient::Instance().Poll();
    lua_pushinteger(L, n);
    return 1;
}

int L_HttpAvailable(lua_State* L) {
    lua_pushboolean(L, dse::http::HttpClient::Available() ? 1 : 0);
    return 1;
}

} // anonymous namespace

void RegisterHttpBindings(lua_State* L) {
    g_lua = L;
    lua_newtable(L);
    const luaL_Reg funcs[] = {
        {"request",   L_HttpRequest},
        {"get",       L_HttpGet},
        {"post",      L_HttpPost},
        {"update",    L_HttpUpdate},
        {"available", L_HttpAvailable},
        {nullptr, nullptr}
    };
    luaL_setfuncs(L, funcs, 0);
}

void PumpHttp(lua_State* /*L*/) {
    dse::http::HttpClient::Instance().Poll();
}

} // namespace dse::runtime::lua_binding

#endif // DSE_ENABLE_HTTP
