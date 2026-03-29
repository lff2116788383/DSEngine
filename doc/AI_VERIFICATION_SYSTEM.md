# DSEngine AI 开发多层级验证体系 (AI-Driven Verification System)

## 背景
DSEngine 是一个由 AI 深度参与实现的 2D/3D 游戏引擎。为了防止 AI 在复杂数学逻辑（如矩阵变换、光影公式、物理同步）中产生“幻觉”并导致错误，本项目建立了一套严密的**多层级验证体系**。该体系旨在通过自动化测试、人工审计、视觉回归和样本对比，确保引擎功能的正确性与稳定性。

## 体系概览
验证体系分为四个核心层级，从底层数学逻辑到高层业务场景全覆盖：

| 层级 | 验证目标 | 核心手段 | 针对的 AI 幻觉 |
| :--- | :--- | :--- | :--- |
| **L1: 自动化单元测试** | 数学公式、ECS 组件、基础算法 | CTest + Catch2 | 矩阵乘法顺序、UV 偏移、物理步进公式 |
| **L2: 逻辑与代码审计** | 渲染管线、系统间同步逻辑 | AI 交叉审计、人工复核 | 系统间竞态条件、Shader 变体逻辑缺失 |
| **L3: 视觉回归测试** | 最终渲染表现、UI 交互响应 | 帧对比 (Pixel Match) | 混合模式错误、层级遮挡异常、UI 缩放抖动 |
| **L4: 样本集成测试** | 完整游戏业务流、跨系统协作 | Sample Samples (如 FrogJump) | 资源加载路径错误、脚本绑定接口不匹配 |

---

## L1: 自动化单元测试 (Bottom-up Logic)
单元测试是引擎最坚实的防线，专注于验证原子级的数学正确性。
- **2D 示例**：[physics2d_system_test.cpp](tests/modules/gameplay_2d/physics/physics2d_system_test.cpp) 验证 Box2D 刚体与 ECS 变换组件的同步。
- **3D 示例**：[frustum_culling_test.cpp](tests/modules/gameplay_3d/rendering/frustum_culling_test.cpp) 验证视锥体剔除算法对 AABB 的判断。
- **执行方式**：通过 `build_all.bat --with-tests` 运行。

## L2: 逻辑与代码审计 (Architectural Guardrail)
针对 AI 容易产生的“逻辑断层”进行专项检查。
- **数学陷阱审计**：重点审计 [transform_system.cpp](engine/scene/transform_system.cpp) 中的变换顺序（Translation * Rotation * Scale）。
- **渲染批处理审计**：检查 [dsengine_bridge.cpp](apps/editor/src/bridge/dsengine_bridge.cpp) 中是否在纹理/材质变化时正确切断了 Batch。

## L3: 视觉回归测试 (Rendering Performance)
确保渲染结果在视觉上“符合物理”且“无抖动”。
- **Golden Scene (黄金场景)**：建立包含所有 2D/3D 特性的基准场景。
- **帧截图对比**：在 Headless 模式下捕获特定帧，与已知正确的基准图进行像素级差异分析。
- **重点关注**：Alpha 混合、级联阴影（CSM）切换边界、PBR 金属度表现。

## L4: 样本集成测试 (Gameplay Samples)
通过真实的 Demo 验证引擎的综合战斗力。
- **2D 样本**：[frog_jump](data/frog_jump/)。验证动画、音效、物理、UI 的整体联动。
- **3D 样本**：加载 [CesiumLogo](data/models/) 验证 PBR 材质加载、骨骼动画播放。

---

## 结论
多层级验证体系不是一次性的任务，而是伴随 DSEngine 开发全周期的**持续回归过程**。每当 AI 生成新的核心逻辑，必须至少通过 L1 级的单元测试验证，并在发布前通过 L4 级的样本回归测试。

*最后更新日期：2026-03-28*
