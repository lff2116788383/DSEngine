# RESOURCE MIGRATION REFERENCE MAP

## 1. 文档目的

本文档用于维护 `reference/VSEngine2.1` 向 `DSEngine` 主仓迁移的资源映射关系，作为 `DOC-20` 第二阶段“迁出第一批源资源”的执行台账。

文档重点回答以下问题：

- 哪些资源已经从 `reference` 体系中被正式选中；
- 这些资源原始位于哪里，计划迁入主仓的哪个目录；
- 当前资源处于“已复制 / 待转换 / 已 cooked / 运行时已接入 / 仍阻塞”的哪个状态；
- 哪些 demo 依赖这些资源；
- 当前是否已经摆脱对 `reference/VSEngine2.1` 路径的直接运行时依赖。

---

## 2. 状态字段说明

| 字段 | 含义 |
| --- | --- |
| `来源路径` | 当前 reference 仓或其他来源中的原始资源位置 |
| `目标路径` | 计划迁入主仓后的目标目录或目标文件 |
| `关联 demo` | 当前资源服务的 demo，如 `15.8`、`15.9` |
| `资源类型` | 如 `fbx`、`texture`、`skybox`、`animation`、`material` |
| `当前状态` | `待复制` / `已复制` / `待转换` / `已 cooked` / `已接入运行时` / `阻塞` |
| `reference 依赖状态` | `仍依赖 reference` / `已脱离 reference` |
| `备注` | 记录替代方案、阻塞原因、转换脚本、人工处理说明等 |

建议统一使用以下状态语义：

- `待复制`：已确认纳入迁移范围，但主仓尚未落地；
- `已复制`：原始文件已进入主仓目标目录，但尚未形成可运行资产；
- `待转换`：已具备原始资源，但 importer / cooker 尚未完成该资源链处理；
- `已 cooked`：已经生成引擎可消费的中间或运行时资产；
- `已接入运行时`：Lua / scene / runtime 已切到主仓资产路径；
- `阻塞`：当前存在明确阻塞，需要记录原因与后续处理人。

---

## 3. 第一批候选资源清单（DOC-20 对齐）

### 3.1 核心试点资源

| 资源名 | 来源路径 | 目标路径 | 关联 demo | 资源类型 | 当前状态 | reference 依赖状态 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `Monster.FBX` | `reference/VSEngine2.1/FBXResource/Monster.FBX` | `assets/source/reference_demo/shared/monster/Monster.FBX` | `15.8`, `15.9` | `fbx` | `15.8 / 15.9 已接入运行时` | `15.8 / 15.9 已脱离 reference` | 已产出 `Monster.dmesh / Monster.dmat / Monster.danim / Monster.dskel`；`15.8` 与 `15.9` scene 已切到 `assets/cooked/reference_demo/shared/monster/Monster.dmesh` |
| `OceanPlane.FBX` | `reference/VSEngine2.1/FBXResource/OceanPlane.FBX` | `assets/source/reference_demo/shared/ocean_plane/OceanPlane.FBX` | `15.8`, `15.9` | `fbx` | `15.8 / 15.9 已接入运行时` | `15.8 / 15.9 已脱离 reference` | 已产出 `OceanPlane.dmesh / OceanPlane.dmat`；`15.8` 与 `15.9` scene 已切到 `assets/cooked/reference_demo/shared/ocean_plane/OceanPlane.dmesh` |
| `MonsterLOD0.FBX` | `reference/VSEngine2.1/FBXResource/MonsterLOD0.fbx` | `assets/source/reference_demo/shared/monster/MonsterLOD0.FBX` | `15.8`, `15.9` | `fbx` | `15.9 已接入运行时` | `15.9 已脱离 reference` | 已产出 `MonsterLOD0.dmesh / MonsterLOD0.dmat`（无动画/骨骼，跳过 `danim/dskel`）；`15.9` scene 已切到 `assets/cooked/reference_demo/shared/monster_lod0/MonsterLOD0.dmesh` |



### 3.2 当前运行期缺口占位

| 资源名 | 当前引用路径 | 建议替代目标 | 关联 demo | 资源类型 | 当前状态 | reference 依赖状态 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `reference_demo_character_placeholder.fbx` | `assets/meshes/reference_demo_character_placeholder.fbx` | 以 `Monster.FBX` / `MonsterLOD0.FBX` 导入后的主仓角色资产替代 | `15.8`, `15.9` | `mesh placeholder` | `已替换` | `已脱离占位路径` | `15.8 / 15.9` scene 已不再引用该占位路径 |
| `default_sky` | `assets/skyboxes/default_sky` | `assets/source/reference_demo/shared/skybox/default_sky/` | `15.8`, `15.9` | `skybox` | `已接入运行时（最小目录式）` | `已脱离占位路径` | 当前采用目录式 cubemap 约定：`cubemap_path` 指向目录，目录内固定六面 `px/nx/py/ny/pz/nz`；主仓已补入最小 `.ppm` 六面资源，`15.8 / 15.9 / 3d_mvp_minimal` 已启用 `SkyboxComponent`，但尚未接入 HDR/IBL/cooked skybox 资产链 |
| `Monster` 贴图集 | `reference/VSEngine2.1/Bin/Resource/Texture/Monster*.tga` | `assets/source/reference_demo/shared/monster/textures/` | `15.8`, `15.9` | `texture` | `已迁移并接入 dmat` | `已脱离 reference` | 已迁入 `Monster_d/e/n/s.tga` 与 `Monster_w_d/e/n/s.tga` 共 8 张贴图；`Monster.dmat` 与 `MonsterLOD0.dmat` 已回写到主仓内 `assets/source/reference_demo/shared/monster/textures/Monster_*` 路径，静态检查 `all_local=true` |

---

## 4. 第二阶段实施建议

### 4.1 建议迁移顺序

1. 先复制 `Monster.FBX` 与 `OceanPlane.FBX`；
2. 再补充 `15.8 / 15.9` 直接需要的纹理与天空盒原始资源；
3. 随后补齐 `MonsterLOD0.FBX` 等可选资源；
4. 等第三阶段 importer 最小主链打通后，再把 scene / Lua 运行路径切到主仓资产。

### 4.2 本阶段完成判定

满足以下条件后，可认为第二阶段基础资源迁移已完成：

- 第一批原始资源已经进入 `assets/source/reference_demo/`；
- 映射表中每一项资源都具备明确状态；
- 不再新增任何新的 `reference/...` 运行时资源路径依赖；
- 对暂未导入成功的资源，已经在 `备注` 中记录阻塞原因与替代计划。

---

## 5. 当前结论（2026-04-14）

- `reference/VSEngine2.1` 子模块当前仍存在，可作为第二阶段资源迁移输入源；
- `assets/source/reference_demo/` 已开始建立，第一批 `Monster.FBX` / `MonsterLOD0.FBX` / `OceanPlane.FBX` 已迁入主仓；
- `15.8 / 15.9` 相关 cooked 资产、材质贴图与 scene 主链已经接入运行时；
- `15.7` 已从程序化材质预览推进到真实模型 scene 主路径：`reference_demo_15_7.scene.json` 现已复用主仓 `Monster` / `OceanPlane` cooked 资产与目录式 skybox 资源；对应 Lua / scene / startup / editor bridge 回归源码已补齐并完成测试目标重编，`engine.lua_runtime.smoke`、`[skybox]`、`[scene][3d][reference_demo]` 与 `[editor][reference_demo]` 路径均已确认通过；当前展示位已从 3 组扩展到 5 组，并显式标记为 `MaterialInstance` 数据源语义，且 Lua 主路径已通过 `set_mesh_material(..., dmat, index)` 绑定 `Monster.dmat` 第 0 / 1 个材质槽，更贴近参考 demo 的材质观察目标；




- `default_sky` 已从占位路径切换到主仓目录式 skybox 资源，`3d_mvp_minimal`、`reference_demo_15_8`、`reference_demo_15_9` 已启用 `SkyboxComponent`，且当前已补齐可稳定解码的最小 `.bmp` 六面资源，修复了此前 `.ppm` 占位面图在单测路径下的 cubemap 解码失败问题；
- Editor 桥接链路已补齐 `SkyLight` / `SpotLight` 与 `MeshRenderer` 材质字段往返，`reference_demo` 场景桥接回归已通过；
- Lua demo 15.8 / 15.9 的 smoke 已确认通过，reference scene 启动后 `missing_resource_count=0`；
- `15.7 / 15.9` 已开始复用 `Monster.dmat` 的多材质槽运行时绑定：Lua `set_mesh_material(entity, dmat, index)` 现支持可选材质索引，`15.7` 左侧两个展示位与 `15.9` 左右两个展示位已分别绑定 `Monster.dmat` 的第 0 / 1 个材质槽；
- 当前主要剩余问题已从“是否可运行”转为“最终视觉质量提升”：后续重点是更高质量 skybox 原始资源、IBL、cooked skybox 资产链与截图基线。










