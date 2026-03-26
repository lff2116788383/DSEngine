# DSEngine 编辑器使用指南（Phase 1）

本文档对应当前主线编辑器实现（`Electron + React + Node-API(C++)`），覆盖运行、功能、发布流水线与常见排障。  
当前编辑器已与 Phase 1 Runtime 世界打通，不再依赖早期 mock 视口流程。

---

## 1. 环境准备

- Node.js（推荐 v16+）
- npm
- CMake（v3.15+）
- C++ 编译器
  - Windows: Visual Studio 2022
  - macOS: Xcode
  - Linux: GCC/Clang
- node-gyp（建议全局安装）

```bash
npm install -g node-gyp
```

---

## 2. 编译与启动

编辑器代码位于 `apps/editor/` 目录。

### 2.1 安装依赖

```bash
cd apps/editor
npm install
```

### 2.2 构建桥接模块（Node-API）

```bash
npx node-gyp configure
npx node-gyp build
```

成功后生成：`apps/editor/build/Release/dsengine_bridge.node`。

### 2.3 启动编辑器

```bash
npm start
```

该命令会先执行 `webpack`，再启动 Electron。

---

## 3. 主要功能

### 3.1 左侧：Scene + FileSystem

- **Scene**
  - 展示实体列表（Hierarchy）
  - 支持创建实体（`+ Node` / `+ Instance`）
  - 支持删除实体（每行 `×`）
- **FileSystem / 资源**
  - 导入纹理（`Import Texture`）
  - 浏览已导入纹理并选择
  - 材质实例管理
    - 创建材质实例（名称 + Shader Variant）
    - 刷新材质列表
    - 回放材质热更新事件（`Replay Material Hot Updates`）
    - 编辑材质参数（Tint / UV / Blend）
    - 应用材质到实体

### 3.2 中间：Viewport（实时视口）

- 通过桥接层持续拉取帧缓冲并渲染到 `<canvas>`
- 支持点击拾取（Picking）并同步选中实体
- 支持拖拽移动实体位置（Transform）
- 顶部显示帧源与尺寸（`source | width x height`）
- 视口叠加实时统计
  - 帧桥接：FrameId、Latency、Copy、Throughput、Dropped
  - 渲染统计：DrawCalls、MaxBatch、Sprites、Entities、Physics

### 3.3 右侧：Inspector / Node / Build

- **Inspector**
  - 展示实体基础信息与 Transform
  - 展示 SpriteRenderer 关键字段（Texture / Variant / Blend / Material）
  - 支持 `Apply To Entity`（贴图）与 `Apply Material`（材质）
- **Node**
  - 节点信号示例面板（用于后续节点工作流扩展）
- **Build**
  - 选择目标平台（`win64` / `mac` / `wasm`）
  - 一键导出（`Export Project`）
  - 后端调用 `apps/editor/scripts/build_pipeline.js`

---

## 4. 发布流水线与报告

编辑器 Build 页与命令行共用同一发布脚本：`apps/editor/scripts/build_pipeline.js`。

### 4.1 默认导出

```bash
npm run pipeline:export:win64
```

### 4.2 严格模式导出（含告警阈值）

```bash
npm run pipeline:export:win64:strict
```

严格模式默认会对迁移失败项做阈值拦截，超阈值即失败退出。

### 4.3 迁移报告产物

导出后在 `build_export_<target>/reports/` 可见：

- `scene_schema_migration_report.json`
- `scene_schema_migration_trend.json`
- `scene_schema_migration_report.html`
- `scene_schema_migration_dashboard.json`
- `scene_schema_migration_dashboard.md`

---

## 5. 开发者接口摘要

- 前端 UI：`apps/editor/src/components/EditorApp.tsx`
- IPC 暴露：`apps/editor/preload.js`
- IPC 主进程转发：`apps/editor/main.js`
- C++ 桥接：`apps/editor/src/bridge/dsengine_bridge.cpp`

前端通过 `window.electronAPI` 调用核心能力，包括：

- 世界与实体：`initEngine / getEntities / createEntity / deleteEntity / pickEntity / updateEntityTransform / tickEngine`
- 视口帧：`getFrameBuffer / getFrameInfo / getFrameBridgeStats / pushExternalFrame / clearExternalFrame`
- 资源与材质：`importTexture / listImportedTextures / applyTextureToEntity / listShaderVariants / createMaterialInstance / listMaterialInstances / updateMaterialInstance / applyMaterialToEntity`
- 材质热更新：`listMaterialHotUpdateEvents / replayMaterialHotUpdates / clearMaterialHotUpdateEvents`
- 发布：`buildProject`

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

- [ ] 在 `apps/editor/` 执行 `npm install`
- [ ] 在 `apps/editor/` 执行 `npx node-gyp configure && npx node-gyp build`
- [ ] 在 `apps/editor/` 执行 `npm start` 并成功打开编辑器窗口
- [ ] 视口左上角能看到 `source | width x height` 信息
- [ ] Output 面板无持续刷新的错误日志

### 6.2 实体与视口交互

- [ ] 左侧 Scene 列表能显示实体
- [ ] 点击实体后，Inspector 正确显示该实体信息
- [ ] 在视口点击实体，Scene 列表跟随切换选中项（Picking 生效）
- [ ] 在视口拖拽实体后，位置变化可见，且 Inspector 中 Position 同步变化
- [ ] 使用 `+ Node` 可创建实体，使用行内 `×` 可删除实体

### 6.3 纹理与材质链路

- [ ] `Import Texture` 可导入一张纹理并显示在 Imported Textures 列表
- [ ] 选中实体后，`Apply To Entity` 可将贴图应用到实体
- [ ] 可创建材质实例（名称 + Shader Variant）
- [ ] 材质列表刷新后能看到新建材质
- [ ] 编辑 Tint / UV / Blend 并执行 `Update Material Params` 后无报错
- [ ] 执行 `Apply Material` 后实体渲染结果有可见反馈
- [ ] `Replay Material Hot Updates` 执行成功并输出回放日志

### 6.4 统计与性能观测

- [ ] 视口叠加信息持续刷新：FrameId / Latency / Copy / BW / Drop
- [ ] 渲染统计可见：DrawCalls / MaxBatch / Sprites / Entities / Physics
- [ ] 执行实体增删和材质变更后，统计值变化符合预期

### 6.5 发布流水线与报告

- [ ] 在 Build 页选择 `win64` 并执行 `Export Project`
- [ ] 可在终端看到 `build_pipeline.js` 的构建输出
- [ ] 生成目录存在：`build_export_win64/Release/`
- [ ] 报告目录存在：`build_export_win64/reports/`
- [ ] 报告文件完整：
  - [ ] `scene_schema_migration_report.json`
  - [ ] `scene_schema_migration_trend.json`
  - [ ] `scene_schema_migration_report.html`
  - [ ] `scene_schema_migration_dashboard.json`
  - [ ] `scene_schema_migration_dashboard.md`

### 6.6 严格模式（可选）

- [ ] 执行 `npm run pipeline:export:win64:strict`
- [ ] 在阈值未超限时流程成功退出
- [ ] 若人为制造失败样例，流程会按阈值策略中断并输出告警

---

## 7. 常见问题

**Q1: 启动时报错找不到 `dsengine_bridge.node`？**  
A: 在 `apps/editor/` 下先执行 `npx node-gyp configure && npx node-gyp build`，确认 `apps/editor/build/Release/dsengine_bridge.node` 存在。

**Q2: 视口黑屏或无更新？**  
A: 先检查 `engine:init` 是否成功，再检查桥接接口是否能返回 `getFrameInfo/getFrameBuffer`。若运行时引擎进程未启动，`source` 可能停留在桥接路径或无有效帧。

**Q3: Build 点击后没有可执行产物？**  
A: 查看 Electron 主进程控制台中 `build_pipeline.js` 输出；确认 CMake 生成器、编译器和目标平台工具链可用。

**Q4: 材质改了但实体没变化？**  
A: 确认该实体已绑定对应 `material_instance_id`，并检查材质参数是否已 `Update Material Params` 后再 `Apply Material` 到实体。
