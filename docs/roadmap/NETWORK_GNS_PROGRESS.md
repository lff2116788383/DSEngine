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
| **Phase 2c** | **Windows 桌面构建 GNS + smoke** | **🔄 进行中** | 见 §3，依赖已预构建，CMake 接线待验证 |
| Phase 3 | Android(NDK arm64) 交叉编译 GNS (host protoc + arm64 OpenSSL) | ⏳ | 未开始 |
| Phase 4 | 固化 `engine/net/` 抽象层 (lanes/质量/事件) + 可选 C ABI | ⏳ | 未开始 |
| Phase 5 | 三端 verify `-WithNet` 回归全绿 | ⏳ | 未开始 |

子模块固定版本：GNS `v1.6.0`、protobuf `v3.21.12`(abseil 前最后一批)、libsodium `1.0.20-FINAL`。
GNS 只初始化顶层，**不要** init 它的 webrtc/abseil/vjson 子模块。

---

## 1. 已落地的代码结构（已提交）

```
engine/net/
  net_types.h            # 与后端无关的值类型 (ConnectionId/SendMode/Address/ConnQuality/MessageView)
  net_transport.h        # 纯接口 INetTransport + INetListener + 工厂 CreateGnsTransport()；零 GNS 耦合
  backends/gns/
    gns_transport.cpp    # 唯一 include <steam/...> 之处，封装 ISteamNetworkingSockets
tests/net/
  net_smoke.cpp          # 单进程 127.0.0.1 回环：握手→发 reliable+unreliable→校验收到；退出码 0=通过
cmake/CMakeLists.txt.gns # 按平台分流依赖；生成 dse_net 静态库 + dse_net_smoke
```

顶层 `CMakeLists.txt`：
- `option(DSE_ENABLE_NET ... OFF)`；ON 时 `include(cmake/CMakeLists.txt.gns)`。
- engine 源 glob **始终排除** `engine/net/**`（由 `dse_net` 独立目标编译），见 `list(FILTER engine_cpp EXCLUDE REGEX ".*engine/net/.*\\.cpp$")`。

`cmake/CMakeLists.txt.gns` 按平台分流（关键）：
- **Linux/macOS 桌面**：直接用**系统** protobuf + libsodium（apt 装好后 GNS `find_package` 自动定位）。✅ 已实测。
- **Windows 桌面**：libsodium（预构建，`-DDSE_NET_SODIUM_DIR=`）+ protobuf。⚠️ 见 §3 待办。
- **Android/iOS**：OpenSSL + 在树 protobuf(host protoc)。⏳ Phase 3。

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

## 3. Windows (Phase 2c) — 进行中，下一步恢复指引

### 3.1 已完成的依赖预构建（在仓库外，**不入库**）
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
  - 复现命令：
    ```
    cmake -S depends/protobuf -B build_protobuf -G "Visual Studio 17 2022" -A x64 \
      -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_MSVC_STATIC_RUNTIME=OFF \
      -Dprotobuf_WITH_ZLIB=OFF -Dprotobuf_BUILD_SHARED_LIBS=OFF \
      -DCMAKE_INSTALL_PREFIX=C:/Users/Administrator/dse_net_deps/protobuf
    cmake --build build_protobuf --config Release --target install
    cmake --build build_protobuf --config Debug   --target install
    ```

### 3.2 待办（下一次会话从这里继续）
1. **改 `cmake/CMakeLists.txt.gns` 的 Windows 分支**：当前是 `add_subdirectory(在树 protobuf)`，
   但 GNS 内部用的是 `find_package(Protobuf CONFIG)` + `protobuf_generate_cpp`，
   add_subdirectory 的目标**不会**被 find_package 发现。
   → 改为**预构建+安装**路线：不 add_subdirectory protobuf，改为把
   `C:/Users/Administrator/dse_net_deps/protobuf` 加入 `CMAKE_PREFIX_PATH`，让 GNS 自己 find_package 到。
   （libsodium 已经走 `find_package`，只需 `sodium_DIR` 指向预构建目录即可。）
2. **配置 + 构建 + 跑 smoke**（大致命令，待验证）：
   ```
   cmake -S . -B build_vs2022_net -G "Visual Studio 17 2022" -A x64 ^
     -DDSE_ENABLE_NET=ON ^
     -DDSE_NET_SODIUM_DIR=C:/Users/Administrator/dse_net_deps/sodium ^
     -DCMAKE_PREFIX_PATH=C:/Users/Administrator/dse_net_deps/protobuf
   cmake --build build_vs2022_net --target dse_net_smoke --config Debug
   bin\dse_net_smoke.exe   # 期望退出码 0
   ```
   预期需要清理的点：MSVC 运行库一致性(/MD)、protobuf 的 `Protobuf_USE_STATIC_LIBS`、
   GNS 的 `/wd` 警告、调试/发布配置下 protobuf lib 名(libprotobuf vs libprotobufd)。
3. **给 `scripts/verify_windows_build.ps1` 加 `-WithNet`**：自动预构建 libsodium + 安装 protobuf
   （或检测 `dse_net_deps` 是否已就绪）→ 配置 `DSE_ENABLE_NET=ON` → 构建并运行 `dse_net_smoke.exe`。
   对齐 Linux 的 `--with-net`。
4. 跑一遍 **OFF 路径回归**（默认 `DSE_ENABLE_NET=OFF`）确认 Windows 引擎构建零影响
   （`engine/net/**` 已被 glob 排除，OFF 不会编译网络源）。

---

## 4. 之后阶段（未开始）
- **Phase 3 Android**：装 NDK（本机 WSL 已有 r26d）；交叉编译 arm64 OpenSSL；用 host protoc；
  `-DDSE_ENABLE_NET=ON -DDSE_NET_OPENSSL_ROOT=<arm64 openssl> -DDSE_HOST_PROTOC=<host protoc>`。
  iOS 同理走 OpenSSL，但需 macOS+Xcode，本环境无法出包，留作后续。
- **Phase 4**：固化 `engine/net/` 抽象（lanes/优先级、质量指标事件、连接生命周期事件），
  可选给 Lua/C# 暴露 `dse_net_*` C ABI。
- **Phase 5**：三端 verify 脚本 `-WithNet`/`--with-net` 全绿回归，收尾。
