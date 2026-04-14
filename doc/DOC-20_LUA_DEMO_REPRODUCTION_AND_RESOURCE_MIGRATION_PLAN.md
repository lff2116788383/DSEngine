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
- Lua demo 层已同步文案状态，`demo15_8.lua` / `demo15_9.lua` 启动 reference scene 时均可在 smoke 中稳定通过，且 `missing_resource_count=0`。
- 当前仍未覆盖 HDR / DDS / KTX / IBL / cooked skybox 资产链，这些属于后续画质提升阶段，不阻塞本轮最小视觉闭环交付。

