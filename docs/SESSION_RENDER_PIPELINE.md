# 新会话指令：渲染管线优化（Clustered Forward+ / 后处理栈）

> 将此文档内容粘贴到新 Cascade 会话的第一条消息中。
> 使用中文回复。

---

## 项目信息

- **仓库**: `c:\Users\Administrator\Desktop\Engine\DSEngine`
- **分支**: `feature/render-pipeline`（需 rebase 到 master d142240）
- **切换分支**: `git checkout feature/render-pipeline && git rebase master`

## 你的任务

在 `feature/render-pipeline` 分支上，按照 `docs/RENDER_PIPELINE_OPTIMIZATION.md` 方案文档，逐步实现 DSEngine 渲染管线优化。

## 前置条件

✅ 三后端统一 Shader 改造已完成并合入 master（commit d142240, 2026-05-11）。
`engine/render/shaders/src/pbr.frag` 是唯一 PBR 源码，三后端 ShaderManager 均加载生成的头文件。
**可直接开始全部 Phase。**

## 实施顺序

### Phase 1: Clustered Forward+（最高优先级）

**目标**: 突破光源 4+4 硬编码上限，支持 256+ 动态光源。

**步骤**:

1. **RHI 层 SSBO 支持**
   - 在 `engine/render/rhi/rhi_device.h` 的 `RhiDevice` 接口新增 SSBO 创建/更新/绑定 API
   - 三后端实现：OpenGL `glBufferData(GL_SHADER_STORAGE_BUFFER)`、Vulkan `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`、DX11 `StructuredBuffer`
   - 这步不涉及 shader 修改，可立即开始

2. **光源 SSBO 数据结构**
   - 新增 `engine/render/light_buffer.h` — 统一光源 GPU 数据布局
   - FramePipeline 每帧收集 World 中所有 PointLight/SpotLight → 上传到 SSBO
   - 替换现有 `MAX_POINT_LIGHTS=4` / `MAX_SPOT_LIGHTS=4` UBO

3. **Cluster 构建 Pass（Compute Shader）**
   - 新增 `engine/render/passes/cluster_build_pass.h/cpp`
   - 新增 `engine/render/shaders/src/cluster_build.comp` — 将光源分配到 16×16 tile × 24 Z-slice
   - RenderGraph 中插入：`PreZ → ClusterBuild → ForwardScene`
   - 输入: PreZ 深度 + 光源 SSBO + 投影矩阵
   - 输出: `cluster_light_indices` SSBO + `cluster_light_offsets` SSBO

4. **修改 PBR Fragment Shader**
   - 修改 `engine/render/shaders/src/pbr.frag`
   - 将暴力 `for (i = 0; i < point_light_count)` 替换为 cluster 查找
   - 用 `gl_FragCoord` + 线性深度定位所属 cluster，只遍历该 cluster 的光源

5. **验证**
   - KF_Framework demo 场景放置 50+ 点光源，帧率 ≥ 30fps (1080p)
   - OpenGL/Vulkan/DX11 三后端视觉一致
   - 光源数 ≤ 8 时与旧路径无视觉差异

### Phase 2: 后处理栈（SSAO + FXAA）

6. **SSAO (GTAO 算法)**
   - 新增 `engine/render/passes/ssao_pass.h/cpp`
   - 新增 `engine/render/shaders/src/ssao.frag` + `ssao_blur.frag`
   - 从 PreZ 深度重建 view-space 法线（无需额外 Normal RT）
   - 半分辨率计算 + 双边模糊上采样
   - 接入 `PostProcessComponent::ssao_enabled` / `ssao_radius` / `ssao_bias`
   - RenderGraph: ForwardScene 之后、Composite 之前

7. **FXAA**
   - 新增 `engine/render/passes/fxaa_pass.h/cpp`
   - 新增 `engine/render/shaders/src/fxaa.frag`（FXAA 3.11）
   - Composite 之后、Present 之前
   - 单 Pass 全屏 fragment shader

### Phase 3: 阴影质量

8. **CSM 级联过渡**
   - 修改 `pbr.frag` 的 `ShadowCalculation()` — 在级联边界 smoothstep 混合两级 shadow
   - ~20 行改动

## 关键文件清单

阅读以下文件了解现有架构：

- `docs/RENDER_PIPELINE_OPTIMIZATION.md` — 完整方案文档（必读）
- `engine/render/render_graph.h` — RenderGraph DAG 核心
- `engine/render/passes/builtin_passes.h` — 现有 9 个 Pass
- `engine/render/passes/render_pass_context.h` — Pass 共享上下文
- `engine/render/shaders/src/pbr.frag` — 当前 PBR shader（385 行，光源遍历在 main()）
- `engine/runtime/frame_pipeline.h` — 帧流水线，组装 RenderGraph
- `engine/render/rhi/rhi_device.h` — RHI 抽象层接口
- `engine/ecs/components_3d.h` — 光源/后处理组件定义

## 约束

- 使用中文回复和 commit 信息
- OpenGL 后端行为绝对不能退化
- 不删改现有注释
- 优先最小改动，逐步验证
- 每完成一个步骤 commit 一次，commit message 格式: `feat(render): <具体内容>`
- 编译命令: `cmake --build build_vs2022 --target dse_standalone --config Release`
- 运行验证: `.\bin\DSEngine_Game_release.exe --script=examples\KF_Framework\script\main.lua --rhi=opengl`
