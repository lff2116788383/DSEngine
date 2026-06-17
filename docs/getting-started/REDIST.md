# 发布清单与 Redist 指南 / Distribution & Redistributable Guide

> 本文说明把 DSEngine 做的游戏（或 SDK）发到玩家/其他开发者机器上时，**需要随包携带哪些运行时文件**。
> This guide lists which runtime files must ship alongside a DSEngine game (or the SDK) so it runs on an end-user machine.

---

## 1. 两种发布形态 / Two distribution shapes

| 形态 | 产物 | 引擎链接方式 | 适用 |
| --- | --- | --- | --- |
| **游戏出包**（默认） | `dsengine_game[_release].exe`（重命名为你的游戏名）+ 资源 | **静态**（`DSE_BUILD_SHARED=OFF`，默认）——引擎与全部第三方库（Jolt / Lua / Box2D / EnTT / assimp …）已编入 exe | 把游戏发给玩家 |
| **SDK 分发** | `DSEngine.dll` + `.lib` 导入库 + 公共头 | **动态**（`DSE_BUILD_SHARED=ON`，SDK 打包脚本使用） | 把引擎发给其他开发者用 `find_package(DSEngine)` 集成 |

`dse build` / `dse dist` 默认走「游戏出包」静态形态，**exe 自包含**，第三方物理/脚本/资源库**不**单独出 DLL。

---

## 2. 游戏出包：必带文件 / Game build redistributables

用 `dumpbin /DEPENDENTS dsengine_game_release.exe` 实测，默认 Release 静态 exe 的运行时依赖如下：

### 2.1 必须确保存在（开发者需关注）

| 文件 | 来源 | 说明 |
| --- | --- | --- |
| `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`, `MSVCP140.dll` | **VC++ 运行库** | 通过 [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe) 安装；或随包拷贝这三个 DLL 到 exe 旁。 |
| `api-ms-win-crt-*.dll`（UCRT） | Windows | Windows 10/11 自带；老系统由 VC++ Redist / Windows Update 提供。 |

> 安装器形态（`dse dist --installer`，Inno Setup）建议直接内嵌 `vc_redist.x64.exe` 静默安装，免去玩家手动操作。

### 2.2 系统/驱动提供（通常无需随包）

| 文件 | 说明 |
| --- | --- |
| `d3d11.dll`, `dxgi.dll`, `D3DCOMPILER_47.dll` | DirectX 11 后端；Windows 自带。 |
| `opengl32.dll` | OpenGL 后端；显卡驱动提供（**软件渲染**见 §3）。 |
| `vulkan-1.dll` | Vulkan loader；现代显卡驱动 / Vulkan Runtime 提供。**缺失时引擎自动回退**到 D3D11 / OpenGL，可用 `--rhi=opengl\|d3d11` 或环境变量 `DSE_RHI_BACKEND` 强制。 |
| `dbghelp.dll`, `winmm.dll`, `gdi32.dll`, `user32.dll`, `shell32.dll`, `msimg32.dll`, `kernel32.dll` | Windows 系统库。 |

### 2.3 可选随包（按需）

| 文件 | 何时需要 | 来源 |
| --- | --- | --- |
| `opengl32.dll` + `libgallium_wgl.dll` + `dxil.dll`（软件 GL 三件套） | 目标机**无独显 / 无 GL 驱动**，用 `dse build --with-swgl` 随发软件 OpenGL（llvmpipe） | `scripts/setup_swgl.ps1` 部署；launch 脚本会设 `GALLIUM_DRIVER=llvmpipe` |
| `game.dsmanifest` | 双击 exe 即玩（记录入口脚本/窗口/品牌 splash） | `dse build` 自动写入 |
| `data/`（着色器等运行时资源）、`scripts/`、`scenes/`、`*.dpak` / `*.bun` | 始终（除非已打进加密包） | `dse build` / `dse pack` 产出 |

---

## 3. 第三方依赖 DLL 一览 / Third-party dependency DLLs

**默认配置（Jolt 物理 + 无网络）下，没有任何第三方库以 DLL 形式随包**——全部静态编入。仅当显式开启可选特性时才引入额外 DLL：

| 特性开关（默认） | 开启后新增依赖 | 备注 |
| --- | --- | --- |
| `DSE_ENABLE_JOLT=ON`（默认开） | 无（静态） | 默认物理后端。 |
| `DSE_ENABLE_PHYSX=OFF` | 视 PhysX 预编译库而定（可能 `PhysX_64.dll` 等） | 默认关；如启用请把 PhysX redist DLL 拷到 exe 旁。 |
| `DSE_ENABLE_HTTP=OFF` | OpenSSL（`libssl-*.dll` / `libcrypto-*.dll`，取决于 OpenSSL 构建为动态时） | 默认关；异步 HTTP(S) 客户端（IXWebSocket + OpenSSL）。ixwebsocket 本身静态编入。 |
| `DSE_ENABLE_VULKAN`（桌面默认开） | 无新增随包（loader 由驱动提供，见 §2.2） | shader 工具链静态编入。 |

---

## 4. SDK 分发清单 / SDK redistributables

`scripts/package_sdk.ps1`（`DSE_BUILD_SHARED=ON`）打出的 SDK 包内：

| 类别 | 内容 |
| --- | --- |
| 运行时 | `bin/DSEngine.dll`（消费者运行需随包；CMake `$<TARGET_RUNTIME_DLLS>` 实测**仅此一个**非系统 DLL——第三方库已静态编入 DLL 内） |
| 链接 | `lib/DSEngine.lib`（导入库）、`lib/cmake/DSEngine/*.cmake`（`find_package` 配置） |
| 头文件 | `include/DSEngine/engine/**`（排除 RHI 后端实现头）、`include/DSEngine/modules/**`、`include/DSEngine/third_party/{glm,glm_ext,entt,box2d}` |
| 脚本 | `share/DSEngine/script/*.lua` |

消费者侧仍需 §2.1 的 VC++ 运行库；其余系统/驱动 DLL 同 §2.2。完整端到端验证见 `scripts/verify_sdk.ps1` 与 `examples/sdk_consumer/`。

---

## 5. 一句话清单 / TL;DR

- **发游戏**：exe + `data/` + 脚本/场景（或 `.dpak`/`.bun`）+ `game.dsmanifest`；确保目标机有 **VC++ x64 运行库**。其余 DX/GL/Vulkan 由系统/驱动提供，引擎自动回退。无显卡时用 `--with-swgl`。
- **发 SDK**：`DSEngine.dll` + `.lib` + headers + cmake config；消费者同样只需 VC++ 运行库。
- **默认不随包任何第三方库 DLL**（Jolt/Lua/Box2D/assimp 等全静态）；仅 PhysX / HTTP 等可选特性开启时才需额外 redist。
