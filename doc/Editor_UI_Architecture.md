# DSEngine Editor UI 架构方案 (基于 Dear ImGui)

## 1. 为什么选择 Dear ImGui？

在 DSEngine 追求“轻量级、高性能”的核心理念下，废弃基于 Electron + React 的旧架构，转向纯 C++ 与 Dear ImGui 结合的方案，是实现编辑器零开销、零拷贝视口渲染的关键。

很多开发者对 ImGui 的印象停留在“程序员用来做粗糙 Debug 面板的工具”，默认其外观是灰色、方正的工业风。但实际上，只要掌握正确的技术点和扩展方案，ImGui 完全可以实现类似于 Unreal Engine 5 或现代前端框架那样的极具现代化和科技感的 UI。

## 2. 实现现代化科技感 UI 的关键技术点

### 2.1 深度定制样式 (Style & Colors)
ImGui 并不是写死样式的，它提供了一个极其强大的 `ImGuiStyle` 结构体。
- **色彩与科技感**：可以通过修改 `ImGui::GetStyle().Colors` 数组，将默认的灰色替换为深色系（Dark Theme），并辅以高亮的科技蓝/霓虹紫点缀。
- **圆角与阴影**：通过调整 `WindowRounding`, `FrameRounding`, `PopupRounding` 等参数，可以实现现代 UI 流行的圆角卡片设计。
- **边框与间距**：精细调整 `WindowPadding`, `ItemSpacing`，让界面布局显得更“透气”和专业。

### 2.2 自定义字体与图标 (Fonts & Icons)
UI 的质感 50% 来源于字体。
- **矢量图标库**：这是现代 UI 的灵魂。在 ImGui 中，可以轻松合并（Merge）著名的图标字体（如 `FontAwesome`, `Lucide`, `Material Design Icons`）。在代码中直接写 `ImGui::Button(ICON_FA_PLAY " Start")` 就能渲染出带有漂亮图标的按钮。
- **现代中英文字体**：放弃默认的像素字体，加载如 `Roboto`, `Inter`, `Segoe UI` 或 `JetBrains Mono` 等无衬线字体，并开启 ImGui 的抗锯齿（Oversampling），文字边缘会变得非常锐利。

### 2.3 高级扩展分支 (Docking & Viewports)
如果要开发专业的游戏编辑器，**必须使用 ImGui 的 docking 分支，而不是 master 分支**。
- **Docking（停靠系统）**：允许用户像在 Visual Studio 或 VSCode 中一样，自由拖拽、拆分、合并面板（Hierarchy, Inspector, Viewport）。
- **Viewports（多视口）**：允许将 ImGui 的窗口直接拖出主窗口之外，变成操作系统的原生独立窗口，这对于多显示器开发者是刚需。

### 2.4 自定义绘制 (ImDrawList API)
如果自带的按钮、滑块依然不够“炫酷”，可以利用 ImGui 底层暴露的 `ImDrawList` API。
- 可以像使用 Canvas 一样，在窗口里直接画线、画圆、画渐变色（Gradient）、画贝塞尔曲线。
- 很多复杂的科技感 UI 组件（比如带有发光边缘的节点编辑器、动态波形图、带有动画过渡的进度条）都是通过 `ImDrawList` 手工绘制出来的。

### 2.5 社区的现代化 UI 库 (ImGui 生态)
不需要所有组件都从零手搓，目前有非常多基于 ImGui 封装的现代化组件库可以直接集成：
- **ImGuiColorTextEdit**：用于在编辑器里集成一个带语法高亮的脚本编辑器。
- **imgui-node-editor**：用于实现极具科技感的蓝图（Blueprint）连线节点系统。
- **ImGuizmo**：用于在视口中绘制 3D 坐标轴（平移、旋转、缩放操纵杆），这是 3D 编辑器必备的。

## 3. 真实案例参考

为了验证 ImGui 的表现力，可以参考以下完全基于 ImGui 构建的现代开源引擎编辑器外观：
1. **Hazel Engine (The Cherno)**：著名的高性能 C++ 引擎，其编辑器界面（深灰底色+蓝紫色点缀，圆角按钮，FontAwesome 图标）完全使用 ImGui 编写，质感非常接近 Unity 的深色主题。
2. **Lumix Engine**：极其硬核的 C++ 引擎，UI 纯 ImGui 打造，排版严谨，达到了 3A 商业引擎的视觉标准。

## 4. 总结与权衡

虽然使用 ImGui 调优 UI 样式（调色、排版）确实不如在 React 中编写 CSS 那么直观快捷，**但在 AI 的辅助下，生成 `ImGuiStyle` 的配置代码和组件封装是非常高效的**。

对于 DSEngine 来说，用**一小部分 UI 调试的成本**，换来了**免除 N-API 桥接、免除共享内存拷贝、极低的内存占用、以及零延迟的视口渲染**，这完美契合了引擎“轻量级、高性能”的初衷，是绝对划算的架构升级方案。