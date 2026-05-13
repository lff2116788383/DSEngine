# RHI 后端分析：现有后端 vs DX12 必要性评估

> 基于 `engine/render/rhi/` 目录下的完整后端代码分析

---

## 一、引擎现有的三个后端

| 后端 | 枚举值 | 编译条件 | 核心实现 |
|------|--------|---------|---------|
| **OpenGL** | `RhiBackend::OpenGL` | 默认始终启用 | [`engine/render/rhi/gl_draw_executor.h`](../engine/render/rhi/gl_draw_executor.h) + 5 个子系统 |
| **Vulkan** | `RhiBackend::Vulkan` | `DSE_ENABLE_VULKAN=ON` | [`engine/render/rhi/vulkan/vulkan_rhi_device.h`](../engine/render/rhi/vulkan/vulkan_rhi_device.h) |
| **D3D11** | `RhiBackend::D3D11` | `DSE_ENABLE_D3D11=ON` | [`engine/render/rhi/dx11/dx11_rhi_device.h`](../engine/render/rhi/dx11/dx11_rhi_device.h) |

三者在 [`engine/render/rhi/rhi_factory.cpp`](../engine/render/rhi/rhi_factory.cpp) 中通过编译宏选择性启用，用户通过环境变量 `DSE_RHI_BACKEND=vulkan|opengl|d3d11` 切换。

---

## 二、三个后端的定位与覆盖范围

### 2.1 OpenGL 4.3+（主力后端）

**适合场景：** 开发调试、2D 游戏、简单 3D、跨平台原型

**现状：** 最成熟的后端，代码量最大，5 个子系统全部稳定：

| 子系统 | 文件 | 功能 |
|--------|------|------|
| `GLResourceManager` | [`gl_resource_manager.h`](../engine/render/rhi/gl_resource_manager.h) | 纹理/缓冲/VAO/RT 创建销毁 |
| `GLPipelineStateManager` | [`gl_pipeline_state_manager.h`](../engine/render/rhi/gl_pipeline_state_manager.h) | Blend/Depth/Cull 状态缓存 |
| `GLShaderManager` | [`gl_shader_manager.h`](../engine/render/rhi/gl_shader_manager.h) | 着色器编译 + Uniform 缓存 |
| `GLDrawExecutor` | [`gl_draw_executor.h`](../engine/render/rhi/gl_draw_executor.h) | 所有绘制命令执行 |
| `UBOManager` | [`ubo_manager.h`](../engine/render/rhi/ubo_manager.h) | UBO 生命周期 |

### 2.2 Vulkan 1.1+（高性能后端）

**适合场景：** 需要低 CPU 开销的 3A 级 3D 游戏、多线程渲染

**现状：** 已实现但尚未完全成熟。从代码结构看：

- 完整的 `VulkanRhiDevice` 类，同样 5 子系统架构
- 自研着色器 DSL（DSSL）已生成 SPIR-V 字节码
- 支持跨平台（Windows + Android via NDK）

### 2.3 D3D11（Windows 兼容后端）

**适合场景：** 在 Windows 上不想装 Vulkan SDK 的用户

**现状：** 最轻量的后端。D3D11 虽然 API 较老，但 Windows 7+ 全部原生支持，无需额外安装 SDK。

---

## 三、平台覆盖矩阵

```
                     Windows   macOS   Linux   Android   iOS
     ┌─────────────────────────────────────────────────────┐
     │  OpenGL  │    ✅     │   ✅   │   ✅   │   ✅    │  ❌  │  ← GLES 桥接可处理
     │  Vulkan  │    ✅     │   ❌   │   ✅   │   ✅    │  ❌  │
     │  D3D11   │    ✅     │   ❌   │   ❌   │   ❌    │  ❌  │
     │  DX12    │    ✅     │   ❌   │   ❌   │   ❌    │  ❌  │
     └─────────────────────────────────────────────────────┘
```

**关键结论：** 现有三个后端已经覆盖了 **Windows / Linux / Android** 三个主要目标平台。macOS/iOS 通过 MoltenVK 可间接支持 Vulkan。

---

## 四、DX12 接入的必要性分析

### 4.1 DX12 vs 现有后端的对比

| 对比维度 | OpenGL | D3D11 | Vulkan | **DX12** |
|---------|--------|-------|--------|----------|
| CPU DrawCall 开销 | 高 | 中 | **极低** | **极低** |
| 多线程录制 | ❌ 难 | ❌ 难 | ✅ 原生 | ✅ 原生 |
| 开发者生态 | 萎缩 | 稳定 | 增长中 | 增长中（3A 主流） |
| Windows 覆盖 | Win7+ | Win7+ | Win10+ (需要驱动) | **Win10+ 1903+** |
| Xbox 主机发布 | ❌ | ❌ | ❌ | ✅ **唯一途径** |
| PSO 缓存 | ❌ | 有限 | ✅ | ✅ |
| 学习/接入成本 | 低（已有基础） | 低（已有基础） | 中 | **极高** |
| DSSL → HLSL 生成 | ✅ 已有 | ✅ 已有 | ✅ 已有 | **需新增 target** |

### 4.2 DX12 的核心价值

1. **Xbox 发布能力** —— 如果 DSEngine 未来要上 Xbox，DX12 是**唯一选择**
2. **Windows 原生性能最优** —— DX12 在 Windows 上比 Vulkan 略优（驱动优化更好）
3. **DXR（光线追踪）** —— DX12 的 DXR 是当前最成熟的光追 API
4. **DirectStorage** —— 高速 IO 加载，次世代游戏标配
5. **Mesh Shader** —— 替代传统顶点渲染管线，提升性能

### 4.3 DX12 的接入成本

对于 DSEngine 当前阶段，接入 DX12 需要：

| 组件 | 预估工作量 |
|------|-----------|
| DX12 RHI 设备 | 2-3 周（基于已有 D3D11 代码迁移） |
| DX12 资源管理 | 1-2 周（Descriptor Heap 管理最复杂） |
| DX12 Pipeline State | 1 周 |
| DX12 Command Buffer | 2 周 |
| DSSL → DXIL 代码生成 | 1-2 周 |
| 调试与验证 | 2-4 周 |
| **总计** | **约 2-3 个月** |

### 4.4 对比：同样的工作量能做更有价值的事

```
同样的 2-3 个月开发时间，投入以下方向回报更高：
┌──────────────────────────────────────────────────────────┐
│  1. 延迟渲染管线接入          —— 提升多光源场景性能        │
│  2. TAA 时间抗锯齿            —— 消除画面闪烁               │
│  3. LOD 系统                  —— 支持大场景渲染             │
│  4. Lightmap 烘焙             —— 提升场景光照品质           │
│  5. BVH + 光线追踪集成        —— 引入混合渲染               │
│  6. DX12 后端                 —— 额外平台覆盖               │
└──────────────────────────────────────────────────────────┘
    ↑ 对普通用户/玩家感知更强        ↑ 对普通用户感知较弱
```

---

## 五、结论与建议

### 先回答你的问题：

> **现有的 OpenGL + Vulkan + D3D11 足够了吗？**

**当前阶段：绰绰有余。**

| 面向对象 | 推荐后端 | 原因 |
|---------|---------|------|
| 开发者/调试 | OpenGL | 启动快，工具链成熟（RenderDoc/Nsight） |
| Windows 玩家 | D3D11 | 无需安装 Vulkan SDK，兼容性好 |
| 性能敏感玩家 | Vulkan | 低 DrawCall 开销，多线程录制 |
| Linux 玩家 | Vulkan | 唯一高性能选择 |
| Android | OpenGL ES / Vulkan | 两者都支持 |

**三个后端已经覆盖了 PC（Windows/Linux）和移动端（Android）的全部需求。**

> **有必要再接入 DX12 吗？**

**当前阶段：没有必要。建议暂缓。**

**原因：**

1. **性能收益对现有引擎不显著** —— 引擎是 2D+3D 混合引擎，不是 3A 大作。当前 DrawCall 瓶颈远没到需要用 DX12 来解决的程度
2. **投入产出比低** —— 2-3 个月的工作量，换来的是普通用户完全感知不到的"底层 API 更换"
3. **现有 Vulkan 后端已覆盖 DX12 的核心优势** —— 低开销、多线程、精细控制，Vulkan 都能做到
4. **D3D11 在 Windows 上仍有广泛的兼容性优势** —— Win7 玩家无法跑 DX12，但可以跑 D3D11
5. **接入 DX12 会让维护成本翻倍** —— 每个新功能都要在 4 个后端上验证

### 什么时候应该考虑 DX12？

```
□ 引擎要发布 Xbox 版本        → 必须接入（DX12 + GDK）
□ 引擎要集成 DXR 光线追踪      → 建议接入
□ 引擎的 DrawCall 超过 5000/帧  → 建议接入
□ 引擎需要 Mesh Shader / DirectStorage → 建议接入
□ 以上都不满足                  → 不要接入
```

目前 DSEngine 显然处于最后一个选项。**等前四个条件满足任何一个时，再考虑 DX12。**

### 优先级的建议

建议按照这个优先级投入开发时间：

| 优先级 | 功能 | 预期收益 |
|--------|------|---------|
| 🔴 1 | **延迟渲染** | 多光源场景性能翻倍 |
| 🔴 2 | **TAA** | 消除画面闪烁，画质质的飞跃 |
| 🔴 3 | **LOD 系统** | 支持更大场景，性能提升明显 |
| 🟡 4 | **Lightmap 烘焙** | 静态场景光照品质提升 |
| 🟡 5 | **完善 Vulkan 后端** | 修复已有 Vulkan 的 bug 比加新后端更重要 |
| 🟢 6 | **DX12 后端** | 目前性价比最低 |

---

## 六、总结

| 维度 | 当前状态 | 结论 |
|------|---------|------|
| 现有后端数 | **3 个**：OpenGL + Vulkan + D3D11 | 充足 |
| 平台覆盖 | **Windows / Linux / Android** | 核心平台全部覆盖 |
| DX12 收益 | 对当前引擎几乎无感知提升 | 低 |
| DX12 接入成本 | 2-3 个月 | 高 |
| **建议** | **暂不接入 DX12** | 把精力放在延迟渲染/TAA/LOD 上 |

> **一句话：** 三个后端已经足够，DX12 是"锦上添花"不是"雪中送炭"。在引擎的内容层面（渲染质量、性能、工具链）还没打磨到 90 分之前，不值得花几个月去换一个"纯粹底层 API"。
