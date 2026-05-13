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
| Phase 3.1: Light Probe SH Bake + 运行时查询 | 2026-05-13 |
| Phase 3.2: Reflection Probe + IBL (Split-Sum) | 2026-05-13 |
| Phase 2.3: TAA (Variance Clipping + Halton Jitter) | 2026-05-13 |

---

## 下一步任务（按优先级排序）

### 任务 1: Phase 5 — 可选 Deferred 路径 (预估 4-6 周)

**目标**: 新增可选的 Deferred Rendering 管线路径。

### 任务 2: 运行时验证

- TAA + IBL 三后端运行时截图验证
- Light Probe 间距混合无跳变验证
- 窗口 resize 时 TAA history RT 重建

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
