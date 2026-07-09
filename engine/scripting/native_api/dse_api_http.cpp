/**
 * @file dse_api_http.cpp
 * @brief DSEngine C ABI - HTTP — 异步完成队列模型（使用 HttpClient）
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#ifdef DSE_ENABLE_HTTP
#include "engine/http/http_client.h"
#include <unordered_map>
#include <cstring>
#endif

using namespace dse_api_internal;


#ifdef DSE_ENABLE_HTTP

struct HttpCompletion {
    uint32_t request_id;
    dse::http::Response response;
};
static std::vector<HttpCompletion> g_http_completed;
// 已完成响应缓存：poll 只上报一次 id，响应保留至 get_response 消费后再删除。
static std::unordered_map<uint32_t, dse::http::Response> g_http_responses;
static uint32_t g_http_next_id = 1;

extern "C" uint32_t dse_http_send(const char* method, const char* url, const char* body,
                                const char* headers_json, int timeout_sec, int verify_peer,
                                const char* ca_file) {
    auto* client = dse::core::ServiceLocator::Instance().Get<dse::http::HttpClient>();
    if (!client || !url) return 0;

    dse::http::Request req;
    req.url = url;
    req.method = method ? method : "GET";
    if (body) req.body = body;
    if (timeout_sec > 0) req.timeout_sec = timeout_sec;
    req.verify_peer = (verify_peer != 0);
    if (ca_file) req.ca_file = ca_file;

    // Parse simple JSON headers string {"K":"V",...}
    if (headers_json && headers_json[0]) {
        std::string hs(headers_json);
        size_t pos = 0;
        while ((pos = hs.find('"', pos)) != std::string::npos) {
            size_t k_end = hs.find('"', pos + 1);
            if (k_end == std::string::npos) break;
            std::string key = hs.substr(pos + 1, k_end - pos - 1);
            size_t colon = hs.find(':', k_end + 1);
            if (colon == std::string::npos) break;
            size_t v_start = hs.find('"', colon + 1);
            if (v_start == std::string::npos) break;
            size_t v_end = hs.find('"', v_start + 1);
            if (v_end == std::string::npos) break;
            std::string val = hs.substr(v_start + 1, v_end - v_start - 1);
            req.headers.emplace_back(key, val);
            pos = v_end + 1;
        }
    }

    uint32_t id = g_http_next_id++;
    client->Send(req, [id](const dse::http::Response& resp) {
        g_http_completed.push_back({id, resp});
        g_http_responses[id] = resp;
    });
    return id;
}

extern "C" int dse_http_poll(uint32_t* out_ids, int max_ids) {
    int count = std::min(static_cast<int>(g_http_completed.size()), max_ids);
    for (int i = 0; i < count; ++i) {
        out_ids[i] = g_http_completed[i].request_id;
    }
    g_http_completed.erase(g_http_completed.begin(), g_http_completed.begin() + count);
    return count;
}

extern "C" int dse_http_get_response(uint32_t request_id, int* out_status,
                                     char* out_body, int body_cap,
                                     char* out_error, int error_cap) {
    auto it = g_http_responses.find(request_id);
    if (it == g_http_responses.end()) {
        if (out_status) *out_status = 0;
        if (out_body && body_cap > 0) out_body[0] = '\0';
        if (out_error && error_cap > 0) out_error[0] = '\0';
        return 0;
    }
    const dse::http::Response& resp = it->second;
    if (out_status) *out_status = resp.status;
    if (out_body && body_cap > 0) {
        std::strncpy(out_body, resp.body.c_str(), static_cast<size_t>(body_cap) - 1);
        out_body[body_cap - 1] = '\0';
    }
    if (out_error && error_cap > 0) {
        std::strncpy(out_error, resp.error.c_str(), static_cast<size_t>(error_cap) - 1);
        out_error[error_cap - 1] = '\0';
    }
    g_http_responses.erase(it);  // 消费后释放，避免缓存无限增长
    return 1;
}

extern "C" void dse_http_update(void) {
    auto* client = dse::core::ServiceLocator::Instance().Get<dse::http::HttpClient>();
    if (client) client->Poll();
}

extern "C" int dse_http_available(void) {
    return dse::http::HttpClient::Available() ? 1 : 0;
}

#else

extern "C" uint32_t dse_http_send(const char*, const char*, const char*, const char*, int, int, const char*) { return 0; }
extern "C" int dse_http_poll(uint32_t*, int) { return 0; }
extern "C" int dse_http_get_response(uint32_t, int*, char*, int, char*, int) { return 0; }
extern "C" void dse_http_update(void) {}
extern "C" int dse_http_available(void) { return 0; }

#endif

