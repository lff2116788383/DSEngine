# DSEngine 资源清单 (Asset Manifest)

> 本文件为 `data/` 目录下所有运行时资源的清单与来源记录。
> 引擎运行时只读取 `data/` 下的资源，不得直接依赖 `reference/` 路径。

## 目录结构

```
data/
├── models/                   # 3D 模型与材质
│   ├── cube.dmesh            # 标准立方体网格（引擎内置生成）
│   ├── cube.dmat             # 标准立方体 PBR 材质
│   └── CesiumLogoFlat.png    # 贴图样本
├── animation/                # 骨骼动画资源
│   └── minimal_rig/          # 最小双骨骼测试资源包
│       ├── two_bone.dmesh
│       ├── two_bone.dmat
│       ├── two_bone.dskel
│       └── two_bone_idle_walk.danim
├── audio/                    # 音频资源
│   └── spatial/              # 3D 空间音频
│       └── spatial_ping.wav  # 最小循环提示音
├── vse_demo/                 # VSEngine 参考场景资源
│   └── 15_22/                # VSE 15.22 综合场景
│       ├── *.dmesh / *.dmat  # Monster 角色 + OceanPlane 网格/材质
│       ├── *.danim           # Idle/Walk/Attack/Attack2/Pos/AddtiveAnim 动画
│       ├── *.dskel           # Monster 骨骼
│       └── *.tga             # 角色贴图
├── terrain/                  # 地形资源
│   ├── heightmap_ridge.bmp   # 山脊高度图
│   └── grass_rock.bmp        # 草地岩石贴图
├── font/                     # 字体资源
│   ├── hkyuan.ttf            # 华康圆体字体
│   └── bitmap_font.png       # 位图字体图集
└── mirror_assets/            # 镜像/运行时生成资源
```

## 资源清单

### models/

| 文件 | 格式 | 用途 | 关联 Demo | 来源 |
|------|------|------|-----------|------|
| `cube.dmesh` | DSEngine Mesh | 标准立方体网格 | `3d_static_model`, `3d_textured_cube`, `3d_material_showcase` 等 | 引擎工具链生成 |
| `cube.dmat` | DSEngine Material | 标准 PBR 材质 | 同上 | 引擎工具链生成 |
| `CesiumLogoFlat.png` | PNG | 贴图样本 | `3d_textured_cube` | 第三方（Cesium） |

### animation/minimal_rig/

| 文件 | 格式 | 用途 | 关联 Demo | 来源 |
|------|------|------|-----------|------|
| `two_bone.dmesh` | DSEngine Mesh | 双骨骼测试网格 | `3d_animation_basic`, `3d_character_third_person` | 引擎工具链生成（最小测试骨骼） |
| `two_bone.dmat` | DSEngine Material | 双骨骼测试材质 | 同上 | 引擎工具链生成 |
| `two_bone.dskel` | DSEngine Skeleton | 双骨骼定义 | 同上 | 引擎工具链生成 |
| `two_bone_idle_walk.danim` | DSEngine Animation | Idle + Walk 双 clip 动画 | 同上 | 引擎工具链生成 |

### audio/spatial/

| 文件 | 格式 | 用途 | 关联 Demo | 来源 |
|------|------|------|-----------|------|
| `spatial_ping.wav` | WAV (PCM) | 最小循环提示音，用于 3D 空间音频验收 | `3d_audio_spatial` | 程序化生成（1kHz 正弦波脉冲） |

### terrain/

| 文件 | 格式 | 用途 | 关联 Demo | 来源 |
|------|------|------|-----------|------|
| `heightmap_ridge.bmp` | BMP | 山脊高度图 | `3d_terrain_heightmap` | 引擎工具链 / 程序化生成 |
| `grass_rock.bmp` | BMP | 草地岩石贴图 | `3d_terrain_heightmap` | 引擎工具链 / 程序化生成 |

### font/

| 文件 | 格式 | 用途 | 关联 Demo | 来源 |
|------|------|------|-----------|------|
| `hkyuan.ttf` | TTF | 华康圆体字体 | 2D UI 文本渲染 | 第三方字体 |
| `bitmap_font.png` | PNG | 位图字体图集 | 2D UI 文本渲染 | 引擎工具链生成 |

### vse_demo/15_22/

| 文件 | 格式 | 用途 | 关联 Demo | 来源 |
|------|------|------|-----------|------|
| `Monster.dmesh` (5份) | DSEngine Mesh | Monster 角色 + OceanPlane 网格 | `3d_vse15_22_scene` | 从 `reference/VSEngine2.1/Demo/15/15.22` 资源转换 |
| `Monster.dmat` (5份) | DSEngine Material | 对应 PBR 材质 | 同上 | 同上 |
| `Monster.dskel` (4份) | DSEngine Skeleton | Monster 骨骼定义 | 同上 | 同上 |
| `*.danim` (4份) | DSEngine Animation | Idle/Walk/Attack/Attack2/Pos/AddtiveAnim 动画 | 同上 | 同上 |
| `*.tga` (4份) | TGA | 角色与环境贴图 | 同上 | 同上 |

## 待补充资源（P4 规划）

以下资源计划从 `reference/VSEngine2.1/` 拷贝或转换，用于推进 demo 从 fallback 到真实后端：

| 目标路径 | 资源描述 | 优先级 | 来源路径 |
|----------|----------|--------|----------|
| `data/models/static/` | 静态模型（建筑/道具） | P4 | `reference/VSEngine2.1/Demo/15/15.22` |
| `data/textures/checker/` | 棋盘格贴图 | P4 | 程序化生成 |
| `data/textures/vse_demo/` | VSE 参考贴图 | P4 | `reference/VSEngine2.1/Demo/15/15.22` |
| `data/terrain/heightmaps/` | 高度图 | P4 | `reference/VSEngine2.1/Demo/13/13.9` |
| `data/animation/character/` | 真实角色动画 | P4 | `reference/VSEngine2.1/Demo/14/14.27` |

## 许可证说明

- **引擎工具链生成资源**（`cube.*`、`minimal_rig/*`、`spatial_ping.wav`）：DSEngine 项目自有，可自由使用。
- **VSEngine 参考资源**（`vse_demo/*`）：仅作为内部开发参考，来源于 VSEngine2.1 项目，不得对外分发。
- **CesiumLogoFlat.png**：来源于 Cesium 项目，遵循 Apache 2.0 许可证。
