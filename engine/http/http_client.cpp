/**
 * @file http_client.cpp
 * @brief HttpClient 实现。DSE_ENABLE_HTTP 开启时基于 IXWebSocket(IXHttpClient) + OpenSSL；
 *        关闭时退化为空实现（所有请求立即返回失败），保证零回归且本 TU 始终可编译。
 */
#include "engine/http/http_client.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#ifdef DSE_ENABLE_HTTP
#include "ixwebsocket/IXHttpClient.h"
#include "ixwebsocket/IXNetSystem.h"
#endif

namespace dse::http {

// ───────────────────────── 完成队列 + 后台线程公共状态 ─────────────────────────
struct HttpClient::Impl {
    std::mutex                mtx;        // 保护 completed / inflight
    std::deque<std::pair<Response, DoneCallback>> completed;
    std::atomic<uint64_t>     next_id{1};
    std::atomic<int>          inflight{0};
    std::condition_variable   idle_cv;    // inflight 归零通知

#ifdef DSE_ENABLE_HTTP
    std::atomic<bool>         net_ready{false};
    std::mutex                net_init_mtx;
    void EnsureNetSystem() {
        if (net_ready.load()) return;
        std::lock_guard<std::mutex> lk(net_init_mtx);
        if (!net_ready.load()) {
            ix::initNetSystem();
            net_ready.store(true);
        }
    }
#endif

    void PushCompleted(Response resp, DoneCallback cb) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            completed.emplace_back(std::move(resp), std::move(cb));
        }
        if (inflight.fetch_sub(1) == 1) {
            std::lock_guard<std::mutex> lk(mtx);
            idle_cv.notify_all();
        }
    }
};

HttpClient::HttpClient() : m_impl(new Impl()) {}

HttpClient::~HttpClient() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

HttpClient& HttpClient::Instance() {
    static HttpClient s_instance;
    return s_instance;
}

#ifdef DSE_ENABLE_HTTP

bool HttpClient::Available() { return true; }

RequestId HttpClient::Send(const Request& req, DoneCallback on_done) {
    Impl* impl = m_impl;
    impl->EnsureNetSystem();

    const RequestId id = impl->next_id.fetch_add(1);
    impl->inflight.fetch_add(1);

    // 每个请求一个独立 ix::HttpClient（单 socket），在分离线程上同步执行，
    // 完成后把结果塞进 completed 队列，由主线程 Poll() 取出触发回调。
    Request   r   = req;       // 拷贝进线程
    DoneCallback cb = std::move(on_done);

    std::thread worker([impl, id, r, cb]() mutable {
        ix::HttpClient client(/*async=*/false);

        ix::SocketTLSOptions tls;
        if (!r.verify_peer) {
            tls.caFile = "NONE";   // 跳过证书校验（不安全，仅本地/测试用）
        } else if (!r.ca_file.empty()) {
            tls.caFile = r.ca_file;
        } // 否则用 IXWebSocket 默认（SYSTEM）
        client.setTLSOptions(tls);

        ix::HttpRequestArgsPtr args = client.createRequest();
        args->connectTimeout  = r.timeout_sec;
        args->transferTimeout = r.timeout_sec;
        args->followRedirects = true;
        args->maxRedirects    = 5;
        args->compress        = false;  // 不请求 gzip，避免依赖 zlib 解压
        for (const auto& h : r.headers) {
            args->extraHeaders[h.first] = h.second;
        }

        ix::HttpResponsePtr ixr = client.request(r.url, r.method, r.body, args);

        Response resp;
        resp.id = id;
        if (ixr) {
            resp.status = ixr->statusCode;
            resp.body   = ixr->body;
            if (!ixr->errorMsg.empty() && ixr->statusCode == 0) {
                resp.error = ixr->errorMsg;
            }
            for (const auto& kv : ixr->headers) {
                resp.headers.emplace_back(kv.first, kv.second);
            }
        } else {
            resp.error = "null response";
        }
        impl->PushCompleted(std::move(resp), std::move(cb));
    });
    worker.detach();
    return id;
}

#else // !DSE_ENABLE_HTTP —— 空实现

bool HttpClient::Available() { return false; }

RequestId HttpClient::Send(const Request& req, DoneCallback on_done) {
    Impl* impl = m_impl;
    const RequestId id = impl->next_id.fetch_add(1);
    Response resp;
    resp.id    = id;
    resp.error = "http disabled (build without DSE_ENABLE_HTTP)";
    std::lock_guard<std::mutex> lk(impl->mtx);
    impl->completed.emplace_back(std::move(resp), std::move(on_done));
    (void)req;
    return id;
}

#endif // DSE_ENABLE_HTTP

RequestId HttpClient::Get(const std::string& url, const Headers& headers, DoneCallback on_done) {
    Request r;
    r.url     = url;
    r.method  = "GET";
    r.headers = headers;
    return Send(r, std::move(on_done));
}

RequestId HttpClient::Post(const std::string& url, const Headers& headers,
                           const std::string& body, const std::string& content_type,
                           DoneCallback on_done) {
    Request r;
    r.url     = url;
    r.method  = "POST";
    r.headers = headers;
    r.body    = body;
    if (!content_type.empty()) {
        bool has_ct = false;
        for (const auto& h : r.headers) {
            if (h.first.size() == 12) { // "Content-Type"
                std::string k = h.first;
                for (auto& c : k) c = static_cast<char>(::tolower(c));
                if (k == "content-type") { has_ct = true; break; }
            }
        }
        if (!has_ct) r.headers.emplace_back("Content-Type", content_type);
    }
    return Send(r, std::move(on_done));
}

int HttpClient::Poll() {
    std::deque<std::pair<Response, DoneCallback>> batch;
    {
        std::lock_guard<std::mutex> lk(m_impl->mtx);
        batch.swap(m_impl->completed);
    }
    int n = 0;
    for (auto& item : batch) {
        if (item.second) item.second(item.first);
        ++n;
    }
    return n;
}

void HttpClient::Wait() {
    std::unique_lock<std::mutex> lk(m_impl->mtx);
    m_impl->idle_cv.wait(lk, [this] { return m_impl->inflight.load() == 0; });
}

void HttpClient::Shutdown() {
    if (!m_impl) return;
    // 等待在途请求结束，避免分离线程在进程退出时访问已释放资源。
    Wait();
    Poll();
}

} // namespace dse::http
