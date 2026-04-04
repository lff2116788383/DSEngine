# 性能分析工具使用指南

## 概述

DSEngine 当前提供 3 类轻量级性能分析工具：

- `CPUProfiler`：用于统计函数/作用域耗时、帧时间与 FPS
- `MemoryProfiler`：用于统计分类内存分配、释放、峰值与泄漏
- `RenderProfiler`：用于统计 Draw Call、顶点数、三角形数、批次数与纹理状态切换

这套 Profiler 适合：

- 引擎模块开发阶段的快速定位
- 回归测试中的指标验证
- 编辑器性能面板接入前的数据采集层

---

## 头文件位置

```cpp
#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/memory_profiler.h"
#include "engine/profiler/render_profiler.h"
```

命名空间：

```cpp
using namespace dse::profiler;
```

---

## CPUProfiler

### 作用

`CPUProfiler` 用于记录：

- 单个作用域耗时
- 嵌套调用深度
- 帧时间
- 平均帧时间 / 最小帧时间 / 最大帧时间
- FPS / 平均 FPS
- 统计结果导出（CSV / JSON）

### 基本用法

```cpp
#include "engine/profiler/cpu_profiler.h"

dse::profiler::CPUProfiler profiler;

profiler.BeginFrame();

profiler.BeginSample("Update");
// 你的更新逻辑
profiler.EndSample();

profiler.EndFrame();
```

### RAII 写法

推荐优先使用作用域写法，减少漏写 `EndSample()` 的风险：

```cpp
dse::profiler::CPUProfiler profiler;

profiler.BeginFrame();
{
    dse::profiler::ScopedCPUProfile scope(profiler, "PhysicsStep");
    // 物理更新
}
profiler.EndFrame();
```

也可以直接使用宏：

```cpp
void UpdatePlayer(dse::profiler::CPUProfiler& profiler) {
    DSE_PROFILE_SCOPE(profiler);
    // 这里会自动记录当前函数名
}
```

### 嵌套采样

```cpp
profiler.BeginFrame();

profiler.BeginSample("FrameLogic");
profiler.BeginSample("AI");
// AI
profiler.EndSample();

profiler.BeginSample("Animation");
// Animation
profiler.EndSample();

profiler.EndSample();
profiler.EndFrame();
```

每个样本会记录 `depth`，可用于后续编辑器树状展示。

### 读取统计结果

```cpp
const auto& stats = profiler.GetStats();
const auto& frame = profiler.GetFrameStats();

for (const auto& [name, item] : stats) {
    // item.total_ms / item.avg_ms / item.min_ms / item.max_ms / item.call_count
}

double fps = frame.fps;
double avg_frame_ms = frame.avg_frame_time_ms;
```

### 导出

```cpp
std::string csv = profiler.ExportCSV();
std::string json = profiler.ExportJSON();
```

适合保存到日志、调试面板或构建产物目录。

---

## MemoryProfiler

### 作用

`MemoryProfiler` 用于按分类标签统计：

- 当前内存占用
- 峰值占用
- 累计分配量
- 累计释放量
- 活跃分配数
- 简单泄漏检测
- CSV 导出

### 基本用法

```cpp
#include "engine/profiler/memory_profiler.h"

dse::profiler::MemoryProfiler profiler;

profiler.RecordAlloc("Textures", 1024);
profiler.RecordAlloc("Textures", 2048);
profiler.RecordFree("Textures", 1024);
```

### 获取快照

```cpp
auto snapshot = profiler.GetSnapshot();

// snapshot.total_allocated
// snapshot.total_freed
// snapshot.current_usage
// snapshot.peak_usage
// snapshot.active_allocations
```

### 分类统计

```cpp
const auto& categories = profiler.GetCategoryStats();

for (const auto& [tag, item] : categories) {
    // item.current_bytes / item.peak_bytes / item.total_allocated / item.total_freed
}
```

### 泄漏检测

```cpp
auto leaks = profiler.DetectLeaks();
```

当前实现是轻量级规则：

- 某 tag 的 `alloc_count > free_count`
- 且 `current_bytes > 0`

则判定该分类存在潜在泄漏。

### 导出

```cpp
std::string csv = profiler.ExportCSV();
```

---

## RenderProfiler

### 作用

`RenderProfiler` 用于统计每帧渲染指标：

- Draw Call 数
- 顶点数 / 三角形数
- Sprite 数 / Batch 数
- Texture Bind 次数
- Shader Switch 次数
- 纹理内存
- 跨帧累计平均值与峰值

### 基本用法

```cpp
#include "engine/profiler/render_profiler.h"

dse::profiler::RenderProfiler profiler;

profiler.BeginFrame();
profiler.RecordDrawCall(100, 50);
profiler.RecordSpriteBatch(32);
profiler.RecordTextureBind();
profiler.RecordShaderSwitch();
profiler.SetTextureMemory(4 * 1024 * 1024);
profiler.EndFrame();
```

### 读取统计

```cpp
const auto& current = profiler.GetCurrentFrameStats();
const auto& accumulated = profiler.GetAccumulatedStats();
```

其中：

- `GetCurrentFrameStats()` 返回当前帧统计
- `GetAccumulatedStats()` 返回累计平均值、峰值和总量

### 导出

```cpp
std::string csv = profiler.ExportCSV();
```

当前 CSV 以 `last_frame_` 作为“当前输出帧”，适合在 `EndFrame()` 后导出。

---

## Reset 行为

三个 Profiler 都支持 `Reset()`：

- 清空累计状态
- 重置峰值/平均值
- 回到初始空白统计状态

适合：

- 场景切换后重新计量
- 测试前清零
- 编辑器中点击“清空统计”按钮

---

## 线程安全说明

当前三个 Profiler 内部都使用 `mutex` 做基础保护，适合常规开发与调试采样。

但仍建议：

- 高频热点路径避免过度细粒度采样
- 尽量由主线程统一做导出
- 后续如进入正式性能基准阶段，可再做无锁或分线程汇总优化

---

## 测试状态

当前已具备对应单元测试：

- `tests/engine/profiler/cpu_profiler_test.cpp`
- `tests/engine/profiler/memory_profiler_test.cpp`
- `tests/engine/profiler/render_profiler_test.cpp`

已验证 `dse_engine_unit_tests.exe "[profiler]"` 通过。

---

## 建议接入方式

### 1. 运行时主循环

- 每帧 `BeginFrame()` / `EndFrame()`
- 在 Update / Physics / Render / Script 阶段加作用域采样

### 2. 渲染管线

- 每次提交批次时调用 `RecordDrawCall()` 或 `RecordSpriteBatch()`
- 纹理切换和 Shader 切换时增加对应统计

### 3. 资源系统

- 纹理、网格、音频等资源加载/卸载时通过 `MemoryProfiler` 记录分类内存

### 4. 编辑器面板

- CPU：树状显示样本层级 + 平均耗时
- Memory：按 tag 排序显示 current / peak
- Render：显示当前帧与平均帧的 Draw Call、Vertices、Texture Binds

---

## 后续建议

Profiler 模块下一步建议继续完善：

1. 编辑器实时面板集成
2. 更细的资源分类与对象级统计
3. 统一导出到 `json/csv` 文件
4. 与自动化回归结合，建立性能基线
