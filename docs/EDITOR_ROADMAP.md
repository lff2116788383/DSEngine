# DSEngine Editor 改造路线图

> 基于当前 `apps/editor_cpp/` 代码审查结果，制定分阶段改造计划。
> 技术路线：**ImGui 深度美化 + 功能补全**，不更换 UI 框架。

---

## 一、当前编辑器现状审查

### 已完成功能 ✅

| 模块 | 文件 | 状态 | 备注 |
|---|---|---|---|
| Docking 布局 | `editor_shell.cpp` | ✅ 完整 | Unity 风格：左 Hierarchy、右 Inspector、中 Scene/Game、下 Project/Console/Profiler |
| 自定义主题 | `main.cpp:59-126` | ✅ 基础 | 暗色科技风 + 蓝色强调色，但无自定义字体/图标 |
| Hierarchy 面板 | `editor_hierarchy_panel.cpp` (210行) | ✅ 基础 | 实体列表/选择/右键创建(Empty/UI/3D)/删除/复制 |
| Inspector 面板 | `editor_inspector_panel.cpp` (975行) | ✅ 丰富 | Transform/Sprite/Camera2D·3D/RigidBody/UI组件/Particle/Animator/Terrain/PostProcess等 |
| Scene Viewport | `editor_viewport_panel.cpp` (123行) | ✅ 基础 | 渲染纹理显示 + ImGuizmo(移动/旋转/缩放) + 2D点选 |
| Game Viewport | 同上 | ✅ 基础 | 渲染纹理显示 |
| Toolbar | `editor_toolbar.cpp` (171行) | ✅ 基础 | Gizmo工具/Local·World/2D·3D/Play·Stop·Pause/语言切换 |
| Project 面板 | `editor_aux_panels.cpp` | ✅ 基础 | 目录浏览/文件拖拽/右键创建文件夹·脚本·材质 |
| Profiler 面板 | `editor_profiler_panel.cpp` (259行) | ✅ 完整 | CPU/Memory/Render 三维度 + 历史曲线 + 导出 |
| Scene IO | `editor_scene_io.cpp` (64KB) | ✅ 完整 | JSON 序列化/反序列化所有组件 |
| Undo 系统 | `editor_undo.h` (282行) | ✅ 框架完成 | Command Pattern: PropertyChange/Lambda/Compound/Manager |
| Play 模式 | `editor_toolbar.cpp` | ✅ 完整 | 进入时备份 registry，退出时恢复 |
| 国际化预览 | `editor_aux_panels.cpp` | ✅ 完整 | 多语言切换 + 参数预览 + 应用到 UILabel |

### 未完成 / 缺失功能

| 优先级 | 模块 | 现状 | 说明 |
|---|---|---|---|
| **P0** | 自定义字体 + 图标字体 | ✅ Phase 1 完成 | Inter + NotoSansSC + FA6 Solid |
| **P0** | Undo/Redo 集成 | ✅ Phase 2.5 完成 | Transform + Entity 操作均可撤销 |
| **P0** | 快捷键系统 | ✅ Phase 2 完成 | Ctrl+Z/Y/S/O/D, Delete, F, F2 |
| **P1** | Console 面板 | ✅ Phase 2 完成 | spdlog 集成 + 过滤 + 右键菜单 + 双击复制 |
| **P1** | Animation 面板 | ✅ Phase 4 完成 | 时间轴编辑器 + Play/Pause/Scrub + 缩放平移 + 关键帧菱形显示 |
| **P1** | Tile Palette | ✅ Phase 5 完成 | 瓦片笔刷/填充/橡皮 + 多尺寸笔刷 + 网格覆盖 + Undo |
| **P1** | Scene 视图相机控制 | ✅ Phase 2 完成 | Orbit/Pan/Zoom + 渲染管线对接 |
| **P1** | Hierarchy 搜索/过滤 | ✅ Phase 2 完成 | 搜索框 + 双击重命名 |
| **P1** | 文件对话框 | ✅ Phase 2 完成 | Windows 原生 OPENFILENAMEW |
| **P2** | Project 面板增强 | ✅ Phase 3 完成 | 搜索/Grid·List/右键菜单/Explorer集成 |
| **P2** | Toolbar 图标化 | ✅ Phase 1 完成 | MDI 图标 + 彩色背景 |
| **P2** | Edit/Window 菜单 | ✅ Phase 2 完成 | Undo/Redo 菜单项 + 灰度状态 |
| **P2** | 状态栏 | ✅ Phase 3 完成 | FPS/实体数/Draw Calls/Gizmo工具/坐标系 |
| **P2** | 多选实体 | ✅ Phase 3 完成 | Ctrl+Click/Shift+Click/批量Delete·Duplicate |
| **P2** | Gizmo 多选同时拖动 | ✅ Phase 5 完成 | 中心点 Gizmo + delta 批量应用 + Undo |
| **P2** | 实体重命名 | ✅ Phase 2 完成 | Hierarchy 双击重命名 |
| **P3** | 材质编辑器 | ✅ Phase 4 完成 | PBR 属性编辑 + 纹理槽拖拽 + 预览球 + Shader选择 |
| **P3** | 地形编辑器 | ✅ Phase 5 完成 | 高度笔刷(Raise/Lower/Smooth/Flatten) + Splat Map 纹理绘制 + Undo |
| **P3** | 音频编辑面板 | ❌ 无 | 引擎有 audio 模块但编辑器无面板 |
| **P3** | Prefab 系统 | ✅ Phase 4 完成 | Save as Prefab / .dprefab 拖拽实例化 / Prefab 标记 |
| **P3** | Settings/Preferences | ✅ Phase 3 完成 | editor_settings.json 持久化 |

---

## 二、分阶段改造计划

### Phase 1：视觉焕新（预计 3-5 天）✅ 已完成

**目标**：不改功能逻辑，纯视觉升级，让编辑器看起来专业。

> **实施记录 (2026-05-06)**：
> - 新增 `editor_theme.h/cpp`：封装字体加载（Inter + NotoSansSC + FA6 合并）+ Hazel 风格暗色主题
> - 新增 `editor_icons.h`：Font Awesome 6 Solid 图标 codepoint 常量子集（UTF-8 编码）
> - 新增 `fonts/download_fonts.ps1`：一键下载所需字体
> - 修改 `main.cpp`：集成字体加载 + 主题迁移
> - 修改 `editor_toolbar.cpp`：图标化按钮 + 竖向分隔线
> - 修改 `editor_inspector_panel.cpp`：组件头图标前缀 + Vec3 彩色 XYZ 标签
> - 修改 `editor_hierarchy_panel.cpp`：实体类型图标 + 选中行圆角高亮
> - 修改 `CMakeLists.txt`：加入 editor_theme.cpp
> - 编译验证通过 (Debug, MSVC)
>
> **Bug Fix (2026-05-06)**：
> - 修复启动即 abort 的崩溃：移除 `editor_theme.cpp` 中的 `io.Fonts->Build()` 调用
> - 根因：ImGui 1.92 引入 `ImGuiBackendFlags_RendererHasTextures`，新版后端自动管理字体图集构建，
>   手动调用 `Build()` 会设置 `PreloadedAllGlyphsRanges=true`，导致 `ImGui::NewFrame()` 内部
>   `IM_ASSERT_USER_ERROR` 断言失败（`imgui_draw.cpp:2774`）
> - 同时清理了调试代码（文件日志、SIGABRT handler、SEH filter 等）

#### 1.1 自定义字体集成 ✅
- ✅ 集成 **Inter** 作为 UI 主字体（16px，清晰的无衬线体）
- ✅ 集成 **Noto Sans SC** 作为中文 fallback
- ✅ 集成 **Material Design Icons** 作为图标字体（merge 到字体 atlas）
- ✅ 字体文件放入 `apps/editor_cpp/fonts/`（含下载脚本）

#### 1.2 主题精调 ✅
- ✅ 迁移 `SetupImGuiStyle()` → `editor_theme.cpp::SetupEditorStyle()`
  - ✅ 参考 Hazel Engine 暗色主题
  - ✅ 强调色调整为 `(0.28,0.56,1)`
- ✅ 增大圆角 `WindowRounding=6, FrameRounding=4`
- ✅ 统一间距：`ItemSpacing=(8,6)`, `FramePadding=(8,5)`

#### 1.3 Toolbar 图标化 ✅
- ✅ 替换 `[H][M][R][S]` 为 MDI 图标（cursor, arrow-all, rotate-3d, resize）
- ✅ Play/Stop/Pause/Step 使用 MDI 图标 + 彩色背景
- ✅ 分组用 `ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical)` 视觉分隔

#### 1.4 Inspector 美化 ✅
- ✅ 所有 Component 折叠头使用 MDI 图标前缀（19 个组件全覆盖）
- ✅ Transform Vec3 编辑器使用 `DrawVec3WithColorLabels` — X(红)/Y(绿)/Z(蓝) 彩色方块标签

#### 1.5 Hierarchy 美化 ✅
- ✅ 实体名前增加类型图标（Camera/Light/Mesh/UI/Particle/Terrain 等，基于组件检测）
- ✅ 选中行高亮用圆角矩形 + accent blue (30% alpha)

### Phase 2：核心功能补全（预计 5-7 天）✅ 已完成

> **实施记录 (2026-05-06)**：
> - 新增 `editor_shortcuts.cpp/h`：全局快捷键系统（Ctrl+Z/Y/S/O/D, Delete, F, F2）
> - 新增 `editor_undo.h`：Command Pattern 框架（ICommand/PropertyChange/Lambda/Compound/UndoRedoManager）
> - 新增 `editor_scene_camera.cpp/h`：编辑器场景相机（Orbit/Pan/Zoom/Focus）
> - 新增 `editor_console_panel.cpp/h`：spdlog 集成 Console 面板（级别过滤/搜索/Clear/Auto-scroll）
> - 新增 `editor_file_dialog.cpp/h`：Windows 原生文件对话框（OPENFILENAMEW）
> - 修改 `editor_hierarchy_panel.cpp`：搜索框过滤 + 双击重命名（Enter 确认/Esc 取消）
> - 修改 `editor_shell.cpp`：Edit 菜单 Undo/Redo 项（灰度状态/命令名显示）
> - 修改 `editor_viewport_panel.cpp`：集成编辑器相机输入处理
> - 修改 `main.cpp`：安装 EditorLogSink、调用快捷键处理
> - 编译验证通过 (Debug, MSVC)

#### 2.1 Undo/Redo 集成 ✅
- ✅ 全局 `UndoRedoManager` 实例（`GetUndoRedoManager()` 单例）
- ✅ Inspector Transform 修改通过 `PropertyChangeCommand` 提交（Position/Rotation/Scale）
- ✅ Hierarchy 创建/删除/复制/重命名通过 `LambdaCommand` 提交
- ⬜ Gizmo 拖拽通过 Merge 机制合并连续操作（待后续优化）

#### 2.2 快捷键系统 ✅
```
Ctrl+Z    → Undo           ✅
Ctrl+Y    → Redo           ✅
Ctrl+S    → Save Scene     ✅
Ctrl+Shift+S → Save As    ✅
Ctrl+O    → Open Scene     ✅
Ctrl+D    → Duplicate Entity ✅
Delete    → Delete Entity  ✅
F2        → Rename Entity  ✅
F         → Focus Selected ✅
```

#### 2.3 Scene 视图相机 ✅
- ✅ 右键拖拽 → 旋转 (Orbit)
- ✅ 中键拖拽 → 平移 (Pan)
- ✅ 滚轮 → 缩放 (Zoom)
- ⬜ Alt+左键 → Orbit（Maya 风格，待后续）
- ✅ F 键 → 聚焦选中实体
- ✅ 编辑器相机独立于游戏相机

#### 2.4 文件对话框 ✅
- ✅ 使用 Windows 原生 `OPENFILENAMEW` API
- ✅ Open Scene → 打开 `.json` / `.dscene` 文件
- ✅ Save As → 选择保存路径
- ⬜ Recent Files 列表（待后续优化）

#### 2.5 Console 面板实现 ✅
- ✅ 集成引擎日志系统（spdlog custom sink → `EditorLogSink`）
- ✅ 显示 Info/Warning/Error + 颜色 + 时间戳 + 级别图标
- ✅ 支持过滤（按级别 toggle + 搜索框关键字）
- ✅ Clear 按钮
- ✅ 自动滚动到底部
- ✅ 右键上下文菜单（Copy All / Clear）
- ✅ 双击日志条目自动复制到剪贴板

#### 2.6 Hierarchy 增强 ✅
- ✅ 搜索框（实时过滤实体名）
- ✅ 双击重命名（Enter 确认 / Esc 取消 / 点击其他区域确认）
- ⬜ 拖拽排序 / 父子关系调整（待后续）

### Phase 2.5：运行时验证 + 质量打磨 ✅ 已完成

> **实施记录 (2026-05-06)**：
> 在 Phase 2 功能代码完成后，执行运行时验证并深度打磨集成质量。

#### 2.5.1 Undo/Redo 深度集成 ✅
- ✅ Inspector Transform（Position/Rotation/Scale）修改时 push `PropertyChangeCommand`
  - 使用 `ImGui::IsItemActivated()` 记录编辑起始值
  - 使用 `ImGui::IsItemDeactivatedAfterEdit()` 检测编辑提交
- ✅ Hierarchy 操作 push `LambdaCommand`：
  - Create Entity → undo = destroy
  - Delete Entity → undo = recreate（保存 name + transform）
  - Duplicate Entity → undo = destroy clone
  - Rename Entity → undo = restore old name
- ✅ Ctrl+Z/Y 验证正常工作

#### 2.5.2 Console 面板增强 ✅
- ✅ 右键上下文菜单：Copy All、Clear
- ✅ 双击日志条目 → 自动复制到系统剪贴板
- ✅ 关键操作添加 `EditorLog` 调用：
  - 场景 New/Open/Save/Save As
  - 实体 Create/Delete/Duplicate

#### 2.5.3 Scene 相机与渲染管线对接 ✅
- ✅ `RenderPassContext` 新增 `use_editor_camera` / `editor_view` / `editor_projection` 字段
- ✅ `ForwardScenePass::Execute` 在 editor 模式下优先使用编辑器相机矩阵
- ✅ `FramePipeline::SetEditorCamera()` / `DisableEditorCamera()` 公共接口
- ✅ `main.cpp` 每帧注入编辑器相机（Edit 模式），Play 模式自动切回游戏相机

#### 2.5.4 样式与 UX 打磨 ✅
- ✅ 窗口标题显示当前场景文件名 + 编辑器状态（`[PLAYING]`/`[PAUSED]`）
- ✅ `GetCurrentScenePath()` / `SetCurrentScenePath()` 全局追踪
- ✅ Play 模式下：Hierarchy 禁用创建/删除/复制、Inspector 禁用编辑、Scene 相机切回游戏相机
- ✅ 文件操作全部同步更新窗口标题

---

### Phase 3：进阶面板实现 ✅ 已完成

> **实施记录 (2026-05-06)**：
> - 新增 `editor_status_bar.cpp/h`：固定高度底部状态栏（FPS/实体数/Draw Calls/Gizmo工具/坐标系）
> - 新增 `editor_selection.h`：`SelectionManager` 单例，管理多选实体列表
> - 新增 `editor_settings.cpp/h`：编辑器配置 JSON 持久化（rapidjson，保存到 bin/editor_settings.json）
> - 修改 `editor_viewport_panel.cpp`：Gizmo 拖拽 Undo/Redo 集成（开始记录→结束 push LambdaCommand）
> - 修改 `editor_hierarchy_panel.cpp`：Ctrl+Click 多选 / Shift+Click 范围选
> - 修改 `editor_inspector_panel.cpp`：多选时显示选中数量 + Transform 平均值
> - 修改 `editor_shortcuts.cpp`：Delete/Duplicate 支持批量操作
> - 修改 `editor_aux_panels.cpp`：Project 面板增强（搜索框/Grid·List切换/排序/右键菜单/Explorer集成）
> - 修改 `editor_shell.cpp`：File 菜单新增 "Recent Files" 子菜单
> - 修改 `main.cpp`：启动加载设置+自动恢复上次场景，退出保存设置
> - 编译验证通过 (Debug, MSVC)

#### 3.1 Status Bar（底部状态栏）✅
- ✅ 固定高度 24px，覆盖窗口最下方（NoDocking）
- ✅ 显示 FPS | 实体数量 | Draw Calls | 当前 Gizmo 工具 | 坐标系(Local/World)
- ✅ 数据来源：`g_cpu_profiler.GetFrameStats()` / `g_render_profiler.GetCurrentFrameStats()`

#### 3.2 Gizmo 拖拽 Undo/Redo 集成 ✅
- ✅ 使用 `ImGuizmo::IsUsing()` 检测拖拽开始/结束
- ✅ 开始时记录初始 Transform（position/rotation/scale）
- ✅ 结束时 push `LambdaCommand`（execute=设为终值，undo=恢复初值）
- ✅ 避免每帧 push（只在"上帧 using → 本帧 not using"时提交一次）

#### 3.3 多选实体支持 ✅
- ✅ `SelectionManager` 单例管理 `std::vector<entt::entity>`
- ✅ Hierarchy 面板：Ctrl+Click 切换选择、Shift+Click 范围选、普通点击单选
- ✅ Inspector 面板：多选时显示选中数量 + Transform 平均值
- ✅ Delete/Duplicate 快捷键支持批量操作
- ✅ Gizmo 多选同时拖动（Phase 5：中心点矩阵 + delta 批量平移 + Undo）

#### 3.4 Asset Browser（Project 面板升级）✅
- ✅ 网格视图 + 列表视图切换（Grid/List 按钮）
- ✅ 文件搜索框（大小写不敏感子串匹配）
- ✅ 文件/文件夹排序（目录优先 + 字母序）
- ✅ 右键菜单：重命名 / 删除 / 复制路径 / 在 Explorer 中显示
- ✅ 内联重命名（Enter 确认 / Esc 取消）
- ✅ 拖拽文件到 Scene（`ASSET_PATH` payload）
- ✅ 缩略图预览（stb_image + GL 纹理缓存，支持 .png/.jpg/.bmp/.tga）

#### 3.5 编辑器配置持久化 ✅
- ✅ `editor_settings.json`（rapidjson 序列化）保存到 `bin/` 目录
- ✅ 持久化内容：Recent Files 列表、上次场景路径、Gizmo 默认工具/坐标系
- ✅ 启动时自动加载上次场景
- ✅ File → Recent Files 子菜单
- ✅ 退出时自动保存

#### 3.6 Animation 面板 / Material Editor（未实现，移入 Phase 4）
- ⬜ 时间轴控件 + 关键帧编辑
- ⬜ PBR 材质可视化编辑

### Phase 4：高级功能（预计 10+ 天，按需）

#### 4.1 Prefab 系统
- 将实体导出为 `.dprefab` 文件
- 从 Prefab 实例化（保持引用关系）
- Prefab 覆盖高亮显示

#### 4.2 Tile Map 编辑器
- 瓦片集加载与选择
- 笔刷/填充/橡皮擦工具
- 多层 Tilemap 支持
- 与 2D 物理碰撞自动绑定

#### 4.3 地形编辑器
- 高度图笔刷绘制
- 纹理绘制（Splat Map）
- 地形尺寸/分辨率设置

#### 4.4 音频编辑面板
- AudioSource 组件参数编辑
- 3D 音频区域可视化
- 音频文件预览播放

#### 4.5 Settings/Preferences
- 编辑器配置面板
- 主题切换（深色/浅色/自定义）
- 快捷键自定义
- 网格显示设置
- 配置持久化到 `editor_settings.json`

---

## 三、第三方依赖清单

| 库 | 用途 | 许可 | 集成方式 |
|---|---|---|---|
| [Inter Font](https://github.com/rsms/inter) | UI 主字体 | OFL | 字体文件 |
| [Noto Sans SC](https://fonts.google.com/noto/specimen/Noto+Sans+SC) | 中文字体 | OFL | 字体文件 |
| [Material Design Icons](https://github.com/google/material-design-icons) | 图标字体 | Apache 2.0 | .ttf merge 到 atlas |
| [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) | 原生文件对话框 | Zlib | 单 .c/.h |
| [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders) | 图标字体常量定义 | Zlib | 单 .h |

所有依赖均免费、MIT/Zlib/OFL/Apache 许可，符合轻量引擎定位。

---

## 四、文件结构规划

```
apps/editor_cpp/
├── src/
│   ├── main.cpp                      # 主循环（已有）
│   ├── editor_shell.cpp/h            # Docking 框架（已有）
│   ├── editor_theme.cpp/h            # ★ 新增：主题 + 字体管理
│   ├── editor_icons.h                # ★ 新增：图标常量定义
│   ├── editor_shortcuts.cpp/h        # ★ 新增：快捷键系统
│   ├── editor_scene_camera.cpp/h     # ★ 新增：编辑器场景相机
│   ├── editor_console_panel.cpp/h    # ★ 新增：替代占位 Console
│   ├── editor_status_bar.cpp/h       # ★ 新增：底部状态栏
│   ├── editor_selection.h            # ★ 新增：多选实体管理器
│   ├── editor_settings.cpp/h         # ★ 新增：编辑器配置持久化
│   ├── editor_toolbar.cpp/h          # 改造：图标化
│   ├── editor_hierarchy_panel.cpp/h  # 改造：搜索 + 重命名 + 拖拽
│   ├── editor_inspector_panel.cpp/h  # 改造：美化 + Undo 集成
│   ├── editor_viewport_panel.cpp/h   # 改造：场景相机控制
│   ├── editor_aux_panels.cpp/h       # 改造：Project 面板升级
│   ├── editor_profiler_panel.cpp/h   # 已完成，微调美化
│   ├── editor_scene_io.cpp/h         # 已完成
│   ├── editor_undo.h                 # 已完成框架
│   └── editor_shared_components.h    # 已有
├── fonts/
│   ├── Inter-Regular.ttf
│   ├── Inter-Bold.ttf
│   ├── NotoSansSC-Regular.ttf
│   └── MaterialIcons-Regular.ttf
```

---

## 五、里程碑与验收标准

### Phase 1 验收
- [x] 编辑器启动后使用 Inter 字体，中文正常显示
- [x] Toolbar 使用图标替代文字标签
- [x] Inspector 的 Vec3 属性带 X/Y/Z 彩色标签
- [x] Component 标题带图标前缀
- [x] 启动无崩溃，主循环正常运行
- [ ] 截图对比改造前后效果

### Phase 2 验收
- [x] Ctrl+Z/Y 可撤销/重做 Inspector 属性修改
- [x] Ctrl+S 保存场景，Ctrl+O 弹出文件对话框打开场景
- [x] Scene 视图可右键旋转/中键平移/滚轮缩放
- [x] Console 面板显示引擎实际日志
- [x] Hierarchy 搜索框可过滤实体

### Phase 2.5 验收
- [x] Undo/Redo 深度集成：Transform 属性修改可撤销
- [x] Undo/Redo 深度集成：实体创建/删除/复制/重命名可撤销
- [x] Console 右键菜单 Copy All / Clear 正常工作
- [x] Console 双击复制到剪贴板
- [x] 编辑器相机视角反映到 Scene 渲染纹理
- [x] Play 模式切回游戏相机
- [x] 窗口标题显示当前场景文件名

### Phase 3 验收
- [x] 状态栏实时显示 FPS/实体数/Draw Calls/Gizmo工具
- [x] Gizmo 拖拽操作可 Undo/Redo
- [x] Hierarchy 支持 Ctrl+Click 多选、Shift+Click 范围选
- [x] Delete/Duplicate 支持批量操作
- [x] Project 面板支持搜索、Grid/List 切换、右键上下文菜单
- [x] 编辑器设置持久化（recent files/gizmo 默认值/上次场景自动加载）
- [x] Animation 面板时间轴（移入 Phase 4）
- [x] Material Editor PBR 编辑（移入 Phase 4）

### Phase 4 验收
- [x] Animation 时间轴面板：自定义 ImDrawList 绘制、缩放/平移、关键帧菱形、红色播放头拖拽
- [x] Material Editor 面板：PBR 属性（Albedo/Metallic/Roughness/AO/Emissive）+ 纹理槽拖拽 + 预览球
- [x] Prefab 系统：Save as Prefab (.dprefab JSON) / Project 面板拖拽实例化 / Hierarchy 标记
- [x] Hierarchy 拖拽父子关系：拖拽建立 ParentComponent / 拖到 Scene 根解除 / Undo 支持 / 缩进显示
- [x] Editor Preferences 面板：Window 菜单打开 / 主题切换 / Snap 设置 / 快捷键查看

### Phase 5 验收
- [x] Gizmo 多选同时拖动：centroid 计算 + ImGuizmo 虚拟矩阵 + delta 批量平移 + Undo
- [x] Asset Browser 缩略图预览：stb_image 加载 + GL 纹理缓存 + 目录切换释放
- [x] Tilemap 绘制 Undo：笔触开始 snapshot tiles、松开 push LambdaCommand
- [x] Terrain 雕刻 Undo：笔触开始 snapshot height_data、松开 push LambdaCommand（merge_id）
- [x] Scene Viewport Color-ID 拾取：FBO + GLSL 着色器 + entity ID→RGB 编码 + glReadPixels
- [x] Splat Map 地形纹理绘制：4层 RGBA splat_data + 高斯笔刷 + 权重归一化 + Undo
- [x] Terrain 面板 Sculpt/Splat 模式切换 + 层选择 + Opacity 滑块 + 纹理路径输入 + Reset

---

### Phase 5：编辑器深度功能（2026-05-06）✅ 已完成

> **实施记录**：
> - 修改 `editor_viewport_panel.cpp`：
>   - 多选 Gizmo：centroid 计算 → ImGuizmo 虚拟矩阵 → delta 批量平移 → Undo（merge_id=gizmo_multi_transform）
>   - Color-ID FBO 拾取：`ColorIDPicker` 结构体（FBO + GLSL 着色器 + entity ID→RGB 编码 + glReadPixels），替换原始距离拾取
> - 修改 `editor_aux_panels.cpp`：
>   - Asset Browser Grid 视图缩略图：stb_image 加载 + GL 纹理缓存 + 目录切换清理
> - 修改 `editor_tilemap_panel.h/cpp`：
>   - Tilemap 绘制 Undo：`painting` 状态跟踪 + `tiles_snapshot` + LambdaCommand
> - 修改 `editor_terrain_panel.h/cpp`：
>   - Terrain 雕刻 Undo：`height_snapshot` + LambdaCommand（merge_id=terrain_sculpt_XXX）
>   - Splat Map 纹理绘制：`ApplySplatBrush` 高斯笔刷权重归一化 + `splat_snapshot` Undo
>   - Sculpt/Splat 模式切换 UI + 层选择 + Opacity 滑块 + 纹理路径输入 + Reset
> - 修改 `engine/ecs/components_3d.h`：
>   - `TerrainComponent` 新增 `splat_data`（4 float/vertex）、`splat_texture_paths[4]`、`splat_texture_handles[4]`、`splat_dirty`
> - 字体文件提交到仓库（`apps/editor_cpp/fonts/*.ttf`）
> - 编译零错误，unit + integration 测试 100% 通过

#### 5.1 Gizmo 多选同时拖动 ✅
- ✅ `SelectionManager::IsMultiSelect()` 检测多选状态
- ✅ 计算所有选中实体 Transform 的中心点（centroid）
- ✅ 用 centroid 矩阵驱动 `ImGuizmo::Manipulate`（仅支持平移）
- ✅ 计算 delta 并应用到所有选中实体
- ✅ Undo：`LambdaCommand` 包含所有实体 before/after 快照（merge_id=gizmo_multi_transform）

#### 5.2 Asset Browser 缩略图预览 ✅
- ✅ Grid 视图中图片文件（.png/.jpg/.jpeg/.bmp/.tga）显示缩略图
- ✅ `stb_image` 加载 + `glGenTextures` 创建 GL 纹理
- ✅ `ThumbnailEntry` 缓存（path→texture_id），目录切换时 `glDeleteTextures` 释放

#### 5.3 Tilemap/Terrain Undo 支持 ✅
- ✅ Tilemap：`painting` + `tiles_snapshot`，笔触结束 push `LambdaCommand`
- ✅ Terrain 雕刻：`height_snapshot`，笔触结束 push `LambdaCommand`（merge_id=terrain_sculpt_XXX）
- ✅ Terrain Splat：`splat_snapshot`，笔触结束 push `LambdaCommand`（merge_id=terrain_splat_XXX）

#### 5.4 Scene Viewport Color-ID 拾取 ✅
- ✅ `ColorIDPicker` 结构体：FBO（RGBA8 + Depth24）+ GLSL 330 着色器
- ✅ Entity ID 编码为 RGB 颜色（+1 偏移避免与 clear 冲突）
- ✅ 屏幕空间投影：VP 矩阵 → NDC → 屏幕坐标 → 14px 色块
- ✅ `glReadPixels` 单点读取 → 解码 entity
- ✅ GL 状态保存/恢复（FBO/Viewport/Blend）

#### 5.5 Splat Map 地形纹理绘制 ✅
- ✅ `TerrainComponent` 新增 `splat_data`（resolution_x × resolution_z × 4 float）
- ✅ 4 层纹理路径 + texture handles
- ✅ `EnsureSplatData()`：首次使用时初始化（layer 0 = 1.0，其余 = 0.0）
- ✅ `ApplySplatBrush()`：高斯衰减 + 目标层增加 + 其他层按比例扣减（总和归一化）
- ✅ UI：Sculpt/Splat 模式按钮切换 + 4 层彩色按钮 + Opacity 滑块 + 纹理路径输入 + Reset

---

### Phase 6：编辑器功能完善（2026-05-06）✅ 已完成

> **实现摘要**：
> - 修改 `editor_inspector_panel.cpp`：
>   - 新增 `InspectorUndoCheck<T>` 通用模板 + `MakeCompSetter<Comp, Field>` 辅助函数
>   - 新增 `INSPECTOR_PROPERTY_U` 变参宏（`/Zc:preprocessor` 适配 MSVC）
>   - 为 SpriteRenderer、RigidBody2D、MeshRenderer PBR(5 属性)、Camera3D(4 属性)、DirectionalLight(4 属性)、PointLight(4 属性)、SpotLight(7 属性)、SkyLight(3 属性) 添加 Undo
>   - MeshRenderer mesh_path、Skybox cubemap_path、Animator3D skeleton/anim_path 添加 ASSET_PATH 拖拽接收
> - 修改 `editor_viewport_panel.cpp`：
>   - Marquee 选框状态结构 + 鼠标拖拽逻辑 + 半透明蓝色选框绘制
>   - 替换点选逻辑为框选/点选混合，支持 Ctrl 追加选择
> - 修改 `main.cpp`：
>   - Edit 模式下跳过 FixedUpdate（物理）和 Update（业务逻辑/脚本/AI），仅保留 Render + Input
> - 修改 `apps/editor_cpp/CMakeLists.txt`：
>   - 添加 `/Zc:preprocessor` 编译选项
> - 编译零错误，unit + integration 测试通过

#### 6.1 音频编辑面板（P3，新增文件）✅
- ✅ `DrawAudioPanel(registry, selected_entity)` 音频源参数编辑
- ✅ volume 滑块 / pitch / loop / spatial_enabled / 距离衰减曲线
- ✅ Play/Stop/Pause 按钮用于编辑器中预听音频
- ✅ 3D 音频：Scene Viewport 中叠加球形衰减范围可视化
- ✅ Inspector Add Component 菜单新增 "Audio Source" 和 "Audio Listener"

#### 6.2 Scene Viewport Grid 网格地面（P2）✅
- ✅ Scene 视图中绘制无限网格地面（XZ 平面，Y=0）
- ✅ 粗线间距 10 单位，细线间距 1 单位，X 轴红线、Z 轴蓝线
- ✅ 根据相机距离自适应 LOD（远处隐藏细线）
- ✅ 可在 Preferences 面板中开关

#### 6.3 Gizmo Snap 吸附支持（P2）✅
- ✅ 按住 Ctrl 拖动 Gizmo 时启用 Snap
- ✅ 平移/旋转/缩放 Snap 间距可配置
- ✅ Snap 设置保存到 `editor_settings.json`

#### 6.4 Hierarchy 拖拽排序（P2）✅
- ✅ 同级实体间拖拽调整顺序（蓝色插入线）
- ✅ `SiblingIndex` 组件保存到场景文件
- ✅ Undo 支持

#### 6.5 Scene Viewport 框选（Marquee Selection）（P2）✅
- ✅ Scene 视口中按住左键拖拽绘制选框矩形（半透明蓝色 + 白色边框）
- ✅ 松开时选中框内所有实体（Color-ID FBO 批量读取）
- ✅ Ctrl+拖拽追加选择，普通拖拽替换选择

#### 6.6 Inspector Undo 补全（P1）✅
- ✅ `InspectorUndoCheck<T>` 通用 Undo 辅助模板 + `MakeCompSetter` 辅助
- ✅ `INSPECTOR_PROPERTY_U` 变参宏自动跟踪 Activated/DeactivatedAfterEdit
- ✅ 覆盖：SpriteRenderer、RigidBody2D、MeshRenderer(5)、Camera3D(4)、DirectionalLight(4)、PointLight(4)、SpotLight(7)、SkyLight(3)

#### 6.7 Asset 拖拽到 Inspector 槽位（P2）✅
- ✅ MeshRenderer mesh_path：接受 .obj/.fbx/.gltf/.glb/.dae 拖入
- ✅ Skybox cubemap_path：接受 ASSET_PATH 拖入
- ✅ Animator3D skeleton_path (.dskel) / anim_path (.danim)：扩展名过滤拖入
- ✅ SpriteRenderer shader_variant：已有 ASSET_PATH 拖拽支持

#### 6.8 编辑器性能优化：Edit 模式系统裁剪（P2）✅
- ✅ Edit 模式下跳过 FixedUpdate（物理 Physics2D/3D）
- ✅ Edit 模式下跳过 Update（业务逻辑/脚本/AI/Steering）
- ✅ 仅保留 `Time::Update()` + `pipeline->Render()` + `Input::Update()`
- ✅ Play 模式下恢复完整 `engine_instance.Tick()`

---

### Phase 7：远期可选功能 ⬜

#### 7.1 Scene Gizmo（右上角方向指示器）⬜
- ⬜ Scene 视口右上角绘制 3D 坐标轴指示器（类似 Unity Scene Gizmo）
- ⬜ 点击轴可切换到正交视图（Top/Front/Right）

#### 7.2 多场景编辑（Scene Tabs）⬜
- ⬜ 支持同时打开多个场景（标签页切换）
- ⬜ 未保存场景标签显示 * 标记

#### 7.3 Console 日志跳转源码 ⬜
- ⬜ 双击带文件路径的日志条目时，在外部编辑器中打开对应文件

#### 7.4 Terrain LOD 实时预览 ⬜
- ⬜ 在 Terrain 面板中可视化当前 LOD 级别
- ⬜ 显示三角形数量和网格线框覆盖

#### 7.5 粒子效果编辑器面板 ⬜
- ⬜ 新建独立面板用于可视化编辑粒子曲线
- ⬜ 使用 ImDrawList 绘制贝塞尔曲线编辑器

#### 7.6 Light Probe / Reflection Probe 可视化 ⬜
- ⬜ Scene 视口中显示光照探针球体
- ⬜ 显示 cubemap 预览
