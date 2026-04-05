# Dark Soul Engine (DSEngine)

DSEngine 是一个面向**个人独立开发者**与**中小型工作室**的轻量级高性能 C++ 游戏引擎。当前主线聚焦：

- **2D Runtime**
- **Lua / C++ 双业务宿主**
- **原生 C++ 编辑器 (`apps/editor_cpp/`)**
- **Windows + CMake + Visual Studio 2022** 开发环境

当前最准确的定位是：**2D 主线已基本成型，编辑器基础可用，测试体系已建立，3D 已接入但不是默认稳定主线。**

## 当前项目定位

DSEngine 不是“大而全”的通用商业引擎。当前更适合以下目标：

- 2D 游戏原型开发
- 小型到中小体量 2D 项目开发
- Lua 驱动的快速玩法迭代
- 需要 C++ 性能与轻量工作流的团队

当前不建议对外表述为：

- 完整商用品质 3D 引擎
- 完整资源数据库 / UUID / meta 工作流引擎
- 完整大型团队协作编辑器平台

## 当前主线

当前仓库主线由三部分组成：

1. **Runtime (`apps/runtime/`)**
   - 提供 `cpp_host` 与 `lua_host`
   - 是当前运行时实际入口

2. **Editor (`apps/editor_cpp/`)**
   - 采用 **C++ + Dear ImGui + GLFW + OpenGL**
   - 直接链接 `dse_engine`
   - 当前是仓库内唯一主线编辑器实现

3. **Launcher (`apps/launcher_tauri/`)**
   - 采用 **Tauri + React + TypeScript**
   - 当前属于辅助入口与后续整合方向

## 当前已可用能力

### Runtime 与基础设施
- C++ / Lua 双业务宿主
- 主循环、固定步长更新、基础帧流水线
- OpenGL-first 渲染后端
- 资源根目录配置、基础 Bundle / VFS 能力
- Lua 启动脚本、实例生命周期、基础热重载状态恢复

### 2D 主线模块
- Sprite 渲染
- 2D Camera
- Box2D 物理
- Animation
- Spine 集成
- Tilemap
- UI（按钮、文本、布局、Anchor、CanvasScaler、动画）
- Localization（语言切换、字体映射、回退）
- Particle（发射器、曲线、随机参数、最小可用碰撞语义）
- CPU / Memory / Render Profiler

### Editor 当前能力
- 基础 DockSpace 编辑器框架
- Hierarchy / Inspector / Scene / Game / Profiler 面板
- 基础实体创建、删除、选中
- 基础 Transform 编辑
- 场景保存 / 加载
- ImGuizmo 基础操作
- Profiler 曲线与导出入口

### 测试
- CTest 统一入口
- `engine.unit`
- `engine.lua_runtime`
- `engine.spine`
- 若干 2D smoke 测试标签
- 部分 3D 组件与冒烟测试入口

## 当前边界

以下能力**不应**被表述为当前已经稳定完成：

- 完整 3D 商用品质闭环
- 完整 Prefab 工作流
- 完整 Undo / Redo 指令体系
- 完整 Play In Editor 隔离
- 完整资源数据库与 UUID / meta 体系
- 完整性能基线平台与 CI 性能门禁

## 3D 当前状态

3D 相关代码、组件、样例和测试入口已经存在，但当前状态更准确地应描述为：

- **已接入**：组件层、部分渲染路径、编辑器部分检视能力、部分测试入口
- **未收口**：默认构建、资源工作流、编辑器闭环、稳定性门禁、最小 MVP 验证流程

因此：

- 3D **不是**当前默认稳定主线
- 3D 当前更适合作为 **MVP 收口中的能力方向**，而不是对外承诺的成熟产品能力

## 快速开始（Windows）

### 1. 构建 Runtime

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build_vs2022 --config Debug --target dse_engine
cmake --build build_vs2022 --config Debug --target DSEngine_c++ DSEngine_lua
```

### 2. 运行 Lua Host

```bash
bin\DSEngine_lua_debug.exe
```

### 3. 开启测试

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_ENGINE_TESTS=ON
cmake --build build_vs2022 --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_spine_tests
ctest --test-dir build_vs2022 -C Debug --output-on-failure -L engine
```

### 4. 构建编辑器

```bash
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=ON
cmake --build build_vs2022 --config Debug --target dse_editor_cpp
bin\dsengine-editor.exe
```

## 推荐阅读顺序

- `doc/DOC-00_INDEX.md`
- `doc/DOC-01_ARCHITECTURE.md`
- `doc/DOC-02_BUILD_AND_RUN.md`
- `doc/DOC-03_EDITOR.md`
- `doc/DOC-04_TESTING.md`
- `doc/DOC-07_ROADMAP.md`

专题文档按需阅读：

- `doc/DOC-05_UI_LOCALIZATION.md`
- `doc/DOC-06_PARTICLE_PROFILER.md`

## 后续方向

DSEngine 后续不会继续追求“功能面越铺越宽”，而会坚持以下路线：

1. 先把 **2D 主线**做成更稳的可交付闭环
2. 再把 **Editor** 做成更高频可用的开发工具
3. 再把 **资源链路、测试门禁、性能基线**做扎实
4. 最后把 **3D** 从“已接入”推进到“最小可验证 MVP”

## 文档索引

完整文档入口见：`doc/DOC-00_INDEX.md`
