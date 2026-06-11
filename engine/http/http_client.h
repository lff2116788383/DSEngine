/**
 * @file http_client.h
 * @brief DSEngine 通用异步 HTTP(S) 客户端（引擎层，零业务逻辑）。
 *
 * 设计要点：
 *  - 纯接口头，**不**包含任何第三方网络头（IXWebSocket/OpenSSL）；上层与脚本绑定
 *    只依赖本头，后端实现可替换。
 *  - 异步：请求在后台线程执行，完成回调在调用 Poll() 的线程（约定为主线程/脚本线程）
 *    触发，因此 Lua 回调天然在单线程上下文中执行，无需加锁。
 *  - 仅提供「发请求 + 拿响应」这一通用能力；DeepSeek / OpenAI 等具体协议、JSON 拼装与
 *    解析属于脚本/游戏层，不进引擎。
 */
#ifndef DSE_HTTP_CLIENT_H
#define DSE_HTTP_CLIENT_H

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace dse::http {

using RequestId = uint64_t;
inline constexpr RequestId kInvalidRequest = 0;

using Header  = std::pair<std::string, std::string>;
using Headers = std::vector<Header>;

struct Request {
    std::string url;                 // 完整 URL，含 scheme（http:// 或 https://）
    std::string method = "GET";      // GET/POST/PUT/DELETE/PATCH...
    Headers     headers;             // 额外请求头
    std::string body;                // 请求体（POST/PUT 等）
    int         timeout_sec = 30;    // 连接/传输超时（秒）
    bool        verify_peer = true;  // https 是否校验证书
    std::string ca_file;             // 自定义 CA bundle 路径；空=用默认（见实现）
};

struct Response {
    RequestId   id     = kInvalidRequest;
    int         status = 0;   // HTTP 状态码；传输层失败时为 0
    std::string body;
    std::string error;        // 非空表示失败（DNS/连接/TLS/超时等）
    Headers     headers;      // 响应头
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

using DoneCallback = std::function<void(const Response&)>;

/**
 * 进程级单例异步 HTTP 客户端。
 * 典型用法：Send/Get/Post 提交请求 → 每帧调用 Poll() 触发已完成回调。
 */
class HttpClient {
public:
    static HttpClient& Instance();

    /// 是否编译进了真实后端（DSE_ENABLE_HTTP）。OFF 构建下恒为 false 且所有请求立即失败。
    static bool Available();

    RequestId Send(const Request& req, DoneCallback on_done);
    RequestId Get(const std::string& url, const Headers& headers, DoneCallback on_done);
    RequestId Post(const std::string& url, const Headers& headers,
                   const std::string& body, const std::string& content_type,
                   DoneCallback on_done);

    /// 在调用线程上触发所有已完成请求的回调。返回本次触发的回调数。
    int  Poll();
    /// 阻塞直到所有在途请求结束（测试/退出时用）。
    void Wait();
    /// 停止后台线程并释放资源。
    void Shutdown();

private:
    HttpClient();
    ~HttpClient();
    HttpClient(const HttpClient&)            = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    struct Impl;
    Impl* m_impl;
};

} // namespace dse::http

#endif // DSE_HTTP_CLIENT_H
