# KF_Framework Demo 复刻

使用 DSEngine Lua 脚本复刻 [KodFreedom/KF_Framework](https://github.com/KodFreedom/KF_Framework)（DirectX9 3D 格斗游戏 demo）。

## 快速开始

### 1. 准备原始资产

```bash
# 克隆原始项目（如未克隆）
git clone https://github.com/KodFreedom/KF_Framework.git <somewhere>/KF_Framework

# 运行资产复制脚本
python tools/setup_assets.py --source <somewhere>/KF_Framework/data
```

该脚本会将纹理/音频直接复制到 `assets/`，将二进制文件（.mesh/.skin/.model/.motion）复制到 `assets/raw/`。

### 2. 转换资产

```bash
# 将 KF 二进制格式转换为 glTF 2.0
python tools/batch_convert.py

# 产出到 cooked/ 目录：.dmesh, .dskel, .danim, .dmat
```

### 3. 运行 Demo

```bash
# 从引擎根目录启动
DSEngineStandalone.exe --script examples/KF_Framework/scripts/main.lua
```

## 目录结构

```
examples/KF_Framework/
├── tools/                  # 转换工具（Python）
├── scripts/                # 游戏 Lua 脚本
├── assets/                 # 原始资产（git ignored，需通过 setup 脚本生成）
│   ├── textures/           # 纹理（直接从 KF_Framework 复制）
│   ├── audio/bgm/          # 背景音乐
│   ├── audio/se/           # 音效
│   └── raw/                # KF 二进制文件（.mesh/.skin/.model/.motion）
├── cooked/                 # DSEngine 格式（git ignored，由 batch_convert 生成）
├── PLAN.md                 # 详细复刻方案
├── SESSION_INSTRUCTIONS.md # AI 会话任务指令
└── README.md               # 本文件
```

**Git 追踪**: tools/, scripts/, *.md, .gitignore  
**Git 忽略**: assets/, cooked/（可通过脚本重新生成）

## 技术要点

- **资产管线**: KF 自定义二进制 → Python 转 glTF 2.0 → AssetBuilder → DSEngine 格式
- **坐标系**: DX9 左手系 → OpenGL 右手系（Z 轴翻转）
- **渲染**: KF HLSL → DSEngine 内建 PBR
- **动画**: KF 逐帧存储 → glTF keyframes → .danim

## 截图对比

> TODO: 复刻完成后在此放置 KF_Framework 原版 vs DSEngine 复刻的截图对比
