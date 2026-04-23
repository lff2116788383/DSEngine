# DSEngine Codebase Concerns

## 总体判断

DSEngine 当前最大的风险不是“完全没有架构”，而是 **主线已经形成，但若干关键中枢尚未完全收口**。这类项目最常见的问题不是目录乱，而是：

- 中心类职责过重
- 旧兼容路径与新注入路径并存
- 2D、Editor、3D、资产链多条主线同时推进
- 文档与实现容易随时间漂移

## 1. `FramePipeline` 职责过重

`engine/runtime/frame_pipeline.cpp` 是当前最明显的高耦合枢纽之一。它同时负责：

- world / asset manager 前置校验
- RHI device 初始化
- data root 配置
- render target 与 pipeline state 创建
- physics / audio / spine / mesh render 系统初始化
- 3D 动态模块加载
- scene regression sample 执行
- 业务 bootstrap 之前的关键中间态装配

这类“超级协调器”会带来：

- 改动面过大
- 测试定位成本高
- 初始化顺序依赖脆弱
- 拆分时高风险

如果后续出现 runtime 侧疑难问题，`FramePipeline` 应被视为首要排查区。

## 2. 编辑器壳层偏重

`apps/editor_cpp/src/main.cpp` 直接感知：

- GLFW / OpenGL / ImGui / ImGuizmo
- 2D / 3D ECS 组件
- 场景视图与相机矩阵计算
- profiler 历史数据
- 各类编辑器面板逻辑

这说明编辑器主程序仍然偏“全知入口”，后续若继续扩展：

- Play/Edit 隔离
- Undo/Redo
- Prefab 工作流
- 资源浏览器
- 更复杂的 3D 检视

则该壳层很容易继续膨胀。

## 3. 全局态向注入式演进尚未完全结束

代码和文档都表明仓库正在从旧式全局态迁移到显式注入，例如：

- `EngineInstance` 注入 `World` / `AssetManager`
- `FramePipeline` 需要注入 `AssetManager`
- Spine / C++ runtime 已有显式依赖接收方式

但这条迁移线还没有完全结束，意味着当前项目可能同时存在：

- 新式注入链路
- 旧兼容调用方式
- 测试中针对旧问题保留的防回归逻辑

这种状态在短期内常常必要，但长期会提高认知成本。

## 4. 3D 已接入，但不是默认稳定主线

README 与测试文档都比较克制地描述了 3D 状态：

- 已有组件、渲染路径、测试入口、最小场景 gate
- 但默认稳定主线仍是 2D + runtime + editor 基础能力

因此风险在于：

- 容易被误判为“3D 已成熟”
- 对 3D 做大改时可能触碰未收口链路
- 资产工作流、编辑器闭环、默认构建口径尚未完全统一

如果 roadmap 不加控制，3D 可能成为持续吞噬工程精力的方向。

## 5. 测试门禁虽已建立，但仍有环境敏感点

`doc-archive/TESTING_CTEST_GUIDE.md` 明确记录了：

- Windows + Lua runtime 存在特定写法导致的假性挂起问题
- 某些 smoke / single test 目标历史上曾出现 exe 产物不稳定问题

这说明：

- 测试体系已不是“没有”，但仍有环境依赖脆弱性
- 本地门禁可靠性本身也需要维护
- 新增测试时若忽略既有经验，很容易重新引入旧坑

## 6. `depends/` 与 `reference/` 体积大，搜索噪音高

仓库中两个非常容易干扰分析的区域：

- `depends/`
- `reference/`

风险包括：

- 搜索结果被第三方源码淹没
- 把 reference 代码误当当前实现
- 在错误位置修 bug 或下结论

这对 AI 辅助开发尤其敏感，因此做变更前必须先聚焦主代码目录。

## 7. 文档可能存在漂移风险

仓库文档量很大，包括：

- README
- `doc-archive/DOC-xx_*.md`
- `doc-archive/TESTING_CTEST_GUIDE.md`
- 各类计划、审计、case study

优点是信息丰富，缺点是：

- 过期文档可能仍残留在仓
- 不同文档可能描述不同阶段口径
- 若代码变动未同步文档，后续规划容易被误导

因此任何涉及架构、测试门禁、主线路线的改动，都应伴随文档对齐检查。

## 8. 多条主线并行推进，优先级管理是核心风险

当前仓库同时存在多条高成本方向：

- 2D 稳定主线
- Lua / C++ 双宿主 runtime
- 原生编辑器
- 3D MVP
- 资产导入链
- Launcher/Tauri 辅助入口

这意味着真正的管理风险不是“没有工作可做”，而是：

- 很容易分散精力
- 容易在未收口 2D/editor 主线前继续铺 3D 或工具面
- 新增能力会增加已有中枢类复杂度

## 9. Windows 优先是现实优势，也是潜在约束

当前整个仓库的真实验证口径明显偏向 Windows：

- bat 脚本是主要工作流
- VS2022 是默认编译器环境
- 文档中的命令和门禁以 Windows 为基线

这有助于快速收口本地主线，但也意味着：

- 跨平台能力不应被默认视为“已验证”
- 未来若做 CI / 跨平台适配，需要专门补基线，而不是直接假设可用

## 建议重点关注区域

如果后续要做规划、重构或大改，优先关注以下路径：

- `engine/runtime/frame_pipeline.cpp`
- `engine/runtime/engine_app.cpp`
- `apps/editor_cpp/src/main.cpp`
- `engine/scene/`
- `engine/assets/`
- `modules/gameplay_3d/`
- `tests/engine/CMakeLists.txt`
- `doc-archive/TESTING_CTEST_GUIDE.md`

这些文件/目录要么是关键中枢，要么是主线路线的真实口径源。

## 结论

DSEngine 当前最真实的 concerns 不是“缺功能”，而是“如何在已有多条主线基础上持续收口复杂度”。

优先级上，更值得投入的通常是：

1. 稳住 runtime 中枢与测试门禁
2. 提升 editor 高频闭环质量
3. 让 3D 只在 MVP 范围内受控推进
4. 保持代码、测试、文档三者口径一致

只要这四点控制住，仓库就具备继续演进的基础；反之，复杂度会比功能增长更快。
