# 新会话指令：渲染管线优化（Clustered Forward+ / 后处理栈）

> 将此文档内容粘贴到新 Cascade 会话的第一条消息中。
> 使用中文回复。

---

## 项目信息

- **仓库**: `.`
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

1. ✅ **RHI 层 SSBO 支持** （已完成）
   - 三后端 `CreateSSBO/UpdateSSBO/BindSSBO/DeleteSSBO` 已实现
   - OpenGL: `glBindBufferBase(GL_SHADER_STORAGE_BUFFER)`
   - Vulkan: `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` Set 1 binding 1-4
   - DX11: `ByteAddressBuffer` Raw SRV, t16+binding 映射

2. ✅ **光源 SSBO 数据结构** （已完成）
   - `engine/render/light_buffer.h` — GPUPointLight/GPUSpotLight 统一布局
   - FramePipeline 每帧收集 → SSBO 上传，支持 256 光源
   - UBO 硬编码上限已移除

3. ✅ **Cluster 构建（CPU 端）** （已完成）
   - `engine/render/cluster_grid.h/cpp` — CPU 端 Build() + Upload()
   - `ClusterGridHeader`（32B）嵌入 SSBO 头部，shader 直接读取网格参数
   - ClusterInfoSSBO (binding 3) + LightIndexSSBO (binding 4) 三后端已绑定
   - ⚠ GPU Compute 版本（`cluster_build.comp`）暂未实现，见下方分析

4. ✅ **修改 PBR Fragment Shader** （已完成）
   - `pbr.frag` 暴力遍历 → cluster 查找（`gl_FragCoord` + 对数 Z 分片定位）
   - 新增 `ClusterInfoSSBO` / `LightIndexSSBO` 声明
   - shader_compiler 重编译 18/18 OK（SPIR-V + GLSL 430 + HLSL SM5）

5. ✅ **验证** （已完成 2026-05-11）
   - 三后端截图对比: OpenGL RMSE 22.1, DX11 23.1, Vulkan 24.5（与改动前 ±0.3，无退化）
   - 850 单元测试全部通过
   - KF_Framework demo 三后端视觉一致

#### GPU Compute 版本分析（cluster_build.comp）

**当前 CPU 版本性能**:
- 4096 clusters × N lights 球体-AABB 相交测试
- N ≤ 20（KF demo 典型值）: < 0.5ms，完全不是瓶颈
- N = 256: ~2-4ms，中端 CPU 上可能影响帧率

**GPU Compute 版本收益**:
- 光源 < 50: 收益几乎为零（CPU 版本已足够快）
- 光源 50-256: 节省 1-3ms CPU 时间，GPU 端 < 0.1ms
- 主要收益是解放 CPU，让光源剔除不占主线程

**实现成本**:
- 新增 `cluster_build.comp`（~120 行）
- 新增 `ClusterBuildPass`（~80 行）+ RenderGraph 注册
- RHI 层 Compute Dispatch 已有基础设施（Bloom 已用）
- 需处理 depth texture 采样 + SSBO 读写屏障
- 约 **1-2 小时工作量**，中等改动

**结论**: 当前 KF demo 光源数 < 20，CPU 版本完全够用。
建议在 Phase 2 完成后、或需要 100+ 光源场景时再实现。
**优先级: Low，不阻塞后续 Phase。**

### Phase 2: 后处理栈（SSAO + FXAA）

6. ✅ **SSAO** （已完成 2026-05-11）
   - `SSAOPass` 类集成到 `builtin_passes.h/cpp`（非独立文件，沿用 Pass 内联模式）
   - OpenGL: 深度重建法线 + 16 样本核心 + 半分辨率 + 5×5 双边模糊
   - `ssao_rt`（半分辨率）+ `ssao_blur_rt` 新增到 `RenderPipelineResources`
   - `CompositePass` 修改: bloom_composite shader 支持 `ssaoTexture` 乘法; 非 bloom 路径走 `ssao_apply`
   - `PostProcessComponent::ssao_enabled`（默认 false）/ `ssao_radius` / `ssao_bias`
   - Lua: `set_post_process_ssao(entity, enabled, radius, bias)`
   - 编辑器: Inspector 面板 SSAO 控件
   - DX11: HLSL SSAO shader（深度重建法线 + 16 核心 + 双边模糊）+ ssao_apply + bloom_composite_ssao
   - Vulkan: GLSL 450 运行时编译 SSAO/SSAO_blur shader + push constants 参数传递

7. ✅ **FXAA** （已完成 2026-05-11）
   - `FXAAPass` 类 + `fxaa_rt`（全分辨率）
   - OpenGL: FXAA 3.11 完整实现（亮度边缘检测 + 方向模糊）
   - `PresentPass` 优先读取 FXAA 输出，fallback 到 main_color
   - `PostProcessComponent::fxaa_enabled`（默认 true）
   - Lua: `set_post_process_fxaa(entity, enabled)`
   - 编辑器: Inspector 面板 FXAA 控件
   - DX11: HLSL FXAA 3.11 完整实现（亮度边缘检测 + 方向模糊）
   - Vulkan: GLSL 450 运行时编译 FXAA shader + push constants 分辨率传递

8. ✅ **验证** （已完成 2026-05-11）
   - 三后端截图对比 vs KF 原版: OpenGL RMSE 22.4, DX11 23.1, Vulkan 24.9
   - 三后端一致性: DX11↔OpenGL 11.3, OpenGL↔Vulkan 15.8
   - 亮度差: ±2.5，无退化
   - 850 单元测试全部通过
   - RenderGraph Pass 数 7→9（+SSAOPass +FXAAPass）
   - 三后端 FXAA/SSAO 专用 shader 全部创建成功（Vulkan 日志确认 handle 540005-540007）

### Phase 3: 阴影质量

9. ✅ **CSM 级联过渡** （已完成 2026-05-11）
   - 提取 `ShadowForCascade()` 辅助函数（单级联阴影采样）
   - `ShadowCalculation()` 在级联范围末尾 20% 区域 `smoothstep` 混合到下一级
   - GLSL (`pbr.frag`) + HLSL (`dx11_shader_sources.h`) 同步修改
   - SPIR-V 重新编译（pbr.frag 39KB→44KB）
   - 验证: OpenGL RMSE 22.5, DX11 23.4, Vulkan 24.5 (vs KF), 850/850 测试通过

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
