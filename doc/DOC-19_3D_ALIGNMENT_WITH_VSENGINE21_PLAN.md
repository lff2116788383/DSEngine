# DOC-19 3D 功能对齐任务周期清单

## 1. 文档目标

本文档直接对照当前代码现状，重建 `DSEngine` 面向 [`reference/VSEngine2.1`](reference/VSEngine2.1) 的 3D 对齐计划。

本文档只回答 4 个问题：

1. 当前 `DSEngine` 的 3D 已经做到哪里。
2. 接下来应该按什么顺序推进。
3. 每个周期要交付什么、验收什么。
4. [`reference/VSEngine2.1/Demo`](reference/VSEngine2.1/Demo) 里哪些 demo 值得作为后续参考列表。

本次重构后的原则非常明确：

- **先对齐功能，再做 demo，编辑器最后补。**
- **每个周期只保留可执行任务，不再堆大段历史说明。**
- **所有计划都以当前代码已经存在的 3D 基线为起点。**

---

## 2. 当前代码基线

### 2.1 当前已经具备的 3D 基础

从当前代码看，`DSEngine` 已经不是纯 3D 占位状态，而是具备了继续扩展的运行时骨架。

#### 运行时组件层

- [`MeshRendererComponent`](engine/ecs/components_3d.h:12)
- [`SkyLightComponent`](engine/ecs/components_3d.h:118)
- [`Animator3DComponent`](engine/ecs/components_3d.h:150)
- [`TerrainComponent`](engine/ecs/components_3d.h:196)

这说明当前已经覆盖：

- 网格渲染
- 基础 PBR 参数承载
- 点光 / 聚光 / 天光
- 骨骼动画最小表达
- 地形组件骨架

#### 运行时系统层

- [`FramePipeline`](engine/runtime/frame_pipeline.h:42)
- [`MeshRenderSystem`](modules/gameplay_3d/rendering/mesh_render_system.h:20)
- [`AnimatorSystem`](modules/gameplay_3d/animation/animator_system.h:11)
- [`TerrainSystem`](modules/gameplay_3d/rendering/terrain_system.h:10)
- [`FrustumCullingSystem`](modules/gameplay_3d/rendering/frustum_culling_system.h:15)
- [`Particle3DSystem`](modules/gameplay_3d/particles/particle3d_system.h:13)

这说明当前 3D 主线已经具备：

- 运行时帧调度
- 3D 网格渲染提交
- 动画更新
- 地形更新
- 视锥剔除
- 3D 粒子骨架

#### 当前已签入的 3D 场景基线

- [`assets/scenes/3d_mvp_minimal.scene.json`](assets/scenes/3d_mvp_minimal.scene.json)
- [`assets/scenes/reference_demo_15_8.scene.json`](assets/scenes/reference_demo_15_8.scene.json)
- [`assets/scenes/reference_demo_15_9.scene.json`](assets/scenes/reference_demo_15_9.scene.json)

说明当前已经不是“只有代码，没有落地场景”，而是已经开始进入参考场景对齐阶段。

### 2.2 当前最关键的差距

虽然基础已经有了，但和 [`reference/VSEngine2.1`](reference/VSEngine2.1) 相比，当前仍然缺少稳定的 3D 功能闭环。

当前最关键的问题不是“没有 3D”，而是：

1. **功能点存在，但层次还不够稳定。**
2. **材质 / 灯光 / 动画 / 阴影 仍偏最小闭环，不够完整。**
3. **demo 还只是开始，不是完整参考矩阵。**
4. **编辑器 3D 工作流明显滞后于运行时。**

因此，后续顺序必须固定为：

1. **先补齐 runtime 3D 功能。**
2. **再建立 reference demo 对齐列表与逐个落地。**
3. **最后补编辑器。**

---

## 3. 总体推进顺序

本文档把后续工作拆成 4 个任务周期。

### 周期顺序

- **P1：3D 运行时功能全面对齐周期**
- **P2：3D 渲染与资源质量收口周期**
- **P3：reference demo 对齐周期**
- **P4：3D 编辑器补齐周期**

其中：

- `P1 + P2` 属于“先对齐所有功能”。
- `P3` 才开始系统做 demo。
- `P4` 最后做编辑器。

---

## 4. P1：3D 运行时功能全面对齐周期

> 目标：先把 3D 运行时从“可跑”推进到“功能面完整且边界清楚”。

### 4.1 材质与网格渲染链路对齐

**当前基础：**
- [`MeshRendererComponent`](engine/ecs/components_3d.h:12)
- [`MeshRenderSystem`](modules/gameplay_3d/rendering/mesh_render_system.h:20)

**本周期任务：**
- 统一 `MeshRenderer` 与 `MaterialAsset` 的职责边界。
- 完成材质实例优先、组件字段兜底的正式约束说明。
- 补齐材质参数在 scene、runtime、test 三层的一致性检查。
- 明确哪些参数已真正参与渲染，哪些只是数据占位。
- 收敛 shader variant 命名与用途，减少隐式规则。

**验收标准：**
- 至少 1 条 mesh + material instance 的稳定回归链路。
- scene 存档 / 读档 / runtime 表现一致。
- 文档里明确材质字段支持矩阵。

**当前进度（2026-04-12）：**
- [x] [`MeshRendererComponent`](engine/ecs/components_3d.h:12) 已显式增加 [`MaterialDataSource`](engine/ecs/components_3d.h:13) 与 [`material_alpha_cutoff`](engine/ecs/components_3d.h:27)，不再只依赖隐式规则。
- [x] [`Scene`](engine/scene/scene.cpp:147) 已完成 [`material_alpha_cutoff`](engine/ecs/components_3d.h:27)、排序字段与 [`material_data_source`](engine/ecs/components_3d.h:37) 的存档 / 读档 roundtrip，并兼容旧 scene。
- [x] [`MeshRenderSystem::Render()`](modules/gameplay_3d/rendering/mesh_render_system.cpp:573) 已按 [`material_data_source`](engine/ecs/components_3d.h:37) 区分 component fallback 与 material instance。
- [x] [`MeshDrawItem`](engine/render/rhi/rhi_device.h:38) 已承载 [`material_alpha_cutoff`](engine/render/rhi/rhi_device.h:59) 与实例数据来源标记。
- [x] [`OpenGLRhiDevice::RealSubmitDrawMeshBatch()`](engine/render/rhi/rhi_device.cpp:1100) 与内置 fragment shader 已真实消费 alpha cutoff，不再是仅数据占位。
- [x] 已补充 [`mesh_material_desc_test.cpp`](tests/engine/mesh_material_desc_test.cpp)、[`scene_flow_test.cpp`](tests/engine/scene/scene_flow_test.cpp)、[`mesh_renderer_test.cpp`](tests/modules/gameplay_3d/rendering/mesh_renderer_test.cpp)、[`mesh_render_system_material_resolution_test.cpp`](tests/modules/gameplay_3d/rendering/mesh_render_system_material_resolution_test.cpp) 覆盖 scene / runtime / draw item 解析链路。
- [ ] 尚未在确认可用的 CMake build 目录中完成一次真实编译与测试执行。

### 4.2 动画系统最小工作流对齐

**当前基础：**
- [`Animator3DComponent`](engine/ecs/components_3d.h:157)
- [`AnimatorSystem`](modules/gameplay_3d/animation/animator_system.h:11)

**当前进度（2026-04-12）：**
- [x] [`Scene`](engine/scene/scene.cpp:262) 已覆盖 [`blend_parameter`](engine/ecs/components_3d.h:170)、[`blend_parameter_value`](engine/ecs/components_3d.h:171) 与 [`blend_nodes`](engine/ecs/components_3d.h:169) 的 serialize / deserialize。
- [x] [`tests/engine/scene/scene_flow_test.cpp`](tests/engine/scene/scene_flow_test.cpp:353) 已对动画 scene roundtrip 中的 blend node 名称、路径、权重、阈值做断言。
- [x] [`SaveScene()`](apps/editor_cpp/src/editor_scene_io.cpp:149) / [`LoadScene()`](apps/editor_cpp/src/editor_scene_io.cpp:463) 已补齐 editor scene 对 [`Animator3DComponent`](engine/ecs/components_3d.h:157) 的 blend 参数与节点数组存取，避免 editor 桥接丢字段。
- [x] [`AnimatorSystem::Update()`](modules/gameplay_3d/animation/animator_system.cpp:124) 已补齐 runtime 侧 clip 校验、统一采样缓存、legacy 单动画路径收口，以及 legacy 1D blend tree 的空节点 / 单节点 / 阈值夹取行为。
- [x] [`AnimatorSystem::Update()`](modules/gameplay_3d/animation/animator_system.cpp:124) 已把 state machine 路径中的 blend tree 采样接入 [`final_bone_matrices`](engine/ecs/components_3d.h:186) 更新链路，填补此前 blend tree TODO 缺口。
- [ ] 尚未补上面向 [`AnimatorSystem`](modules/gameplay_3d/animation/animator_system.cpp:124) 的运行时 blend 边界测试与空资源行为测试。

**本周期任务：**
- 稳定 `dskel` / `danim` / blend 参数的运行时边界。
- 固化最小 1D blend 行为定义。
- 明确动画资源导入、scene 表达、runtime 更新三层的责任。
- 补齐动画空资源、单动画、双节点 blend、阈值边界测试。

**验收标准：**
- 1 个可回归的骨骼动画场景。
- scene roundtrip 与 runtime 行为一致。
- 动画参数与节点阈值有稳定测试覆盖。

### 4.3 灯光模型全面对齐

**当前基础：**
- [`SkyLightComponent`](engine/ecs/components_3d.h:125)
- 点光 / 聚光 / 方向光均已存在于 [`engine/ecs/components_3d.h`](engine/ecs/components_3d.h)

**当前进度（2026-04-12）：**
- [x] [`Scene`](engine/scene/scene.cpp:248) 已覆盖 [`PointLightComponent`](engine/ecs/components_3d.h:102)、[`SpotLightComponent`](engine/ecs/components_3d.h:112)、[`SkyLightComponent`](engine/ecs/components_3d.h:125) 的 scene 存档 / 读档。
- [x] [`tests/engine/scene/scene_flow_test.cpp`](tests/engine/scene/scene_flow_test.cpp:311) 已对 point / spot / sky 三类灯光的 roundtrip 字段做回归。
- [x] [`SaveScene()`](apps/editor_cpp/src/editor_scene_io.cpp:149) / [`LoadScene()`](apps/editor_cpp/src/editor_scene_io.cpp:463) 已补齐 editor scene 对 point falloff、spot 全字段、sky light 参数的桥接。
- [ ] 尚未把多灯运行时表现、阴影开关语义和参数说明矩阵文档化到统一收口材料。

**本周期任务：**
- 固定 Directional / Point / Spot / Sky 的字段边界。
- 补充灯光参数的统一说明：颜色、强度、半径、衰减、阴影开关。
- 清理“参数存在但行为不明确”的部分。
- 增加多灯场景回归。

**验收标准：**
- 四类灯光均可稳定保存、加载、运行。
- 多灯组合在 runtime 中表现稳定。
- 参数语义文档化，不再依赖猜测。

### 4.4 阴影能力主线对齐

**当前基础：**
- 当前主线仍以 `Directional + CSM` 为主。

**本周期任务：**
- 明确当前真正支持的 shadow path。
- 列出 `已支持 / 部分支持 / 未支持` 矩阵。
- 补齐 spot shadow 与 point shadow 的实现任务清单。
- 保证 `RenderStats`、shadow pass 统计可以用来做收口判断。

**验收标准：**
- 阴影支持矩阵明确。
- 至少一条稳定的方向光阴影回归。
- 后续 spot / point shadow 的分解任务准备完成。

### 4.5 地形、剔除、粒子等 3D 子系统边界对齐

**当前基础：**
- [`TerrainComponent`](engine/ecs/components_3d.h:196)
- [`TerrainSystem`](modules/gameplay_3d/rendering/terrain_system.h:10)
- [`FrustumCullingSystem`](modules/gameplay_3d/rendering/frustum_culling_system.h:15)
- [`Particle3DSystem`](modules/gameplay_3d/particles/particle3d_system.h:13)

**本周期任务：**
- 明确这些系统属于“已接入主线”还是“实验性骨架”。
- 为每个系统补一个最小场景或回归测试。
- 把“存在代码但无场景/无测试”的部分列为后续重点。

**验收标准：**
- 每个系统都有状态归类：稳定 / 实验 / 占位。
- 每个系统至少有一条验证入口。

---

## 5. P2：3D 渲染与资源质量收口周期

> 目标：在 P1 功能已经齐全的前提下，把质量、资源链、表现一致性收口。

### 5.1 资源导入与烹饪链收口

**本周期任务：**
- 重新梳理 `gltf/glb -> dmesh/dmat/danim/dskel` 当前能力边界。
- 对照 [`reference/VSEngine2.1/FBXConverter`](reference/VSEngine2.1/FBXConverter) 列出缺口，但不照搬旧工具结构。
- 明确哪些 reference 资源可直接转，哪些需要替代资源。

**验收标准：**
- 至少一套静态网格资源和一套骨骼资源能稳定导入并进入 demo 场景。

### 5.2 渲染表现一致性收口

**本周期任务：**
- 梳理 PBR 参数的真实消费路径。
- 区分“已真实支持”与“仅存档支持”。
- 补齐 shader / RHI / draw item 之间的字段映射清单。
- 清理临时补偿逻辑和不透明行为。

**验收标准：**
- 参数链路清晰可追踪。
- 关键表现项可稳定复现。

### 5.3 运行时回归矩阵建立

**本周期任务：**
- 建立 3D 回归测试矩阵：材质、动画、灯光、阴影、scene roundtrip、runtime startup。
- 统一 smoke、unit、scene regression 的分层意义。
- 明确哪些测试是门禁级别。

**验收标准：**
- 有一份稳定的 3D 回归矩阵清单。
- 每个功能域至少有 1 条强回归。

---

## 6. P3：reference demo 对齐周期

> 目标：在功能对齐完成后，再开始系统做 demo，而不是边补功能边临时拼 demo。

### 6.1 demo 周期原则

本周期只做两件事：

1. 建立 [`reference/VSEngine2.1/Demo`](reference/VSEngine2.1/Demo) 的参考清单。
2. 逐个选择高价值 demo，在 `DSEngine` 中做对应 scene 与运行基线。

demo 周期不再承担底层功能发明职责，底层缺口原则上应在 `P1/P2` 收口。

### 6.2 demo 交付要求

每个 demo 对齐项都必须交付：

- 一个签入的 scene 文件
- 一份资源清单
- 一条 smoke 或 regression
- 一份差异说明
- 一张截图或一条视觉基线说明

### 6.3 reference demo 完整参考列表

下面不是“全部立即实现”，而是后续做 demo 对齐时的参考池。

#### 第一优先级：直接服务当前 3D 主线

- [`reference/VSEngine2.1/Demo/15/15.7/Source.cpp`](reference/VSEngine2.1/Demo/15/15.7/Source.cpp)
- [`reference/VSEngine2.1/Demo/15/15.8/Source.cpp`](reference/VSEngine2.1/Demo/15/15.8/Source.cpp)
- [`reference/VSEngine2.1/Demo/15/15.9/Source.cpp`](reference/VSEngine2.1/Demo/15/15.9/Source.cpp)
- [`reference/VSEngine2.1/Demo/14/14.8/Source.cpp`](reference/VSEngine2.1/Demo/14/14.8/Source.cpp)
- [`reference/VSEngine2.1/Demo/14/14.9/Source.cpp`](reference/VSEngine2.1/Demo/14/14.9/Source.cpp)
- [`reference/VSEngine2.1/Demo/16/16.7/Source.cpp`](reference/VSEngine2.1/Demo/16/16.7/Source.cpp)
- [`reference/VSEngine2.1/Demo/16/16.8/Source.cpp`](reference/VSEngine2.1/Demo/16/16.8/Source.cpp)

建议用途：

- `15.x`：骨骼模型、灯光、观察镜头、材质参数
- `14.x`：场景渲染、材质、后处理或渲染组织参考
- `16.x`：更复杂的 3D 运行表现参考

#### 第二优先级：可作为后续扩展参考

- [`reference/VSEngine2.1/Demo/18/18.2/Source.cpp`](reference/VSEngine2.1/Demo/18/18.2/Source.cpp)
- [`reference/VSEngine2.1/Demo/18/18.4/Source.cpp`](reference/VSEngine2.1/Demo/18/18.4/Source.cpp)
- [`reference/VSEngine2.1/Demo/18/18.7/Source.cpp`](reference/VSEngine2.1/Demo/18/18.7/Source.cpp)
- [`reference/VSEngine2.1/Demo/19/19.2/Source.cpp`](reference/VSEngine2.1/Demo/19/19.2/Source.cpp)
- [`reference/VSEngine2.1/Demo/21/21.1/Source.cpp`](reference/VSEngine2.1/Demo/21/21.1/Source.cpp)

建议用途：

- 更复杂的渲染表现
- 更完整的运行时组合场景
- 用于后续高阶 3D 能力验证

#### 第三优先级：暂列观察池

- [`reference/VSEngine2.1/Demo/3/Demo3.1/Source.cpp`](reference/VSEngine2.1/Demo/3/Demo3.1/Source.cpp)
- [`reference/VSEngine2.1/Demo/3/Demo3.2/Source.cpp`](reference/VSEngine2.1/Demo/3/Demo3.2/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.1/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.1/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.2/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.2/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.3/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.3/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.4/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.4/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.6/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.6/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.7/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.7/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.8/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.8/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.9/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.9/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.10/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.10/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.11/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.11/Source.cpp)
- [`reference/VSEngine2.1/Demo/4/Demo4.12/Source.cpp`](reference/VSEngine2.1/Demo/4/Demo4.12/Source.cpp)
- [`reference/VSEngine2.1/Demo/6/Demo6.1/Source.cpp`](reference/VSEngine2.1/Demo/6/Demo6.1/Source.cpp)
- [`reference/VSEngine2.1/Demo/7/Demo7.1/Source.cpp`](reference/VSEngine2.1/Demo/7/Demo7.1/Source.cpp)
- [`reference/VSEngine2.1/Demo/8/8.5/Source.cpp`](reference/VSEngine2.1/Demo/8/8.5/Source.cpp)
- [`reference/VSEngine2.1/Demo/8/8.6/Source.cpp`](reference/VSEngine2.1/Demo/8/8.6/Source.cpp)
- [`reference/VSEngine2.1/Demo/8/8.7/Source.cpp`](reference/VSEngine2.1/Demo/8/8.7/Source.cpp)
- [`reference/VSEngine2.1/Demo/8/8.8/Source.cpp`](reference/VSEngine2.1/Demo/8/8.8/Source.cpp)
- [`reference/VSEngine2.1/Demo/8/Demo8.1/Source.cpp`](reference/VSEngine2.1/Demo/8/Demo8.1/Source.cpp)
- [`reference/VSEngine2.1/Demo/11/11.1/Source.cpp`](reference/VSEngine2.1/Demo/11/11.1/Source.cpp)
- [`reference/VSEngine2.1/Demo/12/12.1/Source.cpp`](reference/VSEngine2.1/Demo/12/12.1/Source.cpp)
- [`reference/VSEngine2.1/Demo/12/12.3/Source.cpp`](reference/VSEngine2.1/Demo/12/12.3/Source.cpp)
- [`reference/VSEngine2.1/Demo/13/13.9/Source.cpp`](reference/VSEngine2.1/Demo/13/13.9/Source.cpp)
- [`reference/VSEngine2.1/Demo/13/13.12/Source.cpp`](reference/VSEngine2.1/Demo/13/13.12/Source.cpp)
- [`reference/VSEngine2.1/Demo/14/14.2/Source.cpp`](reference/VSEngine2.1/Demo/14/14.2/Source.cpp)
- [`reference/VSEngine2.1/Demo/18/18.1/Demo18.1.cpp`](reference/VSEngine2.1/Demo/18/18.1/Demo18.1.cpp)
- [`reference/VSEngine2.1/Demo/18/18.3/Demo18.3.cpp`](reference/VSEngine2.1/Demo/18/18.3/Demo18.3.cpp)
- [`reference/VSEngine2.1/Demo/18/18.5/Demo18.5.cpp`](reference/VSEngine2.1/Demo/18/18.5/Demo18.5.cpp)
- [`reference/VSEngine2.1/Demo/18/18.6/Demo18.6.cpp`](reference/VSEngine2.1/Demo/18/18.6/Demo18.6.cpp)
- [`reference/VSEngine2.1/Demo/19/19.1/Demo19.1.cpp`](reference/VSEngine2.1/Demo/19/19.1/Demo19.1.cpp)

说明：

- 这部分先保留为观察池。
- 等 `P1/P2/P3` 前两批任务稳定后，再决定是否逐步纳入。

### 6.4 demo 落地顺序建议

建议按以下顺序落地：

1. 已有基线继续收口：
   - [`assets/scenes/reference_demo_15_8.scene.json`](assets/scenes/reference_demo_15_8.scene.json)
   - [`assets/scenes/reference_demo_15_9.scene.json`](assets/scenes/reference_demo_15_9.scene.json)
2. 再扩展 `15.7`
3. 再做 `14.8 / 14.9`
4. 再做 `16.7 / 16.8`
5. 最后视资源与功能成熟度扩展 `18.x / 19.x / 21.x`

---

## 7. P4：3D 编辑器补齐周期

> 目标：等 runtime 和 demo 都稳定后，再补 editor，不反过来拖慢主线。

### 7.1 编辑器周期只做三类事

#### 第一类：可视化检视补齐

- MeshRenderer 检视
- MaterialInstance 检视
- Light 组件检视
- Animator3D 检视
- Terrain 检视

#### 第二类：3D 场景操作补齐

- 层级选择
- 变换 gizmo
- 摄像机观察
- 基础场景保存 / 回滚

#### 第三类：资源驱动工作流补齐

- 材质参数调节
- 动画资源切换
- 场景资源引用检查
- demo 场景在 editor 中可打开、可检查、可保存

### 7.2 编辑器周期验收标准

- reference scene 能在 editor 中稳定打开。
- 关键 3D 组件可视化检视完整。
- 编辑器修改不会破坏 runtime scene roundtrip。

---

## 8. 周期清单总表

## P1：3D 运行时功能全面对齐

- [~] 材质与网格渲染链路对齐（scene / runtime / draw item / OpenGL shader 已打通，待真实构建验证与材质字段支持矩阵文档补全）
- [~] 动画系统最小工作流对齐（scene / editor 桥接已补齐 blend 参数与节点数组，待 runtime blend 边界测试收口）
- [~] 灯光模型全面对齐（scene / editor 桥接已补齐 point / spot / sky 字段，待多灯 runtime 与参数矩阵文档收口）
- [ ] 阴影能力主线对齐
- [ ] 地形 / 剔除 / 粒子等子系统状态归类与验证入口补齐

## P2：3D 渲染与资源质量收口

- [ ] 资源导入与烹饪链收口
- [ ] 渲染表现一致性收口
- [ ] 3D 回归矩阵建立

## P3：reference demo 对齐

- [ ] 收口 [`reference_demo_15_8`](assets/scenes/reference_demo_15_8.scene.json)
- [ ] 收口 [`reference_demo_15_9`](assets/scenes/reference_demo_15_9.scene.json)
- [ ] 新增 `15.7` 对齐 demo
- [ ] 新增 `14.8 / 14.9` 对齐 demo
- [ ] 新增 `16.7 / 16.8` 对齐 demo
- [ ] 视情况扩展 `18.x / 19.x / 21.x`

## P4：3D 编辑器补齐

- [ ] Mesh / Material / Light / Animator / Terrain 检视补齐
- [ ] 3D 场景操作补齐
- [ ] demo 场景 editor 工作流补齐

---

## 9. 当前执行结论

从当前代码状态出发，最合理的推进方式已经固定：

1. **先做 P1，补齐 3D runtime 全功能边界。**
2. **再做 P2，把渲染质量、资源链、回归矩阵收口。**
3. **再做 P3，系统性对齐 [`reference/VSEngine2.1/Demo`](reference/VSEngine2.1/Demo)。**
4. **最后做 P4，补编辑器。**

后续所有 3D 工作，都应以上述周期清单为准，不再回到“功能、demo、编辑器同时摊大饼”的推进方式。