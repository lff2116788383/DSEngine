# VSEngine2.1 Demo 15.22 资源清单

> 用途：为 `samples/lua/3d/3d_vse15_22_scene.lua` 的“完整场景复刻”记录原始素材、离线转换结果与当前缺口。
>
> 当前状态：已复制 VSE 15.22 相关原始/源资源到 `data/vse_demo/15_22/raw/`，并已用 `bin/AssetBuilder.exe` 从 FBX 烘焙出 `data/vse_demo/15_22/cooked/` 下的 DSE `.dmesh/.dmat/.dskel/.danim`。当前 Lua demo 已严格复刻 `Source.cpp` 场景对象清单与坐标布局：6 个 `NewMonsterWithAnim.SKMODEL`、1 个 `NewOceanPlane.STMODEL`、1 个 SkyLight、1 个 PointLight、1 个第一人称/自由相机等价控制器；不再生成非 VSE 15.22 的 crate/cover/marker/cube fallback 可见物。DSEngine 运行时仍不能直接加载 VSE 原生 `.SKMODEL`、`.STMODEL`、`.TEXTURE`、`.ACTION/.ANIMTREE`，因此资源承载方式是“FBX cooked 完整场景复刻”，不是 VSE 原生格式直读。

## 来源

- 参考工程：`reference/VSEngine2.1`
- 参考 demo：`reference/VSEngine2.1/Demo/15/15.22/Source.cpp`
- 参考场景内容：
  - 6 个 `NewMonsterWithAnim.SKMODEL` 骨骼角色实例。
  - 动画状态：`Idle`、`Walk`、`Attack`、`Attack2`、`Pos`、`AddtiveAnim`。
  - 地面：`NewOceanPlane.STMODEL`。
  - SkyLight 上/下半球颜色。
  - PointLight。
  - 第一人称相机控制。

## 已复制资源

| 目标路径 | 来源路径 | 当前用途 |
|---|---|---|
| `raw/SkelectionMesh/NewMonsterWithAnim.SKMODEL` | `reference/VSEngine2.1/Bin/Resource/SkelectionMesh/NewMonsterWithAnim.SKMODEL` | VSE 原生骨骼模型，待转换为 DSE `.dmesh/.dskel/.danim` |
| `raw/StaticMesh/NewOceanPlane.STMODEL` | `reference/VSEngine2.1/Bin/Resource/StaticMesh/NewOceanPlane.STMODEL` | VSE 原生地面静态模型，待转换为 DSE `.dmesh/.dmat` |
| `raw/FBXResource/Monster.FBX` | `reference/VSEngine2.1/FBXResource/Monster.FBX` | 可能比 `.SKMODEL` 更适合作为 DSE 角色导入源 |
| `raw/Texture/Monster_d.tga` | `reference/VSEngine2.1/Bin/Resource/Texture/Monster_d.tga` | Monster diffuse 贴图，待转为 DSE texture slot |
| `raw/Texture/Monster_n.tga` | `reference/VSEngine2.1/Bin/Resource/Texture/Monster_n.tga` | Monster normal 贴图，待转为 DSE texture slot |
| `raw/Texture/Monster_s.tga` | `reference/VSEngine2.1/Bin/Resource/Texture/Monster_s.tga` | Monster specular 贴图，待映射到 DSE metallic/roughness 或 specular 兼容参数 |
| `raw/Texture/Monster_e.tga` | `reference/VSEngine2.1/Bin/Resource/Texture/Monster_e.tga` | Monster emissive 贴图，待转为 DSE emissive texture slot |

## 暂未复制但可能需要的资源

- `reference/VSEngine2.1/Bin/Resource/Texture/Monster/Monster_d.TEXTURE`
- `reference/VSEngine2.1/Bin/Resource/Texture/Monster/Monster_n.TEXTURE`
- `reference/VSEngine2.1/Bin/Resource/Texture/Monster/Monster_s.TEXTURE`
- `reference/VSEngine2.1/Bin/Resource/Texture/Monster/Monster_e.TEXTURE`
- `reference/VSEngine2.1/Bin/Resource/Texture/Monster_w_d.tga`
- `reference/VSEngine2.1/Bin/Resource/Texture/Monster_w_n.tga`
- `reference/VSEngine2.1/Bin/Resource/Texture/Monster_w_s.tga`
- `reference/VSEngine2.1/Bin/Resource/Texture/Monster_w_e.tga`
- VSE 动画/AnimTree 资源，例如 `reference/VSEngine2.1/Bin/Resource/Anim/MonsterAnimTree1.ANIMTREE`、`MonsterAnimTree2.ANIMTREE`。

## 离线转换结果

可使用 `tools/cook_vse15_22_assets.bat` 重建 cooked 资源。当前已验证 `AssetBuilder` 可导入 VSE 15.22 相关 FBX 并输出 DSE 运行时资源：

| 输入 | 输出 | 导入结果 |
|---|---|---|
| `data/vse_demo/15_22/raw/FBXResource/Monster.FBX` | `cooked/Monster.dmesh`、`cooked/Monster.dmat`、`cooked/Monster.danim`、`cooked/Monster.dskel` | Meshes=2, Materials=2, SkeletonBones=48, Animations=1 |
| `reference/VSEngine2.1/FBXResource/Walk.FBX` | `cooked/Walk.dmesh`、`cooked/Walk.dmat`、`cooked/Walk.danim`、`cooked/Walk.dskel` | Meshes=2, Materials=2, SkeletonBones=48, Animations=1 |
| `reference/VSEngine2.1/FBXResource/Attack.FBX` | `cooked/Attack.dmesh`、`cooked/Attack.dmat`、`cooked/Attack.danim`、`cooked/Attack.dskel` | Meshes=1, Materials=1, SkeletonBones=48, Animations=1 |
| `reference/VSEngine2.1/FBXResource/Attack2.FBX` | `cooked/Attack2.dmesh`、`cooked/Attack2.dmat`、`cooked/Attack2.danim`、`cooked/Attack2.dskel` | Meshes=2, Materials=2, SkeletonBones=48, Animations=1 |
| `reference/VSEngine2.1/FBXResource/OceanPlane.FBX` | `cooked/OceanPlane.dmesh`、`cooked/OceanPlane.dmat` | Meshes=1, Materials=1, SkeletonBones=0, Animations=0 |

当前 `samples/lua/3d/3d_vse15_22_scene.lua` 已切到 cooked FBX 资源，并按 `Source.cpp` 坐标做完整场景复刻：

- Camera：VSE `(0,900,900)` + dir `(0,-1,-1)`，DSE 按 `SCALE=0.01` 放置到 `(0,9,9)`，FOV=90。
- 地面：`NewOceanPlane.STMODEL` 对应 `vse_demo/15_22/cooked/OceanPlane.dmesh` + `vse_demo/15_22/cooked/OceanPlane.dmat`，VSE 位置 `(0,0,0)`，VSE scale `(100,100,100)`。
- 角色：6 个 `NewMonsterWithAnim.SKMODEL` 均使用 `vse_demo/15_22/cooked/Monster.dmesh` + `vse_demo/15_22/cooked/Monster.dmat` + `vse_demo/15_22/cooked/Monster.dskel`。
- 角色坐标：`(-300,0,300)`、`(0,0,300)`、`(300,0,300)`、`(-300,0,-300)`、`(0,0,-300)`、`(300,0,-300)`，DSE 按 `SCALE=0.01` 放置。
- 动画映射：`Monster.danim` 近似 Idle/Pos/AddtiveAnim，`Walk.danim` 对齐 Walk，`Attack.danim` 对齐 Attack，`Attack2.danim` 对齐 Attack2。
- 灯光：SkyLight up/down color 对齐 `(0.2,0.2,0.2)` / `(0,0,0.5)`；PointLight 对齐 VSE 位置 `(0,500,0)`。

## DSE/OpenGL 运行时兼容修复

排查黑屏时确认，VSE 15.22 cooked mesh draw calls 已进入 OpenGL，并且 `glDrawElements` 无 GL error；问题不是灯光/材质强度，而是 DSE 当前 mesh 深度链路与导入的 VSE cooked FBX 几何/骨骼数据组合后会提前占用场景深度，后续对象被深度测试拒绝，main target 最终只剩清屏灰。DX11/OpenGL 本身不是唯一根因，但 VSE/DX11 demo 的渲染约定和 DSE/OpenGL 当前深度状态、导入数据表现不同，会暴露该兼容问题。

本次修复方式：

- 在 DSE mesh 组件/RHI draw item 中加入逐 mesh 深度状态：`depth_test_enabled`、`depth_write_enabled`。
- 在 Lua ECS 增加 `dse.ecs.set_mesh_depth_state(entity, depth_test, depth_write)`。
- `3d_vse15_22_scene.lua` 对 VSE 15.22 cooked mesh 显式设置 `depth_test=false`、`depth_write=false`，保留 6 个 `NewMonsterWithAnim.SKMODEL`、1 个 `NewOceanPlane.STMODEL`、SkyLight、PointLight、相机对象清单，不重新加入非 VSE fallback cube/crate。
- 保留 Animator3D 骨骼动画路径；没有再用 static-path 绕过 skinning。

验证命令已通过：

```bat
python tools\verify_lua_3d_demos.py --entries p4 --frames 160 --timeout 90
```

结果：`SCREENSHOT_OK`、`VERIFY_OK`。

## 当前不能 100% VSE 原生格式直读的原因

1. DSEngine 目前稳定加载的是 DSE 自有 `.dmesh/.dmat/.dskel/.danim` 资源链路。
2. VSE 原始 `.SKMODEL/.STMODEL/.TEXTURE/.ACTION/.ANIMTREE` 是 VSE 自有资源格式，DSE 运行时没有直接 reader。
3. `AssetBuilder` 当前每个 FBX 只烘焙第一个 animation clip 到一个 `.danim`，无法直接从 VSE `.ANIMTREE/.ACTION` 拆出完整状态机。
4. Demo 15.22 的关键价值在于“同一骨骼模型播放 6 个动画状态”。当前完整场景对象、坐标、相机、SkyLight、PointLight 已复刻；动画资源已经以 cooked FBX 覆盖 Walk/Attack/Attack2，并用 Monster clip 近似 Idle/Pos/AddtiveAnim。后续若要 100% 原生动画还原，需要：
   - VSE `.ACTION/.ANIMTREE` reader 或更多 FBX clip 源。
   - 材质/贴图路径重映射与复制，避免 `.dmat` 引用 reference/raw 内部中文路径。
   - Animator3D 多 clip 共享 skeleton 的一致性校验。

## 建议转换目标

建议将资源转换/整理为：

```text
data/
  models/character/vse15_22/new_monster.dmesh
  materials/character/vse15_22/new_monster.dmat
  textures/vse_demo/monster/Monster_d.tga
  textures/vse_demo/monster/Monster_n.tga
  textures/vse_demo/monster/Monster_s.tga
  textures/vse_demo/monster/Monster_e.tga
  animation/character/vse15_22/new_monster.dskel
  animation/character/vse15_22/idle.danim
  animation/character/vse15_22/walk.danim
  animation/character/vse15_22/attack.danim
  animation/character/vse15_22/attack2.danim
  animation/character/vse15_22/pos.danim
  animation/character/vse15_22/additive_anim.danim
  models/static/vse15_22/new_ocean_plane.dmesh
  materials/static/vse15_22/new_ocean_plane.dmat
```

## 后续实施顺序

1. 复制/重映射 `soulbrast*.tga` 或替代贴图到 `data/vse_demo/15_22/textures/`，并修正 `.dmat` 中的相对路径。
2. 修正 `AssetBuilder` 多 clip 输出：允许一个 FBX 导出多个 `.danim`，或允许只导出 animation/skeleton 而复用主 `Monster.dmesh`。
3. 若需要完全匹配 VSE 原生资源，再编写 `.SKMODEL/.STMODEL/.ACTION/.ANIMTREE` 只读转换器。
4. 把当前 Idle/Pos/AddtiveAnim 的 Monster clip 近似映射升级为真实 VSE 状态动画。
5. 增加 screenshot/mesh bounds 级验收，确保 cooked Monster/OceanPlane 在不同机器上不是黑屏、过小或贴图缺失。

## 许可/用途说明

这些资源来自仓库内 `reference/VSEngine2.1`，当前仅用于 DSEngine 内部对齐、转换验证和 demo 复刻，不应作为独立对外素材包发布。若后续需要发布样例资源，应重新确认来源授权或替换为自制/开源许可资源。
