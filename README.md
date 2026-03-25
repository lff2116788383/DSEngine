# Dark Soul Engine (DSEngine)

DSEngine 是一个面向现代化 2D/3D 演进的 C++ 引擎工程，当前主线已完成第一阶段 2D 基建落地（ECS、RHI 抽象、资源与编辑器联动）。

## 目录结构

```
DSEngine
├─ apps                        应用层入口
│  ├─ runtime                  运行时宿主
│  │  ├─ cpp_host              C++ 运行时入口
│  │  └─ lua_host              Lua 运行时入口
│  ├─ editor                   Electron + React + Node-API 编辑器
│  └─ launcher                 项目启动器
├─ engine                      引擎内核与基础设施
│  ├─ core                     事件总线、任务系统、内存池
│  ├─ base                     调试、时间、补间等通用工具
│  ├─ platform                 屏幕等平台相关抽象
│  ├─ input                    输入系统
│  ├─ ecs                      ECS 数据层
│  ├─ assets                   资源管理
│  ├─ runtime                  帧调度与主循环
│  ├─ render                   渲染抽象与设备实现
│  ├─ scene                    场景对象与 Transform 系统
│  └─ scripting                Lua / C++ 业务运行时
├─ modules                     面向玩法域的模块层
│  └─ gameplay_2d              2D 摄像机、动画、粒子、UI、Tilemap、渲染等系统
├─ samples                     样例逻辑与样例脚本
│  ├─ cpp                      C++ Demo 业务样例
│  └─ lua                      Lua Demo 启动与场景样例
├─ script                      引擎提供给 Lua 的封装脚本接口
├─ data                        资源目录
└─ depends                     第三方依赖
```

## 新组织方式的优点

- 分层更清晰：`apps / engine / modules / samples` 将应用入口、引擎内核、玩法模块与样例回归拆开，降低跨层耦合
- 扩展更自然：`engine/core / base / platform / input` 明确基础设施边界，`modules/gameplay_2d` 也为后续 3D 或更多玩法模块预留了扩展位
- 工程化更友好：目录结构更接近产品化仓库形态，便于构建、打包、SDK 导出、CI 与多人协作

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
cmake --build build_vs2022 --config Debug --target DSEngine_example_cpp
cmake --build build_vs2022 --config Debug --target dse_example_lua
cmake --install build_vs2022 --config Debug --prefix install
```

不同构建模式都会带模式后缀，例如 Debug 产物为 `bin\DSEngine_debug.dll`，两个宿主可执行也会分别生成 `*_debug.exe`。

如果需要构建 editor，优先使用 `build_all.bat`，它会自动准备 node-gyp 所需的 VS2022 / Python 环境。

运行 C++ Demo：

```bash
bin\DSEngine_c++_debug.exe
```

运行 Lua Demo：

```bash
bin\DSEngine_lua_debug.exe
```

运行时会自动探测 `script/`、`samples/`、`data/` 路径。

默认 Lua 启动脚本为 `samples/lua/main.lua`。

## 编辑器编译运行流程（可选）

在 `apps/editor/` 目录执行：

```bash
npm install
npx node-gyp configure
npx node-gyp build
npm start
```

更多编辑器使用说明见 [Editor_Usage_Guide.md](Editor_Usage_Guide.md)。

## 发布流水线（可选）

在 `apps/editor/` 目录可执行：

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

查看 [DSEngine_新引擎演进步骤方案_2026版.md](DSEngine_新引擎演进步骤方案_2026版.md) 了解后续演进计划。


## 实现目标Demo
2D Demo：
1.ARPG 无双割草小游戏
2.AVG 视觉小游戏

3D Demo：
1.RPG 烽火与炊烟版经营模拟小游戏