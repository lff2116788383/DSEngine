# DOC-03 编辑器说明

本文档描述当前主线编辑器 `apps/editor_cpp/` 的定位、构建方式、已实现能力与边界。

## 1. 当前编辑器定位

当前主线编辑器为：`apps/editor_cpp/`

技术栈：

- C++
- Dear ImGui
- GLFW
- OpenGL
- ImGuizmo

当前编辑器的准确定位应为：

- 当前仓库唯一主线编辑器实现
- 可用于基础场景编辑与运行时预览
- 已进入主线可用阶段
- 仍处于工程化收口阶段

## 2. 构建与启动

### 2.1 构建

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=ON
cmake --build build_vs2022 --config Debug --target dse_editor_cpp
```

### 2.2 启动

```bash
bin\dsengine-editor.exe
```

## 3. 代码入口

编辑器当前主入口：

- `apps/editor_cpp/src/main.cpp`

CMake 入口：

- `apps/editor_cpp/CMakeLists.txt`

当前实现特征：

- 编辑器直接链接 `dse_engine`
- 视口直接复用引擎渲染结果
- 工具界面、状态管理和大部分编辑逻辑集中在主入口文件中

## 4. 当前已实现能力

### 4.1 基础窗口与面板

- DockSpace 风格主窗口
- Scene / Hierarchy 面板
- Inspector 面板
- Viewport / Game View
- Profiler 面板

### 4.2 基础编辑能力

- 实体浏览
- 实体创建 / 删除
- 选中状态反馈
- 基础 Transform 编辑
- 场景预览
- 场景保存 / 加载

### 4.3 与 Runtime 集成

- 通过 editor mode 初始化引擎
- 不允许业务脚本直接改编辑器窗口标题
- 可读取场景纹理 / 主纹理结果
- 复用当前 Runtime 渲染结果

### 4.4 已接入的工具增强

- ImGuizmo
- Profiler 历史采样与导出
- 编辑器内部语言数据初始化
- 一部分本地化预览能力
- 一部分 3D 组件检视与基础操作能力

## 5. 当前边界

当前 `editor_cpp` 仍属于“基础闭环已成型，但未完全工程化”的阶段。

以下能力不应视为当前已经稳定完成：

- 完整 Prefab 工作流
- 完整 Undo / Redo 指令系统
- 完整 Play In Editor 状态隔离
- 完整资源数据库与 UUID / meta 体系
- 完整打包面板与发布链路
- 完整 3D 编辑主线

## 6. 当前主要问题

### 6.1 代码集中度过高

`apps/editor_cpp/src/main.cpp` 体量较大，当前集中承担了：

- 初始化
- 面板绘制
- 状态管理
- 场景逻辑
- Profiler 逻辑
- 本地化预览
- 一部分 3D 检视与操作逻辑

继续在单文件中叠加功能会显著提高维护成本。

### 6.2 工具链闭环仍在补齐

当前编辑器已经可以作为运行时可视化与基础编辑工具，但在以下方向仍需继续增强：

- 资源链路
- Prefab
- Undo / Redo
- 场景版本管理
- PIE 隔离

## 7. 推荐后续拆分方向

建议至少拆成以下模块：

- `editor_app`
- `hierarchy_panel`
- `inspector_panel`
- `viewport_panel`
- `profiler_panel`
- `scene_io`
- `editor_state`
- `play_mode_state`

这样可以把当前集中式实现逐步转换成可维护的编辑器模块结构。

## 8. 当前阶段验收建议

当前阶段建议使用以下最小验收标准：

- 能成功构建 `dse_editor_cpp`
- 能成功启动 `bin/dsengine-editor.exe`
- 能正常显示 DockSpace / Scene / Inspector / Viewport
- 能创建与删除实体
- 能选中实体并编辑基础 Transform
- 能完成基本场景保存 / 加载
- 视口显示正常，无持续错误刷屏

## 9. 结论

当前编辑器不是概念验证级别，而是已经进入主线可用阶段。

但它更准确的定位应是：

- 当前主线编辑器
- 可用于基础场景编辑与运行时预览
- 对独立开发者和小团队具备初步使用价值
- 后续重点是模块化、资源链路与编辑闭环增强
