# cpp-game-engine-book 引擎功能与范例梳理

本文档基于 `cpp-game-engine-book-main` 项目的源码和目录结构整理，旨在为您指导自研游戏引擎提供参考。该项目是一个逐步实现游戏引擎的教学项目，涵盖了现代游戏引擎开发的基础和进阶功能。

## 1. 引擎核心功能 (Core Engine Features)

### 1.1 架构与模式
- **GameObject-Component 模式**: 实现了基于实体与组件的架构，方便逻辑的解耦与挂载（见 `component` 模块及 `rttr` 反射库的集成测试）。
- **生命周期管理**: 提供了引擎初始化、帧更新 (Update) 和渲染循环。
- **模块化设计**: 将引擎核心代码与具体游戏项目代码分离（引擎源码与 Demo 源码拆分架构）。

### 1.2 脚本集成 (Scripting)
- **Lua 脚本绑定**: 集成了 `Lua` 和 `Sol2` (或 LuaBridge)，支持在 C++ 中调用 Lua，以及在 Lua 中编写游戏逻辑。
- **Lua 调试器**: 实现了与 ZeroBrane 等调试器的联调功能（见 `integrate_lua/lua_debuger`）。

### 1.3 性能分析与调试 (Profiling & Debugging)
- **性能分析器 (Profiler)**: 集成了 `easy_profiler`，用于分析 CPU 耗时、多线程性能瓶颈和 DrawCall 耗时。
- **调试输出**: 提供日志和控制台信息输出（集成 `spdlog` / `fmt` 等工具）。

### 1.4 物理引擎 (Physics)
- **PhysX 集成**: 完整集成了 NVIDIA PhysX 物理引擎。
- **连续碰撞检测 (CCD)**: 支持高速物体的防穿透检测 (`physx_ccd`)。
- **触发器与射线检测**: 实现了物理触发器 (`physx_ccd_trigger`) 以及场景查询 (射线投射、重叠检测 `physx_scene_query`)。

### 1.5 音频系统 (Audio)
- **FMOD 集成**: 支持播放基础的 2D 音乐（MP3, Wav）。
- **3D 音效**: 支持带有空间位置的 3D 音效 (`audio_source_3d`)。
- **Wwise 集成**: 支持使用 Wwise 专业音频引擎进行复杂的音效事件解析和 Bank 加载 (`audio_wwise/integrate`, `load_bank`)。

---

## 2. 渲染系统功能 (Rendering Features)

### 2.1 基础渲染管线 (OpenGL)
- **渲染 API**: 基于 OpenGL (glad/glfw) 的图形渲染封装。
- **多线程渲染**: 实现了渲染线程与主逻辑线程的分离，通过命令队列提交 DrawCall。
- **基础图元绘制**: 支持三角形、四边形、立方体的绘制封装 (`draw_triangle`, `draw_quad`, `draw_cube`)。

### 2.2 Shader 与材质系统 (Shader & Material)
- **Shader 管理**: 支持 Vertex Shader 和 Fragment Shader 的编译、链接与错误处理。
- **材质系统**: 支持从文件中读取材质参数并传递给 Shader，实现了自定义材质系统。
- **UBO (Uniform Buffer Object)**: 支持使用 UBO 来高效传递光照等全局变量数据 (`classic_lighting/ubo`)。

### 2.3 贴图与模型加载 (Texture & Mesh)
- **贴图格式支持**: 支持加载 PNG、JPG，以及直接加载 GPU 压缩纹理 (DXT 等)。
- **模型导入**: 
  - 集成 `Assimp` 或 `FBX SDK` 解析 FBX 等模型格式。
  - 支持将解析的模型转储为引擎自定义的 `.mesh` 格式。

### 2.4 相机系统 (Camera)
- **多相机渲染**: 支持多个相机按照指定的深度/顺序渲染场景 (`two_camera`, `camera_depth`)。
- **Culling Mask**: 相机支持 Culling Mask，可选择性地只渲染特定层级的物体 (`cullingmask`)。
- **正交与透视投影**: 支持 3D 透视相机 (`perspective_camera`) 与 2D 正交相机 (`camera_orth`) 的切换与计算。

### 2.5 动画系统 (Animation)
- **骨骼动画 (Skeleton Animation)**: 支持骨骼层级解析与动画矩阵计算。
- **蒙皮网格渲染 (Skinned Mesh Renderer)**: 支持顶点权重解析与骨骼蒙皮动画渲染计算 (`load_fbx/extra_weight`)。

### 2.6 光照与高级渲染 (Lighting & Advanced Rendering)
- **经典光照模型**: 实现了环境光 (Ambient)、漫反射 (Diffuse)、高光等基础光照 (`classic_lighting`)。
- **离屏渲染 (RTT / FBO)**: 支持渲染到纹理 (Render to Texture) 和帧缓冲对象 (`engine_editor/rtt`, `rbo`)。
- **延迟渲染 (Deferred Rendering)**: 实现了 G-Buffer 的写入与解析 (`gbuffer`)。
- **屏幕空间环境光遮蔽 (SSAO)**: 实现了 SSAO 特效以增强场景立体感 (`deferred_rendering/ssao`)。

---

## 3. UI 系统与编辑器功能 (UI & Editor)

### 3.1 GUI 系统基础控件
- **UIImage**: 基础图片显示组件 (`ui_image`)。
- **UIText**: 基于 FreeType 库实现的文字渲染，支持彩色文字和单 Alpha 通道文字绘制 (`ui_text`, `draw_ttf_font`)。
- **UIMask**: 实现了 UI 遮罩功能（如滚动视图裁剪） (`ui_mask`)。
- **UIButton**: 实现了按钮的交互逻辑及状态切换 (`ui_button`)。

### 3.2 引擎编辑器基础设施
- **ImGui 集成**: 集成了 `Dear ImGui` 用于快速搭建引擎编辑器界面。
- **层级面板与属性面板**: 包含对场景树 (Hierarchy) 和对象属性 (Inspector) 的显示与操作探索。

---

## 4. 范例 (Samples) 列表清单

项目 `samples` 目录下提供了非常多细分的测试和教学范例，可以作为开发具体功能时的直接参考：

- **音频范例 (`audio`, `audio_wwise`)**:
  - `fmod_play_2d_audio`: 播放基础 2D 声音。
  - `audio_source_3d`: 3D 空间音效测试。
  - `load_bank`: Wwise Bank 加载测试。
  - `hunter`: 综合音频测试案例。

- **相机与视图范例 (`camera`)**:
  - `camera_depth`: 相机深度排序测试。
  - `cullingmask`: 相机剔除遮罩测试。
  - `perspective_camera` & `two_camera`: 透视及多相机分屏渲染测试。

- **光照范例 (`classic_lighting`)**:
  - `ambient`, `diffuse`: 环境光与漫反射测试。
  - `ubo`: 统一缓冲对象传递光照参数测试。

- **组件与逻辑范例 (`component`, `control`, `integrate_lua`)**:
  - `test_rttr`: C++ 反射库测试（组件系统的基础）。
  - `key_callback`: 键盘、鼠标输入回调测试。
  - `lua_debuger`: Lua 脚本绑定与断点调试测试。

- **基础渲染范例 (`draw_polygon`, `draw_font`)**:
  - `draw_triangle`, `draw_quad`, `draw_cube`: 最基础的 OpenGL 几何体绘制。
  - `draw_ttf_font`: 使用 FreeType 绘制 TTF 字体。

- **高级渲染与编辑器范例 (`engine_editor`, `deferred_rendering`)**:
  - `rtt`, `rbo`, `draw_rtt`: 渲染到纹理与帧缓冲器。
  - `gbuffer`, `ssao`: 延迟渲染与 SSAO 后处理特效。
  - `opengl_in_qt`: 将 OpenGL 渲染嵌入到 Qt 窗口中的测试。

- **UI 控件范例 (`gui`)**:
  - `camera_orth`: 正交相机（UI 渲染基础）。
  - `ui_image`, `ui_text`, `ui_button`, `ui_mask`: 单个 UI 控件的独立测试工程。

- **物理范例 (`physx`)**:
  - `hello_physx`, `integrate_physx`: 物理引擎初始化及刚体掉落测试。
  - `physx_ccd`, `physx_ccd_trigger`: 高速防穿透与触发器测试。
  - `physx_scene_query`: 物理射线与碰撞检测测试。

- **模型导入范例 (`load_fbx`)**:
  - `extra_mesh`, `extra_weight`: 提取 FBX 顶点、骨骼与权重信息并进行渲染。

- **工具与配置 (`template`)**:
  - 提供了一套完整的模板工程，包含了 stb_image, glm, fmt, rapidjson 等游戏引擎必备的第三方库依赖，以及基础的 shader、材质和模型数据。

---
*总结：该项目从 0 到 1 演示了一个现代游戏引擎应具备的核心模块。在您自研引擎时，可以参考它的模块划分结构（如 Renderer、Physics、Audio、GUI、Scripting），以及如何将众多第三方库（OpenGL, PhysX, FMOD/Wwise, FreeType, Sol2, ImGui）优雅地集成到统一的架构中。*