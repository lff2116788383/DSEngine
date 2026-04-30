# VSEngine2.1 Demo 15.22 资源清单

> 用途：为 `samples/lua/3d/3d_vse15_22_scene.lua` 的“完整场景复刻”记录原始素材、离线转换结果与当前缺口。
>
> 当前状态：已复制 VSE 15.22 相关原始/源资源到 `data/vse_demo/15_22/raw/`，并已用 `bin/AssetBuilder.exe` 从 FBX 烘焙出 `data/vse_demo/15_22/cooked/` 下的 DSE `.dmesh/.dmat/.dskel/.danim`。当前 Lua demo 已严格复刻 `Source.cpp` 场景对象清单与坐标布局：6 个 `NewMonsterWithAnim.SKMODEL`、1 个语义等价的 DSE 程序化 `NewOceanPlane.STMODEL` 支撑平面、1 个 SkyLight、1 个 PointLight、1 个第一人称/自由相机等价控制器；不再生成非 VSE 15.22 的 crate/cover/marker/cube fallback 可见物。DSEngine 运行时仍不能直接加载 VSE 原生 `.SKMODEL`、`.STMODEL`、`.TEXTURE`、`.ACTION/.ANIMTREE`，因此资源承载方式是“FBX cooked 角色 + DSE 程序化生成的 VSE 支撑平面语义复刻”，不是 VSE 原生格式直读。

## 来源

- 参考工程：`reference/VSEngine2.1`
- 参考 demo：`reference/VSEngine2.1/Demo/15/15.22/Source.cpp`
- 参考场景内容：
  - 6 个 `NewMonsterWithAnim.SKMODEL` 骨骼角色实例。
  - 动画状态：`Idle`、`Walk`、`Attack`、`Attack2`、`Pos`、`AddtiveAnim`。
  - 地面：`NewOceanPlane.STMODEL` 语义对象；该资源在 VSE material-saver demo 中由共享 `OceanPlane.STMODEL` 生成/派生，DSE demo 当前改为运行时程序化生成等价支撑平面，不再依赖 cooked VSE OceanPlane mesh。
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

- Camera：VSE `(0,900,900)` + dir `(0,-1,-1)`，FOV=90、near/far=`1/8000`。当前 DSE 已支持 Lua 传入 3D camera near/far，并使用 DSE 可见窗口 `(0,5.2,14)` / pitch `-26` 作为截图验收视角；VSE 原始 `(0,9,9)` / `-45` 已验证会因当前坐标/模型尺度组合在自动截图中几乎不可见，后续需继续做坐标系/单位尺度等价推导。
- 地面：`NewOceanPlane.STMODEL` 在 VSE 侧是由共享 `OceanPlane.STMODEL` 生成/派生出的支撑资源；当前 Lua 不再加载 `vse_demo/15_22/cooked/OceanPlane.dmesh/.dmat`，而是在 DSE 运行时生成 `procedural:DSE_OceanPlaneQuad` 四边形，VSE 语义位置 `(0,0,0)`、scale `(100,100,100)`，DSE 当前使用 `dse_half_size=17.5` 匹配自动截图可见范围。
- 角色：6 个 `NewMonsterWithAnim.SKMODEL` 均使用 `vse_demo/15_22/cooked/Monster.dmesh` + `vse_demo/15_22/cooked/Monster.dmat` + `vse_demo/15_22/cooked/Monster.dskel`。
- 角色坐标：`(-300,0,300)`、`(0,0,300)`、`(300,0,300)`、`(-300,0,-300)`、`(0,0,-300)`、`(300,0,-300)`，DSE 按 `SCALE=0.01` 放置。
- 动画映射：`Monster.danim` 近似 Idle/Pos/AddtiveAnim，`Walk.danim` 对齐 Walk，`Attack.danim` 对齐 Attack，`Attack2.danim` 对齐 Attack2。
- 灯光：SkyLight up/down color 对齐 `(0.2,0.2,0.2)` / `(0,0,0.5)`；PointLight 对齐 VSE 位置 `(0,500,0)`。

## DSE/OpenGL 运行时兼容修复

排查黑屏时确认，VSE 15.22 cooked mesh draw calls 已进入 OpenGL，并且 `glDrawElements` 无 GL error；问题不是单纯灯光/材质强度。阶段性定位到两个真实渲染链路问题：第一，`.dmesh` fast path 只提交 `submeshes[0]`，未合并后续 submesh，并且没有处理 `base_vertex`；`Monster.dmesh` 实际为 2 个 submesh，第二个 submesh 从 `base_vertex=2842` 开始。第二，DSE skinned 路径仍采用“CPU 端把 `mesh_model` 预乘到 `bone_matrices`，shader 中 `u_model` 保持单位矩阵”的既有约定；尝试改为 `u_model * localPos` 的统一模型矩阵路径后，当前 cooked skeleton/mesh 组合在截图中退化为近黑，说明骨骼矩阵空间仍未完全厘清。

本轮修复方式：

- 修正 `MeshRenderSystem` 的 `.dmesh` fast path：加载全部 submesh、按 `base_vertex + index` 重建运行时索引，并保留完整 `header->vertex_count` 的 20-float runtime vertex 数据，避免 `Monster.dmesh` 只显示第一个 submesh。
- 保留现有 skinned 空间组合：`bone_matrices = mesh_model * final_bone_matrix`，shader `u_model` 为单位矩阵；这是当前 Animator3D cooked 资源能稳定出图的路径。尝试改成统一 `u_model` 路径后截图近黑，已回退。
- Lua ECS 的 `dse.ecs.add_camera_3d(entity, fov, priority, near, far)` 现在支持传入 VSE near/far 映射值，`3d_vse15_22_scene.lua` 使用 `0.01/80.0` 对齐 `SCALE=0.01`。
- `3d_vse15_22_scene.lua` 保留 6 个 `NewMonsterWithAnim.SKMODEL`、1 个 `NewOceanPlane.STMODEL`、SkyLight、PointLight、相机对象清单，不重新加入非 VSE fallback cube/crate。
- 深度状态：在 submesh 修复后进一步二分确认，导致当前截图近黑的主遮挡源是 `NewOceanPlane.STMODEL` 语义平面：Monster 开启 `depth_test=true`、`depth_write=true` 可以通过；`depth_test=true/depth_write=false` 会回到 `SCREENSHOT_TOO_DARK`。按用户反馈进一步检查 VSE 资源来源后，确认 `NewOceanPlane` 属于 VSE 侧生成/派生支撑资源，因此 Lua 已改为 DSE 程序化平面而不是 cooked VSE OceanPlane mesh。继续验证 DSE 程序化平面后，当前最小通过组合已推进到 OceanPlane 专属 `depth_test=false`、`depth_write=true`：也就是该平面需要跳过已有深度测试以避免被当前深度链路/PreZ 误拒绝，但允许写入地面深度，避免完全游离于深度缓冲之外；Monster 深度保持正常。进一步把 OceanPlane 创建顺序调整到 6 个 Monster 之后，并改回 `depth_test=true/depth_write=true` 验证，仍然稳定复现 `SCREENSHOT_TOO_DARK (max_rgb=15 avg_rgb=15)`，说明问题不是 Lua 创建顺序/简单 draw order 导致的先画地面遮挡角色。后续又做了三项引擎级探针：一是阻断内置 `mesh_render_system_` 在 `Gameplay3D` 动态模块存在时的重复 scene mesh submit；二是临时禁用 PreZ pass；三是把 pipeline 默认 `depth_func` 从 `GL_LESS` 改成 `GL_LEQUAL`。三者均不能让 OceanPlane 在 `depth_test=true/depth_write=true` 下通过，说明当前近黑不是单纯重复提交、PreZ 预写或默认 depth func 不一致导致。当前已新增受控深度诊断：`MeshRenderSystem` 给 Demo 15.22 的 Monster 与程序化 OceanPlane 打上临时 `debug_label`，记录 world-space bounds；`GLDrawExecutor` 在前 3 个相关 frame 打印 clip `w`、NDC `z` 范围，并回读 3x3 scene depth 样本；后续又补充 OceanPlane 三角形级 clip-space 诊断。诊断确认当前通过态下 OceanPlane 为 `depth_test=false/depth_write=true`、world bounds `(-17.5,-0.25,-17.5)` 到 `(17.5,-0.25,17.5)`、NDC z 约 `0.999589`，且 4 个顶点中有 2 个 `clip.w<=0`，说明该大平面横跨/穿过相机近平面或相机背侧区域。三角形级证据更明确：tri0 `indices=(0,1,2)` 只有 1/3 个顶点 `w>0`，tri1 `indices=(2,3,0)` 只有 2/3 个顶点 `w>0`；两三角形均 `crosses_near=true`，NDC z 范围约 `0.999589..1.01683`，其中部分顶点已超出 far/clip 后有效深度区间；两个三角形的 NDC signed area 分别约 `56.8957` 与 `-2.26828`，说明同一四边形被投影/裁剪后屏幕空间方向与面积极不稳定。基于该结论，本轮加入了 `ocean_clip_safe` 受控实验开关：当 `ocean_clip_safe=true` 时，Lua 可把 OceanPlane z 范围裁到 `z_min=-17.5,z_max=12.0` 并启用 `depth_test=true/depth_write=true`。该几何确实消除了 OceanPlane near crossing：`invalid_w=0`，两个 triangle 均 `valid_w=3`、`crosses_near=false`、NDC z 约 `0.994898..0.999589`；但截图仍为 `SCREENSHOT_TOO_DARK (max_rgb=15 avg_rgb=15)`，甚至一次验证在 90 秒内未正常完成，说明“单纯让 OceanPlane 不跨 near plane”仍不足以恢复正常可见性，根因还包含 depth buffer/scene pass/Monster skinning 后深度或 ground plane 专用策略问题。Monster 的日志显示 skinned draw 使用 bind/local mesh + bone matrix 路径，直接用 `vp * item.model * vertex.pos` 的 CPU 诊断会出现大量 `clip.w<=0`，不能等价代表最终 skinning 后顶点深度，需要后续补充 GPU skinning 后或 CPU 复算 skinning 后的深度探针。实验结束后已把配置回退为 `ocean_clip_safe=false`，Lua 默认仍为安全通过态 `depth_test=false/depth_write=true`。Lua 日志现标注 `monster_depth_state=enabled ocean_depth_state=test_disabled_write_enabled procedural_dse_plane=true ocean_creation_order=after_characters duplicate_mesh_submit_guard=true`，范围已从“全部 VSE cooked mesh 关深度”收敛到“只对 OceanPlane 语义平面关闭 depth test、保留 depth write”。后续应重点排查 depth buffer/scene pass 状态、skinned Monster 的实际输出深度，以及 ground/support_plane 专用渲染语义，而不再只盯 near-plane crossing。

验证命令已通过：

```bat
python tools\verify_lua_3d_demos.py --entries p4 --frames 160 --timeout 90
```

结果：`SCREENSHOT_OK`、`VERIFY_OK`。当前通过组合为 DSE 程序化 OceanPlane + OceanPlane 专属 `depth_test=false`、`depth_write=true`；如果把程序化平面改回 `depth_test=true`、`depth_write=true`，验证会因 `SCREENSHOT_TOO_DARK` 失败（本轮再次复现 `max_rgb=15 avg_rgb=15`）。

## 当前阶段视觉差异与后续根因

1. 已修复 `.dmesh` 多 submesh 丢失问题；`Monster.dmesh` 的 2 个 submesh 现在都会进入 `MeshDrawItem`。
2. 当前截图验收不再依赖关闭所有 VSE cooked mesh 深度：6 个 Monster 已恢复正常 depth test/write；剩余兼容开关只作用于 OceanPlane 语义平面，且当前只需关闭 OceanPlane depth test、保留 depth write 即可稳定 `VERIFY_OK`。说明黑屏/近黑的深度根因主要集中在大平面与 depth test/PreZ 链路，而不是 Monster skinned mesh 全局异常。当前 OceanPlane 已由 DSE 程序化生成，避免继续依赖 VSE 生成/派生的 cooked 平面资源。
3. VSE 原始相机 `(0,900,900)` / dir `(0,-1,-1)` 按 `SCALE=0.01` 直映射到 DSE `(0,9,9)` / `-45` 后，自动截图中对象仍过小或不可见；当前继续使用 `(0,5.2,14)` / `-26` 保证肉眼可检查场景，但这仍属于 DSE 可见范围映射，不是 1:1 相机复刻。
4. 材质仍使用 `MESH_UNLIT` 与偏亮 emissive/tint，原因是 PBR + 当前贴图/灯光映射下截图亮度不足；后续需要修正 `.dmat` 贴图路径、VSE specular/emissive 到 DSE PBR 参数映射，再逐步回到 `MESH_PBR`。

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
  # OceanPlane 语义对象建议继续由 DSE 程序化生成，不再沉淀 VSE 生成/派生平面的 cooked mesh/material。
```

## 后续实施顺序

1. 复制/重映射 `soulbrast*.tga` 或替代贴图到 `data/vse_demo/15_22/textures/`，并修正 `.dmat` 中的相对路径。
2. 修正 `AssetBuilder` 多 clip 输出：允许一个 FBX 导出多个 `.danim`，或允许只导出 animation/skeleton 而复用主 `Monster.dmesh`。
3. 若需要完全匹配 VSE 原生资源，再编写 `.SKMODEL/.STMODEL/.ACTION/.ANIMTREE` 只读转换器。
4. 把当前 Idle/Pos/AddtiveAnim 的 Monster clip 近似映射升级为真实 VSE 状态动画。
5. 增加 screenshot/mesh bounds 级验收，确保 cooked Monster/OceanPlane 在不同机器上不是黑屏、过小或贴图缺失。
6. 针对 OceanPlane 单独排查：当前已替换为 DSE 程序化平面，并已从完全禁用深度推进到 `depth_test=false/depth_write=true`；但 `depth_test=true/depth_write=true` 仍会导致截图近黑，即使把 OceanPlane 创建在 Monster 之后也无法恢复。当前受控诊断进一步显示 OceanPlane 大四边形处在极远深度（NDC z 约 `0.999589`）且有 2/4 顶点 `clip.w<=0`；三角形级日志确认两个 OceanPlane triangle 都跨 near plane，其中一个 triangle 只有 1 个有效 `w>0` 顶点，另一个只有 2 个有效 `w>0` 顶点，且裁剪前 NDC z 可到 `1.01683`。本轮进一步测试了 clip-safe OceanPlane：把 z 范围裁为 `[-17.5,12.0]` 后，triangle 级日志显示 `invalid_w=0`、`crosses_near=false`，但 `depth_test=true/depth_write=true` 仍然 `SCREENSHOT_TOO_DARK`，说明 near-plane crossing 是一个真实几何异常，但不是唯一根因。下一步建议把重点转向三组方向：其一，增加 CPU 端 skinning 后 Monster bounds/depth 诊断或 GPU transform feedback/深度可视化，确认角色最终是否写出异常近/远深度；其二，检查 scene pass 深度附件/默认 framebuffer readback 语义与 depth clear/write 状态，解释为什么 3x3 depth readback 长期为 0；其三，在不引入非 VSE fallback 物体的前提下，评估 ground/support_plane 专用 pass 或 depth policy，判断应在引擎层引入 support_plane 专用语义，而不是继续依赖 Lua 的 OceanPlane `depth_test=false` workaround。本轮已排除动态模块 + 内置 mesh render 双提交、PreZ 预写、默认 `GL_LESS`/`GL_LEQUAL` 不一致，并新增 clip-safe 反证：不跨 near plane 仍无法恢复 `depth_test=true`。

## 许可/用途说明

这些资源来自仓库内 `reference/VSEngine2.1`，当前仅用于 DSEngine 内部对齐、转换验证和 demo 复刻，不应作为独立对外素材包发布。若后续需要发布样例资源，应重新确认来源授权或替换为自制/开源许可资源。
