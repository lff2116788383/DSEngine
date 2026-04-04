# DSEngine UI 系统指南

本文档描述当前 P0 阶段已经落地的 2D UI 能力，覆盖基础渲染、交互、布局与动画。

---

## 1. 当前能力范围

当前 UI 系统已支持：

- `UIRendererComponent`：基础 UI 渲染、排序、缩放与交互状态
- `UIButtonComponent`：点击、悬停、按下颜色反馈与回调
- `UILabelComponent`：位图字体文本、数字模式、富文本颜色标签
- `UIPanelComponent`：遮挡/输入拦截
- `UICanvasScalerComponent`：参考分辨率与全局缩放
- `UIAnchorComponent`：九宫格锚点与 Stretch 拉伸
- `UIGridLayoutComponent`：网格布局、对齐、间距、动态子元素排列
- `UIAnimationComponent`：位置 / 缩放 / 透明度 / 颜色动画
- `UILayoutSystem`：统一执行 CanvasScaler / Anchor / GridLayout 布局计算
- `UISystem`：统一执行布局、事件处理、文本同步与 UI 动画更新

---

## 2. 关键组件

### 2.1 UIRendererComponent

用于定义一个 UI 元素的渲染与交互基础数据：

- 纹理与颜色：`texture` / `texture_handle` / `color` / `uv`
- 排序：`order`
- 尺寸与布局：`position` / `size` / `anchor_min` / `anchor_max` / `pivot`
- 交互：`interactable` / `is_hovered` / `is_pressed`
- 运行时：`runtime_model`

### 2.2 UIButtonComponent

提供按钮交互颜色与回调：

- `normal_color`
- `hover_color`
- `pressed_color`
- `on_click`
- `on_pointer_enter`
- `on_pointer_exit`

### 2.3 UILabelComponent

支持两种文本模式：

- 普通字符串模式：`text`
- 高性能数字模式：`numeric_mode + number_value`

并支持：

- 位图字体纹理：`font_texture_handle`
- 字符尺寸与字间距：`glyph_size` / `spacing`
- 颜色：`color`
- 运行时字形缓存：`runtime_glyph_entities`

---

## 3. 布局系统

### 3.1 CanvasScaler

通过 `UICanvasScalerComponent` 和 `UILayoutSystem` 提供全局缩放：

- 参考分辨率默认 `1920x1080`
- 支持宽高综合匹配
- 支持仅宽度主导的缩放策略

适用于同一 UI 在多分辨率下保持相对一致的视觉尺寸。

### 3.2 Anchor

通过 `UIAnchorComponent` 支持：

- 9 种固定锚点位置
- `Stretch` 拉伸模式
- 偏移量修正
- 与 `CanvasScaler` 联动

### 3.3 GridLayout

通过 `UIGridLayoutComponent` 支持：

- 行列配置
- 单元尺寸
- 元素间距
- 9 种对齐方式
- 动态子元素自动排列

适合背包、列表、面板按钮矩阵等场景。

---

## 4. UI 动画

`UIAnimationComponent` 当前支持：

- 位置动画
- 缩放动画
- 透明度动画
- 颜色动画
- 延迟启动
- 循环播放
- PingPong 往返播放
- 正放 / 反放

缓动类型包含：

- `Linear`
- `EaseIn`
- `EaseOut`
- `EaseInOut`

---

## 5. 运行时处理流程

每帧 UI 相关逻辑由系统协同完成：

1. `UILayoutSystem` 计算缩放、锚点和网格布局
2. `UISystem::UpdateLayout(...)` 更新 UI 绝对屏幕矩形
3. `UISystem::HandleEvents(...)` 处理悬停、按下、点击
4. `UISystem::SyncLabels(...)` 同步文本显示
5. `UISystem::UpdateAnimations(...)` 更新 UI 动画状态

---

## 6. 测试覆盖

当前已接入以下测试：

- `tests/modules/gameplay_2d/ui/ui_system_test.cpp`
- `tests/modules/gameplay_2d/ui/ui_layout_test.cpp`
- `tests/modules/gameplay_2d/ui/ui_animation_test.cpp`
- `tests/modules/gameplay_2d/ui/ui_advanced_test.cpp`

并已通过以下回归入口验证：

- `dse_engine_unit_tests.exe "[ui]"`
- `ctest -R engine.2d.ui`

---

## 7. 当前边界

P0 阶段 UI 已满足 2D 运行时与基础编辑验证需求；下列内容属于后续增强方向：

- 更深层的 UI 批处理优化
- 更完整的富文本 / 多语言 UI 自动绑定
- 编辑器内可视化 UI 搭建工具
- 更复杂的列表、滚动视图与样式系统

---

## 8. 相关文件

- `engine/ecs/components_2d.h`
- `modules/gameplay_2d/ui/ui_system.h`
- `modules/gameplay_2d/ui/ui_system.cpp`
- `modules/gameplay_2d/ui/ui_layout.h`
- `modules/gameplay_2d/ui/ui_layout.cpp`
- `tests/modules/gameplay_2d/ui/`
