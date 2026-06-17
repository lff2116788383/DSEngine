# stress_test — 大规模实例化性能基准

可对比的渲染性能基准：多后端 × GPU-Driven ON/OFF × 不同实体数量（支持万级），
采集 FPS / 帧时分位 / draw call / 加载时间，并标注实际渲染设备与是否软件渲染。

## 跑法

```bash
# 默认（100 / 500 / 2000 实体，三后端，CPU+GPU 两种模式）
python examples/stress_test/run_benchmark.py

# 万级物体（建议在有独显的机器上跑硬件基线）
python examples/stress_test/run_benchmark.py --counts 1000 5000 10000 --gpu-only

# 指定后端
python examples/stress_test/run_benchmark.py --backends opengl dx11 vulkan
```

直接跑单次（不经 Python 驱动）：

```bash
DSE_RHI_BACKEND=dx11 DSE_ENTITY_COUNT=10000 DSE_PERF_FRAMES=600 \
  ./bin/dsengine_game_release.exe \
  --script=examples/stress_test/script/main.lua --rhi=dx11
```

常用环境变量：`DSE_ENTITY_COUNT`、`DSE_ANIM_ENABLED`、`DSE_PERF_FRAMES`、
`DSE_NO_SHADOW`、`DSE_LOD_ENABLED`。

## 采集的指标

每次跑结束输出一行机器可解析结果：

```
DSE_PERF_RESULT entities=.. fps_avg=.. fps_min=.. ft_avg=.. ft_p99=.. \
  gpu_driven_active=.. gpu_indirect_draws=.. gpu_instances=.. draw_calls=.. load_ms=..
```

并在设备初始化后输出实际渲染设备：

```
DSE_RENDER_DEVICE backend=.. adapter=".." software=0|1
```

`run_benchmark.py` 解析这两行，落表到 `benchmark_results.csv`，列为：

| 列 | 含义 |
|----|------|
| backend / gpu_driven / entities | 配置维度 |
| fps_avg / fps_min | 平均 / 最差帧 FPS |
| ft_avg_ms / ft_p99_ms | 平均 / p99 帧时 |
| draw_calls | 稳态帧 draw call 数 |
| load_ms | 场景构建 + 资产装载耗时（Awake 区间） |
| adapter | 实际所选 GPU/适配器名 |
| software | 是否软件渲染（true=软渲，结果不可作硬件基线） |

## 硬件 vs 软件渲染（重要）

无独显环境（CI、远程桌面、纯虚拟显示器）下，DX11 会落到 Microsoft Basic
Render Driver、Vulkan 会落到 lavapipe、OpenGL 会落到 GDI/llvmpipe —— 即**软件
渲染**，性能比硬件低 5–20 倍，且不同后端落点不同，数字**互不可比**。

因此基准会显式标注 `software`：表格里 `SW=Y` 的行、CSV 里 `software=true` 的行
都是软渲样例，**不可当硬件性能**。固化的硬件基线必须在**有独显的机器**上采集；
`benchmark_results.csv` 中已提交的硬件数据请勿用软渲数据覆盖。
