# DOC-20 Lua Demo 复现与资源迁移方案

## 1. 文档目标

本文档用于明确 `DSEngine` 后续如何以 **Lua + 可执行程序** 的方式复现 `VSEngine` 中有价值的 demo，并同步回答以下几个当前必须落地的问题：

1. 为什么后续 demo 复现应以 `DSEngine` 的 Lua 运行时为主承载层。
2. 哪些 `reference/VSEngine2.1/Demo` 项目应优先复现，复现顺序如何安排。
3. 代码、资源、配置、可执行程序入口应如何组织。
4. `DSEngine` 为什么必须补齐 `FBX` 支持，以及建议分阶段如何落地。
5. `reference/VSEngine2.1` 后续计划移除时，哪些资源需要迁出并纳入主仓自有资产结构。

本文档的目标不是继续停留在“scene 占位”或“测试占位”，而是把 3D demo 工作正式收敛到 **可运行、可观察、可交付** 的产品形态上。

---

## 2. 当前判断：后续 demo 复现应切到 Lua executable 主线

### 2.1 为什么不能继续只做 scene / test 占位

当前仓库虽然已经存在：

- `assets/scenes/3d_mvp_minimal.scene.json`
- `assets/scenes/reference_demo_15_8.scene.json`
- `assets/scenes/reference_demo_15_9.scene.json`
- `engine.3d.scene_mvp`
- `engine.3d.runtime_mvp_smoke`

但这些内容目前主要证明的是：

- scene 文件可被加载；
- runtime 可以切到指定 startup scene；
- 某些材质/启动分支逻辑存在；
- 基础 3D 组件序列化链路没有断。

**这些内容不能等价证明 demo 已经具备真实视觉效果。**

当前的 `cpp_runtime_startup_scene_test.cpp` 甚至明确在断言资源缺失日志，例如：

- `mvp_resource_missing type=mesh path=assets/meshes/mvp_cube.fbx`
- `mvp_resource_missing type=mesh path=assets/meshes/reference_demo_character_placeholder.fbx`
- `mvp_resource_missing type=skybox path=assets/skyboxes/default_sky`

这说明现阶段的 `reference_demo_15_8` / `reference_demo_15_9` 更接近 **对齐占位基线**，而不是已经可展示的 demo 成品。

### 2.2 为什么 Lua 是当前最合理的 demo 承载层

当前仓库已经具备完整的 Lua 可执行入口链：

- [`apps/runtime/lua_host/main.cpp`](../apps/runtime/lua_host/main.cpp)
- [`engine/scripting/lua/lua_runtime.cpp`](../engine/scripting/lua/lua_runtime.cpp)
- [`samples/lua/main.lua`](../samples/lua/main.lua)
- [`samples/lua/config.lua`](../samples/lua/config.lua)

当前运行模式已经满足：

- 由宿主可执行程序进入 `BusinessMode::Lua`；
- 默认 startup script 指向 `samples/lua/main.lua`；
- 支持通过 `DSE_STARTUP_LUA` 或显式 override 切换启动 Lua；
- `main.lua` 已按 `Config.game_entry` 分发不同 demo。

因此，后续如果要把 VSEngine demo 真正落成 **可运行程序**，最合理的方案不是继续加新的 C++ demo 宿主，而是：

1. 复用现有 Lua host 可执行入口；
2. 在 Lua 层组织具体 demo 逻辑；
3. 等 demo 成熟后，再按需拆分独立可执行程序或 launcher。

**结论：后续有价值 demo 的主线交付物，应定义为“Lua 复现 demo + 对应可执行程序入口 + 最小回归验证”。**

---

## 3. demo 复现目标定义

后续每个参考 demo 的目标不应是“机械翻译 VSEngine C++ 代码”，而应是：

### 3.1 必须对齐的内容

- **场景构图**：相机位置、朝向、FOV、角色/地面/灯光布局；
- **核心表现**：材质、灯光、阴影、动画、地面、天空盒等关键视觉点；
- **交互逻辑**：自由相机、鼠标旋转、键盘参数调节、最小 UI/HUD；
- **资源落地**：使用主仓自身资源链可加载的资产；
- **可执行交付**：通过 DSEngine 可执行程序真实运行，而不是只存在 scene/json；
- **最小回归**：至少有一条 smoke 或 regression 保证 demo 不会静默跑不起来。

### 3.2 不强求逐字照搬的内容

- VSEngine 的旧工程结构；
- VSEngine 的类型命名与类层次；
- VSEngine 专有资源扩展名与旧 material graph 组织方式；
- 不再适合当前 DSEngine 架构的旧脚本/旧宿主逻辑。

**原则：复现的是 demo 的“场景与表现目标”，不是 VSEngine 的历史实现细节。**

---

## 4. demo 选择与落地顺序

### 4.1 第一阶段：立即进入 Lua 可执行复现

建议优先做以下 demo：

1. `15.8`
2. `15.9`
3. `15.7`

原因：

- 这组 demo 直接对应当前 3D 主线的价值验证；
- 涉及骨骼模型、地面、方向光、天光、自由相机、材质差异；
- 非常适合用 Lua 做“可运行 demo”；
- 与当前 `DOC-19` 中已经存在的 `reference_demo_15_8` / `reference_demo_15_9` 试点基线天然衔接。

### 4.2 第二阶段：扩展到更复杂参考项

建议第二批落地：

- `14.8 / 14.9`
- `16.7 / 16.8`

原因：

- 对渲染表现和资源链的要求更高；
- 适合作为第一批 demo 稳定后的扩展；
- 更适合作为 P3 的中段任务，而不是起步项。

### 4.3 第三阶段：高阶观察池

暂列观察池：

- `18.x`
- `19.x`
- `21.x`

这些内容可在以下条件都满足后再纳入：

- `FBX` 资源链稳定；
- 第一批 Lua demo 已具备真实画面与可执行程序；
- 资源迁移已完成，主仓不再依赖 `reference/VSEngine2.1`。

---

## 5. Lua demo 代码组织方案

### 5.1 目录建议

建议在 `samples/lua/` 下新增独立专区：

- `samples/lua/vse_demo/common/`
- `samples/lua/vse_demo/demo15_7.lua`
- `samples/lua/vse_demo/demo15_8.lua`
- `samples/lua/vse_demo/demo15_9.lua`

并保留：

- `samples/lua/main.lua`
- `samples/lua/config.lua`

### 5.2 推荐职责拆分

#### `samples/lua/main.lua`

只负责：

- 读取 `Config.game_entry`；
- 路由到对应 demo 模块；
- 统一执行 `Setup/Update`。

#### `samples/lua/vse_demo/common/`

负责以下共用逻辑：

- demo 公共资源路径表；
- 共享相机控制逻辑；
- 输入映射辅助；
- 公共 UI/HUD 显示；
- 资源加载失败时的诊断输出；
- 公共场景搭建函数（地面、天光、方向光、天空盒、相机等）。

#### `samples/lua/vse_demo/demo15_8.lua`

负责：

- 角色 + 地面 + 天光 + 主方向光 + 自由相机；
- 对齐 `15.8` 的场景构图和表现基线；
- 保证运行后“真的能看到场景”。

#### `samples/lua/vse_demo/demo15_9.lua`

负责：

- 在 `15.8` 基础上引入材质参数交互；
- 通过键盘或 UI 调节材质参数；
- 对齐 `15.9` 的材质演示目标。

### 5.3 `game_entry` 建议命名

建议新增：

- `vse_demo_15_7`
- `vse_demo_15_8`
- `vse_demo_15_9`

通过 `samples/lua/config.lua` 或后续 launcher 配置切换。

---

## 6. 可执行程序交付方案

### 6.1 开发阶段

优先复用现有 Lua host：

- 同一个宿主 exe；
- 通过 `game_entry` 切换不同 demo；
- 快速迭代 Lua 与资源；
- 减少无意义的宿主层重复开发。

### 6.2 对外/对内演示阶段

等 demo 成熟后，再按需拆成两种交付方式：

#### 方式 A：同一 exe + 不同配置

适用于：

- 内部验证；
- 快速多 demo 切换；
- 研发联调。

#### 方式 B：独立 demo executable

例如：

- `DSEngine_Demo15_8.exe`
- `DSEngine_Demo15_9.exe`

适用于：

- 单 demo 定向展示；
- 对外投放；
- 降低使用者配置负担。

### 6.3 当前建议

**先用方式 A 开发，等第一批 demo 真正稳定后再决定是否拆分独立 exe。**

因为当前主要瓶颈不是宿主数不够，而是：

- 真实资源未落地；
- `FBX` 资源链未打通；
- 画面结果未形成闭环；
- `reference` 依赖尚未迁出。

---

## 7. DSEngine 必须补齐 FBX 支持

### 7.1 当前现状

当前主仓 importer 只有：

- [`GltfImporter`](../engine/assets/compiler/importer.h)

并且 `AssetBuilder` 也已经明确拒绝直接处理 `FBX`：

- [`apps/tools/asset_builder/main.cpp`](../apps/tools/asset_builder/main.cpp)

当前提示语义为：

- `FBX is not supported as direct input. Please convert the source asset to .gltf/.glb before cooking.`

这说明目前资源链的真实状态是：

- **支持**：`glTF / GLB -> dmesh / dmat / danim / dskel`
- **不支持**：`FBX -> DSE 运行时资产`

### 7.2 为什么必须支持 FBX

后续如果 `reference/VSEngine2.1` 会被移除，且要把有价值 demo 真正迁入主仓，那么单靠当前 `glTF/GLB` 路线不够，原因包括：

1. `reference/VSEngine2.1/FBXResource` 本身就保存了核心参考资产；
2. demo 的高价值角色、地面、动画、LOD 等很多都首先来自 `FBX`；
3. 如果要求每次都先手工/仓外转换，再导入主仓，会导致：
   - 资源链不可重复；
   - 迁移成本高；
   - 资源真理源不清晰；
   - 后续维护困难。

**因此：DSEngine 后续必须支持 `FBX`，至少要支持“可控范围内的最小 FBX 主链”。**

### 7.3 FBX 支持的建议策略

不建议一上来追求“全量 FBX 特性兼容”，而应分三阶段：

#### P1：先支持离线导入主链

目标：

- 新增 `FbxImporter`；
- 最小支持：
  - 静态网格；
  - 单 skin 骨骼网格；
  - 基础材质槽位；
  - 动画 clip（TRS 主路径）；
  - 贴图引用；
- 产物仍统一落到：
  - `.dmesh`
  - `.dmat`
  - `.danim`
  - `.dskel`

原则：

- `FBX` 只作为 **离线导入格式**；
- runtime 不直接依赖 `FBX SDK`；
- 最终运行时资产仍然是 DSE 自有 cooked 格式。

#### P2：统一 importer 抽象层

建议把 importer 抽象成统一入口，例如：

- `IAssetImporter`
- `GltfImporter`
- `FbxImporter`

统一输出：

- `RawSceneData`

这样后续：

- `glTF/GLB`
- `FBX`
- 甚至其他格式

都能走同一套 `MeshCooker` / `MaterialCooker` / `AnimationCooker` / `SkeletonCooker`。

#### P3：补齐 reference demo 需要的关键边界

例如逐步补齐：

- 多 submesh；
- 多材质槽；
- 基础 LOD；
- 骨骼动画 clip；
- 贴图路径映射；
- 资源命名统一规则；
- demo 迁移所需的基础材质参数转换。

### 7.4 FBX 支持的边界要求

建议正式写清以下原则：

- **支持的是离线导入，不是 runtime 直接读 FBX**；
- **不要求一步到位兼容所有 DCC 输出差异**；
- **先保证 VSEngine demo 迁移所需资源可导入**；
- **以最小可复现 demo 为第一验收目标，而不是以格式大全为目标。**

---

## 8. 引擎资源分类重整方案

### 8.1 当前问题

目前仓库里与 3D 相关的资源路径语义并不统一，既存在：

- `assets/scenes/...`
- `assets/meshes/...`（当前多数仍是占位）
- `assets/skyboxes/...`（当前引用多为占位）
- `assets/terrain/...`（当前引用多为占位）
- `data/`（现有 2D / 样例数据）
- `models/...`（样例 C++ 路径中也存在）

这会导致：

- scene 路径风格不统一；
- runtime 资源根目录认知混乱；
- 主仓与 reference 仓资源边界不清；
- 后续迁移 `reference/VSEngine2.1` 时难以一次性收口。

### 8.2 建议的统一分类

建议把主仓 3D 资源整理成以下结构：

- `assets/scenes/`
- `assets/demo/`
  - `15_7/`
  - `15_8/`
  - `15_9/`
- `assets/meshes/`
- `assets/materials/`
- `assets/textures/`
- `assets/animations/`
- `assets/skeletons/`
- `assets/skyboxes/`
- `assets/terrain/`
- `assets/source/`
  - `fbx/`
  - `gltf/`
  - `reference_demo/`

### 8.3 目录职责建议

#### `assets/source/`

保存**导入前源资源**：

- 原始 `FBX`；
- 中间 `glTF/GLB`；
- 临时迁入的参考源资源；
- 用于 asset builder / importer 的输入。

该目录不应直接被 runtime 当作最终资产目录。

#### `assets/demo/`

保存**按 demo 组织的最终演示资源集合**，例如：

- `assets/demo/15_8/scene/`
- `assets/demo/15_8/meshes/`
- `assets/demo/15_8/materials/`
- `assets/demo/15_8/textures/`

该层适合：

- demo 自包含；
- 后续独立可执行程序打包；
- 资源迁移时按 demo 收口。

#### 全局分类目录

例如：

- `assets/materials/`
- `assets/textures/`
- `assets/animations/`
- `assets/skeletons/`

适合：

- 通用资源复用；
- 跨 demo 共享；
- 非 demo 专属资产管理。

### 8.4 当前建议

**短期以“按 demo 自包含”为主，长期再收敛全局共享资源。**

即：

- 第一批先优先保证 `15_8` / `15_9` 各自资源自包含；
- 等复现完成后，再把公共材质/天空盒/地面贴图抽成共享目录。

这样迁移风险最低，也最容易回溯来源。

---

## 9. `reference/VSEngine2.1` 去依赖与资源拷贝方案

### 9.1 当前前提

后续既然计划移除 `reference/VSEngine2.1`，那么当前所有仍依赖 reference 的 demo 资源都必须迁出主仓自持。

当前最关键的源资源入口是：

- `reference/VSEngine2.1/FBXResource/`

当前可见资源包括：

- `Monster.FBX`
- `OceanPlane.FBX`
- `Walk.FBX`
- `Attack.FBX`
- `Attack2.FBX`
- `RootMotion.FBX`
- `MonsterLOD0~4.FBX`
- 以及若干桌椅、立柱、球体、环体等静态模型。

### 9.2 迁移原则

后续迁移建议遵循：

1. **先做 demo 直接依赖项**，不要一开始全量拷贝整个 `FBXResource`；
2. **保留原始文件名与来源记录**，便于回溯；
3. **源资源与 cooked 产物分层保存**；
4. **每迁一个资源，都要同步更新资源映射文档**；
5. **主仓运行路径不得再直接指向 `reference/...`**。

### 9.3 第一批建议迁出资源

建议第一批只迁：

- `Monster.FBX`
- `OceanPlane.FBX`
- `MonsterLOD0.FBX`（如果 `15.9` 最终需要）
- 与 `15.8 / 15.9` 直接相关的贴图资源
- 与 `15.8 / 15.9` 直接相关的天空盒资源

### 9.4 推荐主仓落点

建议：

- `assets/source/reference_demo/shared/monster/`
- `assets/source/reference_demo/shared/ocean_plane/`
- `assets/source/reference_demo/shared/skybox/`

后续再通过 importer / cooker 输出到：

- `assets/demo/15_8/...`
- `assets/demo/15_9/...`

或者输出到统一 cooked 目录。

---

## 10. 回归验证建议

### 10.1 每个 demo 至少需要三层验证

#### A. 资源存在性回归

例如：

- scene 中引用的 mesh/material/texture/skybox 路径必须存在；
- 启动时不允许再出现 `resource_missing` 日志。

#### B. startup scene / startup lua smoke

例如：

- 指定 `DSE_STARTUP_SCENE=...reference_demo_15_8.scene.json`
- 或指定 `DSE_STARTUP_LUA=...demo15_8.lua`

要求：

- 程序能启动；
- 至少跑若干帧；
- 不崩溃；
- 输出关键 bootstrap 成功标志。

#### C. 最小交互 / 画面结果验证

后续建议逐步增加：

- 屏幕截图 hash/阈值比较；
- 关键运行日志；
- 交互状态切换断言。

### 10.2 当前最值得先补的回归

优先建议：

1. `reference_demo_15_8.scene.json` startup scene smoke；
2. `reference_demo_15_9.scene.json` startup scene smoke；
3. `demo15_8.lua` Lua host smoke；
4. `demo15_9.lua` Lua host smoke。

这样才能把“scene 存在”升级为“demo 真能运行”。

---

## 11. 15.8 / 15.9 天空盒与最终视觉闭环补充（2026-04-14）

### 11.1 当前状态

- `SkyboxComponent` 已从“仅序列化占位”推进到**最小可运行链路**：
  - `Scene` / Editor / Lua 绑定继续承载组件数据；
  - `FramePipeline` 在 `scene_pass` 中会按 `cubemap_path` 懒加载 cubemap；
  - 成功加载后把运行时 `cubemap_handle` 回填到组件，并提交 `DrawSkybox`；
  - OpenGL RHI 已补齐最小 `samplerCube` 绘制路径。
- `15.8 / 15.9 / 3d_mvp_minimal` 已统一接入主仓 skybox 目录资源，不再使用 `assets/skyboxes/default_sky` 占位路径。
- 当前 skybox 只负责**背景绘制**；`SkyLightComponent` 仍然负责简化的半球环境光近似。

### 11.2 资源格式约定

**结论：`cubemap_path` 指向目录，而不是清单文件或单文件。**

目录内固定六面命名：

- `px`
- `nx`
- `py`
- `ny`
- `pz`
- `nz`

当前允许的图片扩展名：

- `.png`
- `.jpg`
- `.jpeg`
- `.bmp`
- `.tga`
- `.ppm`

### 11.3 选择目录方案的理由

- 主仓当前已有 `stb_image` 的 **2D 图片解码能力**，可直接复用；
- 当前仓库内**没有现成可复用**的 `.hdr/.dds/.ktx` 天空盒解析链；
- 若采用单文件 equirectangular / `.hdr` 方案，需要额外补球面到 cubemap 转换、HDR 解码与更多渲染基础设施，不符合本轮“最小可落地”目标；
- 若采用清单文件方案，虽然扩展性更好，但当前六面固定命名已经足够满足 `15.8 / 15.9` 的最小闭环，额外清单只会增加一层解析复杂度。

因此，本轮资源约定以**目录 + 固定六面文件名**为准。

### 11.4 当前最小实现边界

当前最小 skybox/cubemap 链路只覆盖：

- 目录式六面纹理加载；
- OpenGL `GL_TEXTURE_CUBE_MAP` 上传；
- `samplerCube` 背景绘制；
- 运行时句柄懒加载与缓存复用。

**明确未覆盖：**

- HDR 源图导入；
- `.dds/.ktx` 直接加载；
- IBL `irradiance / prefilter / BRDF LUT`；
- skybox 驱动的真实 PBR 反射；
- cooked skybox 资产编译链；
- Editor 的专用 skybox 资源选择器。

### 11.5 验证口径

本轮 skybox 最小验证至少包含：

- `AssetManager` 目录式 cubemap 成功加载；
- 缓存复用有效；
- 缺少任一面时返回失败；
- `reference_demo_15_8.scene.json` / `reference_demo_15_9.scene.json` 已切到主仓 skybox 路径。

### 11.7 当前进度结论（2026-04-14）

- `15.8 / 15.9` 的 `SkyboxComponent` 已完成最小可运行闭环：`cubemap_path` 采用目录式约定，运行时按目录懒加载六面 cubemap，并通过现有 `DrawSkybox` 提交背景绘制。
- `3d_mvp_minimal`、`reference_demo_15_8`、`reference_demo_15_9` 已全部切到 `assets/source/reference_demo/shared/skybox/default_sky`，并启用 `SkyboxComponent`。
- Editor 侧 `editor_scene_io` 已补齐 `SkyLightComponent` / `SpotLightComponent` 拷贝与 `MeshRendererComponent` 材质字段往返，`reference_demo` 场景桥接测试已恢复通过。
- Lua demo 层已继续推进到“reference scene 优先 + 程序化 fallback 兜底”的统一路径；`demo15_8.lua` / `demo15_9.lua` 当前都会输出 `visual_baseline`、`observer_checkpoints`、`fallback_scene_summary` 等人工观察日志，并补齐了天空盒、相机构图、灯光与材质参数的主线配置。
- `demo15_7.lua` 已从“仅程序化材质预览”推进为“优先加载 `reference_demo_15_7.scene.json` 的真实模型展示主路径”，并在失败时回退到原有程序化预览；新 scene 已接入 5 个 `Monster` 展示位、`OceanPlane`、方向光、`SkyLight` 与目录式 skybox，当前观察语义已从 3 组材质近似推进到更贴近参考 demo 的 5 类材质观察位（PhoneTwoPass / Phone / BlinnPhone / OrenNayar / Custom）。
- `reference_demo_15_7.scene.json` 的 5 个展示位已显式标记为 `MaterialInstance` 数据源语义，后续可逐步替换为真实导入的 `NewMonsterPhone*` / `Material*` 变体资产；本轮已重编 `dse_engine_unit_tests` 并确认 `Given_CheckedInReferenceDemo157Scene*` 基线通过。
- `demo15_7.lua` 现已让左侧两个展示位显式调用 `set_mesh_material(..., "assets/cooked/reference_demo/shared/monster/Monster.dmat", index)`，分别绑定 `Monster.dmat` 的第 0 / 1 个材质槽，验证 Lua 层 `dmat -> MaterialInstance` 运行时路径可按材质索引接入当前 `15.7` 主线；其余展示位仍保留标量实例语义，等待后续真实变体资产。
- 同一套 Lua `dmat` 材质索引能力已开始复用到 `demo15_9.lua`：左右展示位现分别绑定 `Monster.dmat` 的第 0 / 1 个材质槽，并继续叠加现有 metallic / roughness 交互覆盖。
- `15.7 / 15.8 / 15.9` 现统一复用 `assets/source/reference_demo/shared/skybox/default_sky`；`default_sky` 已补齐可稳定解码的最小 `.bmp` 六面资源，修复了此前 `.ppm` 占位面图在单测路径下的 cubemap 解码失败问题。
- `lua_runtime_smoke_single_test.cpp` 本轮已补充 RAII 清理守卫，并为 `15.8 / 15.9` 增加专用过滤标签，避免整组运行时的 Lua 全局状态串扰；`lua_test_main.cpp` 也已从 `quick_exit` 改为正常 `return`，减少 CTest 环境下的退出/输出异常。
- `tests/engine/CMakeLists.txt` 已补齐 Lua 单测目标的 runtime DLL 拷贝、工作目录修复和 smoke 过滤收敛；同时尝试将 `dse_lua_runtime_smoke_single_test` 改造为单翻译单元入口，以绕开当前 Windows/VS 生成工程对 smoke 目标编译状态的异常复用。
- 对第三方源码依赖，顶层 `CMakeLists.txt` 已统一关闭 `ASSIMP_INSTALL` / `ENTT_INSTALL`，并在 `depends/assimp` / `depends/entt-3.13.0` 中对 package config / export 逻辑做了最小 install 场景收口，当前 `cmake -S . -B build_vs2022 ...` 已恢复成功。
- 当前**已经确认通过**的验证包括：`15.7` 单独 smoke 用例通过、`15.8` 单独 smoke 用例通过；其中 `15.8` 已实际验证 `startup_scene_loaded`、`missing_resource_count=0`、相机/FOV/灯光强度/阴影强度断言全部通过。
- 当前**尚未闭环**的唯一阻塞是：Windows + VS 生成工程下，`dse_lua_runtime_smoke_single_test(_v2)` 目标仍未稳定产出 exe，导致 `engine.lua_runtime.smoke` 的整组 CTest 门禁暂时无法完整跑通；这属于本机构建链问题，而不是 `DOC-20` 本轮 Lua demo 对齐实现本身的阻塞。

---

## 12. 最终视觉质量提升与截图基线进展（2026-04-14）

### 12.1 本轮已完成的视觉质量提升

本轮没有引入新的渲染架构或第三方依赖，而是在现有 scene / Lua / smoke 链路上做最小收口：

- **相机构图**：
  - `reference_demo_15_8.scene.json` 从较远的 `camera=(0, 6, 18), fov=60` 收紧为 `camera=(0, 5.4, 15.5), fov=52`，让单角色主体更集中；
  - `reference_demo_15_9.scene.json` 从 `camera=(0, 7.5, 20), fov=60` 收紧为 `camera=(0, 6.2, 17.8), fov=50`，让左右材质对比更易观察。
- **方向光 / SkyLight**：
  - 两个 demo 都提高了主方向光强度与阴影强度，避免角色与地面在最小 skybox 下显得过平；
  - `SkyLightComponent` 改为更接近冷色天空 / 暗部补光的半球光参数，继续作为 IBL 缺失前的环境光近似。
- **角色与地面材质**：
  - `15.8` 主角色 roughness 降低到 `0.52`，地面 roughness 降低到 `0.74` 并加入极弱 emissive，用于提升主体与地面的可读性；
  - `15.9` 左右角色拉开成“偏冷哑光”和“偏暖高光”两组参数，右侧 `metallic=0.38 / roughness=0.18`，左侧 `metallic=0.03 / roughness=0.78`。
- **人工观察日志**：
  - `demo15_9.lua` 本轮额外修复了一个关键运行时缺口：scene JSON 中的 `id=1/2` 不能被继续当作 Lua 运行时实体句柄使用。为避免硬编码 entt 运行时实体编号，已在 Lua ECS 绑定层补入最小只读查询接口 `find_entities_by_mesh_path`，并改为“按 mesh_path 找候选实体、再按位置区分左右角色”的绑定策略；`15.9` 的左右材质参数现在已能稳定作用到正确实体。
- 对应地，`dse_lua_runtime_smoke_single_test.exe -s` 已补齐并通过 `15.8 / 15.9` 的相机、灯光与左右材质对比断言，当前最小视觉基线已同时覆盖 scene 层与 Lua runtime 层。
- 当前仍未覆盖 HDR / DDS / KTX / IBL / cooked skybox 资产链，这些属于后续画质提升阶段，不阻塞本轮最小视觉闭环交付。


全面检索后，仓库目前已有 `snapshot` 风格确定性 smoke，但未发现完整可复用的运行时截图 / framebuffer readback / PNG golden comparison 链路。因此本轮没有伪造“像素截图基线”，而是先落地最小可维护的替代验证：

- `tests/engine/scene/scene_flow_test.cpp` 已把 `15.8 / 15.9` 的相机、方向光、SkyLight、skybox 路径、角色材质、地面材质纳入确定性断言；
- `tests/engine/lua_runtime_smoke_single_test.cpp` 已把 Lua 启动后的关键相机 / 灯光 / 材质状态纳入 smoke snapshot 断言；
- 当前基线能防止 scene 参数被误改、Lua 启动未加载 reference scene、`15.9` 左右材质对比退化等问题。

### 12.3 当前基线验证口径

当前推荐验证集合为：

1. `dse_engine_unit_tests.exe "[scene][3d][reference_demo]"`
   - 覆盖 `reference_demo_15_8.scene.json` / `reference_demo_15_9.scene.json` 的 scene 级视觉状态基线；
2. `dse_lua_runtime_smoke_single_test.exe -s`
   - 覆盖 Lua demo 15.8 / 15.9 启动 reference scene 后的最小运行时状态；
3. 手动运行 Lua demo 后观察日志中的 `visual_baseline` / `observer_checkpoints`，再进行人工截图留档。

### 12.4 下一阶段剩余边界

后续如果要从“状态基线”升级为真正截图基线，需要补齐以下能力：

- runtime 或测试层可控窗口尺寸、固定相机朝向与固定渲染帧数；
- framebuffer readback / screenshot 输出能力；
- PNG 或原始像素 hash 的 golden baseline 管理；
- 允许小阈值误差的图像比较工具，避免不同 GPU / 驱动导致无意义抖动；
- 更高质量 skybox 原始资源、HDR / IBL / cooked skybox 资产链，用于替代当前最小 `.ppm` 六面 skybox。

### 12.5 下一阶段最小截图链路建议（已基于当前代码库检索）

结合当前仓库现状，最小可落地的截图链路建议不是直接做“全场景图像回归平台”，而是只补以下四个点：

1. **RHI 层只补一个只读接口**
   - 在 `RhiDevice` 增加类似 `ReadRenderTargetColorRgba8(render_target_handle)` 的只读方法；
   - OpenGL 实现里基于现有 `RenderTarget` / FBO 句柄做 `glBindFramebuffer + glReadPixels`；
   - 先只支持读取颜色附件 0 的 RGBA8，不在第一步扩展深度、cube face、多 mip。

2. **Runtime 层只补一个测试友好的导出入口**
   - 复用 `FramePipeline` 已存在的 `scene_render_target` / `main_render_target`；
   - 在 runtime host 或 test helper 中增加“跑固定帧数后导出一张 PNG”的最小入口；
   - PNG 写出可复用当前已经引入的 `stb_image_write`，避免新增第三方依赖。

3. **测试层先只覆盖一个固定场景**
   - 不建议一下子给 `15.8 / 15.9 / 3d_mvp_minimal` 全部铺开；
   - 首批最多只给 `reference_demo_15_9` 增加 1 张低分辨率基线图，用于验证左右材质反差和天空盒背景是否仍然存在；
   - 固定项至少包括：窗口尺寸、startup scene、最大帧数、相机位置、后处理开关。

4. **图像比较先做弱约束版本**
   - 第一版不做整图逐像素严格相等；
   - 可先做以下二选一：
     - 方案 A：对整图做简单 hash / 均值亮度 / 局部区域均值比较；
     - 方案 B：仅裁剪左右角色所在区域，比较区域均值和高光强度分布；
   - 等链路稳定后，再决定是否增加小阈值像素 diff。

### 12.7 当前代码层面的真实落点（2026-04-14 再确认）

本轮继续向下检索后，最小截图专项需要修改的真实代码位置已经基本明确：

- **窗口尺寸固定**：`engine/runtime/engine_app.h` 中的 `EngineRunConfig.window_width / window_height` 已可直接控制；
- **固定帧数**：`engine/runtime/engine_app.cpp` 已支持环境变量 `DSE_MAX_FRAMES`；
- **截图源 RenderTarget**：`engine/runtime/frame_pipeline.cpp` 已明确维护 `scene_render_target` 与 `main_render_target`；
- **读取目标句柄**：`FramePipeline::GetSceneTextureId()` / `GetMainTextureId()` 当前只能拿到颜色纹理句柄，还不能把像素读回 CPU；
- **RHI 进展**：`engine/render/rhi/rhi_device.h/.cpp` 已补入最小 `ReadRenderTargetColorRgba8(render_target_handle)` 能力，OpenGL 后端现已可基于现有 `RenderTarget` FBO 执行 `glReadPixels` 并返回 RGBA8 缓冲；
- **PNG 输出底座**：`engine/runtime/engine_app.cpp` 已引入 `stb_image_write`，无需额外引入新库。

因此，下一阶段如果正式实现最小截图专项，建议按以下文件顺序推进：

1. `engine/runtime/frame_pipeline.h/.cpp`
   - 视情况补一个“读取 scene/main render target 像素”的薄包装；
2. `engine/runtime/engine_app.cpp` 或单独 test helper
   - 增加固定帧数后导出 PNG 的最小入口；
3. `tests/engine/`
   - 新增单场景截图专项测试，首批仅覆盖 `reference_demo_15_9`。

### 12.8 当前截图专项进度结论

- **已完成**：RHI 层最小 readback API 已落地，新增 `ReadRenderTargetColorRgba8`，并通过 `dse_engine_unit_tests.exe [render][rhi]` 回归；
- **已完成**：runtime 层已补上最小 PNG 导出入口；当前可通过环境变量 `DSE_SCREENSHOT_PATH` 指定输出路径，并用 `DSE_SCREENSHOT_TARGET=scene|main` 选择读取 `scene_render_target` 或 `main_render_target`；导出时会基于实际 RenderTarget 尺寸做 readback 与 PNG 写出，而不是继续假设窗口 framebuffer 尺寸；
- **已完成**：`tests/engine/runtime/reference_demo_screenshot_test.cpp` 已新增 `reference_demo_15_9` 单场景截图专项，并注册为 `engine.reference_demo.screenshot`；当前专项已确认通过，验证口径包括：startup scene 成功加载、PNG 文件真实写出、文件头为 PNG 签名、`IDAT` 载荷非空且可得到非零弱约束 hash；同时已进一步补上 PNG 解码后的最小区域统计，当前会校验中央主体区域与天空区域均非纯黑、且天空区域保持 `blue >= red` 的冷色背景特征；
- **已完成**：runtime 渲染闭环已继续向前推进：当前 `composite_pass` 会先把 scene / UI 合成到 `main_render_target`，runtime 非 editor 模式下也已补入 `present_pass`，会把 `main` 纹理再绘制回默认 framebuffer，避免最终合成结果只停留在离屏目标中；
- **本轮修复与复核结论**：此前 `main_render_target` 的首个问题是 `RenderTargetDesc` 聚合参数错位，旧代码等价于创建 `has_color=false, has_depth=false` 的空 FBO；当前已改为显式字段赋值。随后尝试把 `composite_pass` / `present_pass` 的全屏复制改为 `DrawPostProcess(..., "copy", {})`，但最新复核显示：`main` PNG 现在可以写出且日志可显示 `target=main`，但解码后中心/天空区域仍为全黑。因此当前稳定门禁仍保持在 `scene` 读回，`main` 内容写入问题继续作为后续专项排查项；
- **未完成**：尚未引入真正的 golden PNG 基线文件管理，也还没有做左右展示位的区域均值/高光分布等更细粒度图像比较；另外，`main_render_target` 当前虽然已能导出 PNG，但内容仍为全黑，后续需要继续排查 `composite_pass` 的实际绘制结果为何未反映到读回内容中；
- **当前建议**：下一轮若继续推进，应优先围绕 `main_render_target` 做更小粒度诊断（例如新增只渲染纯色/单纹理 copy 的最小 RHI 或 runtime 测试），在确认最终合成内容真正进入 `main` 后，再正式把截图门禁切换到 `main` 目标；在此之前，`reference_demo_15_9` 继续沿用 `scene` 读回 + 中央/天空区域弱约束作为稳定基线。
















