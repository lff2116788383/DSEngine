/**
 * @file lua_binding_http.cpp
 * @brief Lua 薄包装：通用异步 HTTP(S) 客户端 (dse.http)
 *
 * 仅做 Lua 栈 ↔ C ABI 参数转换，所有逻辑委托 dse_http_* 函数。
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
 *   resp 表字段：{ id, status, body, error, ok (bool) }
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"

#ifdef DSE_ENABLE_HTTP

#include "engine/scripting/native_api/dse_api.h"

#include <cstdio>
#include <string>
#include <unordered_map>

extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

// 绑定所在的主 Lua 状态。HTTP 回调在 PumpHttp/update() 中（同一线程）触发，
// 因此直接用该状态访问注册表是安全的。
lua_State* g_lua = nullptr;

// request_id → Lua 回调引用
static std::unordered_map<uint32_t, int> s_callback_refs;

// 把 Lua headers 表序列化为 JSON 字符串 {"K":"V",...}
std::string HeadersToJson(lua_State* L, int opts_index) {
    std::string json;
    lua_getfield(L, opts_index, "headers");
    bool first = true;
    if (lua_istable(L, -1)) {
        json = "{";
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING) {
                size_t klen = 0, vlen = 0;
                const char* k = lua_tolstring(L, -2, &klen);
                const char* v = lua_tolstring(L, -1, &vlen);
                if (!first) json += ",";
                first = false;
                json += "\"";
                // 简单转义：跳过控制字符
                for (size_t i = 0; i < klen; ++i) {
                    char c = k[i];
                    if (c == '"' || c == '\\') json += '\\';
                    json += c;
                }
                json += "\":\"";
                for (size_t i = 0; i < vlen; ++i) {
                    char c = v[i];
                    if (c == '"' || c == '\\') json += '\\';
                    json += c;
                }
                json += "\"";
            }
            lua_pop(L, 1);
        }
        json += "}";
    }
    lua_pop(L, 1);
    return json;
}

// 提交请求到 C ABI，并存储 Lua 回调引用。返回 request_id。
uint32_t SubmitToCABI(lua_State* L, const char* method, const char* url,
                      const char* body, const char* headers_json,
                      int timeout_sec, int verify_peer, const char* ca_file,
                      int cb_index) {
    uint32_t id = dse_http_send(method, url, body, headers_json,
                                timeout_sec, verify_peer, ca_file);
    if (id == 0) return 0;

    if (cb_index != 0 && !lua_isnoneornil(L, cb_index)) {
        lua_pushvalue(L, cb_index);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        s_callback_refs[id] = ref;
    }
    return id;
}

// 轮询已完成的请求并触发回调，返回已分发的回调数
int PollAndDispatch(lua_State* L) {
    if (!L) return 0;

    constexpr int kMaxBatch = 64;
    uint32_t ids[kMaxBatch];
    int count = dse_http_poll(ids, kMaxBatch);
    int dispatched = 0;

    for (int i = 0; i < count; ++i) {
        uint32_t id = ids[i];
        auto it = s_callback_refs.find(id);
        if (it == s_callback_refs.end()) continue;

        int ref = it->second;
        s_callback_refs.erase(it);

        if (ref == LUA_NOREF || ref == LUA_REFNIL) continue;
        ++dispatched;

        // 获取响应数据
        int status = 0;
        char body_buf[65536];
        char error_buf[1024];
        body_buf[0] = '\0';
        error_buf[0] = '\0';
        dse_http_get_response(id, &status, body_buf, sizeof(body_buf),
                              error_buf, sizeof(error_buf));

        // 取回调并调用
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        if (lua_isfunction(L, -1)) {
            lua_createtable(L, 0, 4);
            lua_pushinteger(L, static_cast<lua_Integer>(id));
            lua_setfield(L, -2, "id");
            lua_pushinteger(L, status);
            lua_setfield(L, -2, "status");
            lua_pushstring(L, body_buf);
            lua_setfield(L, -2, "body");
            lua_pushstring(L, error_buf);
            lua_setfield(L, -2, "error");
            lua_pushboolean(L, (status >= 200 && status < 300) ? 1 : 0);
            lua_setfield(L, -2, "ok");

            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                const char* msg = lua_tostring(L, -1);
                std::fprintf(stderr, "[dse.http] callback error: %s\n", msg ? msg : "?");
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
    return dispatched;
}

int L_HttpRequest(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "url");
    const char* url = luaL_optstring(L, -1, "");
    lua_pop(L, 1);
    if (!url[0]) return luaL_error(L, "dse.http.request: 'url' is required");

    lua_getfield(L, 1, "method");
    const char* method = luaL_optstring(L, -1, "GET");
    lua_pop(L, 1);

    lua_getfield(L, 1, "body");
    const char* body = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);

    lua_getfield(L, 1, "timeout");
    int timeout = lua_isnumber(L, -1) ? static_cast<int>(lua_tonumber(L, -1)) : 30;
    lua_pop(L, 1);

    lua_getfield(L, 1, "verify_peer");
    int verify_peer = lua_isboolean(L, -1) ? (lua_toboolean(L, -1) ? 1 : 0) : 1;
    lua_pop(L, 1);

    lua_getfield(L, 1, "ca_file");
    const char* ca_file = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
    lua_pop(L, 1);

    std::string headers_json = HeadersToJson(L, 1);

    lua_getfield(L, 1, "on_done");
    const int cb_index = lua_gettop(L);

    uint32_t id = SubmitToCABI(L, method, url, body,
                               headers_json.empty() ? nullptr : headers_json.c_str(),
                               timeout, verify_peer, ca_file, cb_index);
    lua_pop(L, 1);

    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

int L_HttpGet(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    uint32_t id = SubmitToCABI(L, "GET", url, "", nullptr, 30, 1, nullptr,
                               lua_isnoneornil(L, 2) ? 0 : 2);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

int L_HttpPost(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    size_t blen = 0;
    const char* body = luaL_optlstring(L, 2, "", &blen);

    std::string headers_json;
    int cb_index;
    if (lua_type(L, 3) == LUA_TSTRING) {
        // post(url, body, content_type, on_done)
        const char* ct = lua_tostring(L, 3);
        headers_json = std::string("{\"Content-Type\":\"") + ct + "\"}";
        cb_index = lua_isnoneornil(L, 4) ? 0 : 4;
    } else {
        headers_json = "{\"Content-Type\":\"application/json\"}";
        cb_index = lua_isnoneornil(L, 3) ? 0 : 3;
    }

    uint32_t id = SubmitToCABI(L, "POST", url, body, headers_json.c_str(),
                               30, 1, nullptr, cb_index);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

int L_HttpUpdate(lua_State* L) {
    dse_http_update();
    int n = PollAndDispatch(L);
    lua_pushinteger(L, n);
    return 1;
}

int L_HttpAvailable(lua_State* L) {
    lua_pushboolean(L, dse_http_available() ? 1 : 0);
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
    dse_http_update();
    PollAndDispatch(g_lua);
}

} // namespace dse::runtime::lua_binding

#endif // DSE_ENABLE_HTTP
