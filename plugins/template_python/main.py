#!/usr/bin/env python3
"""
Batch Rename Plugin for DSEngine.

Connects to the Editor Control Server via WebSocket and provides batch entity
renaming functionality. Demonstrates:
  - Connecting to Control Server (ws://127.0.0.1:9527)
  - Querying scene state
  - Modifying entities
  - Keeping a long-running plugin alive with heartbeat

Usage:
    python main.py [--pattern PATTERN] [--replace REPLACE] [--prefix PREFIX]

Examples:
    python main.py --pattern "Enemy" --replace "Mob"
    python main.py --prefix "Level1_"
"""

import argparse
import json
import re
import sys
import time

try:
    import websocket
except ImportError:
    print("[BatchRename] Error: pip install websocket-client", file=sys.stderr)
    sys.exit(1)

CONTROL_SERVER_URL = "ws://127.0.0.1:9527"


class DSEngineClient:
    """Minimal JSON-RPC client for DSEngine Control Server."""

    def __init__(self, url=CONTROL_SERVER_URL):
        self.url = url
        self.ws = None
        self._id = 0

    def connect(self):
        print(f"[BatchRename] Connecting to {self.url}...", file=sys.stderr)
        self.ws = websocket.create_connection(self.url, timeout=10)
        print("[BatchRename] Connected.", file=sys.stderr)

    def call(self, method, params=None):
        """Send a JSON-RPC request and return the result."""
        self._id += 1
        request = {
            "jsonrpc": "2.0",
            "id": self._id,
            "method": method,
            "params": params or {}
        }
        self.ws.send(json.dumps(request))
        response = json.loads(self.ws.recv())

        if "error" in response:
            raise RuntimeError(f"RPC error: {response['error']}")
        return response.get("result")

    def close(self):
        if self.ws:
            self.ws.close()


def batch_rename(client, pattern=None, replace=None, prefix=None, suffix=None):
    """
    Rename entities matching criteria.

    Args:
        client: DSEngineClient instance
        pattern: Regex pattern to match in entity names
        replace: Replacement string (used with pattern)
        prefix: Add prefix to all entity names
        suffix: Add suffix to all entity names
    """
    # Get current scene state
    state = client.call("dsengine_scene_get_state", {"include_components": False})
    entities = state.get("entities", [])

    if not entities:
        print("[BatchRename] No entities in scene.", file=sys.stderr)
        return 0

    renamed_count = 0

    for entity in entities:
        entity_id = entity.get("id")
        old_name = entity.get("name", "")
        new_name = old_name

        # Apply pattern replacement
        if pattern and replace is not None:
            new_name = re.sub(pattern, replace, new_name)

        # Apply prefix
        if prefix:
            new_name = prefix + new_name

        # Apply suffix
        if suffix:
            new_name = new_name + suffix

        # Only modify if name actually changed
        if new_name != old_name:
            client.call("dsengine_entity_modify", {
                "entity_id": entity_id,
                "name": new_name
            })
            print(f"  [{entity_id}] '{old_name}' -> '{new_name}'", file=sys.stderr)
            renamed_count += 1

    return renamed_count


def main():
    parser = argparse.ArgumentParser(description="DSEngine Batch Rename Plugin")
    parser.add_argument("--pattern", type=str, help="Regex pattern to match in names")
    parser.add_argument("--replace", type=str, default="", help="Replacement string")
    parser.add_argument("--prefix", type=str, help="Add prefix to all entity names")
    parser.add_argument("--suffix", type=str, help="Add suffix to all entity names")
    parser.add_argument("--keep-alive", action="store_true",
                        help="Keep plugin running after rename (for Plugin Manager)")
    args = parser.parse_args()

    has_operation = args.pattern or args.prefix or args.suffix
    daemon_mode = not has_operation or args.keep_alive

    client = DSEngineClient()

    try:
        client.connect()

        # Ping to verify connection
        result = client.call("dsengine_ping")
        print(f"[BatchRename] Server: {result}", file=sys.stderr)

        # Execute batch rename if args provided
        if has_operation:
            count = batch_rename(
                client,
                pattern=args.pattern,
                replace=args.replace,
                prefix=args.prefix,
                suffix=args.suffix
            )
            print(f"[BatchRename] Renamed {count} entities.", file=sys.stderr)
        else:
            print("[BatchRename] No rename args. Running in daemon mode.", file=sys.stderr)
            print("[BatchRename] Tip: use --pattern/--prefix/--suffix for batch rename.",
                  file=sys.stderr)

        # Stay alive: when launched by Plugin Manager (no args) or with --keep-alive
        if daemon_mode:
            print("[BatchRename] Staying alive. Stop via Plugin Manager or Ctrl+C.",
                  file=sys.stderr)
            while True:
                time.sleep(10)
                client.call("dsengine_ping")

    except KeyboardInterrupt:
        print("\n[BatchRename] Interrupted.", file=sys.stderr)
    except Exception as e:
        print(f"[BatchRename] Error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        client.close()
        print("[BatchRename] Done.", file=sys.stderr)


if __name__ == "__main__":
    main()
