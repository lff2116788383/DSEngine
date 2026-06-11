/**
 * @file http_smoke.cpp
 * @brief dse_http 冒烟：本地 HTTP 服务器回环验证异步 GET/POST + 主线程回调；
 *        外加一次可选 https 握手以验证 OpenSSL TLS 链接（网络不可达时不致命）。
 *
 * 退出码：0 = 本地回环 GET+POST 均通过；非 0 = 失败。
 */
#include "engine/http/http_client.h"

#include "ixwebsocket/IXHttpServer.h"
#include "ixwebsocket/IXNetSystem.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono_literals;

static void PumpUntil(dse::http::HttpClient& c, std::atomic<int>& done, int target, int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (done.load() < target && std::chrono::steady_clock::now() < deadline) {
        c.Poll();
        std::this_thread::sleep_for(20ms);
    }
    c.Poll();
}

int main() {
    ix::initNetSystem();

    const int port = 8731;
    ix::HttpServer server(port, "127.0.0.1");
    server.setOnConnectionCallback(
        [](ix::HttpRequestPtr req, std::shared_ptr<ix::ConnectionState>) -> ix::HttpResponsePtr {
            ix::WebSocketHttpHeaders headers;
            headers["Content-Type"] = "text/plain";
            std::string body;
            if (req->method == "POST") {
                body = "echo:" + req->body;
            } else {
                body = "hello-get:" + req->uri;
            }
            return std::make_shared<ix::HttpResponse>(
                200, "OK", ix::HttpErrorCode::Ok, headers, body);
        });

    auto started = server.listenAndStart();
    if (!started) {
        std::fprintf(stderr, "[smoke] local server failed to start on %d\n", port);
        return 1;
    }

    auto& client = dse::http::HttpClient::Instance();
    std::atomic<int> done{0};
    bool get_ok = false, post_ok = false;

    const std::string base = "http://127.0.0.1:" + std::to_string(port);

    client.Get(base + "/ping", {}, [&](const dse::http::Response& r) {
        get_ok = (r.status == 200 && r.body.find("hello-get:/ping") != std::string::npos);
        std::fprintf(stderr, "[GET ] status=%d body=\"%s\" err=\"%s\"\n",
                     r.status, r.body.c_str(), r.error.c_str());
        ++done;
    });

    client.Post(base + "/echo", {}, "{\"x\":1}", "application/json",
                [&](const dse::http::Response& r) {
        post_ok = (r.status == 200 && r.body == "echo:{\"x\":1}");
        std::fprintf(stderr, "[POST] status=%d body=\"%s\" err=\"%s\"\n",
                     r.status, r.body.c_str(), r.error.c_str());
        ++done;
    });

    PumpUntil(client, done, 2, 15000);

    // ── 可选：一次 https 握手，验证 OpenSSL TLS 编进且可用（不可达时仅告警）──
    bool tls_attempted = false, tls_ok = false;
    {
        std::atomic<int> tdone{0};
        dse::http::Request tls_req;
        tls_req.url         = "https://www.google.com/generate_204";
        tls_req.method      = "GET";
        tls_req.timeout_sec = 8;
        client.Send(tls_req, [&](const dse::http::Response& r) {
            tls_attempted = true;
            tls_ok = r.error.empty() && r.status > 0;
            std::fprintf(stderr, "[TLS ] status=%d err=\"%s\"\n", r.status, r.error.c_str());
            ++tdone;
        });
        PumpUntil(client, tdone, 1, 12000);
    }

    client.Wait();
    client.Poll();
    server.stop();
    ix::uninitNetSystem();

    std::fprintf(stderr, "GET_OK=%d POST_OK=%d TLS_ATTEMPTED=%d TLS_OK=%d\n",
                 get_ok, post_ok, tls_attempted, tls_ok);

    if (get_ok && post_ok) {
        std::fprintf(stderr, "HTTP_SMOKE_PASS%s\n",
                     tls_ok ? " (incl. https/TLS)" : " (local-only; TLS link compiled)");
        return 0;
    }
    std::fprintf(stderr, "HTTP_SMOKE_FAIL\n");
    return 2;
}
