# Dark Soul Engine (DSEngine)

DSEngine 是一个面向现代化 2D/3D 演进的 C++ 引擎工程，当前主线已完成第一阶段 2D 基建落地（ECS、RHI 抽象、资源与编辑器联动）。

## 目录结构

- `src/`: 引擎核心 C++ 源码
- `script/`: 引擎提供给 Lua 的封装脚本接口
- `examples/`: 示例逻辑与样例入口
  - `examples/c++/main.cpp`: C++ 业务宿主入口
  - `examples/c++/`: C++ Demo 业务逻辑
  - `examples/lua/main.cpp`: Lua 业务宿主入口（C++ 启动器）
  - `examples/lua/`: Lua Demo 业务逻辑
- `data/`: 资源目录
- `depends/`: 第三方依赖
- `editor/`: Electron + React + Node-API 编辑器

## 当前引擎架构梳理（基于现有代码）

当前代码主线已收敛到 Phase1 单路径，结构性清理已完成。后续仅建议做增强型改造（可测试性、资源生命周期治理、性能工程化），不再需要大规模目录级重构。

### 主干模块（Phase1）

- `src/phase1/ecs`: ECS 数据层（`world` + `components_2d`）
- `src/phase1/systems`: 运行系统层（Transform/Camera/Physics/Sprite/UI/Animation/LuaScript 等）
- `src/phase1/rhi`: 渲染抽象层（`RhiDevice`/`CommandBuffer`/OpenGL 实现）
- `src/phase1/runtime`: 帧调度层（`Phase1FramePipeline`，驱动 Init/Update/FixedUpdate/Render）
- `src/phase1/asset`: 资源层（纹理/Shader/材质实例、同步与异步加载）
- `src/scene`: 场景序列化与回归样例（JSON 读写、schema 兼容迁移）

### 已清理的历史模块

- 已删除 `src/render_device`（旧渲染任务队列体系）
- 已删除 `src/app`（旧应用入口抽象）
- 已删除 `src/lua_binding`、`src/audio`、`src/math`、`src/data_structs` 中未接入 Phase1 的遗留实现
- 当前仓库以 Phase1 主线为唯一引擎执行路径

### 当前主循环与调用链

- 入口：`examples/c++/main.cpp` 与 `examples/lua/main.cpp`
- 引擎库：`DSEngine`（由 `src/` 产出）
- 调度：`Phase1FramePipeline::Init/Update/FixedUpdate/Render`
- 核心流程：
  - Update：资源主线程回调 + 业务逻辑 Update（Lua/Cpp 二选一）+ Transform + Camera
  - FixedUpdate：Box2D 步进与同步
  - Render：RenderGraph（scene_pass → ui_pass → composite_pass）

## 架构评估：是否合理、是否需要修改

### 结论

当前架构方向总体合理（ECS + 分系统 + 渲染抽象 + 脚本扩展），适合作为 2D 引擎 Phase1 基线；但组织层面存在明显“过渡态耦合”和若干会影响稳定性的结构问题，建议进行分阶段重构，而非推倒重来。

### 已具备的合理性

- 分层基本清晰：ECS/Systems/Runtime/Asset/RHI 具备可维护的职责边界
- 渲染流程已有 RenderPass 化雏形，便于继续演进到完整 RenderGraph
- 资源与场景链路已有回归样例，具备工程化演进基础

### 主要问题（当前剩余）

- 全局单例仍偏多：`Phase1AssetManager` 等仍是全局单例，测试隔离与多实例能力仍受限
- 场景世界与运行时世界仍未完全统一：`scene::Scene` 与运行时注入 World 生命周期尚未彻底打通
- 资源层线程模型仍需强化：当前异步加载采用“后台解码 + 主线程上传”，后续建议补充更明确的资源提交队列与状态机
- 渲染资源释放策略需继续完善：纹理/FBO/Program 生命周期管理仍需系统化回收机制

### 是否“有必要修改”

有必要，且建议继续“有限重构”。当前 P0 已基本完成，后续重点转向入口收敛、生命周期统一与可测试性增强。

建议按优先级推进（更新后）：

1. P1（架构清晰）
   - 统一场景与运行时 World 生命周期，减少双 World 状态漂移
   - 固化 Phase1 入口与模块边界，避免新增跨层耦合
2. P2（可扩展）
   - 继续推进去单例化：AssetManager 等模块支持可注入实例
   - 组件定义拆分（运行时句柄与纯数据组件分离，减少头文件膨胀）
3. P3（工程化）
   - 增加针对初始化失败路径、异步资源回调路径的回归测试
   - 补充渲染资源释放与泄漏巡检（纹理/FBO/Program 生命周期）

### 本轮改造完成标记（2026-03-22）

- [x] JobSystem 生命周期接入主流程（启动与退出）
- [x] JobSystem 健壮性修复（重复初始化保护、未初始化回退执行、停止态保护）
- [x] Physics2D 实体与 Box2D Body 用户数据映射修复
- [x] CommandBuffer 执行顺序修复为按录制顺序回放
- [x] Phase1 模块已移除 `legacy_render_bridge`，不再依赖旧渲染任务接口
- [x] Lua 启动、API注册与每帧更新已从 `frame_pipeline` 拆分到独立 runtime 模块
- [x] World 去单例化第一步：`Phase1FramePipeline` 已支持外部注入 World
- [x] World 去单例化第二步：main 主循环已显式注入运行时 World，LuaRuntime 启动前强校验 World 上下文
- [x] World 去单例化第三步：移除 `frame_pipeline` 的 World 单例兜底，未注入时初始化失败并显式报错
- [x] 初始化健壮性增强：`FramePipeline::Init` 改为 bool 返回，主流程失败可中止并清理
- [x] 渲染链路（Phase1 路径）已彻底去除对旧 `render_task_*` 的依赖，改为直接 OpenGL 提交
- [x] Phase1 构建路径已排除 `src/render_device/*`，仅保留独立 Phase1 渲染链路
- [x] 已删除与当前架构无关的历史目录与文件（app/render_device/lua_binding/audio/math/data_structs 等）
- [x] 主入口已清理为双宿主路径（`examples/c++/main.cpp` 与 `examples/lua/main.cpp`）
- [x] 新增显式退出清理链路：`Phase1FramePipeline::Shutdown` + `RhiDevice::Shutdown`
- [x] OpenGLRhiDevice 已补齐核心 GPU 资源释放（Program/VAO/VBO/EBO/Texture/FBO）
- [x] Scene 支持外部 World 绑定（可与 Runtime World 统一生命周期）
- [x] EngineRunConfig 支持注入 `world` 与 `asset_manager`，FramePipeline/LuaRuntime 支持资源上下文注入
- [x] OpenGLRhiDevice 新增资源台账日志（创建/释放统计与泄漏告警）
- [x] CMake 新增 SDK 安装导出（Targets 导出 + 头文件安装 + script Lua 接口安装）
- [x] Lua ScriptComponent 热重载增强：支持脚本文件变更自动重载与状态迁移（OnSerializeState/OnDeserializeState）
- [x] 资源异步观测增强：主线程回调队列高水位统计与拥塞告警

## 环境要求

### 引擎（C++）
- CMake 3.17+
- C++17 编译器
- Windows 推荐 Visual Studio 2022

### 编辑器（可选）
- Node.js 16+
- npm
- node-gyp

## 引擎编译运行流程（Windows）

在项目根目录执行：

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build_vs2022 --config Debug --target dse_engine
cmake --build build_vs2022 --config Debug --target DSEngine
cmake --build build_vs2022 --config Debug --target dse_example_lua
cmake --install build_vs2022 --config Debug --prefix install
```

不同构建模式都会带模式后缀，例如 Debug 产物为 `bin\DSEngine_debug.dll`，两个宿主可执行也会分别生成 `*_debug.exe`。

运行 C++ Demo：

```bash
bin\DSEngine_c++_debug.exe
```

运行 Lua Demo：

```bash
bin\DSEngine_lua_debug.exe
```

运行时会自动探测 `script/`、`examples/`、`data/` 路径。

默认 Lua 启动脚本为 `examples/lua/main.lua`。

## 编辑器编译运行流程（可选）

在 `editor/` 目录执行：

```bash
npm install
npx node-gyp configure
npx node-gyp build
npm start
```

更多编辑器使用说明见 [Editor_Usage_Guide.md](Editor_Usage_Guide.md)。

## 商业化发布流水线（P2-1/P2-2）

在 `editor/` 目录可执行：

```bash
npm run pipeline:export:win64
npm run pipeline:export:win64:strict
```

流水线会生成：
- `build_export_win64/reports/release_manifest.json`（发布包文件清单 + SHA256）
- `build_export_win64/reports/scene_schema_migration_dashboard.json`
- `build_export_win64/reports/quality_dashboard.json`
- `build_export_win64/reports/quality_dashboard.md`

严格模式会启用迁移失败阈值与材质回放回归门禁（失败即中断发布）。

## 常用开发方式

### CLion
1. 直接打开 `DSEngine` 根目录
2. 等待 CMake 加载
3. 选择目标并运行

### VS Code
1. 安装 C/C++ 与 CMake Tools 插件
2. 打开工程目录
3. 使用 CMake Tools 配置并构建运行

## 路线图

查看 [ROADMAP.md](ROADMAP.md) 了解后续演进计划。
