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

- **CMake 目标名**: `dse_standalone`（不是 `dsengine_game`）
- **输出路径**: `bin/dsengine_game_release.exe`
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

---

## 9. EnTT 跨 DLL 模板实例化崩溃（ACCESS_VIOLATION / fast_mod 断言）

**现象**: Release 模式下 KF_Framework Demo 启动时 ~20% 概率崩溃，
异常为 `ACCESS_VIOLATION (0xC0000005)` 或 EnTT `fast_mod` 中 `is_power_of_two(mod)` 断言失败。

### 根因

`DSE_Gameplay3D.dll` 与 `dse_engine.dll` 各自独立实例化 EnTT 头文件模板
（`dense_map`/`sparse_set`/`basic_registry` 等）。配合 `WINDOWS_EXPORT_ALL_SYMBOLS ON`，
Windows loader 非确定地将同名模板符号解析到不同 DLL 的副本，导致：

1. 一个 DLL 中的内联函数调用另一个 DLL 中的 `bucket_count()`
2. 内存布局/内联决策不一致，访问到错误的字段偏移
3. `bucket_count()` 返回非 2 的幂值 → `fast_mod` 断言失败
4. 或直接读到垃圾内存 → ACCESS_VIOLATION

**修复**: 将 `DSE_Gameplay3D` 所有源文件静态编入 `dse_engine`，彻底消除 DLL 边界。

```cmake
# CMakeLists.txt — DSE_ENABLE_3D=ON 时
file(GLOB_RECURSE gameplay_3d_cpp "modules/gameplay_3d/*.cpp")
list(APPEND engine_cpp ${gameplay_3d_cpp})
# 不再构建 DSE_Gameplay3D.dll
```

**关键修改文件**:

| 文件 | 修改内容 |
|------|----------|
| `CMakeLists.txt` | 移除 `add_library(DSE_Gameplay3D SHARED)`，所有 gameplay_3d 源文件编入 engine_cpp |
| `engine/runtime/frame_pipeline.h` | `#ifdef DSE_ENABLE_3D` 时添加 `Gameplay3DModule gameplay3d_module_` 成员 |
| `engine/runtime/frame_pipeline.cpp` | DLL 加载代码替换为直接调用 `gameplay3d_module_.OnInit/OnShutdown` |
| `engine/runtime/runtime_update_graph.cpp` | Update/FixedUpdate 中直接调用 `gameplay3d_module_` |
| `modules/gameplay_3d/gameplay_3d_module.cpp` | 移除 `CreateModule`/`DestroyModule` DLL 工厂函数 |
| `engine/runtime/engine_app.cpp` | `scene::Foo` → `::scene::Foo`（修复传递包含引入的命名空间歧义） |

**效果**: 崩溃率从 ~20% 降至 ~10%（DLL 边界消除后仍有残留崩溃，见下节）。

**教训**: EnTT 等头文件库在跨 DLL 场景下极易产生 ODR 违规和模板实例化不一致。
除非明确需要热重载，3D 模块应直接编入引擎主库。

---

## 10. RenderGraph 并行执行导致堆破坏（多线程竞争条件）

**现象**: 完成 DLL 边界修复后仍有 ~10% 崩溃率（Release），~2% 崩溃率（Debug）。
Release 崩溃为 `0xC0000374: 堆已损坏`，Debug 崩溃为 MSVC 断言 `vector iterators incompatible`。

### 根因

`RenderGraph::ExecuteParallel()` 将同一波次的多个渲染 Pass 通过 `JobSystem` 并行执行。
`PointShadowPass::Execute()`、`CSMShadowPass::Execute()` 等 Pass 内部调用
`registry.view<TransformComponent, PointLightComponent>()` 访问 EnTT registry。

EnTT 的 `basic_registry` **非线程安全**：`registry.view<>()` 内部调用 `assure<T>()`，
该函数在组件类型首次使用时会向 `pools_` vector 执行 `push_back`（懒初始化）。
多个工作线程并发调用 `assure()` 时，vector 重新分配导致迭代器/指针失效。

**完整崩溃调用栈**:
```
JobSystem::WorkerThread(int index)                       ← 工作线程
  → RenderGraph::ExecuteParallel::lambda()               ← 并行任务
    → PointShadowPass::Execute(cmd_buffer) 行 182        ← 渲染 Pass
      → registry.view<Transform, PointLight>()           ← 访问 ECS
        → registry.assure<PointLightComponent>(id) 行 266
          → sparse_set::type() 行 1090                   ← this = 0xFFFFFFFFFFFFFFB7 💥
```

**Debug 模式 MSVC 迭代器检查**直接捕获了竞争：`Expression: vector iterators incompatible`。

### 为什么 Debug 崩溃率极低

Debug CRT 堆有额外保护页和填充字节，使得并发重新分配后旧指针碰巧仍指向可读内存的概率更高。
Release 堆紧凑分配，旧指针更容易命中已释放的页面。

### 修复

将 `ExecuteParallel` 替换为串行 `Execute`：

```cpp
// engine/runtime/frame_pipeline.cpp
void FramePipeline::ExecuteRenderGraphInternal(CommandBuffer& cmd_buffer) {
    // 渲染 Pass 的 Execute() 内部通过 registry.view<>() 访问 ECS，
    // 而 EnTT registry 非线程安全（assure() 可能触发 pools_ 重新分配）。
    // 在实现 Pass 数据隔离（预缓存 view 结果）之前，使用串行执行保证正确性。
    render_graph_dag_.Execute(cmd_buffer);
}
```

**效果**: Release 连续 100 次运行 0 崩溃（之前 ~10%）。

### 性能影响

**对当前 OpenGL 后端无影响**：

- `ExecuteParallel` 本身只对 `OpenGLCommandBuffer` 生效（DX11/Vulkan 原本就走串行 `Execute`）
- OpenGL 是单线程 API，"并行录制"只是在 CPU 侧缓存命令列表，最终仍在主线程提交
- 命令录制本身开销极小（<0.1ms），不是帧时间瓶颈

### 未来恢复并行的条件与方案

**何时值得做并行**:
- 切换到 **Vulkan/DX12** 后端（真正支持多线程命令录制，各有独立 CommandPool）
- 场景规模达到 **50+ 渲染 Pass** 且 CPU 录制耗时 >2ms

**安全并行实现方案**（预计工作量 1-2 天）:

在 `ExecuteParallel` 之前，**主线程预构建所有 view**，将结果缓存传入 Pass：

```cpp
// 主线程（安全）
struct PassViewCache {
    entt::view<TransformComponent, PointLightComponent> point_lights;
    entt::view<TransformComponent, SpotLightComponent>  spot_lights;
    entt::view<TransformComponent, DirectionalLightComponent> dir_lights;
    entt::view<TransformComponent, MeshRendererComponent> meshes;
};
PassViewCache cache;
cache.point_lights = registry.view<TransformComponent, PointLightComponent>();
// ... 预构建其他 view

// 工作线程：Pass 只使用 cache（只读迭代，EnTT 保证安全）
void PointShadowPass::Execute(CommandBuffer& cmd, const PassViewCache& cache) {
    for (auto entity : cache.point_lights) { ... }
}
```

这样 `assure()` 在主线程完成（安全），工作线程只做只读迭代（EnTT 保证线程安全）。

---

## 11. 调试经验总结

### Release 崩溃无 PDB 符号

默认 Release 构建不生成 PDB，调用栈只显示地址。解决方案：
- **方法 A**: 用 Debug 版本复现（有完整 PDB + MSVC 迭代器检查）
- **方法 B**: 用 `RelWithDebInfo` 构建（Release 优化 + PDB）
- **方法 C**: 为 Release 添加 `/Z7` 编译选项

### 自动化稳定性测试脚本

```powershell
$env:DSE_MAX_FRAMES = "5"
$env:DSE_STARTUP_SCENE_REGRESSION = "0"
$crashed = 0; $ok = 0
for ($i = 1; $i -le 50; $i++) {
    $proc = Start-Process -FilePath "bin\dsengine_game_release.exe" `
        -ArgumentList "--script=examples\KF_Framework\script\main.lua" `
        -PassThru -WindowStyle Hidden
    $exited = $proc.WaitForExit(15000)
    if (-not $exited) { $proc.Kill(); $crashed++ }
    elseif ($proc.ExitCode -ne 0) { $crashed++ }
    else { $ok++ }
}
Write-Host "OK=$ok Crashed=$crashed"
```

### Debug 模式的迭代器检查（_ITERATOR_DEBUG_LEVEL）

MSVC Debug 模式自动启用 `_ITERATOR_DEBUG_LEVEL=2`，会在以下场景立即断言：
- 使用一个 vector 的迭代器操作另一个 vector
- 迭代器指向已析构/已 move 的容器
- 迭代器越界

这是定位 Release 堆破坏 (`0xC0000374`) 根因的最有效工具——
**同一个 bug 在 Debug 下触发迭代器断言，在 Release 下触发堆损坏**。
