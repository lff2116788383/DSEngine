# DOC-06 粒子系统与 Profiler

本文档合并原粒子系统与 Profiler 文档，只保留当前代码主线已落地部分。

## 1. 范围

当前文档覆盖：

- 2D 粒子系统
- CPU / Memory / Render Profiler
- 编辑器中已接入的 Profiler 面板基础能力

## 2. 粒子系统当前能力

相关代码主要位于：

- `modules/gameplay_2d/particle/particle_system.h`
- `modules/gameplay_2d/particle/particle_system.cpp`
- `engine/ecs/components_2d.h`

当前已接入能力：

- 持续发射
- Burst 发射
- 随机速度、生命周期、尺寸、旋转、角速度
- 生命周期曲线
- 颜色渐变
- 重力影响
- GroundPlane 碰撞语义
- Box2D 最小可用碰撞处理路径

## 3. 粒子关键数据

当前主要围绕以下结构：

- `ParticleEmitterComponent`
- `Particle2D`
- `ParticleCurve`
- `ParticleCurveType`
- `ParticleCollisionMode`
- `ParticleSystem`

当前曲线能力至少覆盖：

- size
- alpha
- speed

并保留了旧字段兼容逻辑。

## 4. 粒子当前边界

以下内容仍属于后续增强项：

- 更完整的碰撞行为细分
- 更成熟的编辑器粒子资源工作流
- 更大规模场景下的性能优化

因此，当前粒子系统的准确定位是：

- 2D Runtime 可用
- 参数体系已结构化
- 与物理存在最小可用集成
- 已具备继续做编辑器化的基础

## 5. 粒子测试覆盖

当前已接入测试包括：

- `tests/modules/gameplay_2d/particle/particle_system_test.cpp`
- `tests/modules/gameplay_2d/particle/particle_advanced_test.cpp`

并且已经有：

- `engine.2d.particle`
- `[particle]`

等专项回归入口。

## 6. Profiler 当前能力

Profiler 相关代码位于：

- `engine/profiler/cpu_profiler.h`
- `engine/profiler/cpu_profiler.cpp`
- `engine/profiler/memory_profiler.h`
- `engine/profiler/memory_profiler.cpp`
- `engine/profiler/render_profiler.h`
- `engine/profiler/render_profiler.cpp`

当前已接入三类分析器：

- `CPUProfiler`
- `MemoryProfiler`
- `RenderProfiler`

## 7. Profiler 能力概览

### 7.1 CPUProfiler

当前支持：

- BeginFrame / EndFrame
- BeginSample / EndSample
- 作用域式采样
- 帧时间与 FPS 统计
- CSV / JSON 导出

### 7.2 MemoryProfiler

当前支持：

- 分类分配统计
- 当前占用 / 峰值占用
- 累计分配 / 累计释放
- 简单泄漏检测
- CSV 导出

### 7.3 RenderProfiler

当前支持：

- Draw Call
- 顶点数 / 三角形数
- Sprite / Batch
- Texture Bind / Shader Switch
- 纹理内存统计
- 累计统计

## 8. 编辑器接入情况

当前 `apps/editor_cpp/src/main.cpp` 已接入基础 Profiler 面板能力，包括：

- 实时数据显示
- 历史样本曲线
- CSV / JSON 导出入口
- Editor-side runtime metrics preview

因此，Profiler 已经不只是底层工具，而是已经进入当前编辑器主线的一部分。

## 9. Profiler 测试覆盖

当前测试包括：

- `tests/engine/profiler/cpu_profiler_test.cpp`
- `tests/engine/profiler/memory_profiler_test.cpp`
- `tests/engine/profiler/render_profiler_test.cpp`

## 10. 当前边界

Profiler 当前仍然是轻量级分析工具，尚未形成：

- 完整全引擎自动埋点体系
- 跨版本性能基线比对平台
- CI 自动性能回归报警
- 编辑器内深层性能追踪器

因此当前准确定位应为：

- 已可用于开发阶段快速定位
- 已具备导出与编辑器展示能力
- 是建立性能基线的起点，而不是终态

## 11. 推荐后续优先级

- 将更多 Runtime 真实指标接入 Profiler
- 建立固定测试场景的性能基线文档
- 在编辑器中增强历史曲线与对比能力
- 把关键性能回归逐步接入持续集成流程
