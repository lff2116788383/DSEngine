# DOC-05 UI 与国际化

本文档合并原 UI 与 Localization 两类说明，只保留当前主线已经落地并能与代码对应的部分。

## 1. 范围

当前文档覆盖：

- 2D UI 系统
- LocalizationSystem
- FontManager
- UI 与本地化的已接入联动

不覆盖：

- 完整可视化 UI 编辑器
- 完整富文本系统
- 完整多语言资源工作流平台

## 2. UI 系统当前能力

当前 UI 主体位于：

- `modules/gameplay_2d/ui/ui_system.h`
- `modules/gameplay_2d/ui/ui_system.cpp`
- `modules/gameplay_2d/ui/ui_layout.h`
- `modules/gameplay_2d/ui/ui_layout.cpp`
- `engine/ecs/components_2d.h`

已接入能力包括：

- `UIRendererComponent`
- `UIButtonComponent`
- `UILabelComponent`
- `UIMaskComponent`
- `UICanvasScalerComponent`
- `UIAnchorComponent`
- `UIGridLayoutComponent`
- `UIAnimationComponent`
- `UILayoutSystem`
- `UISystem`

## 3. UI 当前可确认能力

### 3.1 基础渲染与交互

- UI 可见性与尺寸控制
- 排序与交互状态维护
- 按钮点击、悬停、按下反馈
- 事件回调与事件总线联动

### 3.2 文本能力

- `UILabelComponent` 支持基础文本
- 支持 numeric mode
- 支持运行时字形实体同步

### 3.3 布局能力

- CanvasScaler
- Anchor
- GridLayout
- 父子层级布局参与

### 3.4 UI 动画

当前已接入：

- 位置动画
- 缩放动画
- 透明度动画
- 颜色动画
- 延迟、循环、PingPong
- 基础缓动类型

## 4. 国际化当前能力

国际化模块位于：

- `modules/gameplay_2d/localization/localization_system.h`
- `modules/gameplay_2d/localization/localization_system.cpp`
- `modules/gameplay_2d/localization/font_manager.h`
- `modules/gameplay_2d/localization/font_manager.cpp`

当前已接入能力：

- JSON 语言配置加载
- 当前语言切换
- 参数化文本
- RTL 检测
- 语言切换回调
- 字体注册与默认字体
- 语言到字体映射
- 字体回退链

## 5. UI 与国际化联动

当前代码和测试表明，UI 与本地化已经形成基础联动闭环：

- `UILabelComponent` 可使用 localization key
- 可在语言切换后刷新文本
- 可在 key 缺失时使用 fallback text
- glyph 实体数量会随文本变化重新同步

这部分已经不只是规划，而是有测试覆盖的现有能力。

## 6. 测试覆盖

### 6.1 UI 相关

当前已接入测试包括：

- `tests/modules/gameplay_2d/ui/ui_system_test.cpp`
- `tests/modules/gameplay_2d/ui/ui_layout_test.cpp`
- `tests/modules/gameplay_2d/ui/ui_animation_test.cpp`
- `tests/modules/gameplay_2d/ui/ui_advanced_test.cpp`

重点覆盖：

- 点击事件
- 首帧布局命中
- 遮罩阻挡
- 文本同步
- 本地化刷新

### 6.2 Localization 相关

当前已接入测试包括：

- `tests/modules/gameplay_2d/localization/localization_system_test.cpp`
- `tests/modules/gameplay_2d/localization/font_manager_test.cpp`

## 7. 当前边界

以下内容不应在当前文档中描述为已经完成：

- 完整 UI 编辑器可视化搭建器
- 完整 RTL UI 布局镜像能力
- 完整富文本样式系统
- 完整多语言资产打包平台

当前准确描述应是：

- UI 运行时主链已可用
- 布局与动画已接入
- Localization 基础能力已可用
- UI × Localization 已形成第一版联动

## 8. 推荐后续优先级

- 在编辑器中补足 UI Inspector 配置入口
- 继续补齐多分辨率与遮罩相关回归
- 逐步增强 UILabel 的本地化绑定体验
- 后续再考虑更完整的富文本与滚动视图体系
