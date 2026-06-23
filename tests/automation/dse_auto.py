#!/usr/bin/env python3
"""DSEngine 编辑器自动化测试控制器 (dse_auto)。

读取 YAML env / suite / case，驱动 ``dsengine-editor.exe`` 完成四类测试：

* Batch（CLI）   —— 每 case 启动独立进程，CLI 参数驱动，校验退出码/产物文件。
* API Session    —— 启动编辑器 + WebSocket JSON-RPC，依次执行 calls[] 并校验。
* Stability      —— 重复运行 suite N 次或按时长循环，检测反复启停异常。
* Soak           —— 单进程长稳，循环操作 + 资源采样，检测内存/资源泄漏。

子命令：``run`` / ``stability`` / ``soak``。详见 tests/automation/README.md。
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time
import traceback
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.stderr.write("缺少依赖 PyYAML，请先 `pip install PyYAML`\n")
    raise

# websocket-client / psutil / pillow / numpy 为可选依赖：
# 仅在用到 API Session / 进程清理 / 截图对比时才要求安装。
try:
    import websocket  # type: ignore
except ImportError:
    websocket = None

try:
    import psutil  # type: ignore
except ImportError:
    psutil = None


# ─── 默认值 ──────────────────────────────────────────────────────────────────

DEFAULT_API_PORT = 9527
DEFAULT_STARTUP_TIMEOUT = 30      # 编辑器启动 + WebSocket 就绪等待（秒）
DEFAULT_EXIT_TIMEOUT = 10         # 编辑器退出等待（秒）
DEFAULT_CASE_TIMEOUT = 120        # 单 case 超时（秒）
DEFAULT_CASE_DELAY = 1.0          # case 间延迟（秒）
EDITOR_PROCESS_NAME = "dsengine-editor"

REPO_ROOT = Path(__file__).resolve().parents[2]


def _now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _ensure_utf8_stdio() -> None:
    """Windows 下 stdout 被重定向到管道时默认用 cp1252，中文日志会抛 UnicodeEncodeError。
    统一切到 utf-8 + replace，保证日志在任何重定向场景下都不崩。"""
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is not None:
            try:
                reconfigure(encoding="utf-8", errors="replace")
            except Exception:
                pass


_ensure_utf8_stdio()


def log(msg: str) -> None:
    try:
        sys.stdout.write(f"[dse_auto] {msg}\n")
    except UnicodeEncodeError:
        sys.stdout.write("[dse_auto] " +
                         msg.encode("ascii", "replace").decode("ascii") + "\n")
    sys.stdout.flush()


# ─── 变量展开 ────────────────────────────────────────────────────────────────

_VAR_RE = re.compile(r"\$\{([^}]+)\}")


class VariableResolver:
    """``${name}`` 变量展开。

    优先级（后者覆盖前者）：
      1. 内置：repo_root / run_id / WORK_ROOT / EDITOR_EXE
      2. 操作系统环境变量
      3. env 文件 variables
      4. 命令行 --var key=value
    支持变量互相引用，迭代展开直至收敛。
    """

    def __init__(self, layers: List[Dict[str, Any]]):
        merged: Dict[str, str] = {}
        for layer in layers:
            for k, v in layer.items():
                if v is None:
                    continue
                merged[str(k)] = str(v)
        self.vars = merged

    def expand(self, value: Any) -> Any:
        if isinstance(value, str):
            return self._expand_str(value)
        if isinstance(value, list):
            return [self.expand(v) for v in value]
        if isinstance(value, dict):
            return {k: self.expand(v) for k, v in value.items()}
        return value

    def _expand_str(self, s: str) -> str:
        for _ in range(10):  # 防止循环引用导致死循环
            new_s = _VAR_RE.sub(self._lookup, s)
            if new_s == s:
                return new_s
            s = new_s
        return s

    def _lookup(self, m: "re.Match[str]") -> str:
        key = m.group(1)
        if key in self.vars:
            return self.vars[key]
        env_val = os.environ.get(key)
        if env_val is not None:
            return env_val
        # 未定义变量保持原样，方便排查
        return m.group(0)


# ─── 取值 / 断言 ─────────────────────────────────────────────────────────────

def deep_get(obj: Any, dotted: str) -> Tuple[bool, Any]:
    """按 ``a.b.c`` 取嵌套值。返回 (found, value)。"""
    cur = obj
    for part in dotted.split("."):
        if isinstance(cur, dict) and part in cur:
            cur = cur[part]
        else:
            return False, None
    return True, cur


def _coerce_ints(value: Any) -> Any:
    """把变量展开后纯整数字符串（如 "524288"）转回 int，
    以便 entity_id / max_frames / api_port 等参数按 JSON 数字下发（工具端要求 IsUint）。"""
    if isinstance(value, dict):
        return {k: _coerce_ints(v) for k, v in value.items()}
    if isinstance(value, list):
        return [_coerce_ints(v) for v in value]
    if isinstance(value, str) and re.fullmatch(r"-?\d+", value):
        return int(value)
    return value


def _loose_eq(actual: Any, expected: Any) -> bool:
    """宽松相等：精确相等优先；否则数值/字符串归一比较。
    用于 capture/变量展开把值带成字符串（如 "5"）后仍能与响应里的 int 5 匹配。"""
    if actual == expected:
        return True
    if isinstance(actual, bool) or isinstance(expected, bool):
        return False
    try:
        return float(actual) == float(expected)
    except (TypeError, ValueError):
        return str(actual) == str(expected)


def match_expect(actual: Any, expected: Any) -> Tuple[bool, str]:
    """单条 expect 匹配。支持精确 / "> N" / "< N" / ">= N" / "包含元素列表"。"""
    if isinstance(expected, str):
        m = re.match(r"^\s*(>=|<=|>|<|==|!=)\s*(-?\d+(?:\.\d+)?)\s*$", expected)
        if m:
            op, num = m.group(1), float(m.group(2))
            try:
                a = float(actual)
            except (TypeError, ValueError):
                return False, f"期望数值比较 {expected}，实际非数值 {actual!r}"
            ok = {
                ">": a > num, "<": a < num, ">=": a >= num,
                "<=": a <= num, "==": a == num, "!=": a != num,
            }[op]
            return ok, "" if ok else f"{a} {op} {num} 不成立"
        ok = _loose_eq(actual, expected)
        return ok, ("" if ok else f"期望 {expected!r}，实际 {actual!r}")
    if isinstance(expected, list):
        if not isinstance(actual, list):
            return False, f"期望列表包含 {expected}，实际非列表 {actual!r}"
        missing = [e for e in expected if e not in actual]
        return (not missing), ("" if not missing else f"缺少元素 {missing}")
    ok = _loose_eq(actual, expected)
    return ok, ("" if ok else f"期望 {expected!r}，实际 {actual!r}")


def evaluate_expect(response: Dict[str, Any], expect: Dict[str, Any]) -> Tuple[bool, List[str]]:
    """对一次 JSON-RPC 响应执行所有 expect 断言。``response`` 形如 {"result": {...}}。"""
    errors: List[str] = []
    for path, expected in expect.items():
        found, actual = deep_get(response, path)
        if not found:
            errors.append(f"{path} 不存在")
            continue
        ok, why = match_expect(actual, expected)
        if not ok:
            errors.append(f"{path}: {why}")
    return (not errors), errors


# ─── 进程管理 ────────────────────────────────────────────────────────────────

def kill_lingering_editors() -> int:
    """清理残留编辑器进程（上次崩溃留下的孤儿）。返回清理个数。"""
    if psutil is None:
        return 0
    killed = 0
    for p in psutil.process_iter(["pid", "name"]):
        name = (p.info.get("name") or "").lower()
        if EDITOR_PROCESS_NAME in name:
            try:
                p.kill()
                killed += 1
            except Exception:
                pass
    return killed


def ensure_terminated(proc: subprocess.Popen, timeout: int = DEFAULT_EXIT_TIMEOUT) -> int:
    """等待进程退出；超时则强杀。返回退出码（强杀返回 None→-9 归一）。"""
    try:
        return proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass
        return proc.returncode if proc.returncode is not None else -9


def wait_for_port(host: str, port: int, timeout: float,
                  proc: Optional[subprocess.Popen] = None) -> bool:
    """轮询等待端口可连接。期间若进程已退出则提前返回 False。"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc is not None and proc.poll() is not None:
            return False
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(1.0)
            if s.connect_ex((host, port)) == 0:
                return True
        time.sleep(0.25)
    return False


# ─── Setup / Teardown 原语 ──────────────────────────────────────────────────

def run_actions(actions: List[Dict[str, Any]], resolver: VariableResolver) -> Tuple[bool, List[str]]:
    """执行 setup/teardown 动作序列。返回 (ok, logs)。"""
    logs: List[str] = []
    for raw in actions or []:
        action = resolver.expand(raw)
        kind = action.get("action")
        try:
            if kind == "create_dir":
                Path(action["path"]).mkdir(parents=True, exist_ok=True)
            elif kind == "remove_dir":
                shutil.rmtree(action["path"], ignore_errors=True)
            elif kind == "copy_file":
                dst = Path(action["dst"])
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(action["src"], dst)
            elif kind == "copy_dir":
                shutil.copytree(action["src"], action["dst"], dirs_exist_ok=True)
            elif kind == "exec_cmd":
                cmd = action["cmd"]
                subprocess.run(cmd, shell=isinstance(cmd, str), check=True,
                               timeout=action.get("timeout", 60))
            elif kind == "sleep":
                time.sleep(float(action.get("seconds", 1)))
            else:
                logs.append(f"未知动作: {kind}")
                return False, logs
            logs.append(f"ok: {kind}")
        except Exception as e:  # noqa: BLE001
            logs.append(f"动作 {kind} 失败: {e}")
            return False, logs
    return True, logs


# ─── 截图对比（RMSE，口径对齐 rhi_pixel_harness.h::ComputeRmse） ──────────────

def compare_screenshot(actual_path: str, baseline_path: str,
                       tolerance: float, diff_output: Optional[str]) -> Tuple[bool, str]:
    """像素 RMSE 对比。similarity = 1 - rmse/255；similarity >= tolerance 视为通过。"""
    try:
        from PIL import Image, ImageChops  # type: ignore
        import numpy as np  # type: ignore
    except ImportError:
        return False, "缺少依赖 pillow/numpy，无法进行截图对比"

    if not os.path.exists(actual_path):
        return False, f"截图文件不存在: {actual_path}"
    if not os.path.exists(baseline_path):
        return False, f"基准图不存在: {baseline_path}"

    a = Image.open(actual_path).convert("RGBA")
    b = Image.open(baseline_path).convert("RGBA")
    if a.size != b.size:
        return False, f"尺寸不一致 actual={a.size} baseline={b.size}"

    arr_a = np.asarray(a, dtype=np.float64)
    arr_b = np.asarray(b, dtype=np.float64)
    rmse = float(np.sqrt(np.mean((arr_a - arr_b) ** 2)))
    similarity = 1.0 - rmse / 255.0

    if diff_output:
        Path(diff_output).parent.mkdir(parents=True, exist_ok=True)
        ImageChops.difference(a.convert("RGB"), b.convert("RGB")).save(diff_output)

    ok = similarity >= tolerance
    return ok, f"similarity={similarity:.4f} (rmse={rmse:.2f}) tolerance={tolerance}"


# ─── WebSocket JSON-RPC 客户端 ──────────────────────────────────────────────

class RpcClient:
    def __init__(self, host: str, port: int, timeout: float = 30.0):
        if websocket is None:
            raise RuntimeError("缺少依赖 websocket-client，请 `pip install websocket-client`")
        self.url = f"ws://{host}:{port}"
        self.timeout = timeout
        self._id = 0
        self.ws = websocket.create_connection(self.url, timeout=timeout)

    def call(self, method: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        self._id += 1
        req = {"jsonrpc": "2.0", "id": self._id, "method": method,
               "params": params or {}}
        self.ws.send(json.dumps(req))
        # 简单同步：读到匹配 id 的响应（编辑器单连接顺序处理）
        while True:
            raw = self.ws.recv()
            resp = json.loads(raw)
            if resp.get("id") == self._id or "error" in resp or "result" in resp:
                return resp

    def close(self) -> None:
        try:
            self.ws.close()
        except Exception:
            pass


# ─── Batch（CLI）Runner ─────────────────────────────────────────────────────

class BatchRunner:
    """每 case 启动独立编辑器进程，CLI 参数驱动，校验退出码 + 产物文件 + 可选报告。"""

    def __init__(self, resolver: VariableResolver, editor_exe: str):
        self.resolver = resolver
        self.editor_exe = editor_exe

    def run_case(self, case: Dict[str, Any], timeout: int) -> Dict[str, Any]:
        cmd = case.get("command", {})
        executable = self.resolver.expand(cmd.get("executable") or self.editor_exe)
        args = [self.resolver.expand(a) for a in cmd.get("args", [])]
        full = [executable] + args
        steps: List[Dict[str, Any]] = []

        try:
            proc = subprocess.Popen(full, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
        except Exception as e:  # noqa: BLE001
            return {"status": "failed", "error": f"启动失败: {e}",
                    "command": full, "steps": steps}

        try:
            out, _ = proc.communicate(timeout=timeout)
            exit_code = proc.returncode
            timed_out = False
        except subprocess.TimeoutExpired:
            proc.kill()
            out, _ = proc.communicate()
            exit_code = proc.returncode if proc.returncode is not None else -9
            timed_out = True

        stdout_text = (out or b"").decode("utf-8", errors="replace")
        assert_cfg = case.get("assert", {}) or {}
        errors: List[str] = []

        if timed_out:
            errors.append(f"超时 {timeout}s 未退出，已强杀")

        # exit_code 断言
        if "exit_code" in assert_cfg:
            exp = assert_cfg["exit_code"]
            ok = (exit_code == exp)
            steps.append({"name": "exit_code", "status": "passed" if ok else "failed",
                          "expected": exp, "actual": exit_code})
            if not ok:
                errors.append(f"退出码 {exit_code} != {exp}")

        # 产物文件存在性断言
        for f in assert_cfg.get("files_exist", []) or []:
            fp = self.resolver.expand(f)
            ok = os.path.exists(fp)
            steps.append({"name": f"file_exists:{os.path.basename(fp)}",
                          "status": "passed" if ok else "failed", "path": fp})
            if not ok:
                errors.append(f"缺少产物文件: {fp}")

        # 可选：编辑器写出的报告 JSON（当前编辑器不写，保留前向兼容）
        report_assert = assert_cfg.get("report")
        if report_assert is not None:
            report_path = self.resolver.expand(cmd.get("report_path", "")) or None
            ok, why = self._verify_report(report_path, report_assert)
            steps.append({"name": "report", "status": "passed" if ok else "failed",
                          "detail": why})
            if not ok:
                errors.append(why)

        return {
            "status": "passed" if not errors else "failed",
            "exit_code": exit_code,
            "command": full,
            "stdout_tail": stdout_text[-2000:],
            "steps": steps,
            "errors": errors,
        }

    def _verify_report(self, report_path: Optional[str], expected: Dict[str, Any]) -> Tuple[bool, str]:
        if not report_path or not os.path.exists(report_path):
            return False, f"报告文件不存在: {report_path}"
        try:
            with open(report_path, encoding="utf-8") as f:
                report = json.load(f)
        except Exception as e:  # noqa: BLE001
            return False, f"报告解析失败: {e}"
        ok, errs = evaluate_expect({"result": report}, {f"result.{k}": v
                                                        for k, v in _flatten(expected)})
        return ok, "; ".join(errs) if errs else "ok"


def _flatten(d: Dict[str, Any], prefix: str = "") -> List[Tuple[str, Any]]:
    out: List[Tuple[str, Any]] = []
    for k, v in d.items():
        key = f"{prefix}.{k}" if prefix else str(k)
        if isinstance(v, dict):
            out.extend(_flatten(v, key))
        else:
            out.append((key, v))
    return out


# ─── API Session Runner ─────────────────────────────────────────────────────

class ApiSessionRunner:
    """每 case 启动独立编辑器进程（headless + api-port），WebSocket 顺序执行 calls[]。

    说明：文档 §2.2.1 提出「多 case 共享一个进程」的会话模型；本实现默认 **每 case 独立进程**
    （与 §4.3 用例自带 startup_args + 末尾 dsengine_editor_quit 的结构一致），隔离性更好、
    崩溃不扩散。suite 可设 ``shared_session: true`` 作为未来增强（当前按独立进程处理）。
    """

    def __init__(self, resolver: VariableResolver, editor_exe: str,
                 host: str = "127.0.0.1"):
        self.resolver = resolver
        self.editor_exe = editor_exe
        self.host = host

    def run_case(self, case: Dict[str, Any], timeout: int) -> Dict[str, Any]:
        cmd = case.get("command", {})
        executable = self.resolver.expand(cmd.get("executable") or self.editor_exe)
        startup_args = [self.resolver.expand(a) for a in cmd.get("startup_args", [])]
        port = int(self.resolver.expand(str(cmd.get("api_port", DEFAULT_API_PORT))))
        calls = cmd.get("calls", []) or []
        steps: List[Dict[str, Any]] = []
        errors: List[str] = []

        full = [executable] + startup_args
        try:
            proc = subprocess.Popen(full, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
        except Exception as e:  # noqa: BLE001
            return {"status": "failed", "error": f"启动失败: {e}",
                    "command": full, "steps": steps}

        client: Optional[RpcClient] = None
        quit_sent = False
        try:
            if not wait_for_port(self.host, port, DEFAULT_STARTUP_TIMEOUT, proc):
                out = self._drain(proc)
                ensure_terminated(proc)
                return {"status": "failed",
                        "error": f"编辑器未在 {DEFAULT_STARTUP_TIMEOUT}s 内就绪（端口 {port}）"
                                 f"，进程退出码={proc.returncode}",
                        "command": full, "stdout_tail": out[-2000:], "steps": steps}

            # 端口就绪后给 ImGui/引擎一点点初始化时间
            time.sleep(0.5)
            client = RpcClient(self.host, port, timeout=timeout)

            for call in calls:
                method = call["method"]
                if method == "dsengine_editor_quit":
                    quit_sent = True
                step = self._run_call(client, call)
                steps.append(step)
                if step["status"] == "failed":
                    errors.append(f"{method}: {step.get('error', '')}")
                    if call.get("stop_on_failure", True) and method != "dsengine_editor_quit":
                        self._capture_failure(client, cmd, steps)
                        break

            # 若用例未显式 quit，则补发以优雅退出
            if not quit_sent and client is not None:
                try:
                    client.call("dsengine_editor_quit")
                except Exception:
                    pass
        finally:
            if client is not None:
                client.close()

        exit_code = ensure_terminated(proc)
        stdout_text = self._drain(proc)

        assert_cfg = case.get("assert", {}) or {}
        if "exit_code" in assert_cfg and exit_code != assert_cfg["exit_code"]:
            errors.append(f"退出码 {exit_code} != {assert_cfg['exit_code']}")

        return {
            "status": "passed" if not errors else "failed",
            "exit_code": exit_code,
            "command": full,
            "stdout_tail": (stdout_text or "")[-2000:],
            "steps": steps,
            "errors": errors,
        }

    def _run_call(self, client: RpcClient, call: Dict[str, Any]) -> Dict[str, Any]:
        method = call["method"]
        params = _coerce_ints(self.resolver.expand(call.get("params", {}) or {}))
        try:
            resp = client.call(method, params)
        except Exception as e:  # noqa: BLE001
            return {"name": method, "status": "failed", "error": f"RPC 异常: {e}"}

        if "error" in resp:
            return {"name": method, "status": "failed",
                    "error": f"JSON-RPC error: {resp['error']}"}

        # capture：把响应中的值（点分路径，如 result.entity_id）存入变量，供后续调用 ${name} 引用
        for var_name, dotted in (call.get("capture", {}) or {}).items():
            found, val = deep_get(resp, dotted)
            if found:
                self.resolver.vars[var_name] = str(val)

        # 截图落盘：若 params.path 存在且响应含 base64 data，则解码写文件
        save_path = params.get("path") if isinstance(params, dict) else None
        if method == "dsengine_editor_screenshot" and save_path:
            self._save_screenshot(resp.get("result", {}), save_path)

        # expect 校验（expect 里的 ${var} 先展开，便于引用 capture/env 变量）
        ok, errs = (True, [])
        if "expect" in call:
            ok, errs = evaluate_expect(resp, self.resolver.expand(call["expect"]))

        # 截图对比
        sc = call.get("screenshot_compare")
        cmp_detail = None
        if sc and save_path:
            cmp_ok, cmp_detail = compare_screenshot(
                self.resolver.expand(save_path),
                self.resolver.expand(sc["baseline"]),
                float(sc.get("tolerance", 0.95)),
                self.resolver.expand(sc["diff_output"]) if sc.get("diff_output") else None,
            )
            if not cmp_ok:
                ok = False
                errs.append(f"截图对比失败: {cmp_detail}")

        step: Dict[str, Any] = {"name": method,
                                "status": "passed" if ok else "failed"}
        if errs:
            step["error"] = "; ".join(errs)
        if cmp_detail:
            step["screenshot_compare"] = cmp_detail
        return step

    def _save_screenshot(self, result: Dict[str, Any], path: str) -> None:
        data = result.get("data")
        if not data:
            return
        try:
            out = self.resolver.expand(path)
            Path(out).parent.mkdir(parents=True, exist_ok=True)
            with open(out, "wb") as f:
                f.write(base64.b64decode(data))
        except Exception as e:  # noqa: BLE001
            log(f"截图保存失败 {path}: {e}")

    def _capture_failure(self, client: RpcClient, cmd: Dict[str, Any],
                         steps: List[Dict[str, Any]]) -> None:
        fpath = cmd.get("failure_screenshot_path")
        if not fpath:
            return
        try:
            resp = client.call("dsengine_editor_screenshot", {})
            self._save_screenshot(resp.get("result", {}), fpath)
            steps.append({"name": "failure_screenshot", "status": "passed",
                          "path": self.resolver.expand(fpath)})
        except Exception as e:  # noqa: BLE001
            steps.append({"name": "failure_screenshot", "status": "failed",
                          "error": str(e)})

    @staticmethod
    def _drain(proc: subprocess.Popen) -> str:
        try:
            if proc.stdout:
                return proc.stdout.read().decode("utf-8", errors="replace")
        except Exception:
            pass
        return ""


# ─── Reporter ────────────────────────────────────────────────────────────────

class Reporter:
    def __init__(self, report_dir: Path):
        self.report_dir = report_dir
        self.report_dir.mkdir(parents=True, exist_ok=True)

    def write_case_report(self, case_id: str, result: Dict[str, Any]) -> None:
        safe = case_id.replace("/", "_").replace("\\", "_")
        with open(self.report_dir / f"{safe}.json", "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2, ensure_ascii=False)

    def write_suite_report(self, suite_id: str, run_id: str,
                           results: List[Dict[str, Any]]) -> Dict[str, Any]:
        total = len(results)
        passed = sum(1 for r in results if r["status"] == "passed")
        failed = sum(1 for r in results if r["status"] == "failed")
        skipped = sum(1 for r in results if r["status"] == "skipped")
        duration = sum(r.get("duration_ms", 0) for r in results)
        report = {
            "suite_id": suite_id,
            "run_id": run_id,
            "status": "passed" if failed == 0 else "failed",
            "generated_at": _now_iso(),
            "summary": {"total": total, "passed": passed, "failed": failed,
                        "skipped": skipped, "duration_ms": duration},
            "cases": results,
        }
        with open(self.report_dir / "suite_report.json", "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        self._write_html(report)
        return report

    def write_named_report(self, name: str, payload: Dict[str, Any]) -> None:
        with open(self.report_dir / name, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2, ensure_ascii=False)

    def _write_html(self, report: Dict[str, Any]) -> None:
        s = report["summary"]
        color = "#2e7d32" if report["status"] == "passed" else "#c62828"
        rows = []
        for c in report["cases"]:
            st = c["status"]
            badge = {"passed": "#2e7d32", "failed": "#c62828",
                     "skipped": "#f9a825"}.get(st, "#555")
            steps_html = ""
            for step in c.get("steps", []):
                sc = "#2e7d32" if step.get("status") == "passed" else "#c62828"
                detail = step.get("error") or step.get("detail") or step.get("screenshot_compare") or ""
                steps_html += (f"<div class='step'><span style='color:{sc}'>● </span>"
                               f"{_esc(step.get('name',''))} "
                               f"<small>{_esc(str(detail))}</small></div>")
            errs = "<br>".join(_esc(e) for e in c.get("errors", []))
            rows.append(
                f"<tr><td><b>{_esc(c.get('case_id',''))}</b><br>"
                f"<small>{_esc(c.get('title',''))}</small></td>"
                f"<td style='color:{badge}'><b>{st}</b></td>"
                f"<td>{c.get('duration_ms',0)} ms</td>"
                f"<td>{steps_html}<div class='err'>{errs}</div></td></tr>"
            )
        html = f"""<!doctype html><html><head><meta charset="utf-8">
<title>{_esc(report['suite_id'])} — {_esc(report['run_id'])}</title>
<style>
body{{font-family:Segoe UI,Arial,sans-serif;margin:24px;color:#222}}
h1{{font-size:20px}} .summary{{font-size:15px;margin:12px 0}}
table{{border-collapse:collapse;width:100%}} th,td{{border:1px solid #ddd;padding:8px;vertical-align:top;text-align:left}}
th{{background:#f5f5f5}} .step{{font-size:13px;margin:2px 0}} .err{{color:#c62828;font-size:13px;margin-top:4px}}
small{{color:#777}}
</style></head><body>
<h1>{_esc(report['suite_id'])} <span style="color:{color}">[{report['status']}]</span></h1>
<div class="summary">run_id=<b>{_esc(report['run_id'])}</b> · 生成于 {report['generated_at']}<br>
total {s['total']} · <span style="color:#2e7d32">passed {s['passed']}</span>
· <span style="color:#c62828">failed {s['failed']}</span>
· skipped {s['skipped']} · {s['duration_ms']} ms</div>
<table><tr><th>Case</th><th>Status</th><th>Duration</th><th>Steps / Errors</th></tr>
{''.join(rows)}</table></body></html>"""
        with open(self.report_dir / "suite_report.html", "w", encoding="utf-8") as f:
            f.write(html)


def _esc(s: str) -> str:
    return (str(s).replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;"))


# ─── 主控制器 ────────────────────────────────────────────────────────────────

class DSETestRunner:
    def __init__(self, env_vars: Dict[str, str], cli_vars: Dict[str, str],
                 editor_exe: str, run_id: str, report_root: Path):
        builtin = {
            "repo_root": str(REPO_ROOT),
            "run_id": run_id,
            "WORK_ROOT": os.environ.get("WORK_ROOT", str(REPO_ROOT / "tests" / "automation")),
            "EDITOR_EXE": editor_exe,
        }
        self.resolver = VariableResolver([builtin, dict(os.environ), env_vars, cli_vars])
        self.editor_exe = editor_exe
        self.run_id = run_id
        self.report_root = report_root
        self.reporter = Reporter(report_root)
        self.batch = BatchRunner(self.resolver, editor_exe)
        self.api = ApiSessionRunner(self.resolver, editor_exe)

    # —— 单 case ——
    def run_case_file(self, case_path: str, timeout: int) -> Dict[str, Any]:
        resolved = self.resolver.expand(case_path)
        if not Path(resolved).is_absolute() and not Path(resolved).exists():
            resolved = str(REPO_ROOT / resolved)
        case = load_yaml(resolved)
        case_id = case.get("case_id", Path(case_path).stem)
        start = time.time()

        setup_ok, setup_logs = run_actions(case.get("setup", []), self.resolver)
        if not setup_ok:
            result = {"case_id": case_id, "title": case.get("title", ""),
                      "status": "skipped", "duration_ms": 0,
                      "errors": ["setup 失败"], "setup_logs": setup_logs, "steps": []}
            self.reporter.write_case_report(case_id, result)
            return result

        ctype = (case.get("command", {}) or {}).get("type", "cli")
        try:
            if ctype == "api_session":
                result = self.api.run_case(case, timeout)
            else:
                result = self.batch.run_case(case, timeout)
        except Exception as e:  # noqa: BLE001
            result = {"status": "failed",
                      "error": f"运行异常: {e}\n{traceback.format_exc()}", "steps": []}

        # teardown（失败不改变 case 状态）
        _, teardown_logs = run_actions(case.get("teardown", []), self.resolver)

        result["case_id"] = case_id
        result["title"] = case.get("title", "")
        result["duration_ms"] = int((time.time() - start) * 1000)
        result["setup_logs"] = setup_logs
        result["teardown_logs"] = teardown_logs
        self.reporter.write_case_report(case_id, result)
        return result

    def run_case_with_retry(self, case_path: str, suite: Dict[str, Any],
                            timeout: int) -> Dict[str, Any]:
        retry = suite.get("retry", {}) or {}
        max_retries = int(retry.get("max_retries", 0))
        retry_status = retry.get("retry_on_status", ["failed"])
        delay = float(retry.get("retry_delay_seconds", 3))
        result = self.run_case_file(case_path, timeout)
        attempt = 0
        while result["status"] in retry_status and attempt < max_retries:
            attempt += 1
            log(f"  重试 {attempt}/{max_retries}: {case_path}")
            time.sleep(delay)
            result = self.run_case_file(case_path, timeout)
            if result["status"] == "passed":
                result["flaky"] = True
        if attempt:
            result["retry_count"] = attempt
        return result

    # —— suite ——
    def run_suite(self, suite: Dict[str, Any], stop_on_failure: bool = False) -> Dict[str, Any]:
        suite_id = suite.get("suite_id", "unnamed-suite")
        cases = suite.get("cases", [])
        case_delay = float(suite.get("case_delay_seconds", DEFAULT_CASE_DELAY))
        case_timeout = int(suite.get("case_timeout_seconds", DEFAULT_CASE_TIMEOUT))
        if suite.get("process_cleanup", True):
            n = kill_lingering_editors()
            if n:
                log(f"清理残留编辑器进程 {n} 个")

        execution = suite.get("execution", {}) or {}
        mode = execution.get("mode", "sequential")
        results: List[Dict[str, Any]] = []

        if mode == "parallel":
            max_workers = int(execution.get("max_workers", 4))
            timeout = int(execution.get("timeout_per_case", case_timeout))
            log(f"并发执行 suite={suite_id} workers={max_workers} cases={len(cases)}")
            with ThreadPoolExecutor(max_workers=max_workers) as ex:
                futures = {ex.submit(self.run_case_with_retry, c, suite, timeout): c
                           for c in cases}
                for fut in as_completed(futures):
                    results.append(fut.result())
            # 保持与 cases 声明顺序一致
            order = {c: i for i, c in enumerate(cases)}
            results.sort(key=lambda r: order.get(r.get("_case_path", ""), 0))
        else:
            for case_path in cases:
                log(f"运行 case: {case_path}")
                result = self.run_case_with_retry(case_path, suite, case_timeout)
                results.append(result)
                log(f"  -> {result['status']} ({result.get('duration_ms',0)} ms)")
                if stop_on_failure and result["status"] == "failed":
                    log("  --stop-on-failure 命中，停止")
                    break
                if case_delay > 0:
                    time.sleep(case_delay)

        report = self.reporter.write_suite_report(suite_id, self.run_id, results)
        return report


# ─── YAML 加载 ───────────────────────────────────────────────────────────────

def load_yaml(path: str) -> Dict[str, Any]:
    with open(path, encoding="utf-8") as f:
        return yaml.safe_load(f) or {}


def load_env(suite: Dict[str, Any], env_override: Optional[str],
             resolver_for_path: Optional[VariableResolver] = None) -> Dict[str, str]:
    env_path = env_override or suite.get("env")
    if not env_path:
        return {}
    if resolver_for_path is not None:
        env_path = resolver_for_path.expand(env_path)
    # 相对路径按 repo_root 解析
    p = Path(env_path)
    if not p.is_absolute():
        p = REPO_ROOT / env_path
    env = load_yaml(str(p))
    return env.get("variables", {}) or {}


def resolve_editor_exe(cli_exe: Optional[str], env_vars: Dict[str, str]) -> str:
    exe = cli_exe or os.environ.get("EDITOR_EXE") or env_vars.get("editor_exe")
    if exe:
        # env 文件里通常写成 ${EDITOR_EXE}；若该环境变量未设置会留下未展开占位符，
        # 按 OS 环境变量再解一遍，解不出则视为缺省。
        exe = _VAR_RE.sub(lambda m: os.environ.get(m.group(1), ""), exe).strip()
    if not exe:
        # 缺省回退到本仓构建产物
        exe = str(REPO_ROOT / "bin" / "dsengine-editor.exe")
    return exe


def parse_vars(pairs: List[str]) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for pair in pairs or []:
        if "=" not in pair:
            raise SystemExit(f"--var 需要 key=value 形式: {pair}")
        k, v = pair.split("=", 1)
        out[k.strip()] = v
    return out


def build_runner(args, suite: Dict[str, Any], run_id: str) -> DSETestRunner:
    cli_vars = parse_vars(getattr(args, "var", []) or [])
    if getattr(args, "work_root", None):
        os.environ["WORK_ROOT"] = args.work_root
    env_vars = load_env(suite, getattr(args, "env", None))
    editor_exe = resolve_editor_exe(getattr(args, "editor_exe", None), env_vars)

    # report_root：env 里若定义则用，否则默认 WORK_ROOT/reports/<run_id>
    tmp_resolver = VariableResolver([
        {"repo_root": str(REPO_ROOT), "run_id": run_id,
         "WORK_ROOT": os.environ.get("WORK_ROOT", str(REPO_ROOT / "tests" / "automation")),
         "EDITOR_EXE": editor_exe},
        dict(os.environ), env_vars, cli_vars])
    report_root_raw = env_vars.get("report_root") or "${WORK_ROOT}/reports/${run_id}"
    report_root = Path(tmp_resolver.expand(report_root_raw))
    return DSETestRunner(env_vars, cli_vars, editor_exe, run_id, report_root)


# ─── 子命令 ──────────────────────────────────────────────────────────────────

def cmd_run(args) -> int:
    suite = load_yaml(args.suite)
    if suite.get("quarantine") and not args.include_quarantine:
        log(f"suite 标记 quarantine，跳过（加 --include-quarantine 强制运行）: {args.suite}")
        return 0
    runner = build_runner(args, suite, args.run_id)
    log(f"editor_exe = {runner.editor_exe}")
    log(f"report_root = {runner.report_root}")
    report = runner.run_suite(suite, stop_on_failure=args.stop_on_failure)
    log(f"SUITE {report['status'].upper()} — {report['summary']}")
    log(f"报告: {runner.report_root / 'suite_report.html'}")
    return 0 if report["status"] == "passed" else 1


def cmd_stability(args) -> int:
    suite = load_yaml(args.suite)
    runner = build_runner(args, suite, args.run_id)
    rounds: List[Dict[str, Any]] = []
    start = time.time()
    iteration = 0
    delay = float(args.delay_seconds or 0)
    log(f"稳定性循环 suite={suite.get('suite_id')} "
        f"iterations={args.iterations} duration={args.duration_seconds}")

    while True:
        iteration += 1
        # 每轮使用独立报告子目录
        sub = runner.report_root / f"round_{iteration:04d}"
        runner.reporter = Reporter(sub)
        report = runner.run_suite(suite, stop_on_failure=False)
        rounds.append({"iteration": iteration, "status": report["status"],
                       "summary": report["summary"]})
        log(f"round {iteration}: {report['status']}")
        if args.iterations and iteration >= args.iterations:
            break
        if args.duration_seconds and (time.time() - start) >= args.duration_seconds:
            break
        if delay > 0:
            time.sleep(delay)

    passed = sum(1 for r in rounds if r["status"] == "passed")
    summary = {
        "suite_id": suite.get("suite_id"),
        "run_id": args.run_id,
        "status": "passed" if passed == len(rounds) else "failed",
        "generated_at": _now_iso(),
        "total_rounds": len(rounds),
        "passed_rounds": passed,
        "failed_rounds": len(rounds) - passed,
        "elapsed_seconds": int(time.time() - start),
        "rounds": rounds,
    }
    Reporter(runner.report_root).write_named_report("stability_report.json", summary)
    log(f"STABILITY {summary['status'].upper()} — {passed}/{len(rounds)} rounds passed")
    return 0 if summary["status"] == "passed" else 1


def cmd_soak(args) -> int:
    case = load_yaml(args.case)
    # soak 复用 ApiSession 的进程驱动，但单进程长稳：循环执行 loop_calls[]
    cli_vars = parse_vars(getattr(args, "var", []) or [])
    if getattr(args, "work_root", None):
        os.environ["WORK_ROOT"] = args.work_root
    env_vars = load_env(case, getattr(args, "env", None))
    editor_exe = resolve_editor_exe(getattr(args, "editor_exe", None), env_vars)
    tmp_resolver = VariableResolver([
        {"repo_root": str(REPO_ROOT), "run_id": args.run_id,
         "WORK_ROOT": os.environ.get("WORK_ROOT", str(REPO_ROOT / "tests" / "automation")),
         "EDITOR_EXE": editor_exe}, dict(os.environ), env_vars, cli_vars])
    report_root = Path(tmp_resolver.expand(
        env_vars.get("report_root") or "${WORK_ROOT}/reports/${run_id}"))
    runner = DSETestRunner(env_vars, cli_vars, editor_exe, args.run_id, report_root)
    return run_soak(runner, case, args.duration_seconds)


def run_soak(runner: DSETestRunner, case: Dict[str, Any], duration_seconds: int) -> int:
    cmd = case.get("command", {})
    resolver = runner.resolver
    executable = resolver.expand(cmd.get("executable") or runner.editor_exe)
    startup_args = [resolver.expand(a) for a in cmd.get("startup_args", [])]
    port = int(resolver.expand(str(cmd.get("api_port", DEFAULT_API_PORT))))
    setup_calls = cmd.get("setup_calls", []) or []
    loop_calls = cmd.get("loop_calls", []) or cmd.get("calls", []) or []
    sample_interval = float(cmd.get("sample_interval_seconds", 5))

    kill_lingering_editors()
    full = [executable] + startup_args
    log(f"soak 启动: {' '.join(full)} (duration={duration_seconds}s)")
    proc = subprocess.Popen(full, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    samples: List[Dict[str, Any]] = []
    iterations = 0
    crashed = False
    error_msg = ""
    api = ApiSessionRunner(resolver, runner.editor_exe)
    client: Optional[RpcClient] = None
    ps_proc = psutil.Process(proc.pid) if psutil else None
    start = time.time()
    last_sample = 0.0

    try:
        if not wait_for_port("127.0.0.1", port, DEFAULT_STARTUP_TIMEOUT, proc):
            error_msg = f"编辑器未就绪（端口 {port}），退出码={proc.returncode}"
            crashed = True
        else:
            time.sleep(0.5)
            client = RpcClient("127.0.0.1", port, timeout=30)
            for call in setup_calls:
                api._run_call(client, resolver.expand(call))

            while (time.time() - start) < duration_seconds:
                if proc.poll() is not None:
                    crashed = True
                    error_msg = f"进程在第 {iterations} 次迭代后退出，码={proc.returncode}"
                    break
                for call in loop_calls:
                    api._run_call(client, call)
                iterations += 1

                now = time.time()
                if now - last_sample >= sample_interval:
                    last_sample = now
                    sample = {"t": round(now - start, 1), "iteration": iterations}
                    if ps_proc is not None:
                        try:
                            sample["rss_mb"] = round(ps_proc.memory_info().rss / 1e6, 2)
                            sample["num_handles"] = getattr(
                                ps_proc, "num_handles", lambda: None)()
                        except Exception:
                            pass
                    try:
                        m = client.call("dsengine_editor_get_metrics")
                        if "result" in m:
                            sample["metrics"] = m["result"]
                    except Exception:
                        pass
                    samples.append(sample)
                    log(f"  sample t={sample['t']}s iter={iterations} "
                        f"rss={sample.get('rss_mb')}MB")
    except Exception as e:  # noqa: BLE001
        crashed = True
        error_msg = f"soak 异常: {e}"
    finally:
        if client is not None:
            try:
                client.call("dsengine_editor_quit")
            except Exception:
                pass
            client.close()
        ensure_terminated(proc)

    # 内存泄漏粗判：尾段 RSS 较首段增长超过 50% 视为可疑
    leak_suspect = False
    rss_vals = [s["rss_mb"] for s in samples if "rss_mb" in s]
    if len(rss_vals) >= 4:
        head = sum(rss_vals[:2]) / 2
        tail = sum(rss_vals[-2:]) / 2
        if head > 0 and (tail - head) / head > 0.5:
            leak_suspect = True

    summary = {
        "case_id": case.get("case_id", Path("soak").stem),
        "run_id": runner.run_id,
        "status": "failed" if crashed else "passed",
        "generated_at": _now_iso(),
        "duration_seconds": int(time.time() - start),
        "iterations": iterations,
        "crashed": crashed,
        "error": error_msg,
        "leak_suspect": leak_suspect,
        "samples": samples,
    }
    Reporter(runner.report_root).write_named_report("soak_report.json", summary)
    log(f"SOAK {summary['status'].upper()} — iterations={iterations} "
        f"crashed={crashed} leak_suspect={leak_suspect}")
    return 0 if not crashed else 1


# ─── argparse ────────────────────────────────────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="dse_auto", description="DSEngine 编辑器自动化测试控制器")
    sub = p.add_subparsers(dest="command", required=True)

    def common(sp):
        sp.add_argument("--env")
        sp.add_argument("--editor-exe", dest="editor_exe")
        sp.add_argument("--work-root", dest="work_root")
        sp.add_argument("--var", action="append", default=[])

    pr = sub.add_parser("run", help="执行测试套件")
    pr.add_argument("--suite", required=True)
    pr.add_argument("--run-id", dest="run_id", required=True)
    pr.add_argument("--stop-on-failure", dest="stop_on_failure", action="store_true")
    pr.add_argument("--include-quarantine", dest="include_quarantine", action="store_true")
    common(pr)
    pr.set_defaults(func=cmd_run)

    ps = sub.add_parser("stability", help="稳定性循环")
    ps.add_argument("--suite", required=True)
    ps.add_argument("--run-id", dest="run_id", required=True)
    ps.add_argument("--iterations", type=int, default=0)
    ps.add_argument("--duration-seconds", dest="duration_seconds", type=int, default=0)
    ps.add_argument("--delay-seconds", dest="delay_seconds", type=float, default=2)
    common(ps)
    ps.set_defaults(func=cmd_stability)

    pk = sub.add_parser("soak", help="驻留型长稳")
    pk.add_argument("--case", required=True)
    pk.add_argument("--run-id", dest="run_id", required=True)
    pk.add_argument("--duration-seconds", dest="duration_seconds", type=int, required=True)
    common(pk)
    pk.set_defaults(func=cmd_soak)
    return p


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    if args.command == "stability" and not args.iterations and not args.duration_seconds:
        raise SystemExit("stability 需要 --iterations 或 --duration-seconds 之一")
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
