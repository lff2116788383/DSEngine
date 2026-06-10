# DSEngine 网络层集成方案 — GameNetworkingSockets (GNS)

> 目标：为 DSEngine 引入一个**高性能、跨平台（Windows / Linux / Android）的异步网络底座**，
> 一步到位支持 FPS、大型多人模拟经营等品类的传输需求（可靠 + 非可靠、内置加密、拥塞控制）。
> 选型结论：**Valve GameNetworkingSockets (GNS)**。
>
> 本文档只以**代码与上游构建系统**为准（已实测 clone 上游 `v1.6.0-3`，并核对其 `CMakeLists.txt`）。

---

## 0. 选型结论与理由（已拍板）

| 维度 | GNS | ENet（备选） |
|---|---|---|
| 可靠 + 非可靠（同一连接） | ✅ 消息级 API，自动分片重组 | ⚠️ 包级，通道较朴素 |
| 内置加密/认证 | ✅ AES-GCM + curve25519 每连接密钥 | ❌ 需自叠 DTLS |
| 拥塞控制/带宽估计 | ✅ 精细（劣质移动网更稳） | ⚠️ 基础自适应节流 |
| 通道/优先级（lanes，防队头阻塞） | ✅ | ⚠️ 基础通道 |
| P2P / NAT 穿透（ICE）+ 中继 | ✅（本期先关） | ❌ 自建 |
| 连接质量指标 / MTU 探测 | ✅ 丰富 | ⚠️ 少 |
| 大规模实战 | ✅ 全 Steam / CS / Dota | ⚠️ 中小规模为主 |
| 集成成本（尤其 Android） | ⚠️ 重（protobuf + 加密后端） | ✅ 极轻 |

**关键事实：** 传输库只负责"把字节可靠/及时送达"。**预测 / 复制 / AOI 兴趣管理**这些"大型"难点都要我们在
`engine/net` 之上自己实现——换哪个库都一样。选 GNS 是因为它把**传输层本身**做到了一步到位。

---

## 1. 依赖拓扑与"按平台选择加密后端"的关键简化

GNS 的硬依赖（核对自上游 `BUILDING.md` 与 `CMakeLists.txt`）：

- **CMake ≥ 3.10、C++11**（DSEngine 用 C++20，满足）
- **Google protobuf ≥ 2.6.1**（构建期需在 **host** 上跑 `protoc` 生成 `*.pb.cc`；运行期链接 protobuf-lite/runtime）
- **一个加密后端（AES/SHA256）**，三选一：
  - `OpenSSL` ≥ 1.1.1
  - `libsodium` —— **⚠️ 仅 x86/x86_64**（`CMakeLists.txt:135-138` 对非 x86 直接 `FATAL_ERROR`，因其 AES 实现依赖 AES-NI）
  - `BCrypt` —— **仅 Windows**
- **WebRTC/ICE（P2P）** —— 上游 submodule（abseil + webrtc + vjson），**本期关闭**（`ENABLE_ICE=OFF`、`USE_STEAMWEBRTC=OFF`），不初始化这些子模块。

### 关键简化：加密后端按平台分流

| 平台 | CPU | `USE_CRYPTO` | `USE_CRYPTO25519` | 说明 |
|---|---|---|---|---|
| Windows x64 | x86_64 | **libsodium** | libsodium | 避免在 Windows 上自建 OpenSSL（需 Perl/NASM，痛点大） |
| Linux x64 (WSL) | x86_64 | **libsodium** | libsodium | apt 装 libsodium 或 in-tree 构建，轻量 |
| Android | arm64-v8a | **OpenSSL** | OpenSSL 或 Reference | arm64 不能用 libsodium AES，**必须** OpenSSL；需交叉编译 |
| iOS（后续） | arm64 | **OpenSSL** | OpenSSL 或 Reference | 与 Android 同理；复用同套交叉编译路径；**需 macOS+Xcode 主机** |

> 这条分流把"桌面两端"的复杂度降到最低（libsodium 体量小、好构建），
> **唯一的硬骨头集中在 移动端（Android/iOS）的 OpenSSL + protobuf arm64 交叉编译**。

### 依赖获取方式（与本仓库 vendored-submodule 习惯一致）

新增三个子模块到 `depends/`：

| 子模块 | 选定版本 | 备注 |
|---|---|---|
| `depends/GameNetworkingSockets` | **`v1.6.0`**（已加并 pin 到该 tag，`2cb93a0`） | 仅顶层，不初始化其 `src/external/*` |
| `depends/protobuf` | **v3.21.12** | 最后一批**不强制 abseil** 的版本，大幅简化交叉编译；满足 GNS ≥2.6.1 |
| `depends/libsodium` | stable | 仅桌面用 |

OpenSSL 不进 submodule：Android 走 **NDK 交叉编译脚本**产出 `libcrypto.a/libssl.a`（见 §4），
桌面不需要 OpenSSL（用 libsodium）。

---

## 2. 总体架构：薄抽象层 `engine/net/`

gameplay / 脚本**绝不直接** include GNS 头。所有上层只依赖 `engine/net` 的纯接口，
GNS 作为可替换后端（日后若需换 ENet / 加 P2P，只动这一层）。

```
engine/net/
├─ net_types.h          // ConnectionId, NetResult, SendFlags(Reliable/Unreliable/NoNagle), LaneId
├─ net_transport.h      // 抽象接口 INetTransport（纯虚，无 GNS 依赖）
├─ net_connection.h     // 连接句柄 + 状态/质量指标快照
├─ net_message.h        // 收发消息（零拷贝 view + 拥有式 buffer）
├─ net_events.h         // 事件：Connecting/Connected/ClosedByPeer/ProblemDetectedLocally
└─ backends/
   └─ gns/
      ├─ gns_transport.h
      └─ gns_transport.cpp   // 唯一 include <steam/steamnetworkingsockets.h> 的地方
```

### `INetTransport` 接口草案（伪代码，最终以代码为准）

```cpp
namespace dse::net {

enum class SendFlags : uint32_t { Reliable=1, Unreliable=2, NoNagle=4, NoDelay=8 };

struct ConnQuality {            // 来自 GNS 的 GetConnectionRealTimeStatus
    float  ping_ms;
    float  packet_loss;        // 0..1
    float  out_bytes_per_sec;
    float  in_bytes_per_sec;
    int    pending_reliable;
};

class INetListener {           // 上层实现，接收事件回调
public:
    virtual void OnConnecting(ConnectionId, const NetAddress&) {}
    virtual void OnConnected(ConnectionId) {}
    virtual void OnClosed(ConnectionId, NetCloseReason) {}
    virtual void OnMessage(ConnectionId, const MessageView&, LaneId) {}
};

class INetTransport {
public:
    virtual ~INetTransport() = default;

    // 生命周期
    virtual bool Init(const NetConfig&) = 0;     // 内部 GameNetworkingSockets_Init
    virtual void Shutdown() = 0;

    // 服务端 / 客户端
    virtual ConnectionId Listen(uint16_t port) = 0;
    virtual ConnectionId Connect(const NetAddress&) = 0;
    virtual void         Close(ConnectionId, NetCloseReason = {}) = 0;

    // 收发（消息级；reliable/unreliable 走同一连接）
    virtual NetResult Send(ConnectionId, const void* data, size_t len,
                           SendFlags, LaneId = 0) = 0;

    // 每帧泵：派发事件 + 收消息到 listener（GNS RunCallbacks + ReceiveMessages）
    virtual void Poll(INetListener&) = 0;

    // 诊断
    virtual bool GetQuality(ConnectionId, ConnQuality& out) = 0;
};

INetTransport* CreateGnsTransport();   // 工厂；后端可替换
} // namespace dse::net
```

### GNS 后端实现要点
- `Init`：`GameNetworkingSockets_Init(nullptr, errMsg)`；`Shutdown`：`GameNetworkingSockets_Kill()`。
- 用 `ISteamNetworkingSockets`（`SteamNetworkingSockets()` 单例）：
  - 服务端 `CreateListenSocketIP` + `CreatePollGroup`；客户端 `ConnectByIPAddress`。
  - 连接状态用 `SteamNetConnectionStatusChangedCallback_t`（`RunCallbacks` 派发）。
  - 收消息 `ReceiveMessagesOnPollGroup` / `...OnConnection`；发消息 `SendMessageToConnection`
    （`k_nSteamNetworkingSend_Reliable / _Unreliable`）。
  - lanes：`ConfigureConnectionLanes` 做优先级/防队头阻塞。
  - 质量：`GetConnectionRealTimeStatus`。
- 静态链接：链 `GameNetworkingSockets::static`（即 `GameNetworkingSockets_s`，
  它对外 `INTERFACE` 定义了 `STEAMNETWORKINGSOCKETS_STATIC_LINK`，无需我们手加宏）。

---

## 3. CMake 集成（默认 OFF，不触动现有三端构建）

新增顶层选项：

```cmake
option(DSE_ENABLE_NET "Enable networking (GameNetworkingSockets backend)" OFF)
```

`DSE_ENABLE_NET=ON` 时（放进 `cmake/CMakeLists.txt.gns` 保持仓库风格）：

```cmake
# 1) 加密后端按平台分流
if(ANDROID)
    set(USE_CRYPTO      "OpenSSL"   CACHE STRING "" FORCE)
    set(USE_CRYPTO25519 "OpenSSL"   CACHE STRING "" FORCE)   # 或 Reference
    # OpenSSL_ROOT 指向 §4 交叉编译产物
    set(OPENSSL_ROOT_DIR "${DSE_ANDROID_OPENSSL_DIR}" CACHE PATH "" FORCE)
else()
    set(USE_CRYPTO      "libsodium" CACHE STRING "" FORCE)
    set(USE_CRYPTO25519 "libsodium" CACHE STRING "" FORCE)
    add_subdirectory(depends/libsodium ...)
endif()

# 2) protobuf（host protoc + target runtime）
#    Android：protoc 用 host 预构建，仅为 arm64 构建 runtime
set(protobuf_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ${_build_protoc} CACHE BOOL "" FORCE)
add_subdirectory(depends/protobuf ...)
if(ANDROID)
    set(Protobuf_PROTOC_EXECUTABLE "${DSE_HOST_PROTOC}" CACHE FILEPATH "" FORCE)
endif()

# 3) GNS 本体
set(BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIB ON  CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(ENABLE_ICE       OFF CACHE BOOL "" FORCE)   # 本期不做 P2P
set(USE_STEAMWEBRTC  OFF CACHE BOOL "" FORCE)
add_subdirectory(depends/GameNetworkingSockets ...)

# 4) 引擎网络模块
target_sources(dse_engine PRIVATE engine/net/...)         # 或独立 dse_net 目标
target_link_libraries(dse_engine PUBLIC GameNetworkingSockets::static)
target_compile_definitions(dse_engine PUBLIC DSE_NET_ENABLED=1)
```

> 上层代码用 `#if DSE_NET_ENABLED` 守卫，`OFF` 时引擎完全不含网络符号 → **现有三端绿色构建零影响**。

---

## 4. Android arm64 交叉编译（唯一硬骨头）

### 4a. protobuf
- **host protoc**：先用本机（Windows/Linux）构建 `protoc`（桌面流程顺带产出），
  路径喂给 Android 配置的 `Protobuf_PROTOC_EXECUTABLE`。
- **target runtime**：用 NDK 工具链只构建 `libprotobuf-lite`/`libprotobuf`（arm64），
  `protobuf_BUILD_PROTOC_BINARIES=OFF`。v3.21.12 不依赖 abseil，避免连锁。

### 4b. OpenSSL（arm64-v8a，NDK）
标准流程（写成 `scripts/build_android_openssl.sh`，可复用、可进 blueprint）：

```bash
export ANDROID_NDK_ROOT=$ANDROID_NDK_HOME
export PATH=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/<host>/bin:$PATH
cd openssl-1.1.1w
./Configure android-arm64 -D__ANDROID_API__=24 no-shared no-tests \
    --prefix=$OUT/android-arm64
make -j && make install_sw     # 产出 libcrypto.a / libssl.a + include
```
产物目录 → CMake 的 `OPENSSL_ROOT_DIR`（即 §3 的 `DSE_ANDROID_OPENSSL_DIR`）。

### 4c. 接入既有 Android 构建
- 复用现有 `apps/android_host` + `verify_android_apk.ps1` 链路。
- `dse_android_host.so` 链入 GNS（静态）→ 需要同时链 `libcrypto.a/libssl.a`、protobuf(arm64) runtime。
- 注意：NDK r26 默认 16KB page、`c++_static` STL 一致性、`-fPIC`。

---

## 5. 冒烟测试（loopback，三端一致）

`tests/net/net_smoke.cpp`（仅 `DSE_ENABLE_NET=ON` 编译）：

1. `INetTransport::Init`
2. 服务端 `Listen(127.0.0.1:27015)`，客户端 `Connect`
3. 等待双方 `OnConnected`（驱动 `Poll`）
4. 客户端发 1 条 **Reliable** + 1 条 **Unreliable**；断言服务端 `OnMessage` 收到、内容一致
5. 查 `GetQuality` 返回合理 ping
6. `Close` + `Shutdown`，进程干净退出（exit 0）

> 桌面直接跑该可执行；Android 先只验证**编译+链接**（与现有 Android 里程碑口径一致），
> 真机运行留作后续。

---

## 6. verify 脚本回归

- 现有 `verify_windows_build.ps1 / verify_linux_build.sh / verify_android_apk.ps1`
  **默认 `DSE_ENABLE_NET=OFF`，行为不变，必须保持全绿**。
- 各加一个可选开关（`-WithNet` / `DSE_ENABLE_NET=ON`）：
  - 桌面：额外构建 `dse_net` + 跑 `net_smoke`（断言 exit 0）。
  - Android：额外构建带 GNS 的 host `.so`，验证链接通过。

---

## 7. 分阶段里程碑与交付物

| Phase | 内容 | 交付物 / 验收 |
|---|---|---|
| **0** | 选型 + 本方案文档 | 本文档（✅） |
| **1** | 加 `depends/{protobuf, libsodium}` 子模块（GNS 已加）+ `DSE_ENABLE_NET` 选项（默认 OFF）+ `cmake/CMakeLists.txt.gns` | 现有三端构建仍全绿（OFF 路径零变化） |
| **2** | 桌面 Win+Linux：`ON` 构建 GNS(libsodium)+protobuf + `engine/net` GNS 后端 + loopback smoke | 两端 `net_smoke` exit 0（可靠/非可靠回环通过） |
| **3** | Android arm64：host protoc + arm64 OpenSSL + arm64 protobuf + GNS，链入 host `.so` | Android 带 GNS 链接通过；APK 可打包 |
| **4** | 固化 `engine/net` 抽象（lanes/质量/事件）+ 可选 `dse_net_*` C ABI（供 Lua/C#） | 接口稳定，上层不依赖 GNS 头 |
| **5** | 三端 verify 脚本集成 `-WithNet` + 回归全绿 + 提交推送 `feature/engine-lib` | CI/本地三端绿，含可选网络验证 |
| **后续** | （可选）P2P/ICE（开 WebRTC submodule）、复制层 / 快照-delta / 客户端预测 / AOI | 玩法级网络（独立工程） |
| **后续** | **iOS arm64**：复用 Android 的 OpenSSL+protobuf 交叉编译套路（三元组换 iOS），需 macOS+Xcode 构建机 | iOS 上 GNS 链接通过 |

### iOS 说明（已评估，对当前设计零结构影响）
- GNS 官方支持 iOS；iOS arm64 与 Android 同样**必须用 OpenSSL**（libsodium AES 仅 x86）。
- `engine/net/` 抽象层**完全不变**，iOS 只是新增一个 OpenSSL/protobuf 的交叉编译目标。
- **前提**：iOS 产物必须在 **macOS + Xcode** 上构建；当前 Windows 环境无法产出 iOS 包，故 iOS
  不纳入本期 Win/Linux/Android 三端的实测范围，留待有 mac 构建机时按 Android 同套路接入。
- 渲染走 Metal/GLES，与网络层无关，不产生额外耦合。

---

## 8. 风险登记与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| Android OpenSSL 交叉编译踩坑 | 阻塞 Phase 3 | 用成熟 `Configure android-arm64` 流程；脚本化；失败先单独验证 libcrypto |
| protobuf 版本牵出 abseil | 交叉编译复杂度爆炸 | **pin v3.21.12**（abseil 前最后一批） |
| host/target protoc 版本不一致 | 生成代码 ABI 不匹配 | host 与 target 用**同一** protobuf 版本 |
| GNS 默认想拉 WebRTC submodule | clone/构建变重 | `ENABLE_ICE=OFF` + `USE_STEAMWEBRTC=OFF`，不 init `src/external/*` |
| 链接体量/构建时长上升 | 迭代变慢 | 默认 OFF；静态库；仅 `dse_net` 受影响 |
| MSVC 对 protobuf 的告警 | 噪声/视警告为错 | GNS 已内置 `/wd4146 /wd4244 /wd4251 /wd4267` 抑制 |
| 跨平台共享 `bin/` 与生成物污染（已知坑） | 与现有 shader 一样 | 沿用 verify 脚本的自愈策略 |

---

## 9. 立即可执行的下一步（Phase 1）

1. `git submodule add` protobuf(v3.21.12) 与 libsodium 到 `depends/`（GNS 已加好）。
2. 顶层 `CMakeLists.txt` 加 `option(DSE_ENABLE_NET OFF)` + `include(cmake/CMakeLists.txt.gns)`（仅 ON 时）。
3. 跑一遍现有 `verify_windows_build.ps1`/`verify_linux_build.sh` 确认 **OFF 路径零回归**。
4. 再进入 Phase 2 桌面打通。

> 全程在 `feature/engine-lib` 分支提交推送，不开新分支/不走 PR（按既定偏好）。
