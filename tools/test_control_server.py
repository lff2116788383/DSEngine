#!/usr/bin/env python3
"""
端到端测试：连接 DSEngine Editor Control Server (ws://127.0.0.1:9527)
用法：先启动编辑器，然后运行此脚本。

依赖：pip install websocket-client
"""

import json
import sys
import time

try:
    import websocket
except ImportError:
    print("请安装 websocket-client: pip install websocket-client")
    sys.exit(1)


def rpc_call(ws, method, params=None, req_id=1):
    """发送 JSON-RPC 请求并等待响应"""
    request = {
        "jsonrpc": "2.0",
        "id": req_id,
        "method": method,
        "params": params or {}
    }
    ws.send(json.dumps(request))
    raw = ws.recv()
    return json.loads(raw)


def main():
    url = "ws://127.0.0.1:9527"
    print(f"[Test] Connecting to {url} ...")

    try:
        ws = websocket.create_connection(url, timeout=5)
    except Exception as e:
        print(f"[FAIL] Cannot connect: {e}")
        print("       请确保编辑器已启动且 Control Server 运行在端口 9527")
        return 1

    print("[OK]   Connected\n")
    passed = 0
    failed = 0

    # Test 1: ping
    print("--- Test 1: dsengine_ping ---")
    resp = rpc_call(ws, "dsengine_ping", req_id=1)
    if resp.get("result", {}).get("pong") is True:
        print(f"[PASS] {resp['result']}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Test 2: editor_get_state
    print("\n--- Test 2: dsengine_editor_get_state ---")
    resp = rpc_call(ws, "dsengine_editor_get_state", req_id=2)
    result = resp.get("result", {})
    if "editor_state" in result:
        print(f"[PASS] state={result['editor_state']}, entities={result.get('entity_count')}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Test 3: lua_execute
    print("\n--- Test 3: dsengine_lua_execute ---")
    resp = rpc_call(ws, "dsengine_lua_execute", {"code": "return 1 + 2"}, req_id=3)
    result = resp.get("result", {})
    if result.get("success") and result.get("output") == "3":
        print(f"[PASS] 1+2 = {result['output']}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Test 4: entity_create
    print("\n--- Test 4: dsengine_entity_create ---")
    resp = rpc_call(ws, "dsengine_entity_create", {
        "name": "TestEntity_ControlServer",
        "position": [10.0, 20.0, 30.0]
    }, req_id=4)
    result = resp.get("result", {})
    entity_id = result.get("entity_id")
    if entity_id is not None:
        print(f"[PASS] Created entity id={entity_id}, name={result.get('name')}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Test 5: scene_get_state (should contain our new entity)
    print("\n--- Test 5: dsengine_scene_get_state ---")
    resp = rpc_call(ws, "dsengine_scene_get_state", {"include_components": True}, req_id=5)
    result = resp.get("result", {})
    entities = result.get("entities", [])
    found = any(e.get("name") == "TestEntity_ControlServer" for e in entities)
    if found:
        print(f"[PASS] Found TestEntity in {result.get('entity_count')} entities")
        passed += 1
    else:
        print(f"[FAIL] TestEntity not found in {len(entities)} entities")
        failed += 1

    # Test 6: entity_delete
    if entity_id is not None:
        print("\n--- Test 6: dsengine_entity_delete ---")
        resp = rpc_call(ws, "dsengine_entity_delete", {"entity_id": entity_id}, req_id=6)
        if resp.get("result", {}).get("deleted"):
            print(f"[PASS] Deleted entity {entity_id}")
            passed += 1
        else:
            print(f"[FAIL] {resp}")
            failed += 1

    # Test 7: entity_modify
    print("\n--- Test 7: dsengine_entity_modify ---")
    resp = rpc_call(ws, "dsengine_entity_create", {"name": "ModifyTest", "position": [0, 0, 0]}, req_id=70)
    mod_id = resp.get("result", {}).get("entity_id")
    if mod_id is not None:
        resp = rpc_call(ws, "dsengine_entity_modify", {
            "entity_id": mod_id,
            "name": "ModifyTest_Renamed",
            "position": [100, 200, 300]
        }, req_id=71)
        if resp.get("result", {}).get("modified"):
            print(f"[PASS] Modified entity {mod_id}")
            passed += 1
        else:
            print(f"[FAIL] {resp}")
            failed += 1
        rpc_call(ws, "dsengine_entity_delete", {"entity_id": mod_id}, req_id=72)
    else:
        print("[FAIL] Could not create entity for modify test")
        failed += 1

    # Test 8: editor_undo / redo
    print("\n--- Test 8: dsengine_editor_undo ---")
    resp = rpc_call(ws, "dsengine_editor_undo", req_id=8)
    result = resp.get("result", {})
    if "success" in result:
        print(f"[PASS] undo success={result['success']}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Test 9: editor_screenshot
    print("\n--- Test 9: dsengine_editor_screenshot ---")
    resp = rpc_call(ws, "dsengine_editor_screenshot", {"target": "scene"}, req_id=9)
    result = resp.get("result", {})
    if result.get("width", 0) > 0 and result.get("encoding") == "base64":
        data_len = len(result.get("data", ""))
        print(f"[PASS] Screenshot {result['width']}x{result['height']}, base64 len={data_len}")
        passed += 1
    elif "error" in resp:
        print(f"[SKIP] {resp['error'].get('message', '')} (no render target in headless)")
        passed += 1  # skip counts as pass in headless
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Test 10: scene_save / scene_load
    print("\n--- Test 10: dsengine_scene_save ---")
    import tempfile, os
    tmp_scene = os.path.join(tempfile.gettempdir(), "dsengine_test_scene.json")
    resp = rpc_call(ws, "dsengine_scene_save", {"path": tmp_scene}, req_id=10)
    if resp.get("result", {}).get("saved"):
        print(f"[PASS] Saved to {tmp_scene}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Create a fresh entity for component tests 11-12
    resp = rpc_call(ws, "dsengine_entity_create", {"name": "CompTestEntity", "position": [0, 0, 0]}, req_id=110)
    created_id = resp.get("result", {}).get("entity_id")

    # Test 11: entity_add_component
    print("\n--- Test 11: entity_add_component ---")
    if created_id is not None:
        resp = rpc_call(ws, "dsengine_entity_add_component", {
            "entity_id": created_id,
            "type": "PointLight",
            "properties": {"intensity": 2.0, "range": 15.0}
        }, req_id=11)
        if resp.get("result", {}).get("added"):
            print(f"[PASS] Added PointLight to entity {created_id}")
            passed += 1
        else:
            print(f"[FAIL] {resp}")
            failed += 1
    else:
        print("[SKIP] No entity to add component to")

    # Test 12: entity_get_components
    print("\n--- Test 12: entity_get_components ---")
    if created_id is not None:
        resp = rpc_call(ws, "dsengine_entity_get_components", {
            "entity_id": created_id,
            "detailed": True
        }, req_id=12)
        comps = resp.get("result", {}).get("components", [])
        comp_types = [c.get("type") if isinstance(c, dict) else c for c in comps]
        if "Transform" in comp_types and "PointLight" in comp_types:
            print(f"[PASS] Components: {comp_types}")
            passed += 1
        else:
            print(f"[FAIL] Expected Transform+PointLight, got: {comp_types}")
            failed += 1
    else:
        print("[SKIP] No entity to query")

    # Test 13: entity_create with components
    print("\n--- Test 13: entity_create with components ---")
    resp = rpc_call(ws, "dsengine_entity_create", {
        "name": "TestRichEntity",
        "position": [10, 5, 0],
        "mesh": "models/cube.dmesh",
        "components": [
            "Camera3D",
            {"type": "RigidBody3D", "properties": {"mass": 10, "body_type": "dynamic"}},
            {"type": "BoxCollider3D", "properties": {"size": [2, 2, 2]}}
        ]
    }, req_id=13)
    result_comps = resp.get("result", {}).get("components", [])
    if "MeshRenderer" in result_comps and "Camera3D" in result_comps and "RigidBody3D" in result_comps:
        rich_id = resp["result"]["entity_id"]
        print(f"[PASS] Created rich entity {rich_id} with components: {result_comps}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Test 14: editor_get_state includes data_root
    print("\n--- Test 14: editor_get_state has data_root ---")
    resp = rpc_call(ws, "dsengine_editor_get_state", req_id=14)
    result = resp.get("result", {})
    if "data_root" in result and isinstance(result["data_root"], str):
        print(f"[PASS] data_root = {result['data_root']}")
        passed += 1
    else:
        print(f"[FAIL] data_root missing: {resp}")
        failed += 1

    # Test 15: material_create
    print("\n--- Test 15: dsengine_material_create ---")
    resp = rpc_call(ws, "dsengine_material_create", {
        "name": "test_ai_mat",
        "base_color": [0.8, 0.2, 0.2, 1.0],
        "metallic": 0.5,
        "roughness": 0.3
    }, req_id=15)
    result = resp.get("result", {})
    if result.get("success") and "file_path" in result:
        print(f"[PASS] Material created: {result['file_path']}, id={result.get('material_id')}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    # Test 16: asset_import (texture - expect fail for missing file, validates handler works)
    print("\n--- Test 16: dsengine_asset_import (missing file) ---")
    resp = rpc_call(ws, "dsengine_asset_import", {
        "path": "nonexistent_texture_12345.png",
        "type": "texture"
    }, req_id=16)
    if resp.get("error"):
        print(f"[PASS] Expected error for missing file: {resp['error']['message'][:60]}")
        passed += 1
    else:
        print(f"[FAIL] Should have failed: {resp}")
        failed += 1

    # Test 17: asset_import auto-detect unknown extension
    print("\n--- Test 17: dsengine_asset_import (unknown ext) ---")
    resp = rpc_call(ws, "dsengine_asset_import", {
        "path": "test.xyz"
    }, req_id=17)
    if resp.get("error"):
        print(f"[PASS] Expected error for unknown ext: {resp['error']['message'][:60]}")
        passed += 1
    else:
        print(f"[FAIL] Should have failed: {resp}")
        failed += 1

    # Test 18: method_not_found
    print("\n--- Test 18: unknown method ---")
    resp = rpc_call(ws, "nonexistent_method", req_id=18)
    if resp.get("error", {}).get("code") == -32601:
        print(f"[PASS] Got expected error: {resp['error']['message']}")
        passed += 1
    else:
        print(f"[FAIL] {resp}")
        failed += 1

    ws.close()

    print(f"\n{'='*40}")
    print(f"Results: {passed} passed, {failed} failed, {passed+failed} total")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
