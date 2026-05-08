# KF_Framework Demo 复刻

使用 DSEngine Lua 脚本复刻 [KodFreedom/KF_Framework](https://github.com/KodFreedom/KF_Framework)（DirectX9 3D 格斗游戏 demo）。

## 快速开始

### 1. 准备原始资产

```bash
# 复制纹理和音频
python tools/setup_assets.py --source <path_to>/KF_Framework/data
```

### 2. 转换资产（FBX 直通）

```bash
# 从原始 FBX 批量转换（主要路径）
python tools/fbx_convert.py --fbx-dir <path_to>/KF_ModelAnalyzer/data/FBX

# 产出到 cooked/ 目录：.dmesh, .dskel, .danim, .dmat
```

备用路径（KF 自定义二进制 → glTF → AssetBuilder）：
```bash
python tools/batch_convert.py --actors knight --skip-cook   # 仅生成 glTF
python tools/batch_convert.py --actors knight               # 完整转换
```

### 3. 运行 Demo

```bash
# 从引擎根目录启动
DSEngineStandalone.exe --script examples/KF_Framework/script/main.lua
```

## 目录结构

```
examples/KF_Framework/
├── tools/                  # 转换 & 诊断工具（Python）
│   ├── setup_assets.py     # 复制纹理/音频
│   ├── batch_convert.py    # KF 二进制批量转换
│   ├── kf_to_gltf.py       # KF 二进制 → glTF
│   ├── verify_scene.py     # 自动截图 & 渲染分析
│   ├── check_dskel.py      # dskel 骨骼诊断
│   ├── check_danim.py      # danim 动画通道诊断
│   └── compare_skeletons.py# 两个 dskel 拓扑对比
├── script/                 # 游戏 Lua 脚本
│   ├── main.lua            # 入口 + Update 循环
│   ├── config.lua          # 全局配置 & 资产路径
│   ├── scene.lua           # 场景搭建 (地面/树木/照明)
│   ├── player.lua          # Knight FSM + 输入 + 摄像机
│   ├── enemy.lua           # Mutant AI (巡逻/追击/攻击)
│   ├── gameflow.lua        # 游戏流程 (Title→Battle→Result)
│   ├── audio.lua           # 音效/BGM 管理
│   ├── hud.lua             # 血条 HUD
│   ├── fade.lua            # Fade 过渡系统
│   └── autoplay.lua        # DemoPlay 自动战斗 AI
├── font/                   # 位图字体 (bitmap_font.png)
├── assets/                 # 原始资产（纹理、FBX 源文件）
│   ├── textures/           # 纹理（直接从 KF_Framework 复制）
│   ├── fbx/                # FBX 模型/动画源文件
│   └── audio/              # bgm/ + se/
├── cooked/                 # DSEngine 格式（由 AssetBuilder 从 FBX 生成）
├── screenshots/            # verify_scene.py 自动截图输出（git ignored）
├── PLAN.md                 # 详细复刻方案
├── TROUBLESHOOTING.md      # 踩坑与经验总结
└── README.md               # 本文件
```

**Git 忽略**: screenshots/（自动截图输出）

## 技术要点

- **资产管线**: 原始 FBX (Mixamo) → AssetBuilder → DSEngine 格式（主要路径）
- **备用管线**: KF 自定义二进制 → Python 转 glTF 2.0 → AssetBuilder（已实现，用于静态 mesh）
- **渲染**: KF HLSL → DSEngine 内建 PBR
- **动画**: FBX 动画 → AssetBuilder → .danim
- **FBX 来源**: KF_ModelAnalyzer/data/FBX/ — 包含 knight、mutant、zombie 及场景模型

## 操作控制

| 按键 | 功能 |
|------|------|
| WASD | 移动 |
| Shift | 奔跑 |
| Space | 跳跃 |
| LMB | 攻击 (连击三段) |
| RMB | 格挡 (100% 免伤) |
| Q | 踢击 |
| E | 施法 |
| Enter | Title 画面开始游戏 |
| F5 | 切换 DemoPlay AI 自动战斗 |

## 游戏流程

```
Title (标题 BGM) → Enter → Fade → Battle (战斗 BGM) → 死亡/全灭 → Fade → Result (结算 BGM) → Any Key → Fade → Title
```

- **软重置**: 每次从 Title 进入 Battle 时自动重置玩家 HP/位置和敌人状态
- **DemoPlay**: F5 开启后 AI 自动控制玩家寻敌/攻击/格挡 (KF: ModeDemoPlay)

## 截图对比

> TODO: 复刻完成后在此放置 KF_Framework 原版 vs DSEngine 复刻的截图对比
