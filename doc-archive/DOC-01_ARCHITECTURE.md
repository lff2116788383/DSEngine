# DOC-01 当前架构说明

本文档只描述当前仓库代码已经体现出的主线架构，不描述废弃旧方案，也不把远期目标写成已完成事实。

## 1. 当前项目定位

DSEngine 当前更适合被定位为：

- 面向个人独立开发者与中小型工作室
- 以 2D Runtime 为当前稳定主线
- 支持 Lua / C++ 双业务宿主
- 提供原生 C++ 编辑器与基础工程化能力
- 3D 已接入，但不是当前默认稳定主线

## 2. 当前主线

当前仓库主线由三部分组成：

- Runtime：`apps/runtime/`
- Editor：`apps/editor_cpp/`
- Launcher：`apps/launcher_tauri/`

其中：

- Runtime 是引擎实际运行入口，支持 C++ 与 Lua 双宿主
- Editor 是基于 C++、Dear ImGui、GLFW、OpenGL 的原生编辑器
- Launcher 是基于 Tauri、React、TypeScript 的辅助入口

## 3. 目录分层

```text
apps/       应用层入口
engine/     引擎内核与基础设施
modules/    玩法模块层
samples/    运行样例与脚本样例
script/     Lua 辅助脚本
data/       资源目录
tests/      单元测试与专项回归
depends/    第三方依赖
```

这种结构已经把“应用入口”“引擎底层”“玩法模块”“样例”和“测试”分开，适合继续做工程化收口。

## 4. Runtime 架构

### 4.1 运行时入口

Runtime 主循环由 `engine/runtime/engine_app.cpp` 驱动，当前已形成实例化运行时外壳：

- `EngineInstance`
- `RunEngine(...)`
- `EngineRunConfig`

当前能力包括：

- 窗口初始化
- editor 模式与普通 runtime 模式切换
- Lua / C++ 双业务模式
- 固定步长物理更新与普通帧更新

对应宿主入口：

- `apps/runtime/cpp_host/main.cpp`
- `apps/runtime/lua_host/main.cpp`

### 4.2 帧流水线

系统编排核心在 `engine/runtime/frame_pipeline.h`。

当前主线特征是：

- ECS 负责数据存储
- `FramePipeline` 负责系统编排
- 2D 系统以内建持有与固定顺序调度为主
- 3D 走按需动态模块接入方向

这说明当前架构已经具备继续演进的基础，但还没有完成全面模块化与完全实例化解耦。

## 5. ECS 与模块层

### 5.1 ECS

项目使用 EnTT 作为 ECS 基础设施。

当前特征：

- `World` 负责实体世界封装
- 组件主要定义在 `engine/ecs/components_2d.h`、`engine/ecs/components_3d.h` 等文件
- 模块系统从 Registry / World 读取组件并执行逻辑

### 5.2 模块层

玩法模块位于 `modules/` 下：

- `modules/gameplay_2d/`
- `modules/gameplay_3d/`

当前主线仍然是 2D：

- Camera
- Animation
- Tilemap
- UI
- Particle
- Localization
- Physics2D
- Spine

3D 目录已有代码、测试和样例入口，但整体仍处于“已接入、待收口”的阶段。

## 6. 渲染架构

### 6.1 当前实际状态

项目存在 RHI 抽象层：

- `engine/render/rhi/rhi_device.h`
- `engine/render/rhi/rhi_device.cpp`

当前稳定后端是 OpenGL。

因此当前准确口径应理解为：

- 已有面向多后端演进的抽象基础
- 当前真正可运行、可验证的稳定后端是 OpenGL
- Vulkan / Metal / WebGPU 不属于当前稳定实现范围

### 6.2 当前渲染能力

2D 方向已接入：

- Sprite 渲染
- UI 渲染
- Tilemap 渲染
- 2D 粒子

3D 方向已能从代码中看到：

- Mesh 渲染路径
- Terrain、Frustum Culling、Animator、FreeCamera、Steering 等模块接入方向
- 一部分光照、阴影、后处理相关接口与实现

但 3D 当前不应在主文档中被表述为“成熟商用品质完整闭环”。

## 7. 资源系统

资源系统核心在：

- `engine/assets/asset_manager.h`
- `engine/assets/asset_manager.cpp`

当前已具备：

- 纹理、材质、音频等基本资产管理
- 路径规范化与 data root 配置
- bundle 打包与挂载
- VFS 内存读取
- RHI 纹理创建接入

同时，当前仍保留一定全局态使用方式，例如 `AssetManager::Instance()`，说明资源系统已进入工程化阶段，但尚未完成完全实例化解耦。

## 8. 脚本系统

脚本系统位于 `engine/scripting/`：

- `engine/scripting/cpp/`
- `engine/scripting/lua/`

当前特点：

- C++ / Lua 双宿主共存
- Lua 运行时支持启动脚本、脚本实例生命周期、包路径配置
- 支持基础热重载态保存与恢复
- 支持 Lua 内存统计

Lua 运行时已经不是简单脚本执行器，而是当前主线运行时的一部分。

## 9. 编辑器架构

编辑器当前主线是 `apps/editor_cpp/`。

主要特点：

- 直接链接 `dse_engine`
- 使用 Dear ImGui 构建工具界面
- 直接复用引擎渲染结果
- 主入口当前集中在 `apps/editor_cpp/src/main.cpp`

当前编辑器已进入“可构建、可启动、可做基础场景编辑”的阶段，但代码组织仍有较强的单文件集中式特征，后续需要继续模块化。

## 10. 测试架构

测试位于 `tests/`，由顶层 `DSE_BUILD_ENGINE_TESTS` 开关控制。

当前已形成：

- `engine.unit`
- `engine.lua_runtime`
- `engine.spine`
- 多个 2D smoke 标签入口
- 部分 3D 组件与冒烟测试入口

测试覆盖已不仅限于基础工具层，也包含：

- Lua Runtime
- UI
- Physics2D
- Particle
- Tilemap
- Localization
- Profiler
- 部分 3D 模块

## 11. 当前架构结论

当前 DSEngine 的真实架构结论如下：

- 已形成完整的仓库主线：Runtime + Editor + Launcher
- 已形成可持续演进的目录分层
- 2D Runtime 主链较完整
- 编辑器、测试、资源系统已进入工程化整理阶段
- 3D 模块已有接入，但不属于当前稳定主线
- RHI 已有抽象基础，但当前稳定后端仍为 OpenGL

当前最重要的架构工作不是继续扩张功能面，而是：

- 继续推进实例化解耦
- 降低编辑器单文件复杂度
- 加强资源链路与测试门禁
- 在 2D 闭环稳定后再推进 3D MVP
