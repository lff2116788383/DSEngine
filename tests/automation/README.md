# DSEngine 编辑器自动化测试框架

YAML 驱动的编辑器进程级自动化测试框架，控制 `dsengine-editor.exe` 完成四类测试。
设计文档见 [`docs/editor/EDITOR_AUTOMATION_TEST_FRAMEWORK.md`](../../docs/editor/EDITOR_AUTOMATION_TEST_FRAMEWORK.md)。

与无头 C++ 测试（`tests/gtest/*`）互补：后者测业务逻辑层（毫秒级、无需 GPU），
本框架测编辑器全链路（秒级、需 GPU + 窗口）。

## 四类测试

| 模式 | 进程模型 | 说明 | 子命令 |
|:-----|:---------|:-----|:-------|
| Batch（CLI） | 每 case 独立进程 | CLI 参数驱动，校验退出码 + 产物文件 | `run` |
| API Session | 每 case 独立进程 + WebSocket | JSON-RPC 顺序执行 `calls[]`，截图/断言 | `run` |
| Stability | 反复启停 suite | 循环 N 次或按时长，检测崩溃 | `stability` |
| Soak | 单进程长稳 | 循环操作 + 采样 RSS/指标，检测泄漏 | `soak` |

## 依赖

```powershell
.\tests\automation\scripts\setup_test_env.ps1
# 等价于：
pip install PyYAML websocket-client psutil pillow numpy
```

## 快速上手

```powershell
$env:EDITOR_EXE = "C:\path\to\bin\dsengine-editor.exe"   # 缺省回退到 <repo>\bin\dsengine-editor.exe
$env:WORK_ROOT  = "C:\dse-autotest"                      # 报告/临时工程输出根目录

# 批处理冒烟
python tests\automation\dse_auto.py run `
  --suite tests\automation\suites\editor-batch-smoke.yaml --run-id batch_001

# API 会话冒烟
python tests\automation\dse_auto.py run `
  --suite tests\automation\suites\editor-api-smoke.yaml --run-id api_001

# 一把跑全部冒烟/回归
.\tests\automation\scripts\run_all.ps1 -WorkRoot C:\dse-autotest
```

报告输出到 `${WORK_ROOT}/reports/<run-id>/`：`suite_report.json` + `suite_report.html` + 各 case JSON + 截图。

### 稳定性 / 长稳

```powershell
# 稳定性：100 次循环
python tests\automation\dse_auto.py stability `
  --suite tests\automation\suites\editor-stability.yaml --run-id stab_001 --iterations 100

# 稳定性：按时长（2 小时）
python tests\automation\dse_auto.py stability `
  --suite tests\automation\suites\editor-api-smoke.yaml --run-id stab_2h `
  --duration-seconds 7200 --delay-seconds 2

# 驻留长稳：单进程 8 小时
python tests\automation\dse_auto.py soak `
  --case tests\automation\cases\soak\resident_edit_session.yaml `
  --run-id soak_8h --duration-seconds 28800
```

## 命令行参数（`run`）

| 参数 | 说明 |
|:-----|:------|
| `--suite <file>` | 必填，指定 suite |
| `--run-id <id>` | 必填，运行 ID（决定报告子目录） |
| `--env <file>` | 覆盖 suite 中配置的 env |
| `--editor-exe <path>` | 覆盖 `EDITOR_EXE` |
| `--work-root <path>` | 覆盖 `WORK_ROOT` |
| `--var key=value` | 覆盖任意变量，可重复 |
| `--stop-on-failure` | 首个失败后停止 |
| `--include-quarantine` | 允许运行标记 `quarantine: true` 的套件 |

## 目录结构

```
tests/automation/
├── dse_auto.py          # 主控制器
├── envs/                # 环境配置（local / ci）
├── suites/              # 套件（batch-smoke / api-smoke / regression / nightly / stability / quarantine）
├── cases/cli/           # 批处理类用例
├── cases/api/           # API 会话类用例
├── cases/soak/          # 驻留长稳用例
├── testdata/            # 测试工程 / 资产 / 截图基准
├── quarantine/          # 隔离用例/数据
├── scripts/             # run_all.ps1 / setup_test_env.ps1
└── reports/             # 报告输出（运行生成）
```

## 用例 DSL

直接复用编辑器已注册的 `dsengine_*` 工具名（共 37 个：32 个既有 + 本框架新增 5 个）。

**Batch（CLI）**：`command.type: cli`，`command.args` 透传给编辑器；`assert` 支持
`exit_code` / `files_exist` /（前向兼容）`report`。

**API Session**：`command.type: api_session`，`startup_args` 启动编辑器，`calls[]` 顺序执行；
每个 call 的 `expect` 支持：

| 操作符 | 示例 | 说明 |
|:-------|:-----|:------|
| 精确匹配 | `result.ok: true` | 值精确等于 |
| 数值比较 | `result.entity_id: "> 0"` | 支持 `> < >= <= == !=` |
| 包含元素 | `result.components: ["TransformComponent"]` | 列表包含 |
| 嵌套匹配 | `result.project.name: "simple_2d_game"` | 点分路径取值 |

`dsengine_editor_screenshot` 若带 `params.path`，响应中的 base64 会自动落盘；
再配 `screenshot_compare`（baseline / tolerance / diff_output）即做像素 RMSE 对比。

### Setup / Teardown / 重试 / 并发

- `setup` / `teardown` 动作原语：`create_dir` / `remove_dir` / `copy_file` / `copy_dir` / `exec_cmd` / `sleep`。
- suite 级 `retry`：`max_retries` / `retry_delay_seconds` / `retry_on_status`；重试后通过标 `flaky: true`。
- suite 级 `execution.mode: parallel` + `max_workers`：**仅 Batch** 可并发（API Session 共享进程不并发）。

## 环境前提（重要）

启动编辑器需要**真实 GPU + 桌面会话**。无 GPU 或运行在 Windows 服务会话（session 0）时，
编辑器的 GL 上下文创建会失败、进程以 -1 退出 → 用例报「编辑器未就绪」。
CI 上 L1+ 必须使用自托管 GPU runner（见设计文档 §8.1）。
