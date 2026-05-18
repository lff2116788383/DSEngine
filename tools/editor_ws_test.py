"""编辑器 WebSocket JSON-RPC 端到端集成测试

用法:
    python tools/editor_ws_test.py
    python tools/editor_ws_test.py --port=9527 --timeout=30 --frames=600

依赖: Python 3.9+ / websockets (pip install websockets)

流程:
  1. 查找并启动 dsengine-editor.exe --headless --max-frames=<N>
  2. 轮询连接 ws://127.0.0.1:<port>，等待服务就绪（最多 <timeout> 秒）
  3. 运行完整 JSON-RPC 链路：
       dsengine_ping
       dsengine_entity_create
       dsengine_scene_get_state（验证实体存在）
       dsengine_entity_modify（名称 + modify_components 批量）
       dsengine_entity_delete
       dsengine_scene_get_state（验证实体已删除）
       dsengine_editor_get_state
  4. 关闭连接，终止进程，打印汇总
"""

import argparse
import asyncio
import json
import os
import pathlib
import subprocess
import sys
import time

# Windows stdout 编码修复
if sys.platform == "win32":
    os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")
    for _s in (sys.stdout, sys.stderr):
        if _s and hasattr(_s, "reconfigure"):
            try:
                _s.reconfigure(encoding="utf-8", errors="replace")
            except Exception:
                pass

ROOT = pathlib.Path(__file__).resolve().parent.parent
BIN_DIR = ROOT / "bin"

_next_id = 0


def _make_request(method: str, params: dict) -> str:
    global _next_id
    _next_id += 1
    return json.dumps({
        "jsonrpc": "2.0",
        "id": _next_id,
        "method": method,
        "params": params,
    })


def find_editor_exe() -> pathlib.Path | None:
    candidates = [
        BIN_DIR / "dsengine-editor.exe",
        BIN_DIR / "Debug" / "dsengine-editor.exe",
        BIN_DIR / "Release" / "dsengine-editor.exe",
        BIN_DIR / "RelWithDebInfo" / "dsengine-editor.exe",
    ]
    for c in candidates:
        if c.exists():
            return c
    return None


async def wait_for_ws(url: str, timeout: float) -> object:
    """轮询直到 WebSocket 服务就绪，返回连接或抛出异常。"""
    try:
        import websockets
    except ImportError:
        print("ERROR: websockets not installed. Run: pip install websockets")
        sys.exit(1)

    deadline = time.monotonic() + timeout
    last_exc = None
    attempt = 0
    while time.monotonic() < deadline:
        attempt += 1
        try:
            ws = await websockets.connect(url, open_timeout=2)
            print(f"  Connected to {url} (attempt {attempt})")
            return ws
        except Exception as exc:
            last_exc = exc
            await asyncio.sleep(0.5)
    raise TimeoutError(
        f"Could not connect to {url} within {timeout}s. Last error: {last_exc}"
    )


async def rpc_call(ws, method: str, params: dict, recv_timeout: float = 10.0) -> dict:
    """发送一条 JSON-RPC 请求并等待响应，返回整个响应 dict。"""
    req = _make_request(method, params)
    await ws.send(req)
    raw = await asyncio.wait_for(ws.recv(), timeout=recv_timeout)
    return json.loads(raw)


def assert_ok(resp: dict, label: str) -> dict:
    """断言响应无错误，返回 result 字段。"""
    if "error" in resp:
        raise AssertionError(
            f"[FAIL] {label}: got error {resp['error']}"
        )
    result = resp.get("result", {})
    print(f"  [OK] {label}: {json.dumps(result)[:120]}")
    return result


async def run_tests(port: int, connect_timeout: float, recv_timeout: float) -> bool:
    """执行完整测试链路，返回是否全部通过。"""
    url = f"ws://127.0.0.1:{port}"
    passed = 0
    failed = 0

    ws = await wait_for_ws(url, connect_timeout)
    try:
        # ── 1. ping ──────────────────────────────────────────────────────────
        resp = await rpc_call(ws, "dsengine_ping", {}, recv_timeout)
        result = assert_ok(resp, "dsengine_ping")
        assert result.get("pong") is True or "pong" in result, \
            f"ping result missing 'pong': {result}"
        passed += 1

        # ── 2. entity_create ─────────────────────────────────────────────────
        resp = await rpc_call(ws, "dsengine_entity_create",
                              {"name": "WSTestEntity", "position": [1.0, 2.0, 3.0]},
                              recv_timeout)
        result = assert_ok(resp, "dsengine_entity_create")
        entity_id = result.get("entity_id")
        assert entity_id is not None, f"entity_create returned no entity_id: {result}"
        passed += 1

        # ── 3. scene_get_state（验证实体存在）────────────────────────────────
        resp = await rpc_call(ws, "dsengine_scene_get_state", {}, recv_timeout)
        result = assert_ok(resp, "dsengine_scene_get_state (after create)")
        entity_count = result.get("entity_count", 0)
        assert entity_count >= 1, \
            f"entity_count expected >=1, got {entity_count}"
        passed += 1

        # ── 4. entity_modify：name + modify_components 批量 ──────────────────
        resp = await rpc_call(ws, "dsengine_entity_modify", {
            "entity_id": entity_id,
            "name": "WSTestEntity_Renamed",
        }, recv_timeout)
        result = assert_ok(resp, "dsengine_entity_modify (rename)")
        assert result.get("modified") is True, \
            f"entity_modify 'modified' expected true: {result}"
        passed += 1

        # ── 5. entity_add_component + modify_components 批量 ─────────────────
        resp = await rpc_call(ws, "dsengine_entity_add_component", {
            "entity_id": entity_id,
            "type": "PointLight",
            "properties": {},
        }, recv_timeout)
        assert_ok(resp, "dsengine_entity_add_component (PointLight)")

        resp = await rpc_call(ws, "dsengine_entity_add_component", {
            "entity_id": entity_id,
            "type": "DirectionalLight",
            "properties": {},
        }, recv_timeout)
        assert_ok(resp, "dsengine_entity_add_component (DirectionalLight)")

        resp = await rpc_call(ws, "dsengine_entity_modify", {
            "entity_id": entity_id,
            "modify_components": [
                {"type": "PointLight",       "properties": {"intensity": 3.5, "range": 12.0}},
                {"type": "DirectionalLight", "properties": {"intensity": 1.8}},
            ],
        }, recv_timeout)
        result = assert_ok(resp, "dsengine_entity_modify (modify_components batch)")
        passed += 1

        # ── 6. entity_delete ─────────────────────────────────────────────────
        resp = await rpc_call(ws, "dsengine_entity_delete",
                              {"entity_id": entity_id}, recv_timeout)
        result = assert_ok(resp, "dsengine_entity_delete")
        assert result.get("deleted") is True, \
            f"entity_delete 'deleted' expected true: {result}"
        passed += 1

        # ── 7. scene_get_state（验证实体已删除）──────────────────────────────
        resp = await rpc_call(ws, "dsengine_scene_get_state", {}, recv_timeout)
        result = assert_ok(resp, "dsengine_scene_get_state (after delete)")
        entity_count_after = result.get("entity_count", -1)
        assert entity_count_after < entity_count, (
            f"entity_count should decrease after delete: "
            f"before={entity_count}, after={entity_count_after}"
        )
        passed += 1

        # ── 8. editor_get_state ───────────────────────────────────────────────
        resp = await rpc_call(ws, "dsengine_editor_get_state", {}, recv_timeout)
        result = assert_ok(resp, "dsengine_editor_get_state")
        assert result.get("editor_state") == "edit", \
            f"editor_state expected 'edit': {result}"
        passed += 1

    except AssertionError as exc:
        print(f"  {exc}")
        failed += 1
    except asyncio.TimeoutError:
        print("  [FAIL] recv timeout — editor did not respond in time")
        failed += 1
    except Exception as exc:
        print(f"  [FAIL] Unexpected exception: {exc}")
        failed += 1
    finally:
        await ws.close()

    print(f"\n{'='*60}")
    print(f"WS E2E Tests: {passed} passed, {failed} failed")
    return failed == 0


async def main_async(args) -> int:
    exe = find_editor_exe()
    if not exe:
        print("ERROR: dsengine-editor.exe not found in bin/")
        return 1

    print(f"Editor: {exe}")
    print(f"Port:   {args.port}")
    print(f"Frames: {args.frames}")
    print(f"Timeout for WS connect: {args.timeout}s\n")

    cmd = [
        str(exe),
        "--headless",
        f"--max-frames={args.frames}",
    ]
    print(f"Starting: {' '.join(cmd)}")

    proc = subprocess.Popen(
        cmd,
        cwd=str(BIN_DIR),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
    )

    try:
        success = await run_tests(
            port=args.port,
            connect_timeout=float(args.timeout),
            recv_timeout=10.0,
        )
    except TimeoutError as exc:
        print(f"\nERROR: {exc}")
        success = False
    finally:
        proc.terminate()
        try:
            out, _ = proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, _ = proc.communicate()
        except ValueError:
            out = ""
        if not success and out:
            print("\n--- Editor stdout (last 800 chars) ---")
            print((out or "")[-800:])

    return 0 if success else 1


def main():
    parser = argparse.ArgumentParser(
        description="DSEngine Editor WebSocket JSON-RPC end-to-end test"
    )
    parser.add_argument("--port", type=int, default=9527,
                        help="ControlServer WebSocket port (default 9527)")
    parser.add_argument("--timeout", type=int, default=30,
                        help="Seconds to wait for WS server to be ready (default 30)")
    parser.add_argument("--frames", type=int, default=600,
                        help="--max-frames passed to editor (default 600)")
    args = parser.parse_args()
    sys.exit(asyncio.run(main_async(args)))


if __name__ == "__main__":
    main()
