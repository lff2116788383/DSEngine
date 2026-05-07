# KF_Framework 踩坑与经验总结

本文档记录 KF_Framework Demo 开发过程中遇到的问题、根因分析和修复方案，供后续开发参考。

---

## 1. 动画模型折叠（Animation Collapse）

**现象**: 启用 Animator 后，Knight (Paladin) 模型从正常 T-pose 折叠成一个点。

### 根因 1: SampleBuffer 覆盖未动画骨骼

`ApplySamplesToLocalTransforms()` 原始实现无条件覆盖所有骨骼的 `local_transform`，
即使该骨骼在 `.danim` 中没有动画通道。Paladin 的 bind-pose 动画只有 42/54 个通道，
剩余 12 根骨骼被覆盖为 `position=(0,0,0), rotation=identity, scale=(1,1,1)`，导致折叠。

**修复**: 在 `SampleBuffer` 中增加 `touched[]` 标志，`EvaluateClip` 写入数据时设置 `touched[bone]=true`，
`ApplySamplesToLocalTransforms` 跳过 `touched==false` 的骨骼。

```cpp
// animator_system.cpp
struct SampleBuffer {
    std::vector<bool> touched;  // 新增
    // ...
};

void ApplySamplesToLocalTransforms(...) {
    for (uint32_t i = 0; i < bone_count; ++i) {
        if (!sample.touched[i]) continue;  // 保留 bind pose
        local_transforms[i] = TRS(sample);
    }
}
```

### 根因 2: dskel 文件 local_transform 与 inverse_bind_matrix 不一致

`.dskel` 文件中每根骨骼同时存储了 `local_transform`（来自 `aiNode::mTransformation`）
和 `inverse_bind_matrix`（来自 `aiBone::mOffsetMatrix`）。

正常情况下：`compose(local_transforms) * inverse_bind_matrix ≈ Identity`

但 Mixamo FBX 的 Armature 节点有自己的变换（旋转/缩放），这个变换**不是一根骨骼**
（没有对应的 `aiBone`），所以不会出现在 dskel 的骨骼列表中。结果：

- `local_transform` 组合出的全局变换 **缺少** Armature 变换
- `inverse_bind_matrix` 是从 mesh 空间到骨骼空间的完整变换（包含了 Armature）
- 两者不一致：53/54 根骨骼偏差高达 95-148 单位

**影响**: 原始公式 `final = compose(local) * ibm` 在 bind pose 时不产生单位矩阵，导致模型变形。

**修复**: 改用**相对变形**公式，完全绕过 inverse_bind_matrix：

```
final_bone_matrix[i] = anim_global[i] * inverse(bind_global[i])
```

- `bind_global[i]` = 从 `local_transform` 组合出的 bind pose 全局变换
- `anim_global[i]` = 从动画 TRS 组合出的当前帧全局变换
- 在 bind pose 时 `anim_global == bind_global`，`final = Identity`（零变形）
- 动画值（从 FBX 节点树导出）与 `local_transform` 处于同一空间，兼容性正确

### 错误方案警示: 从 inverse_bind_matrix 反推 local_transform

曾尝试用 `bind_global[i] = inverse(ibm[i])` 然后 `local[i] = inverse(bind_global[parent]) * bind_global[i]`
来重建 bind pose。这个方案**看似正确但实际有害**：

- 动画 TRS 值是从 FBX 节点树（`aiNode::mTransformation`）导出的
- 这些值与 dskel 的 `local_transform` 处于**同一空间**
- 用 ibm 反推的 local_transform 处于**不同空间**
- 混合两个空间的数据会导致动画变形错误

**结论**: 不要修改 bind pose 的空间基底，而是改用相对变形公式绕过 ibm 不一致问题。

### 根因 3: 骨骼非拓扑排序

Mixamo FBX 导出的骨骼索引不保证**父在子前**（topological order）。
例如 Paladin 骨骼层级：`bone[0].parent=1, bone[1].parent=2, ..., bone[3].parent=51`，
根骨骼 Hips 的索引为 51。

原代码假设前向循环即可计算 `global[i] = global[parent] * local[i]`，
但当 parent > i 时，`global[parent]` 尚未计算，产生错误结果。

**修复**: 使用**迭代传播**替代简单前向循环：

```cpp
// 先标记根骨骼
for (i = 0; i < bone_count; ++i) {
    if (parent < 0) { globals[i] = local[i]; computed[i] = true; }
}
// 逐层传播直到所有骨骼计算完毕
for (pass = 0; pass < bone_count; ++pass) {
    for (i = 0; i < bone_count; ++i) {
        if (!computed[i] && computed[parent]) {
            globals[i] = globals[parent] * local[i];
            computed[i] = true;
        }
    }
}
```

---

## 2. 骨骼不匹配（Skeleton Mismatch）— 已通过名称重映射解决

**现象**: 使用 "Sword And Shield *.danim" 动画文件时模型严重折叠。

**根因**: "Sword And Shield" 系列动画使用 52 骨骼（不同骨骼排序），
而 Paladin mesh 的 `.dskel` 有 54 根骨骼。动画通道的 `target_node_index`
指向错误的骨骼，导致变换被应用到完全不相关的骨骼上。

**诊断**: 使用 `tools/check_dskel.py` 对比两个 dskel 的骨骼数和名称。

**解决方案 — 骨骼名称重映射（v2 格式）**:

扩展 `.dskel` 和 `.danim` 二进制格式到 v2，在运行时按骨骼名称匹配通道：

1. **dskel v2**: `BoneDesc[]` 之后追加骨骼名称表（per-bone `uint16_t len + chars`）
2. **danim v2**: `AnimChannelDesc[]` 之后插入通道名称表（`uint32_t total_size` + per-channel `uint16_t len + chars` + 8 字节对齐填充）
3. **运行时**: 解析 dskel 名称表构建 `bone_name_to_index` 映射，EvaluateClip 中按通道名称查找目标骨骼索引，未匹配的通道自动跳过

```
dskel v2 布局: [SkelHeader] [BoneDesc × N] [名称表: (u16 len + chars) × N]
danim v2 布局: [AnimHeader] [AnimChannelDesc × N] [名称表: u32 total + (u16 len + chars) × N + padding] [关键帧数据]
```

**关键文件**:
- `engine/assets/compiler/raw_scene_data.h` — `RawAnimationChannel.target_node_name`、版本号 → 2
- `engine/assets/compiler/importer.cpp` — `CookToDanim`/`CookToDskel` 写入名称表
- `modules/gameplay_3d/animation/animator_system.cpp` — 解析名称表 + 重映射逻辑

---

## 3. v2 格式对齐崩溃（Name Table Alignment）

**现象**: 使用跨骨骼动画时 Release 模式间歇性 ACCESS_VIOLATION (0xC0000005)，Debug 模式不崩溃。

**根因**: danim v2 名称表大小可变（取决于骨骼名称长度总和），如果名称表字节数不是 4/8 的倍数，
后续关键帧数据的起始偏移不对齐。例如 SaS Idle 名称表为 726 字节 → 关键帧起始偏移 3238，
`3238 % 4 = 2`，导致 `reinterpret_cast<const float*>` 产生未对齐指针。

- Debug 模式不使用 SIMD → 不崩溃
- Release 模式 MSVC 自动向量化生成 SSE 指令 → 要求对齐 → 崩溃

**修复（两层防御）**:

1. **写入端对齐**: `CookToDanim` 中名称表末尾填充到 8 字节边界
   ```cpp
   while (name_table_blob.size() % 8 != 0)
       name_table_blob.push_back(0);
   ```
2. **读取端 memcpy**: `EvaluateClip` 中用 `memcpy` 替代 `reinterpret_cast` 读取关键帧数据，
   彻底消除未对齐指针的未定义行为
   ```cpp
   // 安全读取（替代 reinterpret_cast 迭代器构造）
   std::vector<glm::vec3> positions(key_count);
   std::memcpy(positions.data(), data + offset, key_count * sizeof(glm::vec3));
   ```
3. **偏移量边界检查**: 读取前验证所有偏移量 + 数据大小不超过文件尺寸

**教训**: 在二进制格式中插入可变长度段时，必须保证后续数据的对齐。对 `float`/`vec3`/`quat`
至少 4 字节对齐，对 SIMD 安全建议 8 或 16 字节对齐。

---

## 4. 诊断工具使用指南

| 工具 | 用途 |
|------|------|
| `tools/check_dskel.py <file.dskel>` | 查看骨骼数、父子关系、名称 |
| `tools/check_danim.py <file.danim>` | 查看动画头信息、通道数和目标骨骼 |
| `tools/compare_skeletons.py <a.dskel> <b.dskel>` | 对比两个骨骼文件的拓扑和 ibm |
| `tools/verify_scene.py --frames N` | 自动截图并分析渲染结果 |

---

## 5. 构建注意事项

- **CMake 目标名**: `dse_standalone`（不是 `DSEngine_Game`）
- **输出路径**: `bin/DSEngine_Game_release.exe`
- **强制重编译单文件**: 需要 `touch` 源文件或清理 obj，增量构建有时不重编
- **链接警告**: `lua_error` / `luaL_argerror` 的 LNK4217/4286 是已知无害警告

---

## 6. 资产来源与重新获取

以下资产目录已被 gitignore，不入库。需要时按来源重新获取。

| 目录 | 内容 | 来源 |
|------|------|------|
| `assets/fbx/knight/` | Paladin 骑士 FBX + 动画 (36 MB) | [Mixamo](https://www.mixamo.com/) — 搜索 "Paladin J Nordstrom"，下载 FBX with skin；动画搜索 "Sword And Shield" 系列 |
| `assets/fbx/mutant/` | Mutant 怪物 FBX + 动画 (36 MB) | [Mixamo](https://www.mixamo.com/) — 搜索 "Mutant"，动画搜索 "Mutant Idle/Run/Punch" 等 |
| `assets/fbx/zombie/` | Zombie 僵尸 FBX + 动画 (55 MB) | [Mixamo](https://www.mixamo.com/) — 搜索 "Zombie"，动画搜索 "Zombie Idle/Walk/Run" 等 |
| `assets/fbx/unityChan/` | Unity-Chan 角色 (37 MB) | [Unity Technologies](https://unity-chan.com/) — Unity-Chan model |
| `assets/fbx/erika/` | Erika 角色 (20 MB) | Mixamo |
| `assets/fbx/jugg/` | Juggernaut 角色 (20 MB) | Mixamo |
| `assets/fbx/kachujin/` | Kachujin 角色 (15 MB) | Mixamo |
| `assets/fbx/maria/` | Maria 角色 (31 MB) | Mixamo |
| `assets/fbx/*.fbx` (散装) | 场景道具、武器、建筑 (~13 MB) | 各免费 3D 资源站 (Sketchfab, TurboSquid 等)，原始来源见 KF_Framework 仓库 |
| `assets/raw/` | KF_Framework 原始二进制资产 (25 MB) | [KodFreedom/KF_Framework](https://github.com/KodFreedom/KF_Framework) `data/` 目录 |
| `assets/textures/` | 角色纹理 (62 MB) | 与 FBX 同源（Mixamo 导出包含纹理）或 KF_Framework `data/texture/` |
| `assets/audio/` | BGM + SE (41 MB) | KF_Framework `data/sound/` 目录 |
| `cooked/` | DSEngine 格式资产 (21 MB) | 由 AssetBuilder 从 `assets/fbx/` 生成，可用 `tools/batch_convert.py` 重新转换 |

---

## 7. 资产管线注意事项

- **dmat 路径问题**: AssetBuilder 生成的 `.dmat` 包含绝对路径，运行时可能无法找到纹理。
  解决方案：在 Lua 中手动绑定纹理（`set_mesh_material` + `set_mesh_texture`）
- **AnimChannelDesc 大小**: 48 字节（包含 4 个 `uint64` 偏移），不是 44 字节
- **FBX 导入坐标系**: Mixamo FBX 使用厘米单位，角色约 172 单位高（KF 原始约 1.7 米）

---

## 8. 关键代码位置

| 文件 | 关键内容 |
|------|----------|
| `modules/gameplay_3d/animation/animator_system.cpp` | AnimatorSystem::Update — 动画求值、骨骼名称重映射和骨骼矩阵计算 |
| `engine/assets/compiler/raw_scene_data.h` | AnimChannelDesc / BoneDesc / SkelHeader / AnimHeader 结构体定义 |
| `engine/assets/compiler/importer.cpp` | CookToDanim / CookToDskel — v2 格式写入（含名称表和对齐填充） |
| `examples/KF_Framework/script/main.lua` | Demo 入口脚本，相机/灯光/模型/动画配置 |
