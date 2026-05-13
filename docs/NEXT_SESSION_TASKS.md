# DSEngine 后续会话任务指令

> 更新日期: 2026-05-13
> 基于 `docs/RENDER_PIPELINE_OPTIMIZATION.md` 路线图，编辑器相关任务暂缓。

---

## 已完成

| 任务 | 完成日期 |
|------|----------|
| Phase 1: Clustered Forward+ (256+256 光源) | 2026-05-11 |
| Phase 2.1: SSAO 三后端 | 2026-05-11 |
| Phase 2.2: FXAA 三后端 | 2026-05-11 |
| Phase 4.1: CSM 级联 smoothstep 过渡 | 2026-05-11 |
| Phase 4.2: PCSS 软阴影 | 2026-05-12 |
| KF 标题画面 3D 穿透修复 | 2026-05-12 |
| 三端视觉一致性验证 (RMSE ~22) | 2026-05-12 |
| Phase 2.4a: Auto Exposure 三后端 | 2026-05-12 |
| Phase 2.4b: Color Grading LUT 三后端 | 2026-05-13 |
| Phase 2.4c: Vignette / Film Grain 三后端 | 2026-05-13 |
| Phase 4.3: Contact Shadow 三后端 | 2026-05-13 |

> 注：以上条目标记为“代码与编译接入完成”；`bloom off / vignette on`、`bloom on / grain on`、`LUT + contact shadow + vignette` 三组运行时组合验证待补。

---

## 下一步任务（按优先级排序）

### 任务 1: Light Probe SH Bake (Phase 3.1) — 预估 1-2 周

**目标**: 实现运行时 Light Probe SH 数据生成和查询（不含编辑器 UI）。

**当前状态**:
- ✅ `LightProbeComponent` 有 `sh_coefficients[9]`
- ✅ `pbr.frag` 有 `EvaluateSH(N)` + `u_sh_enabled`
- ✅ 三后端 UBO 管线已通（GL/VK/DX11）
- ❌ Bake 系统未实现
- ❌ 运行时查询未实现

**实现方案**:
1. 新增 `ProbeBakeSystem` — 对每个 probe 位置渲染 6 面 cubemap → 积分为 SH L2
2. 新增运行时查询 — 从 ECS 收集 probe，根据物体位置找最近 probe，传 SH 到 UBO
3. 可选: probe 距离加权混合

**验证**: 关闭方向光后，物体仍有环境光照；probe 之间切换无跳变。

---

### 任务 2: Reflection Probe + IBL (Phase 3.2) — 预估 1-2 周

**目标**: 利用 `ReflectionProbeComponent` 实现间接高光。

**实现方案**:
1. Bake: 渲染 cubemap + 生成预滤波 mipmap（Split-Sum 近似）
2. 预计算 BRDF LUT（512×512 RG16F，构建时一次生成）
3. 运行时: `textureLod(u_reflection_cubemap, R, roughness * maxMip)` + BRDF LUT 采样
4. Box Projection 修正（已有字段）

**验证**: 金属球面可见环境反射，粗糙度越高反射越模糊。

---

### 任务 3: TAA (Phase 2.3) — 预估 2 周

**前置**: 需 Motion Vector RT（Forward Pass 额外输出）+ Jitter（逐帧抖动投影矩阵）+ 历史帧缓冲。

编辑器相关任务暂缓，TAA 在运行时实现后可单独使用。

---

## 通用指令

### 构建 & 验证
```powershell
# 构建
cmake --build build_vs2022 --target dse_standalone --config Release

# 运行 KF demo (OpenGL)
$env:DSE_RHI_BACKEND="opengl"; $env:DSE_MAX_FRAMES="180"; $env:DSE_SCREENSHOT_PATH="screenshots/verify.png"; $env:DSE_SCREENSHOT_TARGET="main"; $env:DSE_AUTO_BATTLE="1"; $env:DSE_DISABLE_STARTUP_SCENE_REGRESSION="1"; $env:DSE_STARTUP_LUA="examples\KF_Framework\script\main.lua"; $env:DSE_DATA_ROOT="examples\KF_Framework"
.\bin\DSEngine_Game_release.exe --script=examples\KF_Framework\script\main.lua

# 三端对比
python examples\KF_Framework\tools\compare_all.py
```

### Shader 编译
统一 GLSL 450 源码在 `engine/render/shaders/src/`，通过 `tools/shader_compiler` 编译为 SPIR-V / GLSL 430 / HLSL SM5。新增 shader 需要:
1. 在源码目录添加 .vert/.frag/.comp
2. 运行 shader compiler 生成头文件
3. 三后端 ShaderManager 加载新 handle

### 提交规范
- 中文 commit message
- `git push origin master`
- 每个 Phase 完成后更新 `docs/RENDER_PIPELINE_OPTIMIZATION.md` 状态标记

### 注意事项
- **编辑器相关任务暂缓**: Light Probe / Reflection Probe 的编辑器 Bake UI 不做，只实现运行时 bake + 查询
- **不改动 KF demo 核心逻辑**: player.lua / enemy.lua / main.lua 核心逻辑不动
- **三端一致性**: 每次改动后验证 OpenGL / DX11 / Vulkan 三端截图一致
