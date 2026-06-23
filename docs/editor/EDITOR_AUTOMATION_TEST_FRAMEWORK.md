# DSEngine 编辑器自动化测试框架设计方案

> 版本：v1.0  
> 日期：2026-06-23  
> 结合 DSEngine 现有 ControlServer（WebSocket JSON-RPC）和 headless 测试基础设施，参考工业级自动化测试框架的最佳实践设计。
> 前置：编辑器需新增 `--automation-mode`、`--automation-api` 命令行支持（见 §五）

---

## 目录

1. [参考框架分析](#一参考框架分析)
2. [框架总体架构](#二框架总体架构)
3. [核心组件设计](#三核心组件设计)
4. [用例与套件格式](#四用例与套件格式)
5. [引擎端改造成本](#五引擎端改造成本)
6. [报告产物](#六报告产物)
7. [运行方式](#七运行方式)
8. [CI 集成建议](#八ci-集成建议)
9. [附录](#九附录)

---

## 一、参考框架分析

### 1.1 参考框架架构

```
controller.py               ← 测试控制器（Python，读取 YAML → 执行 → 报告）
├── envs/local.yaml         ← 环境配置（路径、目标平台、超时）
├── suites/*.yaml           ← 测试套件（组合 case + 指定 env）
├── cases/
│   ├── batch/              ← 批处理类 case（CLI 参数驱动）
│   └── api_session/        ← API 会话类 case（JSON-RPC 驱动）
├── testdata/               ← 测试数据文件
├── report/                 ← JSON + HTML 报告
└── scripts/                ← 批量运行/辅助脚本
```

### 1.2 对 DSEngine 的参考价值

| 参考框架设计 | DSEngine 当前状态 | 可借鉴程度 |
|:------------|:-----------------|:----------:|
| `--automation-mode` CLI 参数驱动批处理 | ❌ 没有 | **直接借鉴** |
| Named pipe JSON-RPC API 会话 | 🟡 有 WebSocket JSON-RPC，缺会话管理 | **协议不同但概念一致** |
| YAML 用例定义（command + assert） | ❌ 没有 | **直接借鉴** |
| Suite 组织 + 环境变量模板 | ❌ 没有 | **直接借鉴** |
| JSON + HTML 报告 | 🟡 有 gtest XML，无自动化报告 | **直接借鉴** |
| 稳定性循环 / 驻留长稳 | ❌ 没有 | **远期借鉴** |

---

## 二、框架总体架构

### 2.1 定位

本框架围绕 DSEngine 编辑器（`dsengine_editor_cpp.exe`）构建，覆盖四种测试场景：

| 场景 | 说明 | 对应模式 |
|:-----|:------|:--------|
| **批处理回归** | 启动编辑器 → CLI 参数操作（打开工程、保存）→ 退出 → 验证退出码+报告 | Batch（CLI） |
| **API 会话测试** | 启动编辑器 → WebSocket JSON-RPC 控制 → 执行 UI 操作/截图/验证 → 退出 | API Session |
| **稳定性循环** | 重复运行 suite N 次，验证反复启停无异常 | Stability |
| **驻留长稳** | 不退出进程，循环执行操作并采集资源指标（内存泄漏检测） | Soak |

### 2.2 进程隔离与间隔策略

#### 2.2.1 进程隔离模型

| 模式 | 隔离策略 | 崩溃影响 | 适用场景 |
|:-----|:---------|:---------|:---------|
| **Batch（CLI）** | **完全隔离。** 每 case 启动独立编辑器进程，执行操作后退出。 | 一个 case 崩溃不影响后续 case。Runner 检测到非零退出码即记录失败，继续执行下一个。 | 确定性流程：打开工程、保存、导出、编译 |
| **API Session** | **进程内隔离。** 多个 case 共享一个编辑器进程，依次通过 JSON-RPC 交互。 | case 崩溃 → 整个会话中断。Runner 应自动重启进程，从断点 case 继续或跳过。 | 复杂工作流：创建实体→修改属性→截图→保存 |
| **Soak** | **单进程长稳。** 在同一进程内循环执行操作序列，不退出。 | 崩溃即测试失败。Runner 记录崩溃时的迭代次数和资源快照。 | 内存泄漏检测、资源泄漏检测 |

#### 2.2.2 进程清理

在 Batch 模式下，Runner 必须确保编辑器进程被正确清理：

```python
import psutil, signal, time, os

def _ensure_process_terminated(proc, timeout=10):
    """确保编辑器进程退出，强制 kill 残留进程"""
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        # 超时未退出，强制终止
        proc.kill()
        proc.wait(timeout=5)

def _kill_lingering_editors():
    """清理残留编辑器进程（上次运行崩溃留下的孤儿进程）"""
    for p in psutil.process_iter(['pid', 'name']):
        if p.info['name'] and 'dsengine_editor' in p.info['name'].lower():
            p.kill()
```

在 suite 运行前和运行后，主动扫描并清理残留的编辑器进程（参考工业框架中 `Get-Process dsengine_editor_cpp | Stop-Process` 的做法）。

#### 2.2.3 间隔策略

| 间隔类型 | 默认值 | 作用 |
|:---------|:------:|:-----|
| **进程等待**（启动超时） | 30s | 编辑器启动就绪等待。超时后标记失败。 |
| **进程退出**（结束超时） | 10s | 编辑器退出等待。超时后 kill。 |
| **case 间隔**（`case_delay_seconds`） | 1s | case 之间留出操作系统资源回收时间（GPU 显存、文件句柄）。 |
| **GPU 冷却**（stability 模式） | 2s | 稳定性循环中，编辑器退出后等待 GPU 驱动/显存回收完成。 |

#### 2.2.4 Suite 级别的间隔配置

```yaml
# suites/editor-batch-smoke.yaml
suite_id: editor-batch-smoke
description: 批处理冒烟

env: tests/automation/envs/local-dse.yaml

cases:
  - tests/automation/cases/cli/open_project.yaml
  - tests/automation/cases/cli/save_project.yaml

# 间隔控制
case_delay_seconds: 1             # case 间延迟（默认 1s）
case_timeout_seconds: 60          # 单 case 超时（默认 30s）
process_cleanup: true             # 运行前清理残留进程

# GPU 相关
gpu_cool_down_seconds: 0          # Batch 模式不需要（每 case 重启进程）
gpu_memory_check: false           # 是否检查显存泄漏
```

### 2.3 整体架构

```
┌───────────────────────────────────────────────────────────────────┐
│                    dse_auto.py (主控制器)                          │
│  读取 YAML suite → 解析 env → 按序执行 case → 汇总报告             │
└───────┬─────────────────────────────────────┬─────────────────────┘
        │                                     │
        ▼                                     ▼
┌───────────────────┐             ┌──────────────────────────┐
│  Batch Mode        │             │  API Session Mode         │
│  每次启动新进程     │             │  启动一次，持久连接        │
│  dsengine_editor   │             │  dsengine_editor          │
│  --automation-mode │             │  --automation-mode        │
│  --<action> ...    │             │  --automation-api         │
│  --exit-on-finish  │             │  WebSocket 127.0.0.1:9527 │
│                    │             │  ┌─ app.ping              │
│                    │             │  ├─ project.open          │
│                    │             │  ├─ editor.screenshot     │
│                    │             │  └─ app.quit              │
└───────────────────┘             └──────────────────────────┘
```

### 2.3 与现有测试体系的关系

```
现有测试体系（已有）                 新增自动化框架（待建）
┌─────────────────────┐           ┌──────────────────────────┐
│  无头 C++ 测试       │           │  编辑器进程级自动化       │
│  (222 例, 毫秒级)    │           │  (YAML case, 秒级)       │
│  测试业务逻辑层       │  互补     │  测试编辑器全链路         │
│  不需要 GPU/窗口     │◄────────►│  需要 GPU + 窗口         │
│  CI 每次提交（L0）    │           │  CI 日构建/夜间（L1/L2）  │
└─────────────────────┘           └──────────────────────────┘

不替代，而是补充：
- 无头测试 → 快速定位逻辑错误（毫秒反馈），适合 PR 验证
- 自动化框架 → 发现编辑器集成问题（崩溃、UI 状态错误），适合日构建
```

### 2.4 目录结构

```
tests/automation/                  ← 自动化测试框架根目录
├── dse_auto.py                    ← 测试控制器（Python）
├── README.md                      ← 快速上手

├── envs/                          ← 环境配置
│   ├── local-dse.yaml             ← 本地环境
│   └── ci-dse.yaml                ← CI 环境

├── suites/                        ← 测试套件
│   ├── editor-batch-smoke.yaml    ← 批处理冒烟
│   ├── editor-api-smoke.yaml      ← API 冒烟
│   ├── editor-regression.yaml     ← 回归
│   ├── editor-nightly.yaml        ← 夜间全量
│   └── editor-quarantine.yaml     ← 已知问题隔离

├── cases/                         ← 测试用例
│   ├── cli/                       ← 批处理类（CLI 参数驱动）
│   │   ├── new_project.yaml
│   │   ├── open_project.yaml
│   │   ├── save_project.yaml
│   │   └── build_game.yaml
│   ├── api/                       ← API 会话类（WebSocket JSON-RPC）
│   │   ├── hierarchy_scene_edit.yaml
│   │   ├── inspector_modify_component.yaml
│   │   ├── prefab_instantiate.yaml
│   │   ├── undo_redo.yaml
│   │   └── screenshot_compare.yaml
│   └── soak/                      ← 驻留长稳
│       └── resident_edit_session.yaml

├── testdata/                      ← 测试数据
│   ├── projects/
│   │   ├── empty_project/
│   │   ├── simple_2d_game/
│   │   └── complex_3d_scene/
│   ├── assets/
│   │   ├── test_texture.png
│   │   └── test_audio.wav
│   └── screenshots/               ← 预期截图基准
│       ├── viewport_default.png
│       └── game_view_default.png

├── quarantine/                    ← 隔离用例/数据
│   ├── cases/
│   └── testdata/

├── scripts/                       ← 辅助脚本
│   ├── run_all.ps1
│   └── setup_test_env.ps1

└── reports/                       ← 报告输出（运行生成）
    └── .gitkeep
```

---

## 三、核心组件设计

### 3.1 `dse_auto.py` — 主控制器

```
dse_auto.py <子命令> [参数]

子命令：
  run          执行测试套件
  stability    稳定性循环
  soak         驻留型长稳
```

Python 类骨架：

```python
#!/usr/bin/env python3
"""
DSEngine 编辑器自动化测试控制器
"""
import argparse, json, os, subprocess, sys, time, yaml
from pathlib import Path

class DSETestRunner:
    def __init__(self, env):
        self.editor_exe = env["editor_exe"]
        self.work_root = Path(env["work_root"])
        self.report_root = self.work_root / "reports"
        self.control_port = env.get("control_port", 9527)

    def run_suite(self, suite_path, run_id):
        """执行一个测试套件"""
        with open(suite_path) as f:
            suite = yaml.safe_load(f)

        results = []
        for case_path in suite["cases"]:
            result = self.run_case(case_path)
            results.append(result)
        self._write_report(results, run_id)
        return all(r["status"] == "passed" for r in results)

    def run_case(self, case_path):
        """执行一个测试用例"""
        with open(case_path) as f:
            case = yaml.safe_load(f)

        case_type = case.get("command", {}).get("type", "cli")
        start = time.time()

        if case_type == "cli":
            result = self._execute_cli_case(case)
        elif case_type == "api_session":
            result = self._execute_api_session_case(case)
        else:
            result = {"status": "failed", "error": f"Unknown type: {case_type}"}

        result["case_id"] = case["case_id"]
        result["duration_ms"] = int((time.time() - start) * 1000)
        return result

    def _execute_cli_case(self, case):
        """批处理：启动编辑器 → CLI 参数 → 验证退出码 + 报告"""
        cmd = case["command"]
        args = [self.editor_exe, "--automation-mode"]
        args.extend(self._expand_vars(a) for a in cmd["args"])

        proc = subprocess.run(args, capture_output=True, timeout=120)
        expected = case.get("assert", {})

        if "exit_code" in expected and proc.returncode != expected["exit_code"]:
            return {"status": "failed",
                    "error": f"Exit code {proc.returncode} != {expected['exit_code']}"}
        return {"status": "passed"}
```

### 3.2 Batch Runner

```python
class BatchRunner:
    def run_case(self, case: dict, env: dict) -> dict:
        # 1. 展开变量（${work_root} → 实际路径）
        # 2. 组装命令行参数
        # 3. subprocess.Popen(editor_exe, args)
        # 4. 等待进程退出（timeout 可配）
        # 5. 读取 --report 指定的 JSON 报告
        # 6. 根据 assert 规则验证
        # 7. 返回 {status, steps, diagnostics}
```

### 3.3 ApiSessionRunner

```python
class ApiSessionRunner:
    def run_case(self, case: dict, env: dict) -> dict:
        # 1. 启动编辑器进程（--automation-mode --automation-api --api-port P）
        # 2. 等待 WebSocket 端口就绪
        # 3. 依次执行 calls[] 中的每个 method
        # 4. 每个 method 验证 expect
        # 5. 最后调用 app.quit
        # 6. 等待进程退出
        # 7. 返回 {status, steps: [{method, ok, result}]}
```

### 3.4 Reporter

```python
class Reporter:
    def write_case_report(self, case_id, result, report_dir):
        # ${report_dir}/${case_id}.json

    def write_suite_report(self, suite_id, run_id, results, report_dir):
        # ${report_dir}/suite_report.json
        # ${report_dir}/suite_report.html

    def write_stability_report(self, run_id, rounds, report_dir):
        # ${stability_dir}/${run_id}/stability_report.json
```

---

## 四、用例与套件格式

### 4.1 Env 环境配置

`tests/automation/envs/local-dse.yaml`：

```yaml
variables:
  editor_exe: "${EDITOR_EXE}"
  work_root: "${WORK_ROOT}/dse-auto"
  report_root: "${work_root}/reports/${run_id}"
  api_port: 9527
  project_dir: "${work_root}/projects"
  testdata_dir: "${repo_root}/tests/automation/testdata"
  timeout_seconds: 30
```

变量展开优先级：

1. Runner 内置：`repo_root`、`run_id`、`WORK_ROOT`、`EDITOR_EXE`
2. 命令行 `--var key=value`
3. Env 文件的 `variables`
4. 环境变量 `EDITOR_EXE`、`WORK_ROOT`

### 4.2 批处理 Case（CLI 模式）

适用于确定性流程，每 case 启停一次编辑器。

```yaml
# cases/cli/open_project.yaml
case_id: cli.open_project
title: 打开已有工程并验证场景加载

command:
  executable: "${editor_exe}"
  args:
    - "--automation-mode"
    - "--project"
    - "${testdata_dir}/projects/simple_2d_game/project.dsproj"
    - "--report"
    - "${report_root}/open_project.json"
    - "--exit-on-finish"

assert:
  exit_code: 0
  report:
    status: passed
    steps:
      open_project: passed
```

```yaml
# cases/cli/save_project.yaml
case_id: cli.save_project
title: 修改场景后保存并重载验证

command:
  executable: "${editor_exe}"
  args:
    - "--automation-mode"
    - "--project"
    - "${testdata_dir}/projects/simple_2d_game/project.dsproj"
    - "--create-entity"
    - "TestCube"
    - "--save"
    - "--verify-reload"
    - "--report"
    - "${report_root}/save_project.json"
    - "--exit-on-finish"

assert:
  exit_code: 0
  report:
    status: passed
    steps:
      open_project: passed
      create_entity: passed
      save_project: passed
      verify_reload:
        entity_count_before: 5
        entity_count_after: 5
```

### 4.3 API 会话 Case（API Session 模式）

适用于需要交互式操作的完整工作流测试。

```yaml
# cases/api/hierarchy_scene_edit.yaml
case_id: api.hierarchy_scene_edit
title: 打开工程 → 创建实体 → 验证 Hierarchy → 保存 → 截图 → 退出

command:
  type: api_session
  executable: "${editor_exe}"
  api_port: ${api_port}
  failure_screenshot_path: "${report_root}/failure.png"
  startup_args:
    - "--automation-mode"
    - "--automation-api"
    - "--api-port"
    - "${api_port}"
  calls:
    - method: app.ping
      expect:
        result.ok: true
        result.app: "dsengine-editor"

    - method: project.open
      params:
        path: "${testdata_dir}/projects/simple_2d_game/project.dsproj"
      expect:
        result.opened: true
        result.entity_count: 4

    - method: hierarchy.get_entities
      expect:
        result.ok: true
        result.entity_count: 4
        result.root_entities: ["Main Camera", "Directional Light", "Player", "Ground"]

    - method: entity.create
      params:
        name: "Enemy"
      expect:
        result.ok: true
        result.entity_id: "> 0"

    - method: scene.save
      params:
        path: "${project_dir}/modified_scene.json"
      expect:
        result.saved: true

    - method: scene.load
      params:
        path: "${project_dir}/modified_scene.json"
      expect:
        result.loaded: true
        result.entity_count: 5

    - method: editor.screenshot
      params:
        path: "${report_root}/viewport.png"
      expect:
        result.saved: true

    - method: app.quit
      expect:
        result.ok: true

assert:
  exit_code: 0
```

### 4.4 用例命名约定

```
cases/
├── cli/
│   ├── project_open.yaml              # 打开工程
│   ├── project_save.yaml              # 保存工程
│   ├── scene_save_load_roundtrip.yaml  # 场景保存→加载→验证
│   └── build_game.yaml                 # 打包游戏
├── api/
│   ├── hierarchy_drag_reorder.yaml     # Hierarchy 拖拽排序
│   ├── inspector_edit_component.yaml   # Inspector 修改组件属性
│   ├── viewport_gizmo_translate.yaml   # 视口 Gizmo 平移
│   ├── terrain_sculpt_undo.yaml        # 地形雕刻+撤销
│   ├── prefab_create_instantiate.yaml  # 创建预制体+实例化
│   ├── shader_graph_compile.yaml       # Shader Graph 编译
│   ├── anim_state_machine.yaml         # 动画状态机编辑
│   └── lua_console_execute.yaml        # Lua 控制台执行
```

### 4.5 Suite 套件格式

```yaml
# suites/editor-api-smoke.yaml
suite_id: editor-api-smoke
description: DSEngine Editor API 冒烟测试

env: tests/automation/envs/local-dse.yaml

cases:
  - tests/automation/cases/api/hierarchy_scene_edit.yaml
  - tests/automation/cases/api/inspector_modify_component.yaml
  - tests/automation/cases/api/prefab_instantiate.yaml

# 可选字段
# quarantine: true               # 标记为隔离套件
# case_delay_seconds: 1           # case 间延迟
```

```yaml
# suites/editor-stability.yaml
suite_id: editor-stability
description: 编辑器稳定性循环
env: tests/automation/envs/local-dse.yaml
cases:
  - tests/automation/cases/api/hierarchy_scene_edit.yaml
  - tests/automation/cases/api/inspector_modify_component.yaml
quarantine: true   # 稳定性套件默认隔离，不进入 CI
```

### 4.6 断言规则

批处理 `assert` 支持：

| 字段 | 说明 |
|:-----|:------|
| `exit_code` | 进程退出码（期望 0） |
| `report.status` | 报告 `passed` / `failed` |
| `report.steps.<step>` | 某一步的期望状态 |
| `report.entity_count` | 期望的实体数量 |

API 会话每个 `call` 的 `expect` 支持：

| 操作符 | 示例 | 说明 |
|:-------|:-----|:------|
| 精确匹配 | `result.ok: true` | 值精确等于 |
| 通配符 | `result.entity_id: "> 0"` | 大于 0 |
| 存在性 | `result.components: ["TransformComponent"]` | 包含指定元素 |
| 嵌套匹配 | `result.project.name: "api_project"` | 嵌套属性匹配 |

---

## 五、引擎端改造成本

要实现本框架，编辑器侧需要新增以下能力：

### 5.1 `--automation-mode` CLI 模式（2 天）

编辑器新增命令行参数，进入自动化模式后：

- 窗口隐藏或最小化
- 执行指定 CLI 操作后退出

```
dsengine_editor_cpp.exe --automation-mode ^
    --open-project "C:/MyGame/project.dsproj" ^
    --create-entity ^
    --save-scene "test_output/scene.json" ^
    --report "report.json" ^
    --exit-on-finish
```

**具体修改**：
- `editor_app.cpp`：解析 `--automation-mode`，初始化引擎但不弹 ImGui 窗口
- `editor_cli_handler.cpp`（新增）：注册 CLI action 列表，顺序执行
- 复用现有底层 API（`SaveScene`、`LoadScene`、`CreateEntity` 等）

### 5.2 新增 ControlServer Tool

| Tool | 功能 | 优先级 | 预估 |
|:-----|:------|:------:|:----:|
| `app.ping` | 返回编辑器状态，确认就绪 | P0 | 0.5天 |
| `app.quit` | 设置退出标志，RunOneFrame 返回 false | P0 | 0.5天 |
| `project.open` | 打开完整工程（触发 SceneTab/Hierarchy/Inspector 刷新） | P0 | 0.5天 |
| `dsengine_editor_screenshot` | 截取当前编辑器窗口并返回 base64 PNG | P1 | 1天 |
| `dsengine_hierarchy_get_entities` | 返回 Hierarchy 面板的实体列表 | P1 | 0.5天 |
| `dsengine_editor_idle` | 跑 N 帧主循环后返回当前状态 | P1 | 0.5天 |
| `dsengine_editor_get_metrics` | 返回 FPS / DrawCall / 实体数等性能数据 | P2 | 0.5天 |

### 5.3 总成本汇总

| 组件 | 预估 | 说明 |
|:-----|:----:|:------|
| `--automation-mode` CLI 解析 + 执行 | 2 天 | 核心基础设施，含 `editor_cli_handler.cpp` 新增文件 |
| 新增 ControlServer Tool | 1 天 | 多数已有底层 API 可复用 |
| 编辑器无头启动（隐藏窗口） | 0.5 天 | `glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE)` |
| **引擎端总计** | **3-4 天** | |

框架 Python 控制器（`dse_auto.py`）预估 **2-3 天**，与引擎端改造可并行进行。

---

## 六、报告产物

### 6.1 报告目录结构

```
${WORK_ROOT}/reports/
├── api_smoke_001/                    # --run-id 指定
│   ├── suite_report.json             # Suite 级别报告
│   ├── suite_report.html             # Suite 级别 HTML 报告
│   ├── hierarchy_scene_edit.json     # 单用例报告
│   ├── inspector_modify_component.json
│   ├── failure.png                   # 失败时的截图
│   └── viewport.png                  # 测试截图产物
├── stability_nightly/
│   └── stability_report.json
└── soak_8h/
    ├── soak_report.json
    └── api_trace.log
```

### 6.2 报告字段

```json
{
    "suite_id": "editor-api-smoke",
    "run_id": "api_smoke_001",
    "status": "passed",
    "summary": {
        "total": 3,
        "passed": 3,
        "failed": 0,
        "skipped": 0,
        "duration_ms": 28500
    },
    "cases": [
        {
            "case_id": "api.hierarchy_scene_edit",
            "status": "passed",
            "duration_ms": 12000,
            "steps": [
                {"name": "app.ping", "status": "passed"},
                {"name": "project.open", "status": "passed"},
                {"name": "hierarchy.get_entities", "status": "passed"},
                {"name": "entity.create", "status": "passed"},
                {"name": "scene.save", "status": "passed"},
                {"name": "editor.screenshot", "status": "passed"},
                {"name": "app.quit", "status": "passed"}
            ]
        }
    ]
}
```

---

## 七、运行方式

### 7.1 运行单个 Suite

```powershell
$env:EDITOR_EXE = "C:\DSEngine\bin\dsengine_editor_cpp.exe"
$env:WORK_ROOT = "C:\dse-autotest"

python tests\automation\dse_auto.py run `
  --suite tests\automation\suites\editor-api-smoke.yaml `
  --run-id api_smoke_001
```

常用参数：

| 参数 | 说明 |
|:-----|:------|
| `--suite <file>` | 必填，指定 suite |
| `--env <file>` | 覆盖 suite 中配置的 env |
| `--editor-exe <path>` | 覆盖 `EDITOR_EXE` |
| `--work-root <path>` | 覆盖 `WORK_ROOT` |
| `--run-id <id>` | 指定运行 ID |
| `--var key=value` | 覆盖任意变量，可重复 |
| `--stop-on-failure` | 首个失败后停止 |
| `--include-quarantine` | 允许运行隔离 suite |

### 7.2 批量运行

```powershell
.\tests\automation\scripts\run_all.ps1 `
  -EditorExe "C:\DSEngine\bin\dsengine_editor_cpp.exe" `
  -WorkRoot "C:\dse-autotest"
```

### 7.3 稳定性循环

```powershell
# 按次数
python tests\automation\dse_auto.py stability `
  --suite tests\automation\suites\editor-stability.yaml `
  --run-id stability_nightly `
  --iterations 100

# 按持续时间
python tests\automation\dse_auto.py stability `
  --suite tests\automation\suites\editor-api-smoke.yaml `
  --run-id stability_2h `
  --duration-seconds 7200 `
  --delay-seconds 2
```

### 7.4 驻留长稳

```powershell
python tests\automation\dse_auto.py soak `
  --case tests\automation\cases\soak\resident_edit_session.yaml `
  --run-id soak_8h `
  --duration-seconds 28800
```

---

## 八、CI 集成建议

### 8.1 分层策略

| 层级 | 内容 | 运行频率 | 预估时间 |
|:-----|:-----|:---------|:--------|
| **L0 - 提交检查** | 无头 C++ 测试（222 例） | 每次 PR | ~1min |
| **L1 - 日构建** | Batch CL 冒烟（3-5 case） | 每天 | ~30s |
| **L2 - 夜间回归** | API 全量 suite（~20+ case） | 每晚 | ~10min |
| **L3 - 稳定循环** | Stability 100 次迭代 | 每晚 | ~1h |

### 8.2 GitHub Actions

```yaml
# .github/workflows/editor-automation.yml
name: Editor Automation Tests
on: [push, pull_request]

jobs:
  editor-automation:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4

      - name: Build Editor
        run: cmake --build build_vs2022 --target dse_editor_cpp --config Release

      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install deps
        run: pip install PyYAML websocket-client

      - name: Run Batch Smoke
        env:
          EDITOR_EXE: ${{ github.workspace }}/bin/dsengine_editor_cpp.exe
          WORK_ROOT: ${{ runner.temp }}/dse-auto
        run: |
          python tests/automation/dse_auto.py run `
            --suite tests/automation/suites/editor-batch-smoke.yaml `
            --run-id ci_smoke

      - name: Run API Smoke
        env:
          EDITOR_EXE: ${{ github.workspace }}/bin/dsengine_editor_cpp.exe
          WORK_ROOT: ${{ runner.temp }}/dse-auto
        run: |
          python tests/automation/dse_auto.py run `
            --suite tests/automation/suites/editor-api-smoke.yaml `
            --run-id ci_api
```

---

## 九、附录

### 附录 A：补充机制

#### A.1 Setup / Teardown

每个 case/suite 支持前置准备和后置清理：

```yaml
# cases/cli/open_project.yaml
case_id: cli.open_project
title: 打开已有工程并验证

setup:
  # case 执行前运行，失败则 case skip
  - action: create_dir
    path: "${work_root}/scratch"
  - action: copy_file
    src: "${testdata_dir}/projects/empty_project"
    dst: "${work_root}/scratch/test_project"

command:
  executable: "${editor_exe}"
  args:
    - "--automation-mode"
    - "--project"
    - "${work_root}/scratch/test_project/project.dsproj"
    - "--exit-on-finish"

teardown:
  # case 执行后运行，失败不改变 case 状态
  - action: remove_dir
    path: "${work_root}/scratch"
```

支持的动作原语：`create_dir` / `remove_dir` / `copy_file` / `copy_dir` / `exec_cmd` / `sleep`。

#### A.2 失败重试机制

对于非确定性测试（如 GPU 驱动偶发超时、编辑器竞态），支持自动重试：

```yaml
# suites/editor-api-smoke.yaml
suite_id: editor-api-smoke

retry:
  max_retries: 2                # 最多重试 2 次
  retry_delay_seconds: 3        # 重试间隔
  retry_on_status: ["failed"]   # 哪些状态触发重试
  retry_on_exit_code: [1, -1]   # 哪些退出码触发重试

cases:
  - tests/automation/cases/api/hierarchy_scene_edit.yaml
```

Runner 实现逻辑：

```python
def run_case_with_retry(self, case_path, suite_config):
    max_retries = suite_config.get("retry", {}).get("max_retries", 0)
    for attempt in range(max_retries + 1):
        result = self.run_case(case_path)
        if result["status"] == "passed":
            return result
        if attempt < max_retries:
            delay = suite_config.get("retry", {}).get("retry_delay_seconds", 3)
            time.sleep(delay)
            result["retry_count"] = attempt + 1
    return result
```

首次重试通过后，报告标注 `flaky: true` 但不计入失败。

#### A.3 截图对比（像素级 diff）

API 会话测试支持截图后与基准图对比：

```yaml
- method: editor.screenshot
  params:
    path: "${report_root}/viewport.png"
  expect:
    result.saved: true
  # 截图后自动与基准图对比
  screenshot_compare:
    baseline: "${testdata_dir}/screenshots/viewport_default.png"
    tolerance: 0.95           # 像素相似度阈值（0-1），低于此值视为失败
    diff_output: "${report_root}/viewport_diff.png"
```

对比算法用像素级 MSE（均方误差）或 SSIM（结构相似性），推荐用 `pixelmatch` 库输出 diff 高亮图。

#### A.4 并发执行

Suite 支持 case 级并发（仅 Batch 模式，API Session 模式因共享进程不能并发）：

```yaml
# suites/editor-batch-smoke.yaml
suite_id: editor-batch-smoke

execution:
  mode: parallel              # sequential（默认）/ parallel
  max_workers: 4              # 最大并发进程数
  timeout_per_case: 120       # 单 case 超时

cases:
  - tests/automation/cases/cli/open_project.yaml
  - tests/automation/cases/cli/save_project.yaml
  - tests/automation/cases/cli/build_game.yaml
```

#### A.5 Python 依赖

```powershell
# 必需
pip install PyYAML           # YAML 解析

# 推荐
pip install websocket-client  # WebSocket JSON-RPC（API Session 模式）
pip install psutil            # 进程管理（进程清理、资源监控）

# 可选（截图对比用）
pip install pillow            # 图片加载 + 像素对比
```

### 附录 B：参考框架与 DSEngine 设计对照

| 维度 | 通用工业实践 | DSEngine 适配 |
|:-----|:-------------|:---------------|
| **Runner** | Python 进程控制器 | `dse_auto.py`（ImGui 应用专用） |
| **传输层** | Named Pipe / WebSocket | WebSocket（复用现有 ControlServer） |
| **获取编辑器状态** | JSON-RPC 同步调用 | WebSocket JSON-RPC（已有 32 Tool） |
| **CLI 批处理** | `--automation-mode --<action>` | 完全复用 |
| **API 会话** | `type: api_session` + `calls[]` | 完全复用，WebSocket |
| **截图** | 窗口/控件截图 | `editor.screenshot`（RHI RT 回读） |
| **报告格式** | JSON + HTML | 完全复用 |
| **环境变量** | YAML env + 变量展开 | 完全复用 |
| **隔离机制** | `quarantine/` 目录 | 完全复用 |
| **Suites** | YAML suite 组合 | 完全复用 |

### 附录 C：框架实施路线图

```
Phase 1（3-4 天）：引擎端改造
  ├── --automation-mode + --exit-on-finish
  ├── --project / --report / --create-entity / --save
  ├── app.ping / app.quit / project.open Tool
  └── 无头启动（隐藏窗口）

Phase 2（2-3 天）：Python 控制器
  ├── dse_auto.py run（Batch + API Session）
  ├── env/suite/case YAML 解析
  ├── 变量展开 + 报告输出
  ├── 进程隔离 + 清理 + 超时
  └── 第一个冒烟 suite 跑通

Phase 3（1-2 天）：补充能力
  ├── hierarchy.get_entities / editor.screenshot
  ├── Setup/Teardown + 失败重试
  ├── 截图对比
  └── 3-5 个核心用例落地

Phase 4（远期）：
  ├── stability / soak 子命令
  ├── 并发执行
  ├── CI 集成
  └── 全量用例覆盖
```
