# DSEngine 「轻量级高性能3D极客引擎」差距分析

> 分析日期：2026-05-15（第三次更新）
> 目标：最少代码、最低硬件、最少资源、极致画质
> 深耕：风格化渲染 + 写实渲染
>
> ⚠️ **本轮重大更新说明：** 自上次文档生成以来，引擎经历了**关键 Phase 1 收官**。完整实现了 Toon/Cel Shading DSSL 材质 + 三后端集成、Clear Coat/Anisotropy/POM 三后端参数管线、Color Banding IGN 抖动三后端同步。至此「画质细腻度 + 风格化启动」全部完成。当前引擎短板重心已从 Phase 1 转移到 Phase 2（SSBO→UBO fallback、.dds 纹理、LTCG 编译）和 Phase 3（Volumetric Fog、Decal、Outline）。

---

## 基线数据

### 自有代码量（全部保留，只增不减）

| 模块                                                     |     行数     | 说明                                    |
| ------------------------------------------------------ | :--------: | ------------------------------------- |
| engine/render/（含 shaders/generated）                    |  ~35,000   | 核心竞争力，渲染管线核心 + 三后端 shader              |
| engine/scripting/                                      |   ~7,500   | Lua 绑定 + sol2                         |
| engine/assets/                                         |   ~4,400   | 资源管理                                  |
| engine/scene/                                          |   ~2,900   | SubScene + Octree                     |
| engine/runtime/                                        |   ~2,600   | FramePipeline + 调度                    |
| engine/ecs/                                            |   ~2,500   | 组件定义（含动画 IK/Layer 组件）                 |
| engine/physics/                                        |   ~2,100   | PhysX 封装                              |
| engine/core/                                           |   ~1,700   | ServiceLocator / EventBus / JobSystem |
| engine/base/ + audio/ + input/ + platform/ + profiler/ |   ~2,000   | 基础设施                                  |
| **engine/ 合计**                                         | **~63,000** | **核心 215 文件**                        |
| modules/gameplay\_3d/ + gameplay\_2d/                  |   ~8,980   | 3D/2D 玩法模块                            |
| apps/editor\_cpp/                                      |   ~9,700   | 编辑器（不删除，暂不扩展）                         |
| apps/runtime/ + standalone/ + tools/                   |   ~1,950   | 宿主程序 + DSSL/Shader 编译器               |
| **自有代码合计**                                             | **~83,600** | **340+ 文件**                           |

### 第三方依赖

**38 个依赖**，其中源码集成 23 个、Vulkan 条件 4 个、预编译二进制 2 个（PhysX / FMOD）、系统 SDK 6 个、CMake 残留引用 3 个。

| 最大依赖                               |      估算行数     |
| ---------------------------------- | :-----------: |
| glslang + SPIRV-Cross              |     \~80K     |
| PhysX 4.1（5 DLL）                   |     \~50K     |
| Assimp                             |     \~50K     |
| FreeType 2.11                      |     \~30K     |
| GoogleTest 1.17                    |     \~30K     |
| GLFW 3.3-3.4                       |     \~30K     |
| imgui + GLM + stb + miniaudio + 其他 |     \~30K     |
| **第三方合计**                          | **\~200K+ 行** |

### 当前硬件要求

| 维度   | 要求                                  |
| ---- | ----------------------------------- |
| GPU  | OpenGL 4.3+（SSBO）或 Vulkan 1.0+ 或 D3D11 |
| VRAM | \~2GB（延迟管线 GBuffer 3 RT）                 |
| RAM  | \~4GB                                    |
| API  | OpenGL ✅ / Vulkan ✅ / D3D11 ✅             |

---

## 一、当前代码功能差距

### 写实渲染：已有 \~88%，缺 \~10%

#### 已有能力

```
PBR GGX+Smith+Schlick BRDF
IBL（Specular IBL + Diffuse SH L2）
Clustered Forward+（256 点光 + 256 聚光）
CSM 3 级联 + PCSS 软阴影
HDR 管线（ACES Filmic Tonemapping）
Bloom / SSAO / TAA / FXAA
DOF / Motion Blur / SSR
Contact Shadow / Auto Exposure
3D LUT Color Grading / Vignette / Film Grain
延迟渲染（GBuffer + DeferredLighting）
Light Probe SH Bake / Reflection Probe IBL
GPU 骨骼动画 + Morph Target
法线 / MR / 自发光 / 遮挡贴图
DSSL 材质系统（surface / light / vertex 三阶段）
```

#### 写实渲染缺失

| 缺失技术                                |  重要性 |  难度  |          画面影响          |
| ----------------------------------- | :--: | :--: | :--------------------: |
| **Volumetric Fog / Light Shaft**    | 🔴 高 | 🔴 高 |      光柱/雾效/氛围最关键因素     |
| **Decal System**                    | 🟡 中 | 🟡 中 |       弹孔/路面标记/血迹       |
| **OIT（Order Independent Transparency）** | 🟢 低 | 🔴 高 | 半透明物体正确排序，进阶需求 |
| **HDR10/BT.2020 Output**            | 🟢 低 | 🟡 中 | 真实 HDR 显示，当前仅 ACES→SDR |

✅ 已补齐：SSS、GPU Instancing、通用 Mesh LOD、Clear Coat、Anisotropy、POM、D3D11 后端

### 风格化渲染：当前 ~35%（已实现 Toon/Cel Shading + Color Banding）

#### 缺失项

| 缺失技术                       |  重要性 |  难度  | 说明                                   |
| -------------------------- | :--: | :--: | ------------------------------------ |
| **Outline/Edge Detection** | 🔴 高 | 🟡 中 | Backface fatten + 后处理 edge detection |
| **Custom NPR Light Model** | 🟡 中 | 🟢 低 | DSSL 的 light() 回调已支持 |

✅ 已补齐：Toon/Cel Shading（`shading_mode=4`，三后端 7 参数 PerMaterial UBO）、Color Banding（IGN 抖动三后端同步）

### 动画系统：Phase 1 全部完成

参考 [animation-enhancement-plan.md](docs/animation-enhancement-plan.md)：

| 能力                                      |     状态    |
| --------------------------------------- | :-------: |
| 1D Blend Tree + 状态机 + Crossfade         |    ✅ 已有   |
| Root Motion 锁定 + 提取                     |    ✅ 已有   |
| Animation Layering（Override / Additive） |   ✅ 已实现  |
| 2D Blend Tree（Shepard 逆距离加权）           |   ✅ 已实现  |
| FABRIK IK / LookAt IK                   |   ✅ 已实现  |
| Bone Mask / Partial Blending            |   ✅ 已实现  |
| AnimatorSystem 两阶段流水线                  |   ✅ 已实现  |
| 动画 Lua API 绑定（35+ 函数）                  |   ✅ 已实现  |

### 其他模块

| 模块            |                状态               | 差距                    |
| ------------- | :-----------------------------: | --------------------- |
| ECS（EnTT）     |               ✅ 成熟              | 无                     |
| 3D 物理（PhysX）  | ✅ 完整（刚体/碰撞体/关节/角色/布娃娃/车辆/软体/浮力） | Overlap API ✅ 已实现 |
| 2D 物理（Box2D）  |             ✅ 基础功能完备            | 缺多边形碰撞体              |
| 音频（miniaudio） |     ✅ 完善（3D 空间化/遮挡/VFS/并发控制）    | 缺 DSP 效果链             |
| 场景管理          |    ✅ SubScene + 异步加载 + Prefab   | 缺通用 LOD ✅ 已实现        |
| 资源管理          | ✅ 同步/异步双管道 + Bundle + 热重载 + LRU | 缺 Skinned Mesh 专属类型   |

---

## 二、性能 + 兼容

### 2.1 GPU 要求降级路径

```
[当前] GL 4.3+（SSBO 必需） / ~2GB VRAM / PBR 写实向策略
       ↓ SSBO→UBO fallback + 前向渲染 + 精简后处理 + Toon Shading 降级
[目标] GL 3.3+ / ~1GB VRAM / D3D11 可用
       ↓ 纹理流送 + Mesh LOD + 风格化渲染优化
[降级] Intel UHD 620 核显 / 512MB VRAM / 三后端全可用（风格化中端画质，参考原神）
```

**说明**：
- DSE 当前 PBR 写实管线（延迟+全后处理）需要中高端显卡，这本身不是问题
- 但 DSE 架构天然支持**降级策略**：前向模式 + 可关闭的 Pass + DSSL 换 Toon Shading
- 原神在 UHD 620 上能跑中端画质（720p~30FPS），证明"核显 + 风格化渲染"方案可行
- DSE 同一个引擎，高端卡跑 PBR 写实，UHD 620 跑 Toon 简化版——这是架构优势

### 2.2 性能优化措施

| 措施                              |  投入  |          性能收益         |       做      |
| :------------------------------ | :--: | :-------------------: | :----------: |
| **通用 Mesh LOD 系统**              | 🟡 中 |    降顶点数 50-80%（远距离）   |       ✅ 已完成      |
| **GPU Instancing**              | 🟡 中 | 降 Draw Call 90%（大量同模） |       ✅ 已完成      |
| **纹理 .dds / BCn 直接上传**          | 🟢 低 |  降 VRAM 30-40% + 加载速度 |       🟡 待办    |
| **纹理流送（Texture Streaming）**     | 🔴 高 |     降 VRAM 峰值 50%+    |     🟡 评估    |
| **Compute Shader 扩展**           | 🟡 中 |     Bloom 等后处理性能提升    | 🟡 优先 Vulkan |
| **SSBO → UBO fallback**         | 🟢 低 | 降 GPU 要求至 GL 3.3    |   🟡 待办    |
| **LTCG 编译**                     | 🟢 低 |      二进制降 10-20%      |       🟡 待办      |
| **Profile-guided Optimization** | 🟡 中 |      运行时性能 5-15%      |      🟡      |

### 2.3 兼容性措施

| 措施                      |  投入  |       兼容收益       |   做  |
| :---------------------- | :--: | :--------------: | :--: |
| **D3D11 后端**             | ✅ 已完成 |    Win7+ 全平台覆盖   | ✅ 已完成 |
| **SSBO → UBO fallback** | 🟢 低 | 降 GPU 要求至 GL 3.3 |   🟡 待办  |
| **GLES 3.1 兼容封装**       | 🔴 高 |       移动端支持      | ❌ 暂不 |
| **Vulkan 最低版本保持 1.0**   | 🟢 低 |   最广 Vulkan 覆盖   |   ✅  |

### 2.4 当前构建性能缺陷

| 问题                            |        影响        | 修复                       |
| ----------------------------- | :--------------: | ------------------------ |
| 缺少 Release 构建脚本               | 无法生成 Release 二进制 | 统一构建脚本支持 Release         |
| LTCG 未启用                      |    二进制大 10-20%   | 加 `/GL` + `/LTCG`        |
| WINDOWS\_EXPORT\_ALL\_SYMBOLS |    DLL 导出冗余符号    | 改用 .def 文件或 dllexport 标注 |

***

## 三、画质跃升

### 3.1 写实渲染跃升路径

```
[当前] PBR + CSM + PCSS + IBL + 全后处理链 + SSS + Clear Coat + Anisotropy + POM + GPU Instancing + Mesh LOD
       ↓ Phase 2「氛围增强」(1-2周)
[阶段1] + Volumetric Fog + Decal
       ↓ Phase 3「极致细节」(2-4周)
[阶段2] + 半透物体(OIT) + 高质量水/海洋 + HDR10
```

**Phase 1 已完成（画质细腻度）：**
- SSS（Pre-integrated Skin）— ~30 行 GLSL/HLSL/SPIR-V，纯 shader 方案
- Clear Coat + Anisotropy — 扩展 PBR BRDF 公式，~20 行
- POM（Parallax Occlusion Mapping）— 16 层视差遮挡，~40 行
- GPU Instancing — 三后端合批，降 Draw Call 90%
- 通用 Mesh LOD — 屏幕空间投影 + hysteresis 死区

### 3.2 风格化渲染跃升路径

```
[当前] Toon/Cel Shading + Color Banding ✅
       ↓ Phase 2「引擎级支持」(1-2周)
[阶段1] + Outline（ForwardScenePass 双 Pass 变体）
       + Edge Detection（新增后处理 Pass）
       ↓ Phase 3「风格化材质库」(3-5天)
[阶段2] + 水彩/素描/动漫风的 DSSL 材质集合
       + NPR 光照模型库
```

**DSSL 材质系统已就绪** — `toon_cel.dssl` 提供 7 参数可配置 Toon Shading：

| 参数 | 类型 | 说明 |
|------|------|------|
| `shadow_color` | vec4(rgb+threshold) | 阴影颜色 + 阈值 |
| `rim_color` | vec3 | 边缘光颜色 |
| `shadow_softness` | float | 阴影过渡软度 |
| `specular_size` | float | 高光大小 |
| `specular_strength` | float | 高光强度 |
| `rim_strength` | float | 边缘光强度 |
| `shadow_threshold` | float | 阴影阈值（DSSL 定义） |

### 3.3 画质提升效果预估

| 技术             |   画面感知提升   |       性能成本       |
| -------------- | :--------: | :--------------: |
| ✅ SSS         | 中高（角色皮肤质感） |    低（1 次纹理采样）    |
| ✅ Toon Shading |  高（风格化基底）  |        无额外       |
| ✅ Clear Coat   |  中（表面光泽）   |    低（BRDF 扩展）    |
| ✅ Anisotropy   |  中（金属发丝）   |    低（BRDF 扩展）    |
| ✅ POM          |  中（微深度）    |     中（16 层采样）    |
| ✅ Mesh LOD     |   低感知但高帧率  |     负成本（降负载）     |
| ✅ GPU Instancing | 低感知但高帧率 | 负成本（降 Draw Call） |
| Volumetric Fog |   高（场景氛围）  |  中（raymarching）  |
| Decal          |   中（场景细节）  |   低（1 次全屏 Pass）  |
| Outline        |  高（风格化轮廓）  |   低（1 次额外 Pass）  |

---

## 四、代码瘦身

### 4.1 原则

- **全部自有代码保留不动**（包括 editor\_cpp / 2d 模块 / 所有 77K 行）
- 瘦身针对：**第三方依赖的条件编译 + 残留引用清理**

### 4.2 第三方依赖优化

| 依赖                      |        建议       | 理由                                  |           自有代码改动           |
| ----------------------- | :-------------: | ----------------------------------- | :------------------------: |
| Assimp                  | 🟢 默认 OFF，需时 ON | 运行时不导入 FBX/glTF，离线 AssetBuilder 使用  |         无，只改 CMake         |
| Spine Runtime           |    🟢 默认 OFF    | 2D 骨骼动画，多数 3D 场景不需要                 | 无，已受 DSE\_ENABLE\_SPINE 控制 |
| Luasocket               |    🟢 默认 OFF    | 引擎未使用                               |              无             |
| easy\_profiler          |    🟢 默认 OFF    | 开发期工具                               |              无             |
| FMOD                    |    🟢 默认 OFF    | miniaudio 已满足                       |              无             |
| Box2D                   |      🟡 不动      | 2D 物理需要时自动启用                        |              无             |
| PhysX                   |      🟡 不动      | 3D 物理基础，DSE\_ENABLE\_PHYSX 控制       |              无             |
| GTest                   |    🟢 默认 OFF    | 非发行版，DSE\_BUILD\_GTESTS 控制          |              无             |
| FreeType                |      🔴 保留      | UI 系统必需                             |              无             |
| glslang+SPIRV-Cross     |      🔴 保留      | Vulkan 必需，已受 DSE\_ENABLE\_VULKAN 控制 |              无             |
| spscqueue/rttr/timetool |  🟢 移除 CMake 引用 | 目录不存在，纯残留                           |         CMake 删 3 行        |

**效果**：

- 第三方源码体积：\~200K → **\~120K**（非发行版关闭测试/工具/2D 库）
- 运行时 DLL 数量不变（Assimp/Spine/FMOD 是源码编入 dse_engine.dll 的，非独立 DLL）
  编译受益在：默认 OFF 后跳过源码编译，引擎 DLL 体积减小，构建时间缩短
- **自有代码：零改动**

### 4.3 构建配置优化

| 方向           | 操作                            |       效果       |
| ------------ | ----------------------------- | :------------: |
| LTCG         | 加 `/GL` + `/LTCG`             |  二进制 -10\~20%  |
| Release 构建脚本 | build\_all.bat 支持 `--release` | 产出 Release 二进制 |
| 条件编译默认值      | DSE\_BUILD\_GTESTS 默认 OFF     |     干净构建更小     |

### 4.4 CMake 残留引用清理

```
当前 CMakeLists.txt 第 119-125 行:
  include_directories("depends/spscqueue/include")  ← 目录不存在
  include_directories("depends/rttr-0.9.6/src")     ← 目录不存在
  include_directories("depends/timetool")            ← 目录不存在

修复: 删除这 3 行，无副作用
```

***

## 五、综合优先级路线

### 整体路线图

```
时间线        Phase 2 (1-2周)          Phase 3 (3-5周)
          ┌─────────────────┐  ┌─────────────────┐
画质       │ Volumetric Fog  │  │ DSSL 风格化库   │
跃升       │ Decal System    │  │ NPR 光照模型    │
          │ Outline 渲染    │  │ OIT 透明排序    │
          │ Edge Detection  │  │ 水/海洋系统     │
          └─────────────────┘  └─────────────────┘
性能       │ SSBO→UBO fallb. │  │                │
+          │ .dds 直接上传   │  │                │
兼容       │ Release 构建    │  │                │
          │ CMake 残留清理   │  │                │
          │ LTCG 编译       │  │                │
          └─────────────────┘  └─────────────────┘
✅ 已完成   │ Phase 1 全部    │  │                │
          │ Mesh LOD        │  │                │
          │ GPU Instancing  │  │                │
          │ 9-Slice UI      │  │                │
          │ D3D11 后端      │  │                │
          │ 动画系统 Phase1 │  │                │
```

### Phase 2 细化（1-2 周）← 当前最高优先级

| 任务                                   |  类型 |  工作量  |
| :----------------------------------- | :-: | :---: |
| SSBO → UBO fallback 路径             |  兼容  | 0.5 天 |
| .dds / BCn 纹理直接上传支持                |  性能  |  1 天  |
| Release 构建脚本 + LTCG                 |  构建  | 0.5 天 |
| 默认关闭非必需第三方 + CMake 清理              |  瘦身  | 0.5 天 |
| Volumetric Fog raymarching           |  写实 |  3 天  |
| Decal System（screen-space）           |  写实 |  2 天  |
| Outline 双 Pass + Edge Detection Pass | 风格化 |  2 天  |

### Phase 3 细化（3-5 周）

| 任务                                         |  类型 | 工作量 |
| :----------------------------------------- | :-: | :-: |
| DSSL 风格化材质库（toon / outline / watercolor 等） | 风格化 | 3 天 |
| NPR 光照模型集合（DSSL light()）                   | 风格化 | 2 天 |
| OIT（Order Independent Transparency）        |  写实 | 4 天 |
| Water/Ocean 系统                             |  写实 | 5 天 |
| 纹理流送评估 + 基础实现                              |  性能 | 3 天 |

***

## 六、核心竞争力定位

```
对比「极客引擎」生态位:

             代码量    最低GPU    画质    风格化    第三方负担    编辑器
UE5         数百万    DX11+     ⭐⭐⭐⭐⭐ ⭐⭐⭐⭐⭐  极大         内置
Unity6      数百万    GLES3+    ⭐⭐⭐⭐  ⭐⭐⭐⭐   极大         内置
Godot4      ~100万   GLES3+    ⭐⭐⭐   ⭐⭐⭐    大           内置
DSE 当前    ~8.4万   GL 4.3+   ⭐⭐⭐⭐⭐ ⭐⭐⭐   中           有（暂不扩展）
DSE 目标    ~8.4万   GL 3.3+   ⭐⭐⭐⭐⭐ ⭐⭐⭐⭐   小           有（后续扩展）

「极简代码 + 极致画质」是独特生态位 —— 没有主流引擎能做到。
DSE 自有代码从 77K 增长至 ~84K（新增动画 IK/Layer + DX11 + Toon/Cel + ClearCoat + POM + Instancing），第三方从 ~200K 可降至 ~120K，
D3D11 已完成！硬件兼容扩展至 GL 3.3+（需 SSBO→UBO fallback），画质补完 Volumetric Fog / Decal / Outline。
```

---

## 总结

| 维度        |     当前    |       Phase 2 后       |    Phase 3 后   |
| :-------- | :-------: | :-------------------: | :------------: |
| 写实渲染完整度   |   ~94%    |         ~96%          |      ~99%      |
| 风格化渲染     |    ~35%   | 65%（+ Outline/Edge Detection） | 100%（材质库） |
| 最低 GPU    |  GL 4.3+  | GL 3.3+（UBO fallback） |     GL 3.3+    |
| D3D11 后端  |   ✅ 可用   |          ✅ 可用         |      ✅ 可用      |
| 第三方依赖     |   ~200K   |         ~120K         |     ~120K      |
| 自有代码      |   ~84K    |       84K（只增不减）       |      84K+      |
| **总体成熟度** | **~85%**  |       **~92%**        |    **~97%**    |

> 核心思路：**自有代码全部保留**（编辑器留着后续扩展），瘦身针对第三方依赖的条件编译和 CMake 残留；画质跃升分写实和风格化两条线并行推进；性能兼容走 UBO fallback + .dds 纹理 + LTCG 三管齐下（Mesh LOD / GPU Instancing / D3D11 已完成）。

