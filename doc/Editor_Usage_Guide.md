# DSEngine 编辑器使用指南（当前主线：editor_cpp）

> 状态：当前有效
>
> 当前仓库主线编辑器为 `apps/editor_cpp/`，采用 **C++ + Dear ImGui + GLFW + OpenGL** 实现，并直接链接 `dse_engine`。  
> 旧版 `Electron + React + Node-API` 编辑器方案已不再作为当前主线，若在其他规划文档中出现，应视为历史背景或演进记录。

本文档覆盖当前 `editor_cpp` 的构建、运行、主要能力与常见排障。

---

## 1. 环境准备

- CMake（v3.15+）
- C++ 编译器
  - Windows: Visual Studio 2022
  - macOS: Xcode
  - Linux: GCC/Clang
- OpenGL 开发环境
- GLFW 相关依赖由仓库内 `depends/` 提供

---

## 2. 编译与启动

编辑器代码位于 `apps/editor_cpp/` 目录，入口源码为 `apps/editor_cpp/src/main.cpp`。

### 2.1 生成工程

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=ON
```

> 注意：顶层 `CMakeLists.txt` 中 `DSE_BUILD_EDITOR` 默认值为 `OFF`。如果不显式传入 `-DDSE_BUILD_EDITOR=ON`，则不会生成 `dse_editor_cpp` 目标。

### 2.2 构建编辑器

```bash
cmake --build build_vs2022 --config Debug --target dse_editor_cpp
```

成功后会在 `bin/` 下生成编辑器可执行文件，例如：`bin/dsengine-editor.exe`。

如果当前 `build_vs2022/` 是旧缓存目录，请重新执行一次 2.1 的配置命令，再进行构建。

### 2.3 启动编辑器

```bash
bin\dsengine-editor.exe
```

---

## 3. 主要功能

### 3.1 左侧：Hierarchy / Scene

- **Scene**
  - 展示实体列表（Hierarchy）
  - 支持创建实体（`+ Node` / `+ Instance`）
  - 支持删除实体（每行 `×`）
- 支持基础场景对象浏览与编辑

### 3.2 中间：Viewport / Game View

- 支持编辑视口与游戏视图
- 支持实体选择、变换与实时预览
- 直接复用引擎渲染结果，不经过旧版桥接层

### 3.3 右侧：Inspector

- **Inspector**
  - 展示实体基础信息与 Transform
  - 可编辑基础组件数据
  - 为后续 Gizmo、材质和更多工具面板预留扩展空间

---

## 4. 当前范围说明

当前 `editor_cpp` 主线重点是：

- 直接集成运行时与渲染视口
- 基于 ImGui 构建原生编辑界面
- 为后续 2D/3D 场景编辑器扩展打基础

旧版 Electron 编辑器中的前端构建流水线、Node-API 桥接与导出脚本，不再属于当前主线说明范围。

---

## 5. 开发者入口摘要

- 编辑器入口：`apps/editor_cpp/src/main.cpp`
- UI 方案说明：`doc/Editor_UI_Architecture.md`
- 运行时宿主：`apps/runtime/`
- 引擎核心：`engine/`

---

## 6. QA 验收清单

建议按以下顺序执行，避免前置条件缺失导致误判。

### 6.0 结果记录模板（通过/失败/备注）

每条验收项可按下表记录，建议一项一行：

| 验收项 | 通过 | 失败 | 备注 |
|---|---|---|---|
| 示例：在 `apps/editor/` 执行 `npm install` | ☐ | ☐ |  |
| 示例：视口显示 `source | width x height` | ☐ | ☐ |  |
| 示例：`Export Project` 成功生成产物 | ☐ | ☐ |  |

复制模板（可重复粘贴）：

| 验收项 | 通过 | 失败 | 备注 |
|---|---|---|---|
|  | ☐ | ☐ |  |
|  | ☐ | ☐ |  |
|  | ☐ | ☐ |  |

### 6.1 启动与基础连通

- [ ] 在项目根目录执行 CMake 生成工程
- [ ] 成功构建 `dse_editor_cpp` 目标
- [ ] 成功启动 `bin/dsengine-editor.exe`
- [ ] 编辑器主窗口成功显示 DockSpace / Hierarchy / Inspector / Viewport
- [ ] 控制台无持续刷新的错误日志

### 6.2 实体与视口交互

- [ ] 左侧 Scene 列表能显示实体
- [ ] 点击实体后，Inspector 正确显示该实体信息
- [ ] 在视口点击实体，Scene 列表跟随切换选中项（Picking 生效）
- [ ] 在视口拖拽实体后，位置变化可见，且 Inspector 中 Position 同步变化
- [ ] 使用 `+ Node` 可创建实体，使用行内 `×` 可删除实体

### 6.3 基础编辑链路

- [ ] 可创建基础实体
- [ ] 可保存场景到本地文件
- [ ] 可从本地文件重新加载场景
- [ ] Sprite / Transform 等基础组件数据编辑后有可见反馈

### 6.4 视图与交互

- [ ] Viewport 可正常显示场景
- [ ] Game View 可正常显示运行结果
- [ ] 执行实体增删与变换后，界面反馈符合预期

### 6.5 后续扩展（非当前主线验收项）

- [ ] Gizmo 编辑能力补全
- [ ] 更完整的资源浏览器接入
- [ ] 与启动器、打包链路进一步联动

---

## 7. 常见问题

**Q1: 启动编辑器失败或窗口未出现？**  
A: 先确认已成功构建 `dse_editor_cpp`，并检查 OpenGL / GLFW 相关环境是否正常；Windows 下建议优先使用 Visual Studio 2022 的 x64 Debug 构建。

**Q2: 视口黑屏或无更新？**  
A: 先检查运行时初始化是否成功，以及 `dse_engine` 是否已正确链接；再确认场景中存在可渲染对象与有效相机。

**Q3: 无法保存或加载场景？**  
A: 检查目标路径是否可写，并确认场景 JSON 文件格式未被手工破坏。

**Q4: 旧版 Electron / Node-API 文档为什么与当前代码不一致？**  
A: 旧版方案已退出当前主线，当前仓库应以 `apps/editor_cpp/` 与 `apps/launcher_tauri/` 为准；历史方案仅作为演进背景参考。
