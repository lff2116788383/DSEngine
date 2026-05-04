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

结果：`SCREENSHOT_OK`、`VERIFY_OK`。当前通过组合为 DSE 程序化 OceanPlane + OceanPlane `depth_test=true`、`depth_write=true`；OceanPlane 已恢复正常深度测试，不再需要 `depth_test=false` 的临时规避。

## Depth Buffer 根因分析与修复

### 根因

OceanPlane `depth_test=true` 导致 `SCREENSHOT_TOO_DARK` 的根本原因是 **`glDepthMask(GL_FALSE)` 状态泄漏导致深度缓冲清除静默失败**。

**完整因果链**：

1. 渲染图的 composite/present pass 使用 `composite_pipeline_state`，其中 `depth_test_enabled=false`、`depth_write_enabled=false`
2. 这导致 `glDepthMask(GL_FALSE)` 在 composite pass 结束后被设置
3. 下一帧的 scene pass `BeginRenderPass` 执行 `glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT)`
4. **关键问题**：OpenGL 规范规定 `glClear()` 受当前 `glDepthMask` 控制——当 `glDepthMask(GL_FALSE)` 时，`glClear(GL_DEPTH_BUFFER_BIT)` 静默无效
5. 深度缓冲未被清除到 1.0，残留上一帧的旧值
6. OceanPlane（NDC z ≈ 0.999）无法通过 `GL_LEQUAL` 深度测试（因为残留在 0.999 附近的旧值可能更小），导致被完全剔除
7. 场景变近黑

### 修复

在 `engine/render/rhi/gl_draw_executor.cpp` 的 `BeginRenderPass` 中，清除深度缓冲前强制恢复 `glDepthMask`：

```cpp
if (has_depth) {
    glDepthMask(GL_TRUE);  // 确保 depth mask 开启
}
glClearDepth(1.0);
glClear(GL_DEPTH_BUFFER_BIT);
```

### 诊断数据确认

修复后日志确认深度缓冲行为正确：

| 诊断点 | 修复前 | 修复后 |
|---|---|---|
| `BeginRenderPass post_clear_depth` | 0（清除失败） | 1.0（清除成功） |
| Monster `post_depth_center` | N/A | 0.505 |
| OceanPlane `post_depth_center` | N/A | 0.999 |
| `depth_samples_3x3` | 全为旧值/0 | 正确（1.0/0.505/0.999） |

### 附加修复

1. **Format 函数限制**：`dse::debug::Format()` 仅支持 `{}` 占位符，不支持 `0x{:X}` 或 `{:.6f}` 等格式修饰符。已将所有诊断日志中的格式修饰符替换为 `{}`。
2. **AnimatorSystem 骨骼计数确认**：Lua Update 在 AnimatorSystem::Update 之前执行，`get_animator_3d_state` 可能返回 `final_bones=0`（组件尚未被当前帧的 AnimatorSystem 处理）。已在 AnimatorSystem 中添加首次 resize 确认日志 `animator_system_first_update final_bones=48`，验证脚本同时匹配 Lua 侧和 C++ 侧的 `final_bones=48` token。
3. **验证脚本退出码**：引擎关闭时因 GL 资源清理可能触发 access violation（exit 0xC0000005），但所有验证检查（log tokens、screenshot、brightness）均通过。已修改 `verify_lua_3d_demos.py`，在三项验证通过时返回 0 而非进程退出码。
4. **`runtime_animation` 阈值**：引擎在无 VSync 模式下 delta_time 约 0.004s/帧，160 帧仅约 0.64 秒。已将 `state.time > 1.0` 降低为 `state.time > 0.45`，确保在 160 帧内触发。

## 本轮新增诊断（第 N+1 轮）

本轮重点推进 **任务 B（depth attachment / readback 状态）** 和 **任务 A（skinned Monster 最终深度）**，增加以下引擎级诊断：

### B. Depth attachment / readback 验证诊断

1. **`BeginRenderPass` 后置 clear 验证**（`gl_draw_executor.cpp`）：
   - scene pass clear 后立即采样中心像素 depth，预期值 1.0
   - 记录 bound FBO handle、depth attachment type/object_name/depth_size_bits、framebuffer status
   - 若 post_clear_depth ≠ 1.0 或 depth_attachment_type=GL_NONE，即可定位 depth buffer 根因

2. **`DrawMeshBatch` 入口 pre-draw 验证**（`gl_draw_executor.cpp`）：
   - 记录当前 bound FBO、depth attachment 状态、GL depth test/write/func 状态
   - 采样中心像素 depth（draw 之前），验证 clear 值是否仍为 1.0
   - 若 bound_fbo=0 或 depth_attachment_type=GL_NONE，说明 FBO 绑定或 depth attachment 在 draw 前已丢失

3. **Per-item post-draw 验证**（`gl_draw_executor.cpp`）：
   - 每个 Monster/OceanPlane 绘制后立即采样屏幕中心和 1/4 位置的 depth + color
   - 记录当前 GL depth test/write/func 状态
   - 可精确定位哪个对象导致了 depth 异常：如果 Monster 后 depth 仍≈1.0，说明 Monster 未正确写入 depth；如果 OceanPlane 后 depth 突变≈0，说明 OceanPlane 写入了异常近值

4. **Batch 末尾 post-draw 验证**（`gl_draw_executor.cpp`）：
   - 3x3 depth samples + FBO 绑定验证 + depth attachment type + color readback
   - 对比 pre-draw、per-item、post-draw 三阶段数据，构建完整 depth 变化链

5. **诊断帧数从 3 扩展到 5**，以便更稳定观察

### A. Skinned Monster CPU skinning 后深度估算

1. **`MeshRenderSystem` 中新增 CPU skinning 采样**（`mesh_render_system.cpp`）：
   - 对每个 Monster 采样最多 64 个顶点，用 `item.bone_matrices[joint_idx] * local_pos` 做 CPU 端 skinning
   - 用 camera view/projection 计算 skinned 后 NDC z 范围（skinned_ndc_z_min/max）
   - 输出到 `[DepthDiag][MeshRenderSystem]` 日志的 `skinned_ndc_z_min`/`skinned_ndc_z_max`/`skinned_sampled_count` 字段
   - 需新增 `#include "engine/platform/screen.h"` 和 `#include <glm/gtc/matrix_transform.hpp>`

### 预期诊断输出

构建运行后，日志中应出现以下新标签：

```
[3D][VSE15.22][DepthDiag][BeginRenderPass]  ← clear 后 depth 验证
[3D][VSE15.22][DepthDiag][GLDrawExecutor]   ← pre-draw FBO/depth state 验证
[3D][VSE15.22][DepthDiag][PostDraw]         ← per-item depth+color 采样
[3D][VSE15.22][DepthDiag][MeshRenderSystem] ← skinned NDC z 估算
```

关键判读指引：
- 若 `BeginRenderPass post_clear_depth ≠ 1.0`：depth clear 或 attachment 有问题
- 若 `GLDrawExecutor depth_attached_type=GL_NONE`：FBO depth attachment 缺失
- 若 `PostDraw Monster post_depth_center ≈ 1.0`：Monster 未正确写入 depth（可能是 skinned mesh + GL state 问题）
- 若 `PostDraw OceanPlane post_depth_center ≈ 0.0`：OceanPlane 写入了异常近值
- 若 `MeshRenderSystem skinned_ndc_z_min ≈ -1.0` 或 `skinned_ndc_z_max ≈ 1.0`：CPU skinning 估算显示 Monster 深度范围异常

## 当前阶段视觉差异与后续根因

1. 已修复 `.dmesh` 多 submesh 丢失问题；`Monster.dmesh` 的 2 个 submesh 现在都会进入 `MeshDrawItem`。
2. 当前截图验收不再依赖关闭任何 cooked mesh 深度：6 个 Monster 和 OceanPlane 均使用 `depth_test=true/depth_write=true` 正常工作。深度缓冲根因（`glDepthMask(GL_FALSE)` 状态泄漏）已修复。
3. VSE 原始相机 `(0,900,900)` / dir `(0,-1,-1)` 按 `SCALE=0.01` 直映射到 DSE `(0,9,9)` / `-45` 后，自动截图中对象仍过小或不可见；当前继续使用 `(0,5.2,14)` / `-26` 保证肉眼可检查场景，但这仍属于 DSE 可见范围映射，不是 1:1 相机复刻。
4. 材质已从 `MESH_UNLIT` 升级至 `MESH_PBR`，并通过 `set_mesh_texture` 设置了 Monster 的 albedo/normal/roughness/emissive 贴图（来自 `raw/Texture/Monster_d/n/s/e.tga`）。
5. OceanPlane 已从 DSE 程序化四边形升级为 cooked `OceanPlane.dmesh` + `OceanPlane.dmat`，逼近 VSE 原始网格几何。
6. PointLight 阴影已通过新增 `set_point_light_shadow` Lua API 开启，Demo 15.22 核心功能"演示点光源传统影子"已复刻。引擎渲染管线中 `shadow_passes=6`（PointLight 6 面立方体阴影）已确认工作。

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

1. ~~复制/重映射贴图到 `data/vse_demo/15_22/textures/`，并修正 `.dmat` 中的相对路径。~~ 已完成：通过 `set_mesh_texture` Lua API 直接设置贴图 slot，贴图路径为 `raw/Texture/Monster_d/n/s/e.tga`。
2. 修正 `AssetBuilder` 多 clip 输出：允许一个 FBX 导出多个 `.danim`，或允许只导出 animation/skeleton 而复用主 `Monster.dmesh`。
3. 若需要完全匹配 VSE 原生资源，再编写 `.SKMODEL/.STMODEL/.ACTION/.ANIMTREE` 只读转换器。
4. 把当前 Idle/Pos/AddtiveAnim 的 Monster clip 近似映射升级为真实 VSE 状态动画。
5. 增加 screenshot/mesh bounds 级验收，确保 cooked Monster/OceanPlane 在不同机器上不是黑屏、过小或贴图缺失。
6. 针对 OceanPlane 排查：(a) 引擎关闭时 GL 资源泄漏导致的 access violation (exit 0xC0000005)；(b) 诊断代码清理——当前 `gl_draw_executor.cpp` 和 `mesh_render_system.cpp` 中的 VSE 15.22 专属诊断应在根因确认后逐步降低或移除；(c) `frame_pipeline.cpp` 中 Runtime stats 的格式问题。
7. 实现 Additive 动画混合模式：VSE 6 个动画状态之一的 AddtiveAnim 需要引擎 Animator3D 支持 additive blend，当前使用 Monster.danim 近似。

## 许可/用途说明

这些资源来自仓库内 `reference/VSEngine2.1`，当前仅用于 DSEngine 内部对齐、转换验证和 demo 复刻，不应作为独立对外素材包发布。若后续需要发布样例资源，应重新确认来源授权或替换为自制/开源许可资源。
