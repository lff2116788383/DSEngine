# Demo 截图回归测试

自动化视觉回归测试：为 3D Lua demo 生成 OpenGL 截图基线，后续引擎改动可自动对比检测退化。

## 快速开始

```bash
# 1. 生成基线（首次或引擎有预期的视觉变更后）
python tools/demo_regression.py --baseline

# 2. 回归对比（引擎改动后验证无退化）
python tools/demo_regression.py --compare
```

## 依赖

- Python 3.9+
- Pillow: `pip install Pillow`
- （可选）NumPy: `pip install numpy`（加速 RMSE 计算）

## 命令参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--baseline` | — | 生成基线截图 |
| `--compare` | — | 与基线对比 |
| `--threshold` | 5.0 | RMSE 失败阈值 |
| `--max-frames` | 30 | 每个 demo 运行帧数 |
| `--screenshot-frame` | 25 | 截图帧号 |
| `--timeout` | 60 | 每个 demo 超时（秒） |
| `--demos` | 全部 59 个 | 指定 demo 子集 |
| `--no-sync` | false | 跳过 samples/ 同步 |
| `--baseline-dir` | tests/regression/screenshots/opengl/ | 基线目录 |

## 输出示例

```
[PASS] 3d_static_model               RMSE=0.00
[PASS] 3d_textured_cube              RMSE=1.23
[FAIL] 3d_lighting_showcase          RMSE=8.45 (threshold=5.0)
...
SUMMARY: 57 pass, 2 fail, 0 skip (threshold=5.0)
```

## 工作原理

1. 对每个 demo 设置环境变量运行引擎：
   - `DSE_DEMO=<name>` — 指定 demo
   - `DSE_RHI_BACKEND=opengl` — OpenGL 后端
   - `DSE_MAX_FRAMES=30` — 运行 30 帧后退出
   - `DSE_SCREENSHOT_FRAME=25` — 第 25 帧截图
   - `DSE_SCREENSHOT_PATH=<path>` — 截图输出路径
2. 收集截图到 `tests/regression/screenshots/opengl/`
3. `--compare` 模式计算逐像素 RMSE，超阈值报告失败

## 文件结构

```
tests/regression/
├── README.md                       # 本文件
└── screenshots/
    └── opengl/                     # OpenGL 基线截图（.gitignore）
        ├── 3d_triangle.png
        ├── 3d_cube.png
        └── ...
```

## 注意事项

- 基线截图目录已加入 `.gitignore`，不提交到版本控制
- 不同 GPU/驱动可能产生像素级差异，阈值建议 3.0~5.0
- 如需跨机器基线，可手动提交基线或使用共享存储
