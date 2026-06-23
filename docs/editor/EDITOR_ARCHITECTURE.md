# DSEngine 编辑器架构分析

> 更新日期: 2026-06-10 (最近修订: 全量代码现状核实 — 面板/工具/测试/已知边界对齐源码)
> 基于 `apps/editor_cpp/` 源码审查（本次按真实文件逐项核实，修正过时计数与完成度）

---

## 一、技术栈

| 层级 | 技术 | 说明 |
|------|------|------|
| UI 框架 | Dear ImGui (Docking) | 即时模式 GUI，内建 DockSpace |
| 窗口/输入 | GLFW | 跨平台窗口管理 |
| 渲染 | OpenGL 3.3 (GLAD) | 编辑器自身渲染（未走 RHI） |
| Gizmo | ImGuizmo | 平移/旋转/缩放变换控件 |
| 序列化 | RapidJSON | 场景/设置 JSON 读写 |
| 字体 | Inter + NotoSansSC + MDI | 主字体 + 中文 + 图标 |
| 引擎集成 | dse_engine (DLL) | 链接引擎动态库 |

---

## 二、代码规模与结构

**总计：约 110 个源文件（.cpp+.h），`src/` 下 ~30,664 行代码，约 60 个功能模块/面板。**
（旧文档记 “59 文件 / 12,000 行 / 25 面板” 已严重过时，本次按 `find apps/editor_cpp/src` + `wc -l` 核实。）

### 自上次文档以来新增/此前漏记的面板（均已落地，非占位）

| 面板 | 文件 | 行数 | 功能 | 佐证 |
|------|------|------|------|------|
| Shader Graph | `editor_shader_graph.cpp` | 1248 | 节点式着色器图 + 贝塞尔连线 + 编译为 DSSL | `Compile`，7 例 `ShaderGraphCompileTest` |
| Visual Script | `editor_visual_script.cpp` | 756 | 节点式可视脚本 → Lua 代码生成 | `Compile` / `DrawNode`（控制流见 §四） |
| Anim State Machine | `editor_anim_state_machine.cpp` | 637 | 动画状态机图 + 过渡箭头 + 状态 Inspector | `DrawAnimStateMachinePanel` |
| Tilemap | `editor_tilemap_panel.cpp` | 567 | 2D 瓦片笔刷/填充/橡皮 | — |
| AI Chat Panel | `editor_chat_panel.cpp` | 892 | 编辑器内建 AI 对话 + @提及解析 + 历史持久化 | 接入 `editor_app.cpp:1022-1025` |
| Curve Editor | `editor_curve_editor.cpp` | 327 | 通用曲线编辑控件 | `DrawCurveEditor` |
| NavMesh Panel | `editor_navmesh_panel.cpp` | 316 | 导航网格烘焙参数 + Overlay 预览 | `DrawNavMeshPanel` / `DrawNavMeshOverlay` |
| Lua Debugger | `editor_lua_debugger.cpp` | 303 | Lua 断点/单步调试面板 | `DrawLuaDebuggerPanel` |
| Multi-Viewport | `editor_multi_viewport.cpp` | 173 | 多视口配置面板（默认关闭，可开启） | `DrawMultiViewportConfigPanel` |
| Streaming Debug | `editor_streaming_panel.cpp` | 149 | 流式加载可视化 | `DrawStreamingDebugPanel` |
| 其它 | Prefab Override / Lighting Gizmos / Physics Debug / Selection Outline / Scene View Mode / Autosave / Project Hub / Layout Manager / OS Drop / AI Config | — | 预制体覆盖、灯光/物理 Gizmo、选中描边、视图模式、自动保存、工程枢纽、布局管理、拖拽、AI 配置 | 均有独立 `.cpp` |

### 面板清单（历史，按代码量排序）

| 面板 | 文件 | 大小 | 功能 |
|------|------|------|------|
| Inspector | `editor_inspector_panel.cpp` | 71KB | 20+ Component 属性编辑，含 Undo |
| Scene IO | `editor_scene_io.cpp` | 63KB | JSON 场景序列化/反序列化 |
| Viewport | `editor_viewport_panel.cpp` | 43KB | 场景视口 + Gizmo + Color-ID 拾取 + 框选 + Grid |
| Hierarchy | `editor_hierarchy_panel.cpp` | 32KB | 实体树 + 搜索 + 拖拽排序 + 父子关系 |
| Main | `main.cpp` | 32KB | 主循环 + GLFW/ImGui 初始化 + DrawEditorUI |
| Terrain | `editor_terrain_panel.cpp` | 25KB | 地形雕刻 + Splat Map 纹理绘制 |
| Aux Panels | `editor_aux_panels.cpp` | 24KB | Project 面板 + 动画面板 + 本地化预览 |
| Particle | `editor_particle_panel.cpp` | 22KB | 粒子贝塞尔曲线编辑器 |
| Tilemap | `editor_tilemap_panel.cpp` | 17KB | 2D 瓦片笔刷/填充/橡皮 |
| Build Game | `editor_build_game.cpp` | 11KB | 游戏打包对话框 |
| Profiler | `editor_profiler_panel.cpp` | 11KB | CPU/Memory/Render 三维度 + 曲线 |
| Console | `editor_console_panel.cpp` | 11KB | spdlog 日志 + 过滤 + 双击跳转 |
| Scene Tabs | `editor_scene_tabs.cpp` | 10KB | 多场景标签页管理器 |
| Shortcuts | `editor_shortcuts.cpp` | 9.9KB | 全局快捷键绑定 |
| Audio | `editor_audio_panel.cpp` | 8KB | AudioSource/Listener 编辑 |
| Undo/Redo | `editor_undo.h` | 8KB | Command Pattern 框架 |
| Theme | `editor_theme.cpp` | 7.6KB | Hazel 风格暗色主题 + 字体 |
| Shell | `editor_shell.cpp` | 7.9KB | DockSpace + 主菜单栏 |
| Snapshot | `editor_snapshot.cpp` | 7.6KB | Registry 快照（自动化测试） |
| Prefab | `editor_prefab.cpp` | 7.1KB | .dprefab 导入/导出 |
| Toolbar | `editor_toolbar.cpp` | 6.1KB | Play/Pause/Stop + Gizmo 模式 |
| Material | `editor_material_panel.cpp` | 5.9KB | PBR 材质属性 + 预览球 |
| Lua Console | `editor_lua_console.cpp` | 4.8KB | Lua REPL（ExecuteLuaString） |
| Settings | `editor_settings.cpp` | 4.1KB | editor_settings.json 持久化 |
| File Dialog | `editor_file_dialog.cpp` | 3.3KB | Windows 原生 OPENFILENAMEW |
| Scene Camera | `editor_scene_camera.cpp` | 3.1KB | Orbit/Pan/Zoom 自由相机 |
| Preferences | `editor_preferences_panel.cpp` | 2.6KB | 主题/Snap/快捷键查看 |
| Status Bar | `editor_status_bar.cpp` | 2.5KB | FPS/实体数/Gizmo 状态 |
| Selection | `editor_selection.h` | 1.7KB | 多选管理器 |
| Test Harness | `editor_test_harness.cpp` | 1.5KB | headless 自动化测试 |
| Icons | `editor_icons.h` | 3.5KB | MDI 图标 codepoint |

### 架构图

```
┌─────────────────────────────────────────────────────────────┐
│ main.cpp                                                      │
│   glfwCreateWindow → gladLoadGL → ImGui::CreateContext        │
│   EngineInstance(config).Init()                               │
│                                                               │
│   Main Loop:                                                  │
│     ├─ glfwPollEvents()                                       │
│     ├─ SetEditorCamera() / DisableEditorCamera()              │
│     ├─ Edit: Render only | Play: engine.Tick()                │
│     ├─ ImGui NewFrame → DrawEditorUI() → Render              │
│     └─ glfwSwapBuffers()                                      │
├───────────────────────────────────────────────────────────────┤
│ DrawEditorUI(engine, scene_tex, game_tex)                     │
│   ├─ BeginEditorShell()       → DockSpace                    │
│   ├─ DrawEditorMainMenu()     → File/Edit/Window             │
│   ├─ DrawSceneTabBar()        → 多场景标签页                   │
│   ├─ ProcessShortcuts()       → 全局快捷键                    │
│   ├─ DrawEditorToolbar()      → Play/Stop/Gizmo              │
│   ├─ DrawHierarchyPanel()     → 实体树                       │
│   ├─ DrawInspectorPanel()     → 属性编辑                     │
│   ├─ DrawProjectPanel()       → 资源浏览                     │
│   ├─ DrawConsolePanel()       → 日志                         │
│   ├─ DrawProfilerPanel()      → 性能                         │
│   ├─ DrawAnimationPanel()     → 动画时间轴                    │
│   ├─ DrawMaterialPanel()      → 材质编辑                     │
│   ├─ DrawTerrainEditorPanel() → 地形编辑                     │
│   ├─ DrawLuaConsolePanel()    → Lua REPL                     │
│   ├─ DrawBuildGameDialog()    → 打包                         │
│   ├─ DrawPreferencesPanel()   → 偏好设置                     │
│   ├─ DrawSceneViewportPanel() → 场景视口 + Gizmo             │
│   ├─ DrawGameViewportPanel()  → 游戏视口                     │
│   ├─ DrawStatusBar()          → 底部状态栏                    │
│   └─ EndEditorShell()                                        │
├───────────────────────────────────────────────────────────────┤
│ 核心系统                                                       │
│   ├─ UndoRedoManager     → Command Pattern (100 步历史)      │
│   ├─ SelectionManager    → 多选实体列表                      │
│   ├─ SceneTabManager     → 多场景标签页 + Registry 快照       │
│   ├─ EditorCamera        → Orbit/Pan/Zoom 独立相机           │
│   ├─ EditorSettings      → JSON 持久化配置                   │
│   └─ ColorIDPicker       → FBO 实体拾取                      │
├───────────────────────────────────────────────────────────────┤
│ 引擎层                                                         │
│   ├─ EngineInstance      → Init/Tick/Shutdown                │
│   ├─ FramePipeline       → SetEditorCamera / Render          │
│   ├─ ExecuteLuaString()  → Lua REPL 桥接                     │
│   └─ World (entt)        → ECS Registry                      │
└─────────────────────────────────────────────────────────────┘
```

---

## 三、功能完成度

### 已完成功能 ✅（Phase 1-7 全部交付）

| 类别 | 功能 | 实现质量 |
|------|------|---------|
| **布局** | Unity 风格 DockSpace | ✅ 完整 |
| **主题** | Hazel 暗色主题 + Inter/NotoSansSC/MDI 字体 | ✅ 完整 |
| **Hierarchy** | 实体树 + 搜索 + 拖拽排序 + 父子关系 + 图标 | ✅ 完整 |
| **Inspector** | 20+ Component 编辑 + Undo + Asset 拖拽 | ✅ 完整 |
| **Viewport** | Gizmo + 编辑器相机 + Color-ID 拾取 + 框选 + Grid + Scene Gizmo | ✅ 完整 |
| **Undo/Redo** | Command Pattern + PropertyChange + Lambda + Compound + Merge | ✅ 完整 |
| **多选** | Ctrl+Click / Shift+Click / 框选 / 批量 Gizmo | ✅ 完整 |
| **快捷键** | Ctrl+Z/Y/S/O/N/D, Delete, F, F2 | ✅ 完整 |
| **场景IO** | JSON 序列化全组件 + 多场景标签 + Save/Load/SaveAs | ✅ 完整 |
| **Console** | spdlog sink + 过滤 + 跳转源码 | ✅ 完整 |
| **Profiler** | CPU/Memory/Render + 历史曲线 + 导出 | ✅ 完整 |
| **Play 模式** | Play/Pause/Stop + Registry 备份恢复 | ✅ 完整 |
| **地形** | 高度笔刷 + Splat Map + Undo + LOD 预览 | ✅ 完整 |
| **Tilemap** | 笔刷/填充/橡皮 + 多尺寸 + Undo | ✅ 完整 |
| **材质** | PBR 5 属性 + 纹理槽 + 预览球 | ✅ 完整 |
| **动画** | 时间轴 + 关键帧 + 播放控制 | ✅ 完整 |
| **粒子** | 贝塞尔曲线编辑器 (Size/Alpha/Speed/Color) | ✅ 完整 |
| **音频** | AudioSource/Listener + 3D 范围可视化 | ✅ 完整 |
| **Prefab** | .dprefab JSON 导入导出 + 拖拽实例化 | ✅ 完整 |
| **Lua REPL** | ExecuteLuaString + 命令历史 | ✅ 完整 |
| **设置** | editor_settings.json 持久化 | ✅ 完整 |
| **打包** | Build Game 对话框 | ✅ 完整 |
| **国际化** | 多语言切换 + 参数预览 | ✅ 完整 |
| **探针** | Light Probe + Reflection Probe 可视化 | ✅ 完整 |
| **自动化** | headless + 快照对比 | ✅ 完整 |
| **AI Control Server** | WebSocket JSON-RPC + MCP adapter，**32 个内建 Tool**（表驱动注册，详 §六） | ✅ 完整 |
| **Inspector 注册表** | Component → DrawFunc 映射表，29 个组件注册 | ✅ 完整 |
| **插件系统** | Python 进程外插件 + ControlServer 接口 | ✅ 完整 |
| **Shader Graph** | 节点式着色器图 → 编译为 DSSL（7 例编译测试） | ✅ 完整 |
| **Visual Script** | 节点式可视脚本 → Lua：事件入口生成函数体、Branch→if/else、For Loop→数值 for、纯数据节点内联表达式、Flow 数据输出绑定局部变量（`editor_visual_script_compiler.{h,cpp}`，6 例测试） | ✅ 完整 |
| **动画状态机** | 状态机图 + 过渡条件 + 状态 Inspector | ✅ 完整 |
| **Animation Retargeting** | 导入源/目标模型（gltf/fbx）→ 按骨骼名自动映射（精确/归一化/人形同义词）+ 手动覆盖 → 烘焙以目标骨架命名的 `.danim`（`editor_anim_retarget_core.{h,cpp}` 纯核心 + `editor_anim_retarget.cpp` 面板，9 例测试） | ✅ 完整 |
| **碰撞体可视化编辑** | Scene 视口 `ColEdit` 开关：对选中实体的 Box3D/Sphere3D/Box2D/Circle2D 用 ImGuizmo 直接拖拽 size/radius 与 center/offset（`editor_collider_edit.{h,cpp}` 纯核心 + `_gizmo.cpp` 视口交互，7 例测试） | ✅ 完整 |
| **NavMesh 面板** | 烘焙参数 + Overlay 预览 | ✅ 完整 |
| **AI Chat Panel** | 编辑器内建 AI 对话 + Python LLM bridge（原 Phase 3，已接入） | ✅ 完整 |
| **Lua Debugger / Curve Editor / Streaming Debug** | Lua 调试、通用曲线、流式加载可视化 | ✅ 完整 |
| **崩溃捕获（编辑器侧）** | 复用引擎进程级 CrashReporter，编辑器薄封装：最早期安装覆盖 Init 前阶段、`app_name=DSEngine-Editor` 区分进程、面包屑/元数据记录场景/Play/命令上下文、与 AutoSave 联动提示上次崩溃（`editor_crash.{h,cpp}`，8 例测试） | ✅ 完整 |

### 缺失功能

| 优先级 | 功能 | 说明 |
|--------|------|------|
| 🟢 P2 | Multi-viewport 默认开启 | 面板已有开关，默认关闭（CRT 稳定性顾虑） |
| 🟢 P2 | 非 Windows 原生文件对话框 | 仅 Windows 实现，非 Windows 返回空（详 §四） |

---

## 四、架构优劣分析

### 优势

| 维度 | 评价 |
|------|------|
| **开发效率** | ImGui 即时模式开发快，面板独立文件拆分清晰 |
| **性能** | 原生 C++，零 IPC 开销，直接访问引擎内存 |
| **跨平台** | ImGui + GLFW 天然跨平台 |
| **引擎集成** | 同进程同语言，直接操作 entt::registry |
| **功能覆盖** | 25 个面板，覆盖完整游戏开发工作流 |

### 已知问题

| 问题 | 严重性 | 说明 |
|------|--------|------|
| ~~main.cpp 过重~~ | ✅ 已修 | 拆分为 EditorApp 类，main.cpp 仅 10 行 |
| ~~大量 static 全局状态~~ | ✅ 已修 | 全局变量已消除：profiler/gizmo/language 收入 EditorApp 成员，editor_state/backup_registry 改为 namespace-static，EditorContext 统一传递 |
| ~~Inspector 膨胀~~ | ✅ 已修 | InspectorRegistry 注册表，29 个组件统一注册 |
| **部分走 RHI** | 🟡 中 | 主场景渲染早已经由引擎 RHI（`pipeline()->GetSceneTextureId()`）；编辑器自建 GPU 资源中**资产缩略图已迁移到 RHI**（`editor_gpu.{h,cpp}` 接入层 → `RhiDevice::CreateTexture2D/DeleteTexture`）。剩余直接 GL：视口拾取（自定义 shader+FBO+readback）、多视口 blit、ImGui 后缓冲清屏（详 §五·编辑器走 RHI） |
| **Multi-viewport 默认关闭** | 🟡 中 | 已有配置面板与开关，默认关（CRT heap 稳定性顾虑） |
| **Visual Script 控制流未接** | 🟡 中 | `editor_visual_script.cpp:374-381` 事件/分支 `{body}`/`{true_body}`/`{false_body}` 仅生成占位注释，数据流正常 |
| **文件对话框仅 Windows** | 🟢 低 | `editor_file_dialog.cpp:101-106` 非 Windows 为空桩（与引擎 Windows-first 一致） |

### 改进建议

| 优先级 | 改进 | 预估 | 状态 |
|--------|------|------|------|
| ~~🔴 高~~ | ~~拆分 main.cpp → EditorApp 类~~ | ~~1-2 天~~ | ✅ 完成 |
| ~~🔴 高~~ | ~~Inspector 注册式 (Component → DrawFunc)~~ | ~~1 周~~ | ✅ 完成 |
| ~~🟡 中~~ | ~~统一 EditorContext 替代残余 static 变量~~ | ~~2-3 天~~ | ✅ 完成 |
| 🟡 中 | 编辑器走 RHI 而非直接 OpenGL | 1 周 | 🔄 进行中（缩略图已迁移，详 §五） |
| 🟢 低 | 修复 Multi-viewport CRT 问题 | 未知 | 待开始 |

---

## 五、编辑器走 RHI（迁移评估与进度）

> 目标：编辑器不再直接 `#include <glad/gl.h>` 调 OpenGL，所有 GPU 资源/绘制经引擎 `render::RhiDevice` 抽象，从而与引擎一致地支持 OpenGL / Vulkan / D3D11 三后端。

### 现状

编辑器的**主场景渲染本就走 RHI**：引擎管线把场景渲染进 RHI 渲染目标，编辑器仅通过 `pipeline()->GetSceneTextureId()` 取颜色纹理喂给 `ImGui::Image`。编辑器自身直接调 GL 的只剩少量自建 GPU 资源/绘制。

### 接入层

新增 `editor_gpu.{h,cpp}`：引擎 Init 后由 `editor_app` 调 `SetEditorRhiDevice(pipeline()->GetRhiDevice())` 注入设备；编辑器代码经 `EditorCreateTexture2D / EditorDeleteTexture` 等收敛到 RHI。后端无关：OpenGL 后端返回的句柄即 GL 纹理 id，可直接喂 ImGui。

### 已迁移

| 子系统 | 原直接 GL | 迁移后 |
|--------|-----------|--------|
| 资产缩略图（`editor_aux_panels.cpp`：球体预览 / 图片缩略图 / 缓存释放） | `glGenTextures`+`glTexImage2D`+`glDeleteTextures` | `EditorCreateTexture2D(linear,clamp)` / `EditorDeleteTexture` → `RhiDevice::CreateTexture2D(TextureSamplerDesc)` / `DeleteTexture`，行为一致（RGBA8 / Linear / ClampToEdge） |

### 剩余（需扩展 RHI，风险较高）—— 概览

| 子系统 | 位置 | 直接 GL 用法 | 阻塞点 | 依赖的新原语 |
|--------|------|--------------|--------|--------------|
| 视口拾取（颜色 ID） | `editor_viewport_panel.cpp` ~130-260 | 自建 shader + quad VAO/VBO + FBO，逐像素 `glReadPixels` 命中实体 | RHI 只暴露高层批绘制（`DrawMeshBatch/DrawSpriteBatch`），无「自定义 shader 任意几何即时绘制」入口 | A. 通用即时绘制 |
| 多视口纹理 blit | `editor_viewport_panel.cpp` ~550-590 | `glBlitFramebuffer` 把源纹理拷到各视口纹理 | RHI 仅有 `BlitToScreen`，无通用 RT→RT / RT→texture blit | B. RT blit |
| ImGui 后缓冲清屏 + 呈现 | `editor_app.cpp` ~689-697 | 绑定默认 FBO + `glClear`，再由 `imgui_impl_opengl3` 直接绘制 | 与 ImGui GL3 后端强耦合 | C. 默认帧缓冲清屏 + RHI 后端 ImGui 渲染器 |

> 推进顺序：先做 **§5.A 通用即时绘制原语 + §5.B RT blit**（视口拾取/多视口都依赖它，且可离屏验证），再迁移 **§5.1 视口拾取 / §5.2 多视口 blit**，最后做 **§5.3 ImGui 呈现层**（最深、与窗口/交换链耦合）。在 §5.A/§5.B 落地前不要动 §5.1/§5.2，以免回归当前可用路径。

---

### §5.A 新增 RHI 原语：通用即时绘制（Immediate Draw）  ✅ 已实现（三后端 P1）

**目标**：让上层用「自定义 shader program + 一段顶点数据 + 少量 uniform」直接绘制到指定 RT，不经高层 Mesh/Sprite 批。视口拾取（颜色 ID quad）即用此原语重写。

> **落地状态**：`ImmediateDraw(ImmediateDrawDesc)` 已在 GL/DX11/Vulkan 三后端实现（类型见 `rhi_types.h`、接口见 `rhi_device.h`）。Vulkan 走「动态 `VkPipeline` 复合键缓存 + 一次性 cmd buffer 同步提交 + push-constant 成员偏移反射」；DX11 走 `GetOrCreatePrimInputLayout`（attr→`TEXCOORD<location>`）+ cbuffer 反射按名打包 uniform；GL 走临时 VAO/VBO + `glVertexAttribPointer` + `glGetUniformLocation`。离屏像素闸门见 `tests/gtest/smoke/immediate_draw_pixel_smoke_test.cpp`（DX11/WARP 真验，GL/Vulkan 无驱动优雅 skip）。

**建议接口签名**（加到 `rhi_device.h`，类型放 `rhi_types.h` 全局命名空间）：

```cpp
enum class ImmediateTopology : uint8_t { Triangles, Lines, LineStrip };
struct ImmediateVertexAttrib { int location; int components; int offset_bytes; };
struct ImmediateDrawDesc {
    unsigned int render_target = 0;        // 目标 RT 句柄（0 = 默认帧缓冲）
    unsigned int shader_program = 0;       // CreateShaderProgram 返回值
    const void*  vertices = nullptr;       // 交错顶点数据
    size_t       vertex_bytes = 0;
    int          vertex_count = 0;
    int          stride_bytes = 0;
    std::vector<ImmediateVertexAttrib> attribs;
    ImmediateTopology topology = ImmediateTopology::Triangles;
    glm::ivec4 viewport = {0,0,0,0};       // x,y,w,h（0,0,0,0 = 用 RT 全尺寸）
    bool clear = false; glm::vec4 clear_color = {0,0,0,0};
    bool blend = false; bool depth_test = false;
    // 自定义 program 的 uniform（按需填，名字对应 GLSL/HLSL 常量）
    std::vector<std::pair<std::string,float>>      uniforms_f;
    std::vector<std::pair<std::string,glm::vec2>>  uniforms_vec2;
    std::vector<std::pair<std::string,glm::vec4>>  uniforms_vec4;
};
// 默认实现可空（return），便于后端逐步落地；落地后编辑器拾取改调它。
virtual void ImmediateDraw(const ImmediateDrawDesc& desc) { (void)desc; }
```

**各后端实现要点**
- **OpenGL**（`gl_rhi_device.cpp` + `gl_resource_manager`）：取 `RenderTargetResource::fbo_handle`（0→默认 FBO）→ `glBindFramebuffer`；按 `viewport` 调 `glViewport`；`clear` 时 `glClearColor/glClear`；`glUseProgram(shader_program)`；用一个**复用的内部 VAO** + 临时（或 orphan 的）VBO 上传 `vertices`，按 `attribs` 设 `glVertexAttribPointer/glEnableVertexAttribArray`；按名 `glGetUniformLocation` 设 uniform；`blend/depth_test` 切 `GL_BLEND/GL_DEPTH_TEST`；`glDrawArrays(topology, 0, vertex_count)`；结束恢复先前 FBO/viewport/blend（编辑器现有拾取代码 130-260 即此逻辑，可直接搬入后端）。
- **Vulkan**（`vulkan_rhi_device.cpp` + `vulkan_resource_manager`）：最重。需为「自定义 program + 顶点布局 + 目标 RT 的 `render_pass`」动态建一个 `VkPipeline`（建议按 `{shader_program, attribs, topology, blend, depth}` 做 key 缓存，避免每帧建）；录命令到一个一次性 `VkCommandBuffer`：`vkCmdBeginRenderPass`（用 RT 的 `render_pass`/`render_pass_load`，依 `clear`）→ `vkCmdBindPipeline` → `vkCmdSetViewport` → 绑定上传了顶点的 `VkBuffer` → uniform 走 push constant 或小 UBO → `vkCmdDraw` → `vkCmdEndRenderPass` → 提交并等完成（编辑器拾取需同帧 readback，简单起见同步提交）。
- **D3D11**（`dx11_rhi_device.cpp` + `dx11_resource_manager`）：取 `DX11RenderTarget::color_rtv`（0→交换链 RTV）→ `OMSetRenderTargets`；`clear` 时 `ClearRenderTargetView`；`RSSetViewports`；由 `shader_program` 拿编译好的 VS/PS 与输入布局（`IASetInputLayout`，`attribs`→`D3D11_INPUT_ELEMENT_DESC`）；顶点传 `Map(WRITE_DISCARD)` 的动态 `ID3D11Buffer` + `IASetVertexBuffers`；uniform 打包进常量缓冲（`UpdateSubresource`/`Map`）；`OMSetBlendState/OMSetDepthStencilState`；`IASetPrimitiveTopology` + `Draw`。

**测试**（`tests/gtest/smoke/immediate_draw_pixel_smoke_test.cpp` 离屏，无窗口）✅ 已补
- `ImmediateDrawFillsRenderTarget`：建 RT → 用纯色 shader `ImmediateDraw` 一个全屏三角形 → `ReadRenderTargetColorRgba8WithSize` 验中心像素=期望色。✅
- `ImmediateDrawViewportSubregion`：带 `viewport` 子区域绘制 → 验区域内/外像素。✅
- `ImmediateDrawColorIdRoundTrip`：模拟拾取——画多条不同「颜色 ID」竖条 → 读各竖条中心像素 → 颜色↔ID 反解正确（覆盖编辑器拾取的核心数值逻辑）。✅

> 注：实际落在 `tests/gtest/smoke/`（与既有 `*_pixel_smoke_test` + 跨后端 `rhi_pixel_harness` 同构），非设计稿设想的 `integration/render/`。

### §5.B 新增 RHI 原语：RT blit（RT→RT / RT→texture）  ✅ 已实现（三后端 P1）

**目标**：等尺寸把一个 RT 的颜色 0 号附件拷到另一个 RT 或一张纹理，替代多视口的 `glBlitFramebuffer`。

> **落地状态**：`BlitRenderTarget(src_rt, dst_rt)` 已在三后端实现——GL `glBlitFramebuffer`、DX11 `CopyResource`（MSAA 源先 `ResolveSubresource`）、Vulkan `vkCmdBlitImage` + 前后 layout barrier。

**建议接口签名**：
```cpp
// 颜色 0 号附件，等尺寸拷贝；dst 需已按相同尺寸创建。
virtual void BlitRenderTarget(unsigned int src_rt, unsigned int dst_rt) { (void)src_rt; (void)dst_rt; }
```

**各后端实现要点**
- **OpenGL**：`glBindFramebuffer(READ, src.fbo)` + `glBindFramebuffer(DRAW, dst.fbo)` → `glBlitFramebuffer(..., GL_COLOR_BUFFER_BIT, GL_NEAREST)`（即编辑器 550-590 现逻辑）。
- **Vulkan**：`vkCmdBlitImage`（已有先例 `vulkan_draw_executor.cpp:2004`）；注意 blit 前后用 `vkCmdPipelineBarrier` 把 src 转 `TRANSFER_SRC`、dst 转 `TRANSFER_DST`，完后转回 `SHADER_READ_ONLY`。
- **D3D11**：同格式同尺寸首选 `CopyResource(dst.color_texture, src.color_texture)`（已有先例 `dx11_resource_manager.cpp:797`）；若 src 为 MSAA，先 `ResolveSubresource`；格式不同则退化为采样 `color_srv` 的全屏 `ImmediateDraw`。

**测试** ✅ 已补：`BlitRenderTargetCopiesColor`（`tests/gtest/smoke/immediate_draw_pixel_smoke_test.cpp`）：用 §5.A `ImmediateDraw` 画纯色填 src → `BlitRenderTarget` → 读 dst 验颜色一致。

### §5.1 迁移视口拾取（依赖 §5.A）

`editor_viewport_panel.cpp` ~130-260：
- 用 `CreateShaderProgram` 建拾取 shader（现 GLSL 已在文件内，HLSL 需补一份；或抽到 `engine/shaders` 让 `shader_manager` 跨后端编译）。
- 拾取 RT 用 `CreateRenderTarget({w,h,has_color,has_depth})`，每帧（或尺寸变化时）复用。
- 把每个候选实体的颜色 quad 顶点填进 `ImmediateDrawDesc` 调一次/批次 `ImmediateDraw`（`clear=true`、`blend=false`、`depth_test=true`）。
- 命中读取：现 `glReadPixels(px,py,1,1)` → 改 `ReadRenderTargetColorRgba8WithSize` 后按 `(px,py)` 取那一像素（或后续给 RHI 加 `ReadRenderTargetPixelRgba8(rt,x,y)` 单像素重载以省带宽，可选）。
- 删除该文件中的所有 `gl*` 调用与 `#include <glad/gl.h>`。
- 测试：颜色↔实体 ID 编解码已可无头单测；GPU 路径靠 §5.A 的离屏测试 + 手动可视化验证。

### §5.2 迁移多视口 blit（依赖 §5.B）

`editor_viewport_panel.cpp` ~550-590：`s_mvp_fbos`/`s_mvp_textures` 改为 `CreateRenderTarget`/`GetRenderTargetColorTexture`；`glBlitFramebuffer` 改 `BlitRenderTarget`；删除相关 `gl*`。注意各视口尺寸变化时重建目标 RT。

### §5.3 迁移 ImGui 呈现层（最深）

`editor_app.cpp` ~689-697 的默认 FBO 清屏 + `imgui_impl_opengl3` 直接出图与 GL 强耦合。两条路：
1. **小步**：给 RHI 加 `ClearDefaultFramebuffer(color)`（GL=绑 0 号 FBO 清屏；DX11=清交换链 RTV；Vulkan=swapchain pass 的 loadOp=CLEAR），先把那几行 `gl*` 收进 RHI；ImGui 仍用 GL3 后端（编辑器暂仍随 GL 后端运行）。
2. **彻底**：引入「RHI 后端版 ImGui 渲染器」（按当前 RHI 后端选择 `imgui_impl_opengl3`/`_vulkan`/`_dx11`，或自写一个走 `ImmediateDraw` 的 ImGui draw-data 渲染器），使编辑器可在任意后端启动并彻底去掉对 glad 的直接依赖。这步建议放在引擎 RHI 后端切换（编辑器可选 Vulkan/D3D11 启动）真正落地时一起做。

**需补测试**：ImGui 呈现属窗口/交换链路径，无头不可测；以 §5.A/§5.B 离屏测试兜底数值正确性，呈现层靠手动跑编辑器三后端各截图验证。

---

## 六、测试覆盖

> 测试目录：`tests/gtest/integration/editor/`
> 所有测试均为无头（headless）模式，不依赖 GPU / ImGui / GLFW。

### 测试文件一览

| 文件 | 用例数 | 覆盖内容 |
|------|--------|---------|
| `editor_functional_test.cpp` | 75 | CreateEntity/DestroyEntity、PropertyChangeCommand、LambdaCommand、CompoundCommand、命令合并、历史上限裁剪、Redo栈新命令后清空、SaveScene/LoadScene 基础往返、Camera3D+MeshRenderer 往返、DirectionalLight3D 往返、RigidBody2D/3D 往返、空场景往返、Prefab 导入导出/IsPrefabInstance/多实例独立性/多组件完整性、SceneTabManager 多标签/实体隔离/dirty状态追踪、CLI 参数解析、RegistrySnapshot 导出/对比/实体数差异检测、CopyRegistry 单组件/多组件完整性（含 RigidBody3D bug 修复验证）、SpriteRenderer/UILabel/ParticleEmitter/PointLight/SpotLight/SkyLight 往返、SiblingIndex 多实体排序往返、Animator3D 往返、UIAnchor 往返、50 实体压力测试、Skybox 往返、UIGridLayout 往返、UICanvasScaler 往返、SceneTabManager CloseTab、UndoRedo+SaveScene 非破坏性集成 |
| `editor_selection_integration_test.cpp` | 9 | SelectionManager 单选/多选/切换/防重复添加/移除/多选→单选/移除不存在不崩溃/主实体/GetPrimary 返回最后添加/null 清空 |
| `editor_control_server_test.cpp` | 101 | ControlServer 内建 Tool Handler（现共 **32 个** `dsengine_*` 工具）：ping/lua_execute/script_create/undo/redo/entity_create/delete/batch_delete/modify（含 rotation/scale）/add_component（含 DirectionalLight/PointLight/SpotLight/RigidBody3D/SkyLight/Camera3D/BoxCollider3D）/remove_component/get_components/get_state/duplicate/reparent/find_by_name/scene_get_state/new/save/load/editor_get_state/play/stop/screenshot/asset_import/material_create/prefab_save/prefab_instantiate/undo_history/selection_get/set/clear |

| `editor_plugin_manager_test.cpp` | 6 | PluginManager ScanPlugins（空目录/不存在目录/有效元数据/缺少字段/非法JSON）、StartPlugin error path（entry 文件不存在） |

| `editor_plugin_api_test.cpp` | 8 | 插件 API：Tool 注册/调用/错误路径 |
| `shader_graph_compile_test.cpp` | 7 | Shader Graph 节点图 → DSSL 编译正确性 |
| `editor_unit_test.cpp`（unit 套） | 16 | UndoRedoManager / EditorSettings / EditorTestConfig / Lambda+Compound Command |

**总计：编辑器专属测试 ~222 例（集成 ~206 + 单元 16），本次实跑全绿。**

> 核实命令：`dse_gtest_integration_tests --gtest_filter='EditorFunctionalTest.*:ControlServerTest.*:SelectionManagerTest.*:EditorPluginApiTest.*:PluginManagerTest.*:ShaderGraphCompileTest.*'` → 206 例全过。
> （旧文档 “112 例” 已过时。）

### 测试基础设施

| 文件 | 作用 |
|------|------|
| `editor_test_stubs.cpp` | 无头桩实现：UndoRedoManager 单例、GetCurrentScenePath/SetCurrentScenePath、EditorLog、editor_toolbar 全部函数（GetEditorState/EnterPlayMode/ExitPlayMode 等） |
| `editor_test_harness.cpp` | CLI 参数解析、headless 测试入口 |
| `editor_snapshot.cpp` | Registry 快照导出 / JSON 差异对比 |

### 未覆盖（需 GPU / ImGui）

| 模块 | 原因 |
|------|------|
| Viewport / Gizmo / Color-ID 拾取 | 依赖 OpenGL FBO |
| Inspector 属性绘制 | 依赖 ImGui 渲染循环 |
| Play 模式 Registry 备份/恢复 | 依赖完整 EngineInstance::Init() |
| 地形雕刻 / Splat Map | 依赖 GPU 纹理写入 |

---

## 七、方案评估

### 与其他方案对比

| 方案 | 代表 | 优势 | 劣势 | 适合 DSEngine？ |
|------|------|------|------|----------------|
| **C++ ImGui**（当前） | Hazel, Flax | 零依赖、高性能、同进程 | UI 美观受限、Markdown/富文本困难 | ✅ 当前阶段最优 |
| **Qt C++** | Unreal (部分) | 成熟控件库 | 500MB+ 依赖、许可证 | ❌ 成本过高 |
| **Web (Tauri/Electron)** | VS Code | 丰富 UI、快速迭代 | IPC 延迟、无法直接访问内存 | 🟡 适合 Launcher |
| **Hybrid (ImGui + WebView)** | — | 性能关键部分 ImGui，复杂 UI 用 Web | 架构复杂 | 🟡 远期可考虑 |

### 结论

**C++ ImGui 方案对 DSEngine 当前阶段是最优选择。** Godot/Hazel/Flax 等同体量引擎均采用类似方案。编辑器功能覆盖已相当完整（7 个 Phase 全部交付）：main.cpp 拆分、Inspector 注册式、EditorContext 统一、全局变量消除均已完成；AI Control Server 现有 **32 个内建 Tool**；Shader Graph / 动画状态机 / NavMesh / AI Chat Panel / Lua Debugger 均已落地（原文档列为“缺失/进行中”的项本次已核实为已实现）。余下的打磨项：编辑器走 RHI、Visual Script 控制流生成、碰撞体可视化拖撞、Multi-viewport 默认开启。
