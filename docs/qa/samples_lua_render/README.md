# samples/lua 三端渲染验证报告

> 日期：2026-06-14 ｜ 分支：`feature/engine-lib` ｜ 引擎版本：0.1.0-alpha
> 目的：在无 GPU 的 Windows Server 上以软件渲染驱动跑通 `samples/lua` 全部 demo，验证 OpenGL / D3D11 / Vulkan **三端**渲染与 Lua 脚本层可用性，并归档日志+截图供后续分析/验证/修复。

---

## 1. 测试流程（端到端步骤）

可照此完整复现本次验证。

### 步骤 0 — 构建运行时
```powershell
# 已构建可跳过；产物 bin\DSEngine_Game_release.exe
scripts\bootstrap_windows.ps1            # 配置工具链 + 子模块
cmake --build --preset windows-x64-release
```

### 步骤 1 — 部署三端软件渲染驱动（本机无独显）
| 后端 | 驱动 | 部署 |
|---|---|---|
| OpenGL | Mesa **llvmpipe**（GL 4.x） | `dse build --with-swgl` 或 `scripts\setup_swgl.ps1`，把 `opengl32.dll`+`libgallium_wgl.dll` 放入 `bin\`；运行时 `GALLIUM_DRIVER=llvmpipe` |
| D3D11 | **Microsoft Basic Render Driver** | 系统自带，无需部署 |
| Vulkan | Mesa **lavapipe**（软件 Vulkan） | 从 Mesa 包提取 `vulkan_lvp.dll`+`lvp_icd.x86_64.json` 放入 `bin\`；运行时 `VK_ICD_FILENAMES`/`VK_DRIVER_FILES` 指向该 ICD（**否则 `--rhi=vulkan` 会 `VK_ERROR_INCOMPATIBLE_DRIVER(-9)` 并回退 GL**） |

### 步骤 2 — 枚举 demo
- 注册式 demo（经 `samples/lua/main.lua` 的 `--demo=<name>` 派发）：`samples/lua/3d/*.lua`（60 个）+ 顶层 `phase1_2d_showcase`、`phase1_2d_physics_showcase`（2 个）= **62 个**。
- DSSL 材质示例（不在 `main.lua` 注册表，直接当入口脚本跑）：`samples/lua/dssl/{dssl_material_demo,demo_npr_light,demo_toon}.lua`（3 个）。
- 非 demo：`config.lua`/`main.lua`（基础设施）、`pipelines/custom_lite.lua`（管线定义，被引用）、`dssl/*.dssl`（着色器源，由 `dse_dssl_compiler` 编译）。

### 步骤 3 — 无头跑帧 + 截图机制（`engine/runtime/engine_app.cpp`）
| 环境变量 | 作用 |
|---|---|
| `DSE_MAX_FRAMES=N` | 跑满 N 帧自动退出 |
| `DSE_SCREENSHOT_FRAME=K` | 第 K 帧回读场景色 |
| `DSE_SCREENSHOT_PATH=<png>` | 截图输出路径 |
| `DSE_SCREENSHOT_TARGET=` | 空=场景色（默认）/ `scene` |

派发机制：`apps/standalone/main.cpp` 的 `--demo=<name>` → `_putenv_s("DSE_DEMO",...)` → `main.lua` 读取 `DSE_DEMO` → `require("3d.<name>")`。

### 步骤 4 — 批量运行（每端一遍）
用 `demo_harness.ps1`（本目录附带）对 62 个注册 demo 逐个跑 70 帧、第 55 帧截图、超时 100s，落 `logs/<demo>.log` + `shots/<demo>.png` + `summary.csv`：
```powershell
.\demo_harness.ps1 -Backend opengl -Frames 70 -ShotFrame 55 -TimeoutSec 100
.\demo_harness.ps1 -Backend d3d11  -Frames 70 -ShotFrame 55 -TimeoutSec 100
.\demo_harness.ps1 -Backend vulkan -Frames 70 -ShotFrame 55 -TimeoutSec 120
```
单个手动复现：
```powershell
$env:GALLIUM_DRIVER='llvmpipe'; $env:DSE_MAX_FRAMES='70'; $env:DSE_SCREENSHOT_FRAME='55'; $env:DSE_SCREENSHOT_PATH='shot.png'
# OpenGL / D3D11
./bin/DSEngine_Game_release.exe --demo=3d_lighting_showcase --script=samples/lua/main.lua --rhi=opengl
./bin/DSEngine_Game_release.exe --demo=3d_lighting_showcase --script=samples/lua/main.lua --rhi=d3d11
# Vulkan（先部署 lavapipe ICD 到 bin/）
$icd="$PWD/bin/lvp_icd.x86_64.json"; $env:VK_ICD_FILENAMES=$icd; $env:VK_DRIVER_FILES=$icd
./bin/DSEngine_Game_release.exe --demo=3d_lighting_showcase --script=samples/lua/main.lua --rhi=vulkan
```

### 步骤 5 — DSSL 示例（直接入口脚本，不经 main.lua）
```powershell
$env:DSE_MAX_FRAMES='60'; $env:DSE_SCREENSHOT_FRAME='45'; $env:DSE_SCREENSHOT_PATH='dssl.png'
./bin/DSEngine_Game_release.exe --script=samples/lua/dssl/dssl_material_demo.lua --rhi=opengl
```

### 步骤 6 — 重场景的软件 Vulkan 处理
lavapipe 纯软件极慢（全后处理管线 1280×720 单帧约 110s）。重场景降分辨率并把帧数调小补跑：
```powershell
... --rhi=vulkan --width=320 --height=180    # 配 DSE_MAX_FRAMES=5 / DSE_SCREENSHOT_FRAME=3
```

### 步骤 7 — 分析与汇总
- 日志归类：`Select-String '\[ERROR\]'`、`Shader compile error|X35\d\d`（D3D11 FXC）、`Failed to (read|open|capture)`、`Lua (Awake|Update) failed`、`avg_render_ms`（单帧耗时）。
- 出图判定：`summary.csv` 的 `shot` 列 + `DSE_SCREENSHOT_WRITTEN`。
- 拼图：`ffmpeg`（先把每张归一到 300×165 再 `tile=8x8`）生成 `montage_<backend>.png`。

---

## 2. 三端结果总览

### 注册式 demo（62 个，`--demo=` 派发）
| 后端 | 适配器 | 可渲染/出图 | 着色器编译失败 | 备注 |
|---|---|---|---|---|
| **OpenGL** (llvmpipe) | llvmpipe (LLVM 22.1.6, 256-bit) | **61 / 62** | 0 | 唯一未出图 = `3d_morph_target`（脚本 bug，非渲染问题） |
| **D3D11** (Basic Render) | Microsoft Basic Render Driver | **61 / 62** | 1 个内置 HLSL 着色器（全 demo 共有，非致命） | 同样仅 `3d_morph_target` 未出图 |
| **Vulkan** (lavapipe) | llvmpipe (LLVM 22.1.6) | **57 / 61** | 0 | 4 个最重后处理/大气 demo 在纯软件 Vulkan 下单帧 >100s，超时截不到帧（性能，非正确性） |

### DSSL 材质示例（3 个，直接入口脚本）
| demo | OpenGL | D3D11 | Vulkan | 备注 |
|---|---|---|---|---|
| `dssl_material_demo` | ✅ 15 PASS | ✅ 15 PASS | ✅ 15 PASS（低分辨率） | DSSL 材质系统端到端，三端均出图、自带断言全过 |
| `demo_npr_light` | ❌ 加载失败 | ❌ | ❌ | 脚本 bug（见 §3 A3），三端一致，与后端无关 |
| `demo_toon` | ❌ 加载失败 | ❌ | ❌ | 脚本 bug（见 §3 A4），三端一致，与后端无关 |

**结论：三端（OpenGL / D3D11 / Vulkan）均能初始化并渲染 Lua demo，Lua 脚本层正常加载执行；DSSL 材质链路三端亦验证通过。"三端可以使用"成立。**

---

## 3. 发现的问题（按归类，均非渲染管线本身缺陷）

### A. Sample 脚本 bug（与渲染后端无关，三端一致复现）

> **状态：A1–A4 均已修复**（本分支）。修复后 OpenGL/D3D11 复跑：4 个 demo 全部 `shot=1`、`[ERROR]=0`、无 `bootstrap FAILED`。

| # | demo | 位置 | 报错 | 影响 | 修复 |
|---|---|---|---|---|---|
| A1 | `3d_morph_target` | `samples/lua/3d/3d_morph_target.lua:71` | `bad argument #3 to 'morph_add_target' (table expected, got number)` | Awake 失败 → 场景未建立 → **三端均无法出图** | 已修复：`morph_add_target` 第 3 参改传每顶点 6 float（dpx,dpy,dpz,dnx,dny,dnz）的扁平 delta table（新增 `make_morph_deltas`/`morph_shapes`） |
| A2 | `3d_physics_triggers` | `samples/lua/3d/3d_physics_triggers.lua:206` | `bad argument #1 to 'poll_collision_event' (number expected, got no value)` | 每帧 Update 报错（日志刷 33 次）；场景仍正常渲染 | 已修复：`poll_collision_event(entity)` 传入探针实体并按 `has_event,other,is_trigger,is_enter` 接收返回 |
| A3 | `dssl/demo_npr_light` | `samples/lua/dssl/demo_npr_light.lua:5` | `module 'dse.ecs' not found`（用了 `require("dse.ecs")`） | 启动 load 失败，FramePipeline 初始化失败，**完全跑不起来** | 已修复：按现行 API 重写（`local ecs = dse.ecs`、内联 cube mesh、补相机/光照/地面、`apply_material(entity,id)` 修正参数顺序） |
| A4 | `dssl/demo_toon` | `samples/lua/dssl/demo_toon.lua:5` | `attempt to call a nil value (field 'get_world')` | 启动 load 失败，**完全跑不起来** | 已修复：弃用失效的 `dse.get_world()/get_scene()/world:*`，按现行 `dse.ecs`+`dssl.*` API 重写（toon_basic/metal/rim 三材质） |

### B. Sample 缺资源（仍能渲染其余场景）

| # | demo | 缺失资源 | 影响 |
|---|---|---|---|
| B1 | `3d_hair` | `models/cube.dmesh`（毛发载体网格） | 毛发不显示，其余正常（日志刷 280×2 次 load 失败） |
| B2 | `3d_animation_ik_layers` | `data/animation/minimal_rig/character.dmesh`、`character.dskel` | 骨骼/蒙皮模型不显示（日志刷 140 + 70 次） |
| B3 | `phase1_2d_showcase` | 少量 `mirror_assets/Resources/{map,ui,item}/*.png` | 精灵主体仍正常显示，个别贴图缺省 |

### C. 引擎侧（D3D11 着色器可移植性，非致命）

- **现象**：某内置 HLSL 着色器在 FXC 编译时报：
  ```
  warning X3570: gradient instruction used in a loop with varying iteration, attempting to unroll the loop   (行 ~92)
  error  X3511: unable to unroll loop, loop does not appear to terminate in a timely manner (523 iterations)  (行 ~145)
  ```
- **范围**：**所有** D3D11 demo 启动时各出现一次（同一个共享着色器），但**不影响主体场景渲染**（截图均正常）。
- **建议**：该循环内含纹理梯度采样（`Sample`/隐式 mip）。给循环加 `[unroll(n)]` 显式上界，或将循环内的 `Sample` 改为 `SampleLevel`/`SampleGrad`（显式 LOD/梯度）以消除 varying-iteration 下的梯度告警。GL / Vulkan(SPIR-V) 不受影响。

### D. 环境性能（非引擎缺陷）

- Vulkan 用 lavapipe **纯软件光栅**，全后处理管线（阴影/SSAO/SSR/体积雾/Bloom/TAA 等约 30+ pass）在 1280×720 下单帧约 **110s**。
- 以下 4 个最重 demo 即便降到 320×180 也无法在合理时间截到帧，故 Vulkan 出图数为 57/61：
  `3d_fog_atmosphere`、`3d_postprocess_effects`、`3d_postprocess_showcase`、`3d_render_quality_showcase`。
- 这些场景在 GL / D3D11 下均正常出图；且 Vulkan 已成功渲染其余 57 个 demo（含 `3d_textured_cube`/`3d_lighting_showcase`/`3d_shadow_showcase`/`3d_particles_showcase` 等）。属软件 Vulkan 性能限制，**非渲染正确性问题**。

---

## 4. 渲染正确性观察

- 几何、材质（含 PBR / DSSL）、点光/方向光、阴影、粒子、实例化、地形、Cornell-box 红绿墙、天空盒渐变、透明、物理塔/布料/破碎等均正常成像；**三端画面互相一致**（见各 `montage_<backend>.png` 与三端 `3d_textured_cube` / `dssl_dssl_material_demo` 对比）。
- 多数场景整体偏暗：demo 多用极简光照 + 软件渲染缺强环境光/IBL 所致，属预期表现，非渲染错误。

---

## 5. 产物索引（本目录）

```
docs/qa/samples_lua_render/
├── README.md                       # 本报告
├── demo_harness.ps1                # 本次批量运行脚本
├── montage_opengl.png              # OpenGL 全量出图拼图（8×8，61 demo）
├── montage_d3d11.png               # D3D11 全量出图拼图（61 demo）
├── montage_vulkan.png              # Vulkan 全量出图拼图（57 demo）
├── opengl/
│   ├── summary.csv                 # 逐 demo：exit / shot / [ERROR] 数 / 着色器失败数 / 适配器
│   ├── shots/<demo>.png            # 注册 demo 截图（第 55 帧）+ dssl_*.png（DSSL 示例）
│   └── logs/<demo>.log             # 逐 demo 完整 stdout+stderr 日志 + dssl_*.log
├── d3d11/  { summary.csv, shots/, logs/ }
└── vulkan/ { summary.csv, shots/, logs/ }   # 含 *_lowres.log（重场景的低分辨率补跑记录）
```

定位问题示例：
- A1：`*/logs/3d_morph_target.log` 搜 `Lua Awake failed`
- A3/A4：`opengl/logs/dssl_demo_npr_light.log.err` / `dssl_demo_toon.log.err`
- C：任一 `d3d11/logs/*.log` 搜 `Shader compile error` / `X3511`
- D：`vulkan/logs/3d_postprocess_showcase.log` 搜 `avg_render_ms`（单帧耗时）
