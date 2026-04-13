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
| `Monster.FBX` | `reference/VSEngine2.1/FBXResource/Monster.FBX` | `assets/source/reference_demo/shared/monster/Monster.FBX` | `15.8`, `15.9` | `fbx` | `已完成导入验证` | `仍依赖 reference` | 已产出 `Monster.dmesh / Monster.dmat / Monster.danim / Monster.dskel` |
| `OceanPlane.FBX` | `reference/VSEngine2.1/FBXResource/OceanPlane.FBX` | `assets/source/reference_demo/shared/ocean_plane/OceanPlane.FBX` | `15.8`, `15.9` | `fbx` | `已完成导入验证` | `仍依赖 reference` | 已产出 `OceanPlane.dmesh / OceanPlane.dmat`（无动画/骨骼，跳过 `danim/dskel`） |
| `MonsterLOD0.FBX` | `reference/VSEngine2.1/FBXResource/MonsterLOD0.fbx` | `assets/source/reference_demo/shared/monster/MonsterLOD0.FBX` | `15.8`, `15.9` | `fbx` | `已完成导入验证` | `仍依赖 reference` | 已产出 `MonsterLOD0.dmesh / MonsterLOD0.dmat`（无动画/骨骼，跳过 `danim/dskel`） |



### 3.2 当前运行期缺口占位

| 资源名 | 当前引用路径 | 建议替代目标 | 关联 demo | 资源类型 | 当前状态 | reference 依赖状态 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `reference_demo_character_placeholder.fbx` | `assets/meshes/reference_demo_character_placeholder.fbx` | 以 `Monster.FBX` 导入后的主仓角色资产替代 | `15.8`, `15.9` | `mesh placeholder` | `阻塞` | `仍依赖占位路径` | 当前 scene 能加载但会报缺资源日志 |
| `default_sky` | `assets/skyboxes/default_sky` | `assets/source/reference_demo/shared/skybox/default_sky/` | `15.8`, `15.9` | `skybox` | `阻塞` | `仍依赖占位路径` | 需补齐最小天空盒来源与导入策略 |

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

## 5. 当前结论（2026-04-13）

- `reference/VSEngine2.1` 子模块当前仍存在，可作为第二阶段资源迁移输入源；
- `assets/source/reference_demo/` 已开始建立，第一批 `Monster.FBX` / `MonsterLOD0.FBX` / `OceanPlane.FBX` 已迁入主仓；
- 当前 `15.8 / 15.9` 已具备 Lua demo 骨架，但 importer、cooker 与运行时切换尚未完成；
- 第三阶段最小链路已打通：`FbxImporter`、`AssetBuilder` 的 `.fbx` 分支与最小静态测试已补入，并已成功生成新的 `AssetBuilder.exe`；
- 已完成 `Monster.FBX` 的手工 cooked 验证，产出 `Monster.dmesh / Monster.dmat / Monster.danim / Monster.dskel`；
- 当前主要剩余问题是运行时接入与主工程稳定性：`15.8 / 15.9` scene 仍未切到新 cooked 产物，且主工程仍存在 Box2D 3.x API 与 `depends/box2d-2.4.1` 依赖版本不匹配问题。





