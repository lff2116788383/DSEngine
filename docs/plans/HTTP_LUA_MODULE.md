# 异步 HTTP(S) 客户端模块（`dse.http`）

> 目的：让 Lua 脚本能调用外部 REST API（如 DeepSeek、各类 AI/后端服务）做「智能 NPC」等业务，
> 而不阻塞游戏主循环。引擎**只**提供通用异步 HTTP 客户端；DeepSeek/JSON/对话管理等业务逻辑
> 全部放在脚本层（见 `examples/lua/`），引擎核心保持通用。

状态：✅ 已落地并验证（Windows 实跑：本地回环 GET/POST + 真实 https/TLS 握手 + Lua 绑定回调，均 PASS）。

---

## 1. 设计要点

- **为什么不用 `dse_net`(GNS)**：GNS 是游戏专用 UDP 可靠/非可靠传输（自定义协议），
  不是 HTTP 客户端，连不上 `https://` REST 接口。两者定位正交，互不依赖。
- **后端**：复用已在树的 `depends/IXWebSocket`（`IXHttpClient`）+ **OpenSSL** TLS。
- **异步模型**：每个请求在后台线程执行（IXWebSocket 同步请求），完成结果进**互斥队列**；
  `Poll()` 在主线程/脚本线程把结果取出并触发回调 → Lua 回调天然单线程安全。
- **特性开关**：`DSE_ENABLE_HTTP`（默认 **OFF**）。OFF 路径完全不引入 IXWebSocket/OpenSSL，
  现有构建零回归；引擎主目标不含 `engine/http/**`（编为独立 `dse_http` 静态库）。

## 2. C++ 结构

| 文件 | 作用 |
|------|------|
| `engine/http/http_client.h` | 公共 API（`Request`/`Response`/`HttpClient`），零三方头暴露（pimpl） |
| `engine/http/http_client.cpp` | `IXHttpClient` 封装 + 后台线程 + 互斥回调队列 + `Poll()` |
| `engine/scripting/lua/bindings/lua_binding_http.cpp` | 注册 `dse.http`；回调经 `luaL_ref` 存注册表，在 `Poll` 上下文 `pcall` |
| `cmake/CMakeLists.txt.http` | 条件接入 IXWebSocket(`USE_TLS`+OpenSSL) + 生成 `dse_http` + smoke |
| `tests/http/http_smoke.cpp` | C++ 层冒烟：本地回环 GET/POST + 可选 https 握手 |
| `tests/http/http_lua_smoke.cpp` | 无头 Lua 绑定冒烟：裸 `lua_State` 上跑脚本调 `dse.http` 验证回调 |

引擎 Tick（`lua_runtime.cpp` 的 `TickLuaRuntime`）每帧调用 `PumpHttp` → `HttpClient::Poll()`，
脚本无需手动轮询即可收到回调。

## 3. Lua API（`dse.http`）

```lua
-- 通用请求（推荐）
dse.http.request{
  url     = "https://api.example.com/v1/x",   -- 必填，含 scheme
  method  = "POST",                            -- 默认 "GET"
  headers = { ["Authorization"] = "Bearer …", ["Content-Type"] = "application/json" },
  body    = '{"k":"v"}',
  timeout = 30,            -- 秒，默认 30
  verify_peer = true,      -- https 是否校验证书，默认 true
  ca_file = "…/ca.pem",   -- 可选自定义 CA bundle
  on_done = function(resp) --[[ resp = {id,status,body,error,ok,headers={K=V}} ]] end,
} -- 返回 request_id

dse.http.get(url, on_done)                       -- 便捷 GET
dse.http.post(url, body [, content_type], on_done) -- 便捷 POST（默认 application/json）
dse.http.update()    -- 手动触发已完成回调（引擎每帧自动调用；独立脚本驱动时用）
dse.http.available() -- 是否编进真实后端（DSE_ENABLE_HTTP=ON）→ bool
```

回调在主线程/脚本线程触发，可安全操作 Lua 世界（创建对话气泡、改 ECS 组件等）。

## 4. 构建（Windows）——OpenSSL 自动预构建，无需手动安装

**一条命令即可**：`DSE_ENABLE_HTTP=ON` 时若 Windows 上找不到 OpenSSL，CMake 会自动调用
`scripts/build_windows_openssl.ps1` 预构建一次（幂等；之后 configure 秒过）：

```bat
cmake -S . -B build_http -G "Visual Studio 17 2022" -A x64 ^
  -DDSE_ENABLE_HTTP=ON -DDSE_ENABLE_3D=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5

cmake --build build_http --config Debug --target dse_http_smoke --parallel
cmake --build build_http --config Debug --target dse_http_lua_smoke --parallel
bin\dse_http_smoke.exe       :: 期望 HTTP_SMOKE_PASS, EXIT=0
bin\dse_http_lua_smoke.exe   :: 期望 HTTP_LUA_SMOKE_PASS, EXIT=0
```

或用一键脚本（预构 OpenSSL + 配 HTTP=ON + 构建 + 跑两个 smoke，和 `-WithNet` 对齐）：

```powershell
.\scripts\verify_windows_build.ps1 -WithHttp -HttpOnly
```

OpenSSL 相关开关 / 覆盖：
- `scripts/build_windows_openssl.ps1`：幂等预构脚本（默认装到 `C:\ossl-win64\install`；
  自动确保 Strawberry Perl、复用/下载 1.1.1w 源码、`VC-WIN64A no-asm no-shared` 构建）。
- `-DDSE_HTTP_OPENSSL_DIR=<dir>`：手动指定已有 OpenSSL 安装根（跳过自动预构）。
- `-DDSE_HTTP_AUTO_BUILD_OPENSSL=OFF`：关闭 CMake 自动预构（缺 OpenSSL 时直接报错）。
- 环境变量 `DSE_HTTP_OPENSSL_DIR` 同样被识别。

> ⚠️ OpenSSL 必须**不带** `no-deprecated` 构建（脚本已保证）：IXWebSocket 的
> `IXSocketOpenSSL.cpp` 用到 `OpenSSL_add_ssl_algorithms`/`SSL_load_error_strings`/
> `SSL_set_ecdh_auto` 等 1.0/1.1 兼容初始化符号，`no-deprecated` 会把它们裁掉导致编译失败。

## 5. 验证结果（本机实跑）

- `dse_http_smoke.exe`：`[GET]=200 hello-get`、`[POST]=200 echo`、`[TLS]=204`（真实 https 握手成功）→ **HTTP_SMOKE_PASS**。
- `dse_http_lua_smoke.exe`：Lua 脚本 `dse.http.request{...on_done}` 回调拿到 `status=200`、`body={"echo":{"q":42}}` → **HTTP_LUA_SMOKE_PASS**。
- 全引擎 `DSE_ENABLE_HTTP=ON` 构建：`DSEngine_lua_debug.exe` 链接通过（`dse.http` 绑定编入引擎）。
- **回归**：`DSE_ENABLE_HTTP=OFF`（默认）全引擎构建通过，零影响。

## 6. 脚本层示例（不进引擎核心）

- `examples/lua/json.lua` — 极简纯 Lua JSON 编/解码器（自测通过；生产建议换 lua-cjson）。
- `examples/lua/deepseek_npc.lua` — 用 `dse.http` + `json.lua` 调 DeepSeek `chat/completions`
  的智能 NPC 示例（多轮对话、异步回复、错误处理）。运行需自备 `DEEPSEEK_API_KEY`：

  ```bat
  set DEEPSEEK_API_KEY=sk-...
  bin\DSEngine_lua_debug.exe --script=examples/lua/deepseek_npc.lua
  ```
  （宿主须以 `DSE_ENABLE_HTTP=ON` 构建，否则 `dse.http` 不存在。）
