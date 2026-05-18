#!/usr/bin/env python3
"""
Hello World Plugin for DSEngine.
Connects to the Control Server and creates a test entity.
"""

import json
import time
import sys

try:
    import websocket
except ImportError:
    print("[HelloWorld] Error: pip install websocket-client", file=sys.stderr)
    sys.exit(1)


def main():
    url = "ws://127.0.0.1:9527"
    print(f"[HelloWorld] Connecting to {url}...", file=sys.stderr)

    try:
        ws = websocket.create_connection(url, timeout=10)
    except Exception as e:
        print(f"[HelloWorld] Connection failed: {e}", file=sys.stderr)
        sys.exit(1)

    # Ping
    ws.send(json.dumps({"jsonrpc": "2.0", "id": 1, "method": "dsengine_ping"}))
    resp = json.loads(ws.recv())
    print(f"[HelloWorld] Ping response: {resp.get('result')}", file=sys.stderr)

    # Create entity
    ws.send(json.dumps({
        "jsonrpc": "2.0", "id": 2,
        "method": "dsengine_entity_create",
        "params": {
            "name": "HelloWorld_PluginEntity",
            "position": [0, 5, 0]
        }
    }))
    resp = json.loads(ws.recv())
    print(f"[HelloWorld] Created entity: {resp.get('result')}", file=sys.stderr)

    # Keep alive (plugin stays running until stopped)
    print("[HelloWorld] Plugin running. Press Ctrl+C to stop.", file=sys.stderr)
    try:
        while True:
            time.sleep(5)
            ws.send(json.dumps({"jsonrpc": "2.0", "id": 99, "method": "dsengine_ping"}))
            ws.recv()
    except (KeyboardInterrupt, Exception):
        pass
    finally:
        ws.close()
        print("[HelloWorld] Plugin stopped.", file=sys.stderr)


if __name__ == "__main__":
    main()
