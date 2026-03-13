# DSEngine 演进路线图 (Roadmap)

## 愿景 (Vision)
打造一款 **轻量级、高性能、开发者友好** 的 2D 游戏引擎（兼顾 3D 扩展能力）。
*   **对标竞品**: Godot (通用性/易用性), RPG Maker MZ (特定领域生产力).
*   **核心定位**: 填补 "纯代码框架" 与 "重型通用引擎" 之间的空白。提供比 Godot 更轻量的运行时，比 RPG Maker 更灵活的定制能力。

## 差距分析 (Gap Analysis)

| 功能模块 | DSEngine (现状) | 商业化标准 (目标) | 缺失关键技术 |
| :--- | :--- | :--- | :--- |
| **渲染系统** | 基础 Sprite/Tilemap, 无批处理 | 高性能 2D 渲染, 动态光照, 粒子特效 | 自动合批 (Auto Batching), 2D Lighting, Particle System, Spine/Live2D 支持 |
| **编辑器** | 基础 ImGui 面板, 无操作历史 | 可视化场景编辑, 资源管理, 预制体 | Gizmos (操作轴), Undo/Redo (命令模式), Prefab 系统, Asset Pipeline |
| **UI 系统** | 仅基础定位 | 复杂界面布局, 富文本, 动效 | Auto Layout (自动布局), Rich Text, Tween 动画库 |
| **构建发布** | CMake 源码编译 | 多平台一键打包 (PC/Mobile/Web) | 交叉编译工具链, 资源加密打包, CI/CD 集成 |
| **脚本系统** | Lua 绑定 | 完善的调试与 IDE 支持 | 远程调试器 (Remote Debugger), 代码补全插件 |

## 演进阶段规划

### 基础设施补全 (Infrastructure)
*目标：为上层功能提供坚实的底层支持。*

1.  **序列化系统 (Serializer)** (已实现 JSON 序列化逻辑: `src/core/serializer.cpp`)
    *   **现状**: 缺乏统一的对象持久化机制。
    *   **方案**: 基于 **RTTR** (反射) 实现通用的 JSON/Binary 序列化器。
    *   **用途**: 支持 Prefab、场景保存/加载、编辑器 Undo/Redo。

### 阶段一：核心 2D 能力补全 (Core 2D Foundation)
*目标：让引擎具备制作高品质 2D 动作/RPG 游戏的基础渲染与动画能力。*

1.  **高性能 2D 渲染器 (BatchRenderer2D)** (已实现核心合批逻辑: `src/renderer/batch_renderer_2d.cpp`)
    *   **现状**: 每个 Sprite 即使材质相同也是单独的 Draw Call。
    *   **方案**: 实现动态合批渲染器。每一帧将相同材质/纹理的 Sprite 顶点数据合并到一个大 Vertex Buffer 中提交。
    *   **技术点**: `Dynamic VBO`, `Texture Atlasing` (运行时图集或离线打包)。

2.  **骨骼动画集成 (Skeletal Animation)** (已创建骨架: `src/renderer/spine_renderer.h`)
    *   **现状**: 仅支持序列帧 (`AnimationClip2D`)。
    *   **方案**: 集成 **Spine-cpp** 或 **DragonBones** 运行时。
    *   **实现**: 创建 `SpineRenderer` 组件，桥接渲染数据到引擎的 Mesh 系统。

3.  **2D 粒子系统 (Particle System)** (已实现渲染对接: `src/renderer/particle_system.cpp`)
    *   **现状**: 无。
    *   **方案**: 实现基于 CPU 的粒子发射器（考虑到 2D 兼容性）。
    *   **特性**: 发射器形状、生命周期颜色/大小变化、重力修改器、纹理动画。

4.  **2D 光照与阴影 (2D Lighting)** (已实现 LightManager: `src/lighting/light_manager.h`)
    *   **方案**: 实现 2D 延迟渲染或前向渲染管线扩展。
    *   **特性**: 点光源、聚光灯、法线贴图支持 (Normal Mapping)、2D 阴影投射 (Shadow Caster)。

### 阶段二：工作流与编辑器升级 (Workflow & Editor)
*目标：提升开发效率，从"写代码"转向"可视化编辑"。*

1.  **预制体系统 (Prefab System)** (已实现基础逻辑: `src/resource/prefab.cpp`)
    *   **痛点**: 无法复用配置好的游戏对象。
    *   **方案**: 实现序列化/反序列化机制。支持将 GameObject 及其组件树保存为 `.prefab` (JSON/YAML) 文件。
    *   **特性**: 实例化 (Instantiate), 属性覆写 (Overrides)。

2.  **撤销/重做系统 (Undo/Redo)** (已实现命令模式框架: `src/editor/command_impl.h`)
    *   **方案**: 引入 **命令模式 (Command Pattern)**。
    *   **实现**: 所有的编辑器操作（移动、添加组件、修改属性）都封装为 `ICommand`，压入历史栈。

3.  **场景编辑器交互优化 (Scene Gizmos)** (已实现基础交互逻辑: `src/editor/scene_gizmo.cpp`)
    *   **现状**: 只能通过属性面板修改坐标。
    *   **方案**: 集成 **ImGuizmo**。
    *   **特性**: 可视化拖拽手柄（位移、旋转、缩放），网格吸附 (Snap)。

4.  **资源导入管线 (Asset Pipeline)** (已创建 AssetImporter: `src/editor/asset_importer.h`)
    *   **现状**: 手动拷贝文件，无法处理资源设置。
    *   **方案**: 引入 `.meta` 文件机制。
    *   **特性**: 图片导入设置（压缩格式、Filter Mode、Wrap Mode）、音频转码、自动生成图集。

### 阶段三：游戏性系统扩展 (Gameplay Systems)

1.  **高级 UI 系统 (UGUI-like)** (已实现基础布局逻辑: `src/ui/layout_group.cpp`)
    *   **方案**: 扩展现有的 UI 组件。
    *   **特性**: 
        *   `Canvas Scaler`: 屏幕适配策略。
        *   `Layout Group`: Horizontal/Vertical/Grid 自动布局。
        *   `Event System`: 统一的点击、拖拽事件分发。

2.  **2D 寻路与导航 (Navigation)** (已实现基础接口逻辑: `src/ai/nav_mesh.cpp`)
    *   **方案**: 集成 **Recast & Detour** (虽然主要用于3D，但可适配) 或 **A* Pathfinding Project** 的 C++ 实现。
    *   **特性**: NavMesh 烘焙、动态避障。

3.  **瓦片地图增强 (Tilemap++)** (已实现 AutoTile 逻辑: `src/renderer/auto_tile.cpp`)
    *   **特性**: 自动瓦片 (Auto-tiling) 规则编辑器，支持 RPG Maker 风格的地形绘制。

### 阶段四：生态与发布 (Ecosystem & Release)

1.  **多平台构建工具** (已添加 Android 构建模板: `cmake/android.toolchain.cmake`)
    *   **方案**: 基于 CMake 的交叉编译脚本。
    *   **目标**: Android (NDK), iOS, WebAssembly (Emscripten), Windows/macOS/Linux。

2.  **Lua 调试器** (已实现调试器通信 Mock: `src/script/lua_debugger.cpp`)
    *   **方案**: 集成 `MobDebug` 或基于 VSCode 的 Lua Debugger Adapter。支持断点、变量监视。

## 技术架构调整建议

### 1. 渲染架构重构
目前 `RenderTaskConsumer` 设计较复杂。建议简化为 **RenderPass** 架构：(已创建基础类: `src/renderer/render_pass.h`, `render_queue.h`)
*   `RenderPass`: 描述一次渲染过程（Target, Clear, Sorting, Draw）。
*   `RenderQueue`: 渲染指令队列，支持排序（Opaque 近到远, Transparent 远到近, UI Order）。

### 2. 资源管理重构
目前直接加载文件。建议引入 **ResourceManager**：
*   **引用计数**: 自动卸载未使用的资源。
*   **异步加载**: 避免加载大资源卡顿主线程。
*   **资源句柄 (Handle)**: 使用 ID 而非裸指针访问资源，防止悬空指针。

## 总结
要达到商业化标准，DSEngine 至少需要完成 **阶段一** 和 **阶段二** 的核心内容。建议优先从 **自动合批渲染** 和 **预制体系统** 入手，这两点是性能和开发效率的基石。
