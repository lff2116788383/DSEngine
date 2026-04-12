# DOC-19 DSEngine 3D 对齐 `reference/VSEngine2.1` 改造清单

## 1. 文档目标

本文档用于回答一个更具体的问题：

> 如果要让 `DSEngine` 的 3D 能力逐步向 [`reference/VSEngine2.1`](reference/VSEngine2.1) 看齐，应该按什么顺序、以什么边界、补哪些能力，而不把当前仓库重新带回“功能点很多但闭环不清晰”的状态？

本文档不是要把 `DSEngine` 直接改造成旧引擎的复制品，而是要把 [`reference/VSEngine2.1`](reference/VSEngine2.1) 中已经证明有价值的 3D 能力，拆成适合当前仓库结构的对齐改造清单。

约束前提：

- 当前稳定主线仍然是 2D，见 [`doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md`](doc/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md:16)
- 当前 3D 更准确的定位仍然是 MVP 收口中，见 [`doc/DOC-15_3D_GAP_AUDIT_AND_TODO.md`](doc/DOC-15_3D_GAP_AUDIT_AND_TODO.md:18)
- 对齐目标应优先补“高价值、可收口、可验证”的能力，不做无边界扩张
- 新增一个明确目标：不仅要“能力对齐”，还要把 [`reference/VSEngine2.1`](reference/VSEngine2.1) 的代表性 3D demo 用同类资源在 `DSEngine` 中重现，形成可对照演示结果

---

## 2. 对齐基线：当前 `DSEngine` 与 `reference/VSEngine2.1` 的差异

### 2.1 `DSEngine` 当前已具备的 3D 基线

当前仓库已经具备以下 3D MVP 基础：

- Mesh / Camera3D / Directional Light / Point Light / Skybox / Terrain 的最小场景闭环，见 [`assets/scenes/3d_mvp_minimal.scene.json`](assets/scenes/3d_mvp_minimal.scene.json:1)
- 3D MVP runtime 启动与 smoke 样例，见 [`samples/cpp/phase1_demo_logic.cpp`](samples/cpp/phase1_demo_logic.cpp:65)
- 3D Inspector 基础检视能力，见 [`apps/editor_cpp/src/editor_inspector_panel.cpp`](apps/editor_cpp/src/editor_inspector_panel.cpp:200)
- Mesh / Terrain / Frustum Culling / Particle3D 等模块化子系统，见 [`doc/DOC-15_3D_GAP_AUDIT_AND_TODO.md`](doc/DOC-15_3D_GAP_AUDIT_AND_TODO.md:33)
- 3D Scene save/load 与 editor bridge 回归，见 [`tests/engine/scene/scene_flow_test.cpp`](tests/engine/scene/scene_flow_test.cpp:117) 与 [`tests/engine/scene/editor_scene_io_bridge_test.cpp`](tests/engine/scene/editor_scene_io_bridge_test.cpp:30)

### 2.2 相比 [`reference/VSEngine2.1`](reference/VSEngine2.1) 的主要缺口

结合 [`reference/VSEngine2.1/VSGraphic`](reference/VSEngine2.1/VSGraphic) 与渲染后端代码，当前缺口主要集中在：

1. **动画工作流深度不够**
   - `DSEngine` 目前只有 `Animator3DComponent` 与基础状态机雏形
   - [`reference/VSEngine2.1`](reference/VSEngine2.1) 已经有 [`VSAnimTree`](reference/VSEngine2.1/VSGraphic/VSAnimTree.h:12)、[`VSAnimSet`](reference/VSEngine2.1/VSGraphic/VSAnimSet.h:203)、[`VSSkeleton`](reference/VSEngine2.1/VSGraphic/VSSkeleton.h:8)、[`VSMorphSet`](reference/VSEngine2.1/VSGraphic/VSMorphSet.h:76)

2. **灯光 / 阴影体系深度不够**
   - `DSEngine` 当前以 Directional Light + CSM 为主，Point Light 能力较基础
   - [`reference/VSEngine2.1`](reference/VSEngine2.1) 已覆盖 [`VSDirectionLight`](reference/VSEngine2.1/VSGraphic/VSDirectionLight.h:10)、[`VSPointLight`](reference/VSEngine2.1/VSGraphic/VSPointLight.h:12)、[`VSSpotLight`](reference/VSEngine2.1/VSGraphic/VSSpotLight.h:7)、[`VSSkyLight`](reference/VSEngine2.1/VSGraphic/VSSkyLight.h:7)，以及 [`VSShadowPass`](reference/VSEngine2.1/VSGraphic/VSShadowPass.h:82) 下多种 shadow path

3. **材质 / 渲染通道体系不够完整**
   - `DSEngine` 现在偏“组件字段驱动 + 少量 shader variant”
   - [`reference/VSEngine2.1`](reference/VSEngine2.1) 已形成 [`VSMaterial`](reference/VSEngine2.1/VSGraphic/VSMaterial.h:389)、[`VSMaterialInstance`](reference/VSEngine2.1/VSGraphic/VSMaterial.h:612)、[`VSSceneRender`](reference/VSEngine2.1/VSGraphic/VSSceneRender.h:51)、[`VSPostEffectSet`](reference/VSEngine2.1/VSGraphic/VSPostEffectSet.h:15) 等完整体系

4. **Terrain 生态不够完整**
   - `DSEngine` 当前只有最小 `TerrainComponent` 与 `terrain_system`
   - [`reference/VSEngine2.1`](reference/VSEngine2.1) 有 [`VSCLodTerrainNode`](reference/VSEngine2.1/VSGraphic/VSCLodTerrainNode.h:6)、[`VSDLodTerrainNode`](reference/VSEngine2.1/VSGraphic/VSDLodTerrainNode.h:5)、[`VSGPULodTerrainNode`](reference/VSEngine2.1/VSGraphic/VSGPULodTerrainNode.h:6)、[`VSRoamTerrainGeometry`](reference/VSEngine2.1/VSGraphic/VSRoamTerrainGemotry.h:23)

5. **资源导入 / 烹饪链深度不够**
   - `DSEngine` 已有 [`apps/tools/asset_builder/main.cpp`](apps/tools/asset_builder/main.cpp:8) 的 `gltf/glb -> dmesh/dmat/danim/dskel` 路径
   - 但 [`reference/VSEngine2.1`](reference/VSEngine2.1) 有独立 [`FBXConverter`](reference/VSEngine2.1/FBXConverter/FBXConverter.cpp) 与更重的 FBX 资源使用习惯

6. **编辑器 3D 工作流不够完整**
   - 当前有 Inspector 基础项，但离成熟的 Scene View / Gizmo / 资源调参工作流还有明显差距

---

## 3. 改造总原则

为了避免“功能面变宽，但主线更乱”，本轮 3D 对齐必须遵循以下原则：

### 3.1 原则一：先补 runtime 高价值骨架，再做 demo 对齐，最后补 editor 工作流

优先级必须是：

1. 材质渲染骨架
2. 动画骨架
3. 光照阴影骨架
4. reference demo 重现基线
5. 编辑器工作流与 terrain / 粒子 / 高级后处理等扩展生态

这里需要明确一个边界：本轮对齐的第一目标是让 [`DSEngine`](.) 在 **runtime 3D 能力** 与 **reference demo 可对照结果** 上逐步向 [`reference/VSEngine2.1`](reference/VSEngine2.1) 靠拢，而不是先去追求一套完整 editor 制作流。

原因是：[`reference/VSEngine2.1`](reference/VSEngine2.1) 本身并不是以 editor 为主要参考对象；当前更有价值的对齐对象，是其 3D runtime 功能体系、资源链深度和 demo 呈现结果。因此 editor 应视为 `DSEngine` 自身的后续产品化增强项，而不是本轮 reference 对齐阶段的前置硬门槛。

### 3.2 原则二：每补一层，都要先形成 scene + test + 文档边界闭环；关键阶段再补 demo 闭环

任何新增 3D 子系统都不能只停留在“代码存在”，至少要同步补：

- 一个最小场景入口
- 一条 smoke 或 regression
- 一份文档边界说明
- 在进入 reference 对齐阶段时，补一个可运行的对齐 demo 或 demo 子场景，用于和 [`reference/VSEngine2.1`](reference/VSEngine2.1) 做肉眼对照

`editor` 检视入口、Scene View、Gizmo 与资源调参工作流属于 `DSEngine` 的工作流增强项，原则上放在 runtime 骨架与 reference demo 基线收口之后推进；除非某项能力必须依赖 editor 才能验证，否则不将其作为当前阶段的统一硬验收条件。

### 3.3 原则三：优先复用当前模块化方向，不回退到旧引擎式重耦合

当前仓库已经明确 3D 要走模块化能力线，见 [`doc/DOC-14_ARCHITECTURE_OPTIMIZATION_PLAN.md`](doc/DOC-14_ARCHITECTURE_OPTIMIZATION_PLAN.md:321)。

因此对齐方式应是：

- 学习 [`reference/VSEngine2.1`](reference/VSEngine2.1) 的能力结构
- 不照搬其全部类型层级与历史包袱
- 继续把 3D 收敛在 `modules/gameplay_3d/`、scene、editor bridge、render pipeline 的清晰边界内

### 3.4 原则四：3D 永远不能反向污染 2D 主线稳定性

任何对齐改造都必须确保：

- `DSE_ENABLE_3D=OFF` 仍然稳定
- 2D 常用门禁不被拖慢或打散
- 3D 的构建、测试、样例、资源路径尽量独立收口

---

## 4. 分阶段对齐改造清单

## 4.1 P0：先把“像引擎”的 3D 基础骨架补齐

这一阶段的目标不是功能多，而是让 `DSEngine` 从“3D MVP”升级为“3D 基础骨架完整”。

### P0-1 材质描述与实例系统收口

目标：把当前 `MeshRendererComponent` 上分散的材质字段，逐步收口为更稳定的材质描述层。

建议项：

- 新增 `MaterialAsset` / `MaterialInstance` 运行时抽象
- `MeshRendererComponent` 从“直接持有全部材质参数”逐步转向“引用材质实例 + 少量覆盖字段”
- 建立更清晰的 shader variant 与 pass 组织关系
- 让 scene save/load、runtime 使用同一套材质序列化语义；editor inspector 作为后续增强项再接入

阶段进展（已落地）：

- [`engine/assets/asset_manager.h`](engine/assets/asset_manager.h:140) 已扩展 [`MaterialAsset`](engine/assets/asset_manager.h:140) 的 `base_color`、`emissive_color`、`TextureSlots`、`ScalarOverrides` 与 `blend_mode`，形成第一批 PBR 参数承载层
- [`engine/assets/asset_manager.cpp`](engine/assets/asset_manager.cpp:143) 已为 mesh / pbr 命名材质实例补默认 `MESH_PBR` + `Opaque` 初始化策略
- [`engine/ecs/components_3d.h`](engine/ecs/components_3d.h:12) 已为 [`MeshRendererComponent`](engine/ecs/components_3d.h:12) 增加 `albedo/normal/metallic_roughness/emissive/occlusion` 纹理句柄字段，保留旧组件字段作为兼容回退层
- [`engine/scene/scene.cpp`](engine/scene/scene.cpp:938) 与 [`tests/engine/scene/scene_flow_test.cpp`](tests/engine/scene/scene_flow_test.cpp:129) 已完成 mesh 材质纹理句柄的 scene roundtrip 回归
- [`modules/gameplay_3d/rendering/mesh_render_system.cpp`](modules/gameplay_3d/rendering/mesh_render_system.cpp:573) 已改为优先通过 [`AssetManager::GetMaterialInstance()`](engine/assets/asset_manager.h:388) 解析材质实例，并把 shader variant / base color / emissive / texture slots / scalar overrides 注入 [`MeshDrawItem`](engine/render/rhi/rhi_device.h:38)
- [`modules/gameplay_3d/rendering/mesh_render_system.cpp`](modules/gameplay_3d/rendering/mesh_render_system.cpp:640) 已补 `normal_map_handle` 写入，打通法线贴图句柄到运行时 draw item 的链路
- [`tests/engine/assets/asset_manager_test.cpp`](tests/engine/assets/asset_manager_test.cpp:76)、[`tests/engine/mesh_material_desc_test.cpp`](tests/engine/mesh_material_desc_test.cpp:4)、[`tests/engine/mesh_render_material_desc_static_test.cpp`](tests/engine/mesh_render_material_desc_static_test.cpp:19)、[`tests/modules/gameplay_3d/rendering/mesh_renderer_test.cpp`](tests/modules/gameplay_3d/rendering/mesh_renderer_test.cpp:8) 已补资产存储、组件字段、静态实现约束与默认值回归

验收：

- 至少一条 `Mesh + MaterialInstance` 的最小场景回归
- 旧字段可迁移，不直接打断当前 MVP scene
- 文档中明确运行时材质实例与旧字段兼容边界；Inspector 可视化作为后续增强项

当前结论：运行时、scene 与 asset 三层已完成第一版材质实例/PBR 参数收口，当前仍以“材质实例优先 + 组件字段兼容回退”模式工作；Inspector 可视化与 demo 材质效果对齐继续后置。

对应参考：[`VSMaterial`](reference/VSEngine2.1/VSGraphic/VSMaterial.h:389)、[`VSMaterialInstance`](reference/VSEngine2.1/VSGraphic/VSMaterial.h:612)

### P0-2 3D 动画最小工作流升级

目标：从“有 Animator3DComponent”升级到“有最小动画图与骨骼资源闭环”。

建议项：

- 明确 `dskel` / `danim` / `anim graph` 三层边界
- 让 [`Animator3DComponent`](assets/scenes/3d_mvp_minimal.scene.json:27) 不再只停留在开关与路径，而能表达：
  - 默认状态
  - 参数表
  - 状态切换
  - 混合节点
- 先做最小 1D blend，再考虑更高阶树
- 资源导入链补“带骨骼动画模型”的稳定样例

阶段进展（已落地）：

- [`engine/ecs/components_3d.h`](engine/ecs/components_3d.h:123) 已为 `AnimBlendNode` 增加 `name` / `threshold`，并为 `Animator3DComponent` 增加 `blend_parameter` / `blend_parameter_value`
- [`engine/scene/scene.cpp`](engine/scene/scene.cpp:219) 已完成动画混合参数与 `blend_nodes` 的序列化 / 反序列化
- [`modules/gameplay_3d/animation/animator_system.cpp`](modules/gameplay_3d/animation/animator_system.cpp:300) 已将 legacy anim tree 从纯手工 `weight` 混合提升为基于 `threshold + blend_parameter_value` 的最小 1D blend
- [`tests/modules/gameplay_3d/animation/animator_system_test.cpp`](tests/modules/gameplay_3d/animation/animator_system_test.cpp:8) 与 [`tests/engine/scene/scene_flow_test.cpp`](tests/engine/scene/scene_flow_test.cpp:181) 已补参数默认值、节点阈值与 scene roundtrip 验证

验收：

- 一个带骨骼动画的最小 scene
- Animator3D 的 smoke / unit / scene roundtrip
- 文档中明确当前 anim graph 的表达边界；动画状态可视化与 editor graph 作为后续增强项

当前结论：引擎运行时与 scene 数据层已具备最小 anim-tree 表达能力，后续重点转向资源样例与编辑器可视化。

对应参考：[`VSAnimTree`](reference/VSEngine2.1/VSGraphic/VSAnimTree.h:12)、[`VSAnimSet`](reference/VSEngine2.1/VSGraphic/VSAnimSet.h:203)、[`VSSkeletonMeshNode`](reference/VSEngine2.1/VSGraphic/VSSkeletonMeshNode.h:10)

### P0-3 Spot Light 与 Sky Light 接入

目标：把当前灯光从“基础可用”升级为“常见 3D 场景够用”。

建议项：

- 新增 `SpotLightComponent`
- 新增 `SkyLightComponent` 或把环境光/天空光从当前 Directional/Skybox 逻辑中拆清
- 建立灯光统一参数结构：颜色、强度、阴影、范围、衰减、投射选项
- 优先统一 scene serialization / runtime render 路径，Inspector 入口后置

阶段进展（已落地）：

- [`engine/ecs/components_3d.h`](engine/ecs/components_3d.h:90) 已为 `PointLightComponent` / `SpotLightComponent` 增加 `falloff`，并新增 `SkyLightComponent`
- [`engine/scene/scene.cpp`](engine/scene/scene.cpp:206) 已完成 `PointLightComponent.falloff`、`SpotLightComponent`、`SkyLightComponent` 与 `Animator3DComponent.blend_parameter` / `blend_nodes` 的 scene save/load 接入
- [`modules/gameplay_3d/rendering/mesh_render_system.cpp`](modules/gameplay_3d/rendering/mesh_render_system.cpp:528) 已让点光 / 聚光半径受 `falloff` 影响，并将 `SkyLightComponent` 以最小环境光方式接入运行时渲染
- [`tests/engine/scene/scene_flow_test.cpp`](tests/engine/scene/scene_flow_test.cpp:117)、[`tests/modules/gameplay_3d/rendering/advanced_3d_components_test.cpp`](tests/modules/gameplay_3d/rendering/advanced_3d_components_test.cpp:8) 已补充 roundtrip/default/storage 回归

验收：

- scene 可保存 / 加载 `SpotLightComponent`
- runtime 能跑基础 spot lighting
- 文档中写清 `Spot/Sky Light` 的运行时边界与下一步 shadow 计划；Inspector 可调参与可视化后置

当前结论：引擎侧最小闭环已具备，编辑器 Inspector 接入仍后置。

对应参考：[`VSSpotLight`](reference/VSEngine2.1/VSGraphic/VSSpotLight.h:7)、[`VSSkyLight`](reference/VSEngine2.1/VSGraphic/VSSkyLight.h:7)

### P0-4 阴影路径统一清单化

目标：明确当前支持哪些 shadow path，哪些是下一步要补的，不再让阴影能力处于“代码里似乎有一些”的状态。

建议项：

- 固定当前主线阴影方案：`Directional + CSM`
- 规划下一步阴影能力顺序：
  1. Spot Shadow
  2. Point Light Shadow（优先 cubemap）
  3. 更高级阴影技术放后
- 为每一种阴影建立最小 scene + test
- 输出 draw call / shadow pass / frame time 的粗粒度 profiling 字段

阶段进展（已落地）：

- [`engine/ecs/components_3d.h`](engine/ecs/components_3d.h:82) 已明确当前三类灯光的阴影表达边界：`DirectionalLight3DComponent.cast_shadow + cascade_splits`、`PointLightComponent.cast_shadow + shadow_map_handle`、`SpotLightComponent.cast_shadow + shadow_map_handle`
- [`engine/scene/scene.cpp`](engine/scene/scene.cpp:191) 已完成 Directional / Point / Spot 灯光阴影相关字段的 scene save/load，当前可稳定保存 `cast_shadow`、`shadow_strength`、`cascade_splits`
- [`engine/render/rhi/rhi_device.cpp`](engine/render/rhi/rhi_device.cpp:1038) 已明确运行时当前实际生效的阴影主线仍为 `Directional + CSM`，mesh 提交阶段统一绑定 `u_shadow_maps` 与 `u_cascade_splits`
- [`engine/render/rhi/rhi_device.h`](engine/render/rhi/rhi_device.h:108) 已为 [`RenderStats`](engine/render/rhi/rhi_device.h:108) 增加 `mesh_count` 与 `shadow_passes`，[`engine/render/rhi/rhi_device.cpp`](engine/render/rhi/rhi_device.cpp:968) 已补运行时粗粒度阴影 pass 统计
- [`tests/modules/gameplay_3d/rendering/csm_test.cpp`](tests/modules/gameplay_3d/rendering/csm_test.cpp:8) 与 [`tests/engine/render/rhi_device_test.cpp`](tests/engine/render/rhi_device_test.cpp:14) 已覆盖 CSM 默认配置与新增渲染统计字段默认值

验收：

- `engine.3d.shadow.*` 基础门禁分组
- 文档写清“已支持 / 实验性 / 暂不纳入”

当前结论：当前稳定支持路径为 `Directional + CSM`；`SpotLightComponent.cast_shadow` 与 `PointLightComponent.cast_shadow` 已完成数据层和 scene 层占位，但运行时 shadow map 采样仍未真正接入，继续归类为下一批能力。

对应参考：[`VSShadowPass`](reference/VSEngine2.1/VSGraphic/VSShadowPass.h:82)、[`VSCubeShadowPass`](reference/VSEngine2.1/VSGraphic/VSShadowPass.h:6)

### P0-5 建立“参考 demo 重现”基线

目标：把“功能对齐”升级为“可见结果对齐”，至少选取 1~2 个 [`reference/VSEngine2.1`](reference/VSEngine2.1) 代表性 3D demo，在 `DSEngine` 用同类资源和接近的镜头/灯光/材质效果重现场景。

建议项：

- 先选 1 个静态展示类 demo + 1 个骨骼/材质参数类 demo 作为第一批目标
- 优先参考 [`reference/VSEngine2.1/Demo/15/15.8/Source.cpp`](reference/VSEngine2.1/Demo/15/15.8/Source.cpp:78) 与 [`reference/VSEngine2.1/Demo/15/15.9/Source.cpp`](reference/VSEngine2.1/Demo/15/15.9/Source.cpp:84) 这类场景：都包含 camera、sky light、directional light、skeleton mesh、ground plane、可交互观察
- 为每个目标 demo 建立“资源清单 / 场景搭建说明 / 差异说明 / 截图基线”四件套
- 在 `DSEngine` 中固定对应 scene，避免只在临时代码里拼装
- 明确哪些效果属于“视觉接近即可”，哪些属于“必须行为一致”

验收：

- `DSEngine` 中至少有 1 个签入的 reference-demo 对齐 scene
- 同类资源可稳定导入、保存、加载、运行
- 有截图或录屏对照基线，能用于回归检查
- 文档中能说明当前还原到什么程度、剩余差距是什么

阶段进展（已落地首个 scene 基线）：

- 已确认 [`reference/VSEngine2.1/Demo/15/15.8/Source.cpp`](reference/VSEngine2.1/Demo/15/15.8/Source.cpp:78) 与 [`reference/VSEngine2.1/Demo/15/15.9/Source.cpp`](reference/VSEngine2.1/Demo/15/15.9/Source.cpp:84) 的共同最小场景骨架均为：`Camera + 1st-person observe + Skeleton Mesh + Ground Plane + Sky Light + Directional Light`
- 其中 [`reference/VSEngine2.1/Demo/15/15.8/Source.cpp`](reference/VSEngine2.1/Demo/15/15.8/Source.cpp:82) 已被选为首个签入 scene 的视觉基线，因为它主要验证镜头、灯光、骨骼模型与地面组合，不依赖运行时交互材质参数
- [`reference/VSEngine2.1/Demo/15/15.9/Source.cpp`](reference/VSEngine2.1/Demo/15/15.9/Source.cpp:98) 仍作为第二阶段目标，用于验证 `skeleton mesh -> material instance -> runtime parameter override` 的连续链路；其关键行为是对两个材质实例持续写入 `SpecularPow`
- 当前 [`DSEngine`](.) 侧对齐 scene 已签入为 [`assets/scenes/reference_demo_15_8.scene.json`](assets/scenes/reference_demo_15_8.scene.json:1)，采用“角色占位 mesh + ground plane + directional light + sky light + skybox”的第一阶段近似构图
- 对齐实现已优先走“签入 scene 文件 + 运行时直接加载”路径；其 scene 骨架继续复用 [`engine/scene/scene.cpp`](engine/scene/scene.cpp:56) 现有 `material_schema_version/materials/entities` 序列化结构
- 对应最小验证已补到 [`tests/engine/scene/scene_flow_test.cpp`](tests/engine/scene/scene_flow_test.cpp:385)、[`tests/engine/scene/editor_scene_io_bridge_test.cpp`](tests/engine/scene/editor_scene_io_bridge_test.cpp:30) 与 [`tests/engine/cpp_runtime_startup_scene_test.cpp`](tests/engine/cpp_runtime_startup_scene_test.cpp:75)，用于覆盖 checked-in scene 反序列化、editor bridge roundtrip 与 startup smoke

建议的首批 demo 映射：

1. Demo 15.8 -> `DSEngine` 首个签入对齐 scene（优先落地）
   - Camera：使用 [`Camera3DComponent`](engine/ecs/components_3d.h:50) + [`FreeCameraControllerComponent`](engine/ecs/components_3d.h:188)，初始位姿尽量贴近 `VSVector3(0,100,-300)` 的观察关系，但按当前单位系折算为更适合 MVP 资源尺度的距离
   - Skeleton Mesh：先使用当前可稳定导入的骨骼模型资源，占位复现“主展示角色”
   - Ground Plane：优先用大尺度平面 mesh 或放大 cube 近似 [`NewOceanPlane.STMODEL`](reference/VSEngine2.1/Demo/15/15.8/Source.cpp:94)，并保持 `cast_shadow/receive_shadow` 策略与参考接近
   - Sky Light：使用 [`SkyLightComponent`](engine/ecs/components_3d.h:118)，颜色直接映射参考 demo 的 `up/down color`
   - Directional Light：使用 [`DirectionalLight3DComponent`](engine/ecs/components_3d.h:82)，保留当前稳定主线 `Directional + CSM`

2. Demo 15.9 -> 材质实例交互验证 scene（次级落地）
   - 复用 15.8 的场景构图，减少视觉变量
   - 额外要求：场景中的 skeleton mesh 至少绑定 2 个可寻址材质实例
   - 运行时需要最小参数覆盖接口，把“`SpecularPow` 可按键调节”映射到 `DSEngine` 当前材质实例/PBR 参数体系可表达的等价字段
   - 如果当前 shader 语义中尚无逐帧 `SpecularPow` 专用入口，则先以“roughness / metallic / emissive / normal_strength 中一项可交互改变并能稳定反映到画面”作为第一阶段替代基线

四件套落地模板：

- 资源清单：列出 skeleton mesh、ground mesh、可能需要的贴图与材质实例 ID
- 场景搭建说明：记录 camera 初始位姿、光照颜色/方向、地面缩放、阴影开关
- 差异说明：明确哪些地方是“资源不同但构图等价”，哪些是“当前引擎能力暂以近似效果替代”
- 截图基线：至少固定 1 张正视角 + 1 张斜视角，后续用于 smoke / 人工回归对照

当前结论：P0-5 已从“仅有首个 checked-in reference-demo scene 基线”推进到“15.8 静态构图 + 15.9 占位材质交互入口”并行阶段；[`assets/scenes/reference_demo_15_9.scene.json`](assets/scenes/reference_demo_15_9.scene.json:1) 已签入两份可寻址材质实例占位，[`samples/cpp/phase1_demo_logic.cpp`](samples/cpp/phase1_demo_logic.cpp:15) 已在 startup-scene 模式下为其创建运行时材质实例，并将参考 demo 中的 `SpecularPow0/1` 按键交互近似映射到 `roughness + emissive` 变化。下一步应继续补齐 `reference_demo_15_9` 的 smoke / 场景回归 / 截图基线收口，并把 [`assets/scenes/reference_demo_15_8.scene.json`](assets/scenes/reference_demo_15_8.scene.json:1) 与 [`assets/scenes/reference_demo_15_9.scene.json`](assets/scenes/reference_demo_15_9.scene.json:1) 从占位资源版本升级为更接近参考 demo 的真实骨骼资源；editor bridge 继续保留，但不再作为本轮 reference 对齐阶段的主优先级。

---

## 4.2 P1：先把 reference demo 对齐与资源导入链做实

### P1-1 资源导入链扩充到 `FBX + glTF` 双入口

目标：保留现代化 `glTF/glb` 主路径，同时补齐老项目常见 `FBX` 输入兼容。

建议项：

- 在 [`apps/tools/asset_builder/main.cpp`](apps/tools/asset_builder/main.cpp:8) 现有链路上扩充 `FBX` 输入
- 输出统一仍建议落到 `dmesh / dmat / danim / dskel`
- 明确资源导入失败日志、依赖缺失日志、路径规范
- 增加一组“签入资源 + 构建期或测试期导入验证”

验收：

- 至少 1 个 glTF 样例、1 个 FBX 样例稳定导入
- 动画 / skeleton / material 输出文件完整
- 文档说明推荐格式与兼容格式

### P1-2 reference demo 结果固化与回归收口

目标：把当前已签入的 `reference_demo_*` 从“可运行占位 scene”升级为“可稳定回归的 reference 对齐样例”。

建议项：

- 为 [`assets/scenes/reference_demo_15_8.scene.json`](assets/scenes/reference_demo_15_8.scene.json:1) 与 [`assets/scenes/reference_demo_15_9.scene.json`](assets/scenes/reference_demo_15_9.scene.json:1) 固化截图基线、参数记录与资源映射
- 优先替换当前占位 skeleton mesh，提升与 [`reference/VSEngine2.1/Demo/15/15.8/Source.cpp`](reference/VSEngine2.1/Demo/15/15.8/Source.cpp:78) / [`reference/VSEngine2.1/Demo/15/15.9/Source.cpp`](reference/VSEngine2.1/Demo/15/15.9/Source.cpp:84) 的视觉接近度
- 把 `15.9` 中的材质交互行为继续收敛为稳定 smoke / regression
- 文档中明确“视觉近似项”和“行为必须一致项”的边界

验收：

- 至少 2 个 reference demo 对齐 scene 具备可复现截图基线与参数记录
- scene load / startup smoke / 关键资源存在性检查可以稳定回归
- 文档能解释当前还原程度、资源差异与剩余能力缺口

### P1-3 Morph Target 最小链路

目标：补齐面部、形变类动画所需的基础能力。

建议项：

- 定义 Morph 资源格式或在现有资产格式中扩展
- Runtime 增加 morph weight 驱动
- 材质 / shader 路径支持 morph data
- Inspector 能看到 morph channel 与权重

验收：

- 一个最小 morph 样例场景
- scene save/load 不丢 morph 配置
- 单元测试能覆盖权重变更与边界情况

对应参考：[`VSMorphSet`](reference/VSEngine2.1/VSGraphic/VSMorphSet.h:76)、[`VSDx11ShaderConstant.cpp`](reference/VSEngine2.1/VSDx11Renderer/VSDx11ShaderConstant.cpp:65)

## 4.3 P2：补 `DSEngine` 自身 editor 工作流与 preview 能力

这一阶段的重点不再是直接对齐 [`reference/VSEngine2.1`](reference/VSEngine2.1) 的 runtime 功能，而是补齐 `DSEngine` 自身的 3D 制作流、预览流与后处理体验。只有在 P0/P1 的 runtime 与 demo 收口完成后，才适合进入。

### P2-1 3D Scene View / Gizmo / Camera 操作增强

目标：让编辑器从“能检视 3D 组件”升级到“能基本编辑 3D 场景”。

建议项：

- 统一 3D Scene View 摄像机导航
- 完善位移 / 旋转 / 缩放 Gizmo 行为
- 增加对齐、局部/世界坐标切换、吸附等高频能力
- 增加基础 3D 选中反馈与包围盒辅助

验收：

- 可在 editor 中完成最小 3D 场景布局编辑
- Play / Edit 不污染 3D 组件状态
- 有 editor smoke 覆盖关键交互

### P2-2 3D Post Process 基础矩阵

目标：把当前 `PostProcessComponent` 从零散参数提升为有限可控的后处理集合。

建议项：

- 先固定 2~3 个高价值后处理：Bloom、Tone Mapping、Color Adjust
- 明确每个效果的开关、参数、执行顺序
- 在 render pipeline 中收口 pass 组织
- Editor inspector 与 runtime 输出一致

验收：

- 可在最小 scene 中稳定启用/禁用
- 有最小性能观测字段
- 有配置 roundtrip 测试

对应参考：[`VSPostEffectSet`](reference/VSEngine2.1/VSGraphic/VSPostEffectSet.h:15)、[`VSPostEffectPass`](reference/VSEngine2.1/VSGraphic/VSPostEffectPass.h:6)

## 4.4 P3：扩展到真正“功能面接近 `VSEngine2.1`”

这一阶段不应过早开始，只有在 P0/P1/P2 收口完成后才适合进入。

### P3-1 Terrain LOD 生态

建议顺序：

1. 当前 TerrainComponent 参数清理与性能基线
2. Chunk / patch 化管理
3. CPU LOD 或距离驱动简化
4. 再考虑 GPU LOD / ROAM 类复杂方案

对应参考：[`VSCLodTerrainNode`](reference/VSEngine2.1/VSGraphic/VSCLodTerrainNode.h:6)、[`VSGPULodTerrainNode`](reference/VSEngine2.1/VSGraphic/VSGPULodTerrainNode.h:6)

### P3-2 Point Light Shadow / 高级阴影方案

建议顺序：

1. Point Light Cubemap Shadow
2. Spot Shadow 稳定化
3. 再评估 Volume / Dual Paraboloid 是否还有投入价值

对应参考：[`VSCubeShadowPass`](reference/VSEngine2.1/VSGraphic/VSShadowPass.h:6)、[`VSDualParaboloidShadowPass`](reference/VSEngine2.1/VSGraphic/VSShadowPass.h:116)

### P3-3 动画图增强

建议项：

- 2D 参数驱动 blend
- partial body / additive blend
- 动画事件
- 更可视化的 editor graph

对应参考：[`VSPartialAnimBlend`](reference/VSEngine2.1/VSGraphic/VSPartialAnimBlend.h:12)、[`VSAdditiveBlend`](reference/VSEngine2.1/VSGraphic/VSAdditiveBlend.h:6)、[`VSRectAnimBlend`](reference/VSEngine2.1/VSGraphic/VSRectAnimBlend.h:31)

### P3-4 3D 粒子与效果生态

建议项：

- 补 billboard / mesh particle 二分
- 补基础 emitter shape、lifetime、velocity over time、color over time
- 把 3D 粒子放进材质/后处理/阴影边界中统一看待

---

## 5. 结构化执行清单

以下清单用于后续逐项落地。

### 5.1 必做项（优先级最高，先做 runtime foundation）

- [ ] 建立 `3D Alignment` 文档与路线口径统一
- [ ] 收口材质实例系统，减少 `MeshRendererComponent` 直接材质字段散落
- [ ] 升级 `Animator3DComponent` 为最小动画图工作流
- [ ] 接入 `SpotLightComponent`
- [ ] 接入 `SkyLightComponent` / 环境光统一结构
- [ ] 明确阴影主线与非主线能力分层
- [ ] 为新增 3D 能力同步补 scene / smoke / regression / 文档边界

### 5.2 应做项（P0 完成后，先做 demo 与资源链）

- [ ] 扩展资源导入链到 `FBX + glTF`
- [ ] 固化 `reference demo -> DSE scene` 的截图、参数、资源映射与 smoke/regression 口径
- [ ] 增加 Morph Target 最小链路
- [x] 给 3D profiling 增加 draw call / shadow pass / material switch / frame timing 观测（首版运行时统计）
- [x] 建立 `reference demo -> DSE scene` 的截图、参数、资源映射表（文档级首版）

### 5.3 后续工作流增强项（demo 收口后进入）

- [ ] 强化 Scene View 与 3D Gizmo 编辑体验
- [ ] 固定一组最小后处理矩阵

当前已固定的首版映射建议：

| Reference demo | `DSEngine` scene | 资源映射 | 参数映射 | 当前截图基线建议 |
| --- | --- | --- | --- | --- |
| `15.8` | [`assets/scenes/reference_demo_15_8.scene.json`](assets/scenes/reference_demo_15_8.scene.json:1) | `Skeleton Mesh -> assets/meshes/reference_demo_character_placeholder.fbx`（占位）、`Ground -> models/cube.dmesh`、`Sky -> assets/skyboxes/default_sky`（占位） | `DirectionalLight/SkyLight` 直接映射到 [`DirectionalLight3DComponent`](engine/ecs/components_3d.h:82) 与 [`SkyLightComponent`](engine/ecs/components_3d.h:118) | 固定 1 张正视角、1 张斜视角，机位取 scene 中 [`Camera3DComponent`](engine/ecs/components_3d.h:50) 初始位姿 |
| `15.9` | [`assets/scenes/reference_demo_15_9.scene.json`](assets/scenes/reference_demo_15_9.scene.json:1) | `Skeleton Mesh -> assets/meshes/reference_demo_character_placeholder.fbx`（占位）、两个可寻址材质实例槽位 `430001/430002`、`Ground -> models/cube.dmesh` | `SpecularPow0/1 -> roughness + emissive`，运行时更新实现位于 [`samples/cpp/phase1_demo_logic.cpp`](samples/cpp/phase1_demo_logic.cpp:94) | 固定 1 张初始状态图，再分别记录 `spec0/spec1` 调整后的高/低两组参数截图，优先局部角色区域 |

### 5.4 延后项（P2/P3 后评估）

- [ ] Terrain 多级 LOD 生态
- [ ] Point Light Cubemap Shadow
- [ ] 更完整动画图编辑器
- [ ] 更系统 3D 粒子生态
- [ ] 更复杂大场景 streaming / async upload

---

## 6. reference demo 对齐交付要求

为了避免“文档里说对齐，实际没有可看结果”，后续每一轮 3D 对齐都建议附带以下交付物：

1. 一个参考目标：明确对应哪个 [`reference/VSEngine2.1/Demo`](reference/VSEngine2.1/Demo)
2. 一份资源映射：旧 demo 使用了哪些 mesh / skeleton / material / texture，`DSEngine` 里分别如何导入与落盘
3. 一个签入 scene：能直接在 `DSEngine` 启动或 editor 打开
4. 一组视觉基线：至少包含同机位截图、灯光参数、材质参数记录；遵循 [`doc/DOC-11_AI_DRIVEN_TEST_STRATEGY.md`](doc/DOC-11_AI_DRIVEN_TEST_STRATEGY.md:268) 的“少量基线图、固定分辨率、固定相机、优先局部区域比对”原则
5. 一条回归：验证 scene load、关键组件存在、关键资源加载成功

建议把这类场景单独归档为 `reference demo reproduction` 子集合，而不是混入普通 smoke scene。

---

## 7. 推荐里程碑

### M1：3D Foundation

完成标志：

- 材质实例系统落地第一版
- Spot / Sky light 可用
- Animator3D 最小图可运行
- 至少 1 个 reference demo 对齐 scene 可运行
- 新增能力具备 scene + test + 文档边界闭环

### M2：Reference Demo Reproduction

完成标志：

- glTF / FBX 双入口可导入
- 至少 2 个 reference demo 对齐结果有截图基线与参数记录
- `reference_demo_*` 的 startup smoke / scene regression / 资源映射口径稳定
- 后处理与 profiling 有最小统一口径
- 3D `engine.3d.*` 门禁成组稳定

### M3：3D Preview Workflow

完成标志：

- Editor 可完成最小 3D 制作流
- Scene View / Gizmo / Camera 操作具备最小可用闭环
- Play / Edit 隔离不污染 3D 组件状态
- editor smoke 能覆盖关键交互

### M4：3D Feature Expansion

完成标志：

- Terrain / shadow / animation / particle 进入第二层扩展
- 3D 能力广度开始逼近 [`reference/VSEngine2.1`](reference/VSEngine2.1)
- 但仍保持当前仓库的模块化与可验证边界

---

## 8. 不建议直接照搬的部分

为了避免把旧引擎包袱复制进来，以下内容不建议直接照搬：

- 过重的类层级与历史命名体系
- DX9/DX11 时代耦合严重的 renderer / material 接口形式
- 大量以继承层次扩张功能的做法
- 在未完成 MVP/Preview 闭环前提前铺满所有 terrain / shadow / animation 变体

建议保留的是：

- 功能分层思路
- 资源链路深度
- 动画 / 灯光 / 阴影 / 地形这些核心 3D 子系统的完整性意识

---

## 9. 最终结论

如果要让 `DSEngine` 的 3D 向 [`reference/VSEngine2.1`](reference/VSEngine2.1) 看齐，最合理的方式不是“全面补功能”，而是分四步走：

1. **先补 runtime 骨架**：材质实例、动画图、Spot/Sky Light、阴影分层
2. **再补 demo 重现基线**：挑选代表性 reference demo，用同类资源在 `DSEngine` 跑出可对照结果
3. **再补 `DSEngine` 自身工作流**：资源导入、Scene View、后处理、profiling、editor 闭环
4. **最后补生态深度**：Terrain LOD、高级阴影、Morph、复杂动画图、粒子生态

一句话总结：

> `DSEngine` 3D 对齐 [`reference/VSEngine2.1`](reference/VSEngine2.1) 的核心，不只是把功能点补齐，还要把 reference demo 用同类资源真实重现出来；因此主线应升级为“材质、动画、光照阴影、资源导入、编辑器工作流、demo 重现”六条主链共同收口。