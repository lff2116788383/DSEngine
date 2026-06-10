# 网络层 (GNS) 集成进度 — 交接文档

> 给下一次会话用。**只记录已实测验证的事实**与精确的恢复步骤。
> 方案设计见 `NETWORK_GNS_INTEGRATION_PLAN.md`；本文件只讲“做到哪、怎么接着做”。
> 仓库约定：所有开发/提交/推送**直接在 `feature/engine-lib` 分支**，不开分支、不走 PR。

---

## 0. 当前状态速览

| 阶段 | 内容 | 状态 | 证据 |
|---|---|---|---|
| 选型/文档 | 选定 GNS + 方案文档 + GNS 子模块 v1.6.0 | ✅ | commit `c2003127` |
| Phase 1 | protobuf/libsodium 子模块 + `DSE_ENABLE_NET`(默认 OFF) + 三端零回归 | ✅ | commit `b492a793` |
| Phase 2a | WSL/Linux 独立构建 GNS 静态库 | ✅ | `libGameNetworkingSockets_s.a` 24.8MB |
| Phase 2b | `engine/net` 抽象 + GNS 后端 + Linux 回环 smoke + `verify_linux_build.sh --with-net` | ✅ | commit `1c8e36c7`，smoke EXIT=0 |
| **Phase 2c** | **Windows 桌面构建 GNS + smoke** | **✅** | 见 §3，`dse_net_smoke.exe` EXIT=0；`verify_windows_build.ps1 -WithNet` 已实测 |
| **Phase 3** | **Android(NDK arm64) 交叉编译 GNS (host protoc + arm64 OpenSSL)** | **✅** | 见 §3.5，`bin/dse_net_smoke`=arm64 ELF（编译+链接通过）；`scripts/build_android_net.sh` 已实测 PASS |
| **Phase 4** | **固化 `engine/net/` 抽象层 (lanes/质量/事件) + 可选 C ABI** | **✅** | 见 §3.6，`dse_net_smoke`(含 lane 回环) + `dse_net_capi_smoke` 两端 EXIT=0（Win 运行 / Android 编译+链接） |
| Phase 5 | 三端 verify `-WithNet` 回归全绿 | 🔄 | Linux ✅ / Windows ✅ / Android ✅（编译+链接口径） |

子模块固定版本：GNS `v1.6.0`、protobuf `v3.21.12`(abseil 前最后一批)、libsodium `1.0.20-FINAL`。
GNS 只初始化顶层，**不要** init 它的 webrtc/abseil/vjson 子模块。

---

## 1. 已落地的代码结构（已提交）

```
engine/net/
  net_types.h            # 与后端无关的值类型 (ConnectionId/SendMode/LaneId/LaneConfig/Address/ConnQuality/MessageView)
  net_transport.h        # 纯接口 INetTransport + INetListener + 工厂 CreateGnsTransport()；零 GNS 耦合（含 lanes/Flush）
  net_c_api.h            # 扁平 C ABI 声明 (dse_net_*)，供 Lua/C#/FFI 宿主调用；不暴露任何 C++/GNS 类型
  net_c_api.cpp          # C ABI 实现：包住 INetTransport，仅依赖 engine/net 抽象（不 include GNS 头）
  backends/gns/
    gns_transport.cpp    # 唯一 include <steam/...> 之处，封装 ISteamNetworkingSockets
tests/net/
  net_smoke.cpp          # 单进程 127.0.0.1 回环：握手→发 reliable+unreliable+lane1→校验收到；退出码 0=通过
  net_capi_smoke.cpp     # 同上但全程只用 dse_net_* C 接口（模拟 Lua/C# 宿主）；退出码 0=通过
cmake/CMakeLists.txt.gns # 按平台分流依赖；生成 dse_net 静态库 + dse_net_smoke + dse_net_capi_smoke
```

顶层 `CMakeLists.txt`：
- `option(DSE_ENABLE_NET ... OFF)`；ON 时 `include(cmake/CMakeLists.txt.gns)`。
- engine 源 glob **始终排除** `engine/net/**`（由 `dse_net` 独立目标编译），见 `list(FILTER engine_cpp EXCLUDE REGEX ".*engine/net/.*\\.cpp$")`。

`cmake/CMakeLists.txt.gns` 按平台分流（关键）：
- **Linux/macOS 桌面**：直接用**系统** protobuf + libsodium（apt 装好后 GNS `find_package` 自动定位）。✅ 已实测。
- **Windows 桌面**：libsodium + protobuf **均走「预构建 + find_package」**（`-DDSE_NET_SODIUM_DIR=` / `-DDSE_NET_PROTOBUF_DIR=`）。✅ 已实测。
- **Android/iOS**：OpenSSL + **预构建 arm64 protobuf**(host protoc 生成 .proto)。✅ Android 已实测（见 §3.5）；iOS 同理但需 macOS+Xcode。

---

## 2. Linux 怎么跑（已验证，可复现）

```bash
# 依赖（一次性）：
sudo apt-get install -y libsodium-dev libprotobuf-dev protobuf-compiler \
     libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
# 端到端验证（构建引擎 + 网络层 + 跑回环 smoke）：
WITH_NET=1 BUILD_DIR=$HOME/dse_verify_net bash scripts/verify_linux_build.sh --with-net
# 期望：末尾 “网络 smoke: 通过”，EXIT=0；smoke 打印 reliable+unreliable 回环成功。
```

---

## 3. Windows (Phase 2c) — ✅ 已完成（可一键复现）

> 一键：`powershell -ExecutionPolicy Bypass -File scripts\verify_windows_build.ps1 -WithNet`
> （仅验证网络层、跳过引擎构建：追加 `-NetOnly`）。缺依赖时脚本会自动预构建 libsodium + protobuf 到
> `%USERPROFILE%\dse_net_deps`（可用 `-NetDepsDir` 改）。已实测 `dse_net_smoke.exe` EXIT=0。

### 3.1 依赖预构建（在仓库外，**不入库**；脚本会自动完成）
- **libsodium**：用自带 MSVC 解决方案构建（`StaticRelease|x64` + `StaticDebug|x64`），
  并整理成 GNS `Findsodium.cmake` 期望的布局，落地在：
  `C:\Users\Administrator\dse_net_deps\sodium\`
  - `include\sodium.h` …
  - `x64\{Debug,Release}\{v143,v144}\static\libsodium.lib`
  - 注：本机 MSVC = 19.44（`_MSC_VER` 1944），Findsodium 算出工具集后缀为 **v144**；
    libsodium.sln 实际输出到 v143 目录，故 v143/v144 都放了一份。
  - 复现命令：
    ```
    MSBuild depends\libsodium\builds\msvc\vs2022\libsodium.sln /p:Configuration=StaticRelease /p:Platform=x64
    MSBuild ... /p:Configuration=StaticDebug /p:Platform=x64
    # 产物在 depends\libsodium\bin\x64\{Release,Debug}\v143\static\libsodium.lib
    ```
- **protobuf v3.21.12**：MSVC 静态库、`/MD` 运行库（`protobuf_MSVC_STATIC_RUNTIME=OFF`，与引擎一致）、
  关 tests/zlib，Release+Debug 都 install 到：
  `C:\Users\Administrator\dse_net_deps\protobuf\`（含 `lib\libprotobuf.lib`/`libprotobufd.lib`、
  `bin\protoc.exe`、`cmake\protobuf-config.cmake`）。
  - **关键**：必须带上与引擎一致的 CRT 宏，否则与在树编译的 `.pb.cc`/GNS 目标链接时报
    `LNK2038: _CRT_STDIO_ISO_WIDE_SPECIFIERS 0 vs 1`（顶层 `CMakeLists.txt` 全局
    `add_compile_definitions(_CRT_STDIO_ISO_WIDE_SPECIFIERS=1 ...)`）。
  - 复现命令：
    ```
    set CRT=/D_CRT_STDIO_ISO_WIDE_SPECIFIERS=1 /D_CRT_NONSTDC_NO_WARNINGS=1 /D_CRT_DECLARE_NONSTDC_NAMES=1
    cmake -S depends/protobuf -B build_protobuf -G "Visual Studio 17 2022" -A x64 \
      -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_MSVC_STATIC_RUNTIME=OFF \
      -Dprotobuf_WITH_ZLIB=OFF -Dprotobuf_BUILD_SHARED_LIBS=OFF \
      -DCMAKE_INSTALL_PREFIX=C:/Users/Administrator/dse_net_deps/protobuf \
      -DCMAKE_CXX_FLAGS="%CRT%" -DCMAKE_C_FLAGS="%CRT%"
    cmake --build build_protobuf --config Release --target install
    cmake --build build_protobuf --config Debug   --target install
    ```

### 3.2 已做的接线改动（已提交、已实测）
1. **`cmake/CMakeLists.txt.gns` 的 Windows 分支**：原先是 `add_subdirectory(在树 protobuf)`，
   但 GNS 内部用的是 `find_package(Protobuf QUIET CONFIG)`，add_subdirectory 的目标（局部作用域）
   **不会**被它发现。
   → 已改为**预构建+安装**路线：不再 add_subdirectory protobuf，改为
   `set(Protobuf_USE_STATIC_LIBS ON)` + 当传入 `DSE_NET_PROTOBUF_DIR` 时
   `list(PREPEND CMAKE_PREFIX_PATH "${DSE_NET_PROTOBUF_DIR}")`，让随后的 GNS `add_subdirectory`
   继承该前缀、自行 `find_package(Protobuf CONFIG)` 命中预构建安装目录。
   （libsodium 仍走 `find_package`，`sodium_DIR` 指向预构建目录即可。）
2. **配置 + 构建 + 跑 smoke**（已实测 EXIT=0）：
   ```
   cmake -S . -B build_vs2022_net -G "Visual Studio 17 2022" -A x64 ^
     -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_ENABLE_3D=OFF ^
     -DDSE_ENABLE_NET=ON ^
     -DDSE_NET_SODIUM_DIR=C:/Users/Administrator/dse_net_deps/sodium ^
     -DDSE_NET_PROTOBUF_DIR=C:/Users/Administrator/dse_net_deps/protobuf ^
     -DCMAKE_POLICY_VERSION_MINIMUM=3.5
   cmake --build build_vs2022_net --target dse_net_smoke --config Debug
   bin\dse_net_smoke.exe   # 实测退出码 0：reliable + unreliable 回环都收到
   ```
   - 已清理：protobuf 的 CRT 宏一致性（见 §3.1，修掉 LNK2038）。
   - 残留告警（不影响功能、smoke 通过）：`LNK4098 LIBCMTD 冲突` —— libsodium 自带 `.sln`
     的 Static{Release,Debug} 用 `/MT(d)`，与引擎 `/MD` 混链。后续若要彻底干净，可让
     libsodium 也按 `/MD` 出静态库。
3. **`scripts/verify_windows_build.ps1` 已加 `-WithNet` / `-NetOnly` / `-NetDepsDir`**：
   缺依赖时自动预构建 libsodium + 安装 protobuf（含上面 CRT 宏）→ 配置 `DSE_ENABLE_NET=ON`
   → 构建并运行 `dse_net_smoke.exe`，对齐 Linux 的 `--with-net`。已实测「复用」与「从零预构建」两条路径都 EXIT=0。
4. **OFF 路径回归**：默认 `DSE_ENABLE_NET=OFF` 时 `build_vs2022` 引擎构建零影响（实测
   `DSEngine_debug.lib` 正常产出）；`engine/net/**` 被 glob 排除，OFF 不编译网络源，
   §3 的接线全部在 `WIN32` 且仅 NET=ON 时 include，对 OFF 无作用。

---

## 3.5 Android (Phase 3) — ✅ 已完成（可一键复现）

> 一键（Git bash，Windows 主机 + NDK windows-x86_64 工具链）：
> `bash scripts/build_android_net.sh`
> 已实测产出 `bin/dse_net_smoke` = `ELF 64-bit ... ARM aarch64`（编译+链接通过；
> 真机运行留作后续，与现有 Android 里程碑口径一致）。脚本自动完成 OpenSSL/protobuf
> 预构建（已存在则跳过）+ 引擎配置 + 构建。

### 3.5.1 依赖（仓库外，**不入库**；脚本会自动构建，已存在则跳过）
- **NDK**：`r26d`（本环境 `C:\Android\android-ndk-r26d`，windows-x86_64 工具链）。
  另需 `ninja`（`choco install ninja`）。
- **arm64 OpenSSL `1.1.1w`（静态）** → `C:\Android\ossl-android-arm64`（`lib\libcrypto.a`/`libssl.a` + `include\`）。
  - 走 OpenSSL 自带 `Configure android-arm64`（**Perl 流程**）。**坑**：Git 自带精简 perl 缺
    `Pod::Usage`，`configdata.pm` 顶层 `use Pod::Usage` 编译期必触发 → 脚本在源码树
    （`.` 在 `@INC`）放一个最小 `Pod/Usage.pm` 桩绕过（不影响产物）。
  - NDK clang.exe 原生跑在 Windows；OpenSSL 的 unix Makefile 在 **Git bash** 下用
    NDK 的 `aarch64-linux-android24-clang`（无扩展名 bash 包装脚本）即可编译/链接。
  - 复现：`Configure android-arm64 -D__ANDROID_API__=24 no-shared no-tests no-engine no-asm`
    → `make -j` → `make install_sw`。（注：WSL1 无法 exec NDK 的 linux clang ELF，故走 Windows 主机。）
- **arm64 protobuf `v3.21.12`（静态 install + config）** → `C:\Android\protobuf-android-arm64`
  （`lib\libprotobuf.a` + `lib\cmake\protobuf\protobuf-config.cmake`）。
  - NDK 工具链 + `-Dprotobuf_BUILD_PROTOC_BINARIES=OFF -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_WITH_ZLIB=OFF`。
    （runtime 的 well-known types `.pb.cc` 源码自带，构建不需要 protoc。）
- **host protoc**：复用 Phase 2c 的 `…\dse_net_deps\protobuf\bin\protoc.exe`（须与在树
  `depends/protobuf` 版本一致，仅用于生成 GNS 的 `.proto`）。

### 3.5.2 接线改动（已提交、已实测）
1. **`cmake/CMakeLists.txt.gns` 的 Android/iOS 分支**（与 Windows 一致走「预构建 + find_package(CONFIG)」）：
   - OpenSSL：NDK 把 `find_*` 限制在 sysroot（`CMAKE_FIND_ROOT_PATH` ONLY），sysroot 外的预构建
     OpenSSL 不会被 `find_package` 发现 → 把 `DSE_NET_OPENSSL_ROOT` 追加进 `CMAKE_FIND_ROOT_PATH`
     并**显式**给出 `OPENSSL_INCLUDE_DIR`/`OPENSSL_CRYPTO_LIBRARY`/`OPENSSL_SSL_LIBRARY`。
   - protobuf：**不再** `add_subdirectory(在树 protobuf)`（GNS `src/CMakeLists.txt` 优先
     `find_package(Protobuf QUIET CONFIG)`，在树目标不被发现）→ 改为要求 `-DDSE_NET_PROTOBUF_DIR=<arm64 安装目录>`，
     `list(PREPEND CMAKE_PREFIX_PATH)` + `list(APPEND CMAKE_FIND_ROOT_PATH)` 让 CONFIG 命中。
   - protoc 目标：预构建 protobuf 关了 protoc（arm64 protoc 主机跑不了），但 GNS 的
     `protobuf_generate_cpp` 在 CONFIG 模式会引用 `protobuf::protoc` 目标 → 先用 host protoc
     造一个 GLOBAL 的 imported `protobuf::protoc`，`protobuf-config` 里的 `if(NOT TARGET …)` 守卫会复用它。
2. **GNS `add_subdirectory` 前把 `CMAKE_SYSTEM_NAME` 临时映射为 `Linux`**：GNS v1.6.0 的 OS
   判定链只认 Linux/Darwin/BSD/Windows，`Android` 会 `FATAL_ERROR "Could not identify your
   target operating system"`；Android 本质是 Linux（posix/epoll），crypto 已强制 OpenSSL，与处理器无关。
3. **GNS 源码两处 Android(Bionic) 兼容补丁**（`cmake/patches/gns-android.patch`，Android 配置时**幂等自动应用**到子模块工作树，
   不改子模块指针）：
   - `tier1/netadr.cpp`：Bionic 的 `INADDR_LOOPBACK/INADDR_BROADCAST` 是 `unsigned long`(8B)，
     触发 `DWordSwap` 的 `sizeof==4` 静态断言 → 强转 `(uint32)`。
   - `tier0/dbg.cpp`：`Plat_IsInDebugSession` 缺 Android 分支（`IsAndroid()!=IsLinux()`）→ `#error "HALP"`；
     Android 有 `/proc`，复用 Linux 实现（`#elif IsLinux() || IsAndroid()`）。
4. **NET=OFF Android 回归**：`DSE_ENABLE_NET=OFF` 的 Android 配置零影响（实测 configure EXIT=0，
   无 GNS/dse_net 目标）；§3.5 全部接线仅在 `ANDROID/IOS` 且 NET=ON 时 include。

### 3.5.3 关键命令（脚本已封装）
```bash
# Git bash；NDK + ninja 在 PATH
bash scripts/build_android_net.sh
# 末尾期望：PASS，bin/dse_net_smoke = ELF 64-bit ... ARM aarch64
```

---

## 3.6 抽象层固化 + C ABI (Phase 4) — ✅ 已完成

固化 `engine/net/` 抽象，补齐 lanes/质量/事件，并新增可选的扁平 C ABI（供 Lua/C#）。
上层（gameplay/脚本）始终只依赖 `engine/net` 头，**不 include 任何 GNS 头**。

### 3.6.1 抽象层变更（已提交、已实测）
- **lanes（防队头阻塞）**：`net_types.h` 增 `LaneId`/`kDefaultLane` + `LaneConfig{priorities, weights}`；
  `INetTransport::ConfigureLanes(conn, cfg)` → GNS `ConfigureConnectionLanes`；
  `Send(..., LaneId lane = 0)`：lane 0 走 `SendMessageToConnection`，非 0 走 `AllocateMessage`+`SendMessages`(设 `m_idxLane`)；
  `INetListener::OnMessage(conn, msg, LaneId lane)` 回传发送方所用通道（取 `m_idxLane`）。
- **Nagle 控制**：`INetTransport::Flush(conn)` → `FlushMessagesOnConnection`（低延迟场景）。
- **连接生命周期事件**：`OnConnecting(conn, const Address& peer)` 现回传对端地址（取 `m_info.m_addrRemote`）。
- **质量事件**：沿用 `GetQuality`（`GetConnectionRealTimeStatus`）。

### 3.6.2 C ABI（`dse_net_*`，可选）
- `net_c_api.h`：`extern "C"` 扁平接口，不透明句柄 `dse_net_transport*` + `dse_net_conn`；
  事件经 `dse_net_callbacks` 回调结构体派发（`on_connecting/connected/closed/message`，带 `user` 透传）。
- 覆盖：create/destroy/init/shutdown、listen/connect/close、configure_lanes、send(mode+lane)/flush、poll、get_quality。
- `net_c_api.cpp` 只依赖 `engine/net` 抽象，编入 `dse_net` 静态库（与 `gns_transport.cpp` 一起，但本身不碰 GNS 头）。

### 3.6.3 验证（已实测）
- **Windows（运行）**：`dse_net_smoke.exe` EXIT=0 —— 收到 reliable/unreliable（lane 0）+ **lane 1** 消息（`OnMessage` 回传 lane=1）；
  `OnConnecting` 打印 `peer=127.0.0.1:<port>`。`dse_net_capi_smoke.exe` EXIT=0 —— 全程仅用 `dse_net_*` C 接口完成回环。
- **Android（编译+链接）**：`bin/dse_net_smoke`、`bin/dse_net_capi_smoke` 均为 `ELF 64-bit ... ARM aarch64`（NDK arm64-v8a 交叉编译通过）。
- **回归**：`DSE_ENABLE_NET=OFF` 三端构建零影响（抽象层与 C ABI 均在 `dse_net` 目标内，OFF 时不参与）。

---

## 4. 之后阶段（未开始）
- **Phase 5**：三端 verify 脚本 `-WithNet`/`--with-net` 全绿回归收尾（Linux ✅ / Windows ✅ / Android ✅ 编译+链接）。
- **后续（可选）**：P2P/ICE（开 WebRTC 子模块）、复制层/快照-delta/客户端预测/AOI（玩法级网络）；iOS arm64（复用 Android 套路，需 macOS+Xcode）。
