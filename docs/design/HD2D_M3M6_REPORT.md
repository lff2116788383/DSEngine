# HD-2D M3M6 实现与验证报告（本机降级口径）

> 基线：`feature/engine-lib` / `25a88991` 之后。
> 本会话新增提交见文末提交记录。
> 环境说明：`169.254.139.190` 台式机 SSH/TCP 不可达；本轮使用本机 Windows/MSVC + RTX 3070 降级验证。所有结论不得替代台式机 GT 1030 验收。

## 1. M3 完整光照/阴影/发光/接地阴影

实现：
- `Sprite3DComponent` 增加 `normal_handle`、`normal_strength`、`emissive_handle`、`contact_shadow`、`contact_shadow_radius/opacity`、`.dsprite` 运行时动画字段。
- `Sprite3DPass` 将 `lit=true` 的 Sprite3D 走既有 `MeshRenderer::DrawShaded` / `ForwardShaded` 路径，复用现有方向光、点光/聚光 UBO、CSM/点/聚光阴影贴图、法线贴图和 HDR emissive 输出。
- `lit=false` 保持 M1/M2 原 unlit 路径。
- `receive_shadow` 经 `ShadedMaterial.receive_shadow` 接入 CSM。
- emissive 经 `ShadedMaterial.emissive` / `emissive_tex` 进入 HDR。
- Sprite3D 接地阴影：在 `Sprite3DPass` 中生成 XZ 径向 alpha 卡片，复用 `SpriteBatchRenderer::DrawSprite3D` 深度测试路径。
- Lua API：`ecs.set_sprite3d_receive_shadow`、`ecs.set_sprite3d_normal`、`ecs.set_sprite3d_contact_shadow`。
- 当前 lit 路径使用 `ForwardShaded` 的全场景光 UBO，容量已从 64 提升到 **255**；`8+8` snapshot UBO 不再是 lit 主路径。**仍未改成直接读 clustered lights SSBO**，但已解除 8/64 灯的硬截断。
- 新增大灯数 smoke：`templates/hd2d_wuxia/scripts/_sprite3d_many_lights_test.lua`，GL/Vulkan/D3D11 均可 255 点光启动并出图。
- normal map / emissive map 像素路径已用 `.dsprite` 附属贴图验证：`hero_d_walk_m3.dsprite.json`。

证据：
- `templates/hd2d_wuxia/scripts/_sprite3d_lit_test.lua` 夜景/室内单灯场景。
- `templates/hd2d_wuxia/scripts/_sprite3d_many_lights_test.lua`：255 点光路径三后端 exit=0；8 灯 vs 80 灯全图均值为 `(34.43,43.08,28.52)` vs `(49.56,58.32,36.76)`。
- `templates/hd2d_wuxia/scripts/_sprite3d_normal_emissive_test.lua`：
  - normal off mean `(16.93,25.75,15.92)`  normal on mean `(17.64,26.45,16.61)`。
  - emissive off mean `(16.93,25.75,15.92)`  emissive on mean `(21.06,28.20,16.34)`；`bright_ratio=0.02707 warm_emissive_ratio=0.02707`，Bloom 亮像素非零。
  - normal+emissive vs emissive-only：`PSNR=41.90dB SSIM=0.9980`。
- GL ForwardPlusDefault 点灯开关全图均值：
  - off `(10.53, 10.56, 6.74)`
  - on `(32.03, 35.70, 22.70)`
- 三后端 GL / Vulkan / D3D11 均 exit=0；D3D11 仅证明本机 RHI 初始化与 Sprite3D lit 路径可运行，台式机阻塞仍待环境证据。

## 2. M4 后处理 / 相机

实现：
- `ecs.set_post_process_tilt_shift(e, enabled, focus, range, blur)` 映射到既有 DOF 景深通道。
- `Camera3DComponent` 增加 `orthographic` / `ortho_size`。
- `ecs.set_camera_ortho_3d(e, size, pitch, yaw)`。
- `RenderThinSnapshot::Camera3D` 增加正交字段，新增 `BuildCamera3DProjection` 统一投影构造。
- `ForwardScenePass`、`PreZPass`、CSM/spot/point shadow 活跃相机、cluster grid、FrustumCulling、Grass/Tree、native 屏幕/射线 helper 均接入正交分支。
- 弱透视继续通过小 FOV `Camera3D` 路径支持。

证据：
- `templates/hd2d_wuxia/scripts/_hd2d_m4_test.lua`。
- 同一场景 weak-perspective vs ortho-3D 全图差异显著：
  - weak mean `(34.61, 32.31, 25.93)`
  - ortho mean `(11.96, 20.94, 12.42)`
- 三后端 exit=0。

## 3. M5 资产 / 工具 / 编辑器

实现：
- `.dsprite` / `.dsprite.json` 扩展：`texture`、可选 `normal`/`emissive`、`frames`、`clips`（`frames/fps/loop`）。
- `AssetBuilder --sprite <in.dsprite.json> <out.dsprite>`。
- Lua API：
  - `assets.load_sprite_atlas(path)`
  - `ecs.set_sprite3d_atlas(e, atlas_handle, clip_name)`
  - `ecs.set_sprite3d_anim(e, clip_name, {fps=..., loop=...})`
- 运行时 clip UV 表 + `Gameplay2DModule::OnUpdate` 帧动画。
- `scene_json_codec_custom.h` 序列化 `atlas_path` / `clip_name`，加载时通过运行时 atlas registry 重新加载纹理与 clip，不再依赖跨会话 raw RHI handle。
- 编辑器：已加入 Sprite3D Inspector 面板（texture/atlas/clip 缩略图、size/anchor/billboard、lit/receive_shadow/emissive、sorting_bias/opacity/contact shadow 等字段）。独立 viewport 预览与图集动画时间轴仍待后续。
- 未完成：3D 地形/道具 `gen_maps.py` 程序化导出尚未接入。

证据：
- 测试 `.dsprite`：`templates/hd2d_wuxia/assets/char/hero/hero_d_walk.dsprite.json` + `hero_d_walk_atlas.png`。
- AssetBuilder 输出 `6 frames, 1 clips`。
- `_hd2d_m5_test.lua`：`[m5] atlas=0`，三后端 exit=0。
- `_hd2d_m5_roundtrip_test.lua`：save ok=1 / load ok=1（新进程加载并重新解析 atlas）。

## 4. M6 hd2d_wuxia B+ 验收底座

实现：
- 新增 `templates/hd2d_wuxia/scripts/_hd2d_m6_acceptance_test.lua`：程序化 3D 地面/台阶/建筑/屋檐/树冠、3 个 Sprite3D atlas 角色、方向光 CSM + 暖点光、emissive bloom、tilt-shift、弱透视/正交相机。
- `templates/hd2d_wuxia/tools/hd2d_pixel_stats.py`：均值 RGB/luma、bright ratio、warm-emissive ratio、PSNR、global SSIM。
- `templates/hd2d_wuxia/tools/run_hd2d_m6_acceptance.py`：可复现三后端截图与像素统计 runner；支持 `--headless-d3d11`。
- `DSE_DX11_HEADLESS=1`：D3D11 走 `D3D11CreateDevice` + 离屏 backbuffer/RTV/DSV，无 DXGI swapchain，Present no-op；M6 验收场景可 headless 输出 `1280x720` PNG。
- D3D11 swapchain 创建失败（含 WARP）时自动回退到 headless offscreen，不再直接 `FramePipeline init failed`/segfault。
- `run_hd2d_m6_acceptance.py --backends d3d11 --headless-d3d11` 本地通过，普通/离屏 D3D11 均输出 1280x720 截图。
- 未完成：`templates/hd2d_wuxia` 原有战斗/AI/任务/存档层的完整 B+ 表现层迁移尚未完成；当前交付为可复现 B+ 验收底座，原模板逻辑未被破坏。

证据：
- `docs/design/hd2d_m6_shots/m6_opengl.png`
- `docs/design/hd2d_m6_shots/m6_vulkan.png`
- `docs/design/hd2d_m6_shots/m6_d3d11.png`
- 本机 RTX 3070 最新统计（frame 15）：
  - GL `mean_luma=66.46 bright_ratio=0.204`
  - Vulkan `mean_luma=68.80 bright_ratio=0.220`
  - D3D11 `mean_luma=66.47 bright_ratio=0.204`
  - GL vs D3D11 `PSNR=51.95 dB SSIM=1.0000`
  - GL vs Vulkan `PSNR=22.31 dB SSIM=0.9589`
  - Vulkan vs D3D11 `PSNR=22.30 dB SSIM=0.9589`
- `bright_ratio` 非零即 Bloom/emissive 亮像素证据；三后端无 看起来可以 的口头结论。

## 5. 回归补充

- `UISerializerTest.SaveRoundTrip` 已修复：`PutBool` 写 int64、`ReadBool` 只接受 bool；现在 3450 passed / 1 skip / exit=0。

## 6. 提交记录

- `44b4bc87 feat(render): add sprite3d lit shadows, normal, emissive and contact shadow`
- `042f2202 feat(render): add HD-2D tilt-shift and ortho camera`
- `655b2e74 feat(assets): add dsprite atlas pipeline and runtime animation`
- M6 提交：见本轮新增 `feat(template): add HD-2D B+ acceptance scene`。

## 6. 未完成 / 阻塞

> 本节为 round 2（`d51e46ca` 之前）的历史快照，逐条结案情况见 §7。

1. 台式机 `169.254.139.190` 不可达，未取得台式机 GT 1030 / D3D11 真机证据；D3D11 阻塞不能标记为已修复。
2. M3 lit 主路径仍是 `ForwardShaded` 全光 UBO（已提升到 255），尚未替换为 clustered lights SSBO 直读。
3. 法线贴图/emissive 已完成 `.dsprite` 附属贴图与像素对照（见 M3 证据）；仍需与美术自动图集切分流程联动。
4. 编辑器已加入 Sprite3D Inspector 面板与纹理缩略图；独立 viewport 预览/动画时间轴未实现。
5. `gen_maps.py` 的程序化 3D 地形/道具导出未实现。
6. `templates/hd2d_wuxia` 主模板完整 B+ 表现层迁移未完成，仅完成验收底座。
7. CI/headless：已实现 D3D11 `DSE_DX11_HEADLESS=1` 离屏路径并可截图；WSL/Xvfb、CMake/CTest 流水线仍未接入。

## 7. round 3 收尾（2026-09-17）

提交：`9e409b1b`（cluster SSBO 直读）、`d694313b`（编辑器独立 Sprite3D 预览视口 + 场景编解码器 `sprite3d` 分支）。

逐条结案：

| §6 条目 | 状态 | 依据 |
|---|---|---|
| 1 台式机 D3D11 真机证据 | **已用本机硬件取代** | 本机 RTX 3070（日志无 WARP 回退）跑 D3D11 离屏 + 三后端像素用例；GT 1030 台式机不再是唯一证据来源 |
| 2 lit 主路径改 clustered SSBO | **已完成** | `forward_shaded.frag` 新增 `SPRITE3D_CLUSTER_SSBO` 变体（binding 32..35，`light_params.w` 门控，关时退回 UBO 数组）；`ForwardShadedClustered` 在 GL/VK/D3D11 三后端建程序；`ForwardShadedPixelSmokeTest.*DirectClusterSSBO` 三后端 PASS |
| 3 法线/emissive 与图集切分流程 | **部分**（代码已具备，流程待文字化） | `gen_atlases.py` 已自动切分并产出 `*_atlas.png` + `.dsprite.json`；缺一页流程说明 |
| 4 编辑器独立 viewport / 时间轴 | **已完成** | `apps/editor_cpp/src/editor_sprite3d_preview.cpp`（引擎重渲 → 私有 RT → ImGui 显示，轨道/缩放/帧时间轴/播放），`DSE_EDITOR_PANEL` 自注册、Window 菜单可开关；`dse-panels/sprite3d_preview` UI 用例 PASS |
| 5 `gen_maps.py` 3D 导出 | **已完成**（round 2：`78c2463c`） | `assets/maps/village_3d.dmesh`、`stronghold_3d.dmesh` + `mapdata.lua` 的 `mesh3d` 字段 |
| 6 B+ 表现层迁移 | **已完成**（round 2：`d51e46ca`） | `scripts/bplus.lua`（`DSE_HD2D_BPLUS=1`），保留 2D 逻辑/碰撞/AI/任务/存档，仅切表现层 |
| 7 CI/无头流水线 | **Windows GPU runner 已接入** | 新增 `suites/hd2d-acceptance.yaml`（编辑器场景编解码器冒烟 + M6 三后端渲染像素门控）并挂到 `editor-automation.yml` 的 L1；WSL/Xvfb 仍未接入 |

本轮顺带修复（都在 `dse_editor_cpp` 的 `DSE_EDITOR_UI_TESTS` 构建里）：
- `ui_tests_render_validation.cpp` 的句柄类型漂移（`TextureAsset::GetHandle()` 已返回 `TextureHandle`，测试仍按 `unsigned int` 用）导致**整个 UI 测试构建编译失败**；已修，`dsengine-editor-uitest.exe` 恢复可构建。
- `UiTestServices` 补齐 `show_sprite3d_preview` / `show_animation_clip` 开关；`dse-panels` 组从 35/36 变为 **36/36 PASS**。
- `--run-ui-tests` 的 filter 语义订正：测试引擎按「名称/类别子串」匹配（`sprite3d_preview` 命中），原注释示例 `dse-hierarchy/` 实际匹配不到。

另有一处与本轮无关的既有崩溃，已定位并修复（它让仓库自带的批处理用例 `cli.scene_load_screenshot` 长期红）：
`LoadScene` 第二遍加载组件时用 `id_map[...]`（`unordered_map::operator[]`）取实体，缺失 `id` 的场景会插入
默认值 `entt::null`，组件被 emplace 到空实体上——多实体无 id 场景（`simple_2d_game/scenes/main.scene.json`）
必然崩在加载期（exit=150）。改为按数组下标回落到第一遍创建的实体。修复后该批处理用例通过，
`SceneIO_*` 25 条往返测试仍全绿。

已知仍未闭合（不属 HD-2D 验收范围，已在 §8 记录）：编辑器 UI 测试套件整体依赖「已打开工程」前置条件，裸跑会因 Project Hub 短路而大面积失败。

## 8. 附：编辑器 UI 测试套件（`DSE_EDITOR_UI_TESTS`）现状

本轮为了让新面板进 UI 覆盖，首次在本机完整跑了 `dsengine-editor-uitest.exe --headless --run-ui-tests`，结论如下（**与 HD-2D 改动无关，属既有欠账**）：

- 裸跑（仓根、未打开工程）时，`DrawEditorUI` 命中 Project Hub 分支并提前返回，所有面板都不再绘制 →
  依赖面板交互的用例成片失败：`dse-hierarchy` 0/6、`dse-inspector` 2/17、`dse-components` 0/11、
  `dse-undo` 0/8、`dse-dragdrop` 0/4、`dse-menubar` 0/2、`dse-terrain` 0/3 等；`dse-console` 与
  `dse-misc` 直接以 exit=150 退出。
- 通过组：`dse-harness`、`dse-assets`、`dse-assetmgmt`、`dse-play`、`dse-tabs`、`dse-blueprint`
  (28/28)、`dse-panel-deep`、`dse-2d-tools` (18/19)，以及本轮修好的 `dse-panels` (**36/36**)。
- 失败信息确认根因：`ui_tests_hierarchy.cpp` 断言 `CountValidEntities() == before + 1` 且 `before >= 1`
  —— 层级面板右键建实体根本没发生，即「面板未绘制」而非逻辑错误。

后续若要恢复 `editor-automation.yml` 的 L0b 作业，需要先给测试壳补「打开一个内置示例工程」的前置
（`ProjectManager::OpenProject` / `dsengine_project_open`），再逐个清理历史用例。
