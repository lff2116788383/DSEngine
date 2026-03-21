# DSEngine 编辑器使用指南 (Electron + React 架构)

本文档介绍了如何编译、运行和使用 DSEngine 的新版可视化编辑器。新版编辑器抛弃了传统的 ImGui，采用了现代化的 `Electron + React + Node-API(C++)` 架构，支持所见即所得 (WYSIWYG) 的场景编辑和多平台一键打包。

---

## 1. 环境准备

在编译和运行编辑器之前，请确保您的开发环境已安装以下工具：

*   **Node.js** (推荐 v16 或更高版本): 用于运行 Electron 和 React 的打包工具。
*   **npm** (随 Node.js 安装): 用于管理前端和 Node-API 依赖。
*   **CMake** (v3.15+): 用于构建底层 C++ 引擎代码和 Node-API 桥接层。
*   **C++ 编译器**:
    *   Windows: Visual Studio 2022 (MSVC)
    *   macOS: Xcode (Clang)
    *   Linux: GCC/Clang
*   **node-gyp**: 用于编译 C++ Node-API 原生模块。通常可以通过 `npm install -g node-gyp` 全局安装。

---

## 2. 编译与运行

编辑器的所有代码均位于项目根目录的 `editor/` 文件夹下。

### 第一步：安装依赖

打开终端，进入 `editor` 目录，安装所需的 npm 依赖包：

```bash
cd editor
npm install
```
*此步骤会自动安装 Electron, React, Webpack 以及 `node-addon-api` 等依赖。*

### 第二步：编译 C++ 引擎桥接模块 (Node-API)

编辑器需要通过 Node-API 与 DSEngine 的 C++ 核心进行通信。在 `editor` 目录下执行以下命令编译 C++ 原生模块 (`.node` 文件)：

```bash
npx node-gyp configure
npx node-gyp build
```
*编译成功后，会在 `editor/build/Release/` 目录下生成 `dsengine_bridge.node` 文件。*

### 第三步：启动编辑器

前端 React 代码使用 Webpack 进行打包，随后由 Electron 加载。您可以通过以下命令一键打包并启动编辑器：

```bash
npm start
```
*这会先执行 `webpack` 编译 TypeScript/React 代码到 `dist/bundle.js`，然后启动 Electron 窗口。*

---

## 3. 编辑器界面与功能使用

启动后，您将看到一个现代化的深色主题界面，主要包含以下几个区域：

### 3.1 层次结构面板 (Hierarchy) - 左侧
*   **功能**：显示当前场景中所有的实体 (Entity) 列表。
*   **交互**：点击列表中的任意项，可以选中该实体。选中后，实体会高亮显示，并且可以在中间的视口中对其进行操作。

### 3.2 场景视口 (Viewport) - 中间
*   **功能**：通过高性能的 C++ 内存共享机制，以 60FPS 实时渲染游戏画面。目前以色块代表实体。
*   **所见即所得 (WYSIWYG)**：
    *   **点选 (Picking)**：直接在画布上点击代表实体的色块，即可在左侧 Hierarchy 中自动选中该实体。
    *   **拖拽 (Gizmo)**：按住鼠标左键并拖拽，可以实时改变实体在世界空间中的坐标 (`TransformComponent.position`)。松开鼠标后，C++ 底层的 ECS 数据会被同步修改。

### 3.3 检查器与工具面板 (Inspector & Tools) - 右侧
面板包含三个选项卡：

*   **Inspector (属性面板)**:
    *   显示当前选中实体的详细信息（ID、名称）。
    *   可以实时查看和手动修改实体的 `Transform` 坐标 (X, Y, Z)。修改输入框的值会立即同步到视口中。
*   **Particle (粒子编辑器)**:
    *   用于调节粒子系统参数的可视化界面。
    *   目前预留了参数接口，后续可连接至 C++ 的 `ParticleEmitterComponent` 实现特效的实时预览。
*   **Build (发布流水线)**:
    *   提供了一键将游戏打包为可执行文件的功能。
    *   在下拉菜单中选择目标平台（目前支持 `win64`, `mac`, `wasm`）。
    *   点击 **"Build Now"** 按钮。编辑器会调用后端的 Node.js 脚本 (`scripts/build_pipeline.js`)，自动执行 CMake 编译，并将 `example/data` 下的游戏资产拷贝到生成的 Release 目录中。
    *   打包完成后，您可以在项目根目录下的 `build_export_win64/Release/` (以 Windows 为例) 中找到独立运行的游戏可执行文件。

---

## 4. 架构简介 (写给开发者)

如果您希望为编辑器贡献代码，请了解以下基础架构：

*   **前端 UI (`editor/src/components/`)**: 使用 React + TypeScript 编写，负责界面渲染和状态管理。
*   **IPC 通信 (`editor/preload.js` & `editor/main.js`)**: 采用 Context Isolation 策略，React 前端不能直接调用 C++，而是通过 `window.electronAPI` (在 `preload.js` 中定义) 发送 IPC 消息给 Electron 主进程。
*   **C++ 桥接层 (`editor/src/bridge/dsengine_bridge.cpp`)**: 使用 `node-addon-api` 编写。它接收来自主进程的调用，操作 `Phase1World` (EnTT ECS 注册表)，并将渲染结果（FrameBuffer）通过共享内存直接传回给 JS 的 `<canvas>` 渲染。

## 5. 常见问题 (FAQ)

**Q: 运行 `npm start` 报错，提示找不到 `dsengine_bridge.node`？**
A: 请确保您已经执行了 `npx node-gyp build` 并且没有编译错误。检查 `editor/build/Release/` 目录下是否生成了该文件。

**Q: 点击 "Build Now" 后没有反应？**
A: 打包过程是在后台异步执行的。您可以查看启动编辑器的终端控制台，里面会输出详细的 CMake 编译日志和资产拷贝状态。

**Q: 视口 (Viewport) 是黑屏的？**
A: 确保 C++ 引擎桥接层成功初始化了 `Phase1World`。目前视口中渲染的是彩色的小方块，如果没有显示，可能是桥接层的 `MockEngineLoop` 线程未正确启动。