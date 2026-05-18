#!/usr/bin/env python3
"""
DSEngine MCP stdio adapter.

MCP (Model Context Protocol) stdio 适配器，让 Cursor / Windsurf / Claude Desktop
通过标准 MCP 协议直接控制 DSEngine 编辑器。

架构:
    AI 客户端 (Cursor) ←stdio→ 本进程 ←WebSocket→ DSEngine Editor Control Server

用法:
    python dsengine_mcp.py [--port 9527]

MCP 配置 (Cursor settings / Claude Desktop config):
{
    "mcpServers": {
        "dsengine": {
            "command": "python",
            "args": ["<path>/tools/mcp_adapter/dsengine_mcp.py"]
        }
    }
}
"""

import json
import sys
import os
import argparse
import threading

try:
    import websocket
except ImportError:
    sys.stderr.write("Missing dependency: pip install websocket-client\n")
    sys.exit(1)

# ─── MCP Tool 定义 ──────────────────────────────────────────────────────────

MCP_TOOLS = [
    {
        "name": "dsengine_ping",
        "description": "Test connectivity with DSEngine Editor",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    },
    {
        "name": "dsengine_lua_execute",
        "description": "Execute Lua code in DSEngine. Full access to dse.ecs / dse.audio / dse.assets / dse.render APIs.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "code": {"type": "string", "description": "Lua code to execute"}
            },
            "required": ["code"]
        }
    },
    {
        "name": "dsengine_scene_get_state",
        "description": "Get current scene entity list with component summaries",
        "inputSchema": {
            "type": "object",
            "properties": {
                "include_components": {
                    "type": "boolean",
                    "description": "Include component details per entity",
                    "default": True
                }
            }
        }
    },
    {
        "name": "dsengine_entity_create",
        "description": "Create a new entity in the scene with optional mesh, color, and components. "
                       "Supported component types: MeshRenderer, Camera3D, DirectionalLight, PointLight, SpotLight, "
                       "RigidBody3D, BoxCollider3D, SphereCollider3D, AudioSource, AudioListener, SkyLight, Skybox, PostProcess. "
                       "Components can be strings (type name only) or objects {type, properties}.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "name": {"type": "string", "description": "Entity name"},
                "position": {"type": "array", "items": {"type": "number"}, "minItems": 3, "description": "[x,y,z]"},
                "rotation": {"type": "array", "items": {"type": "number"}, "minItems": 3, "description": "[pitch,yaw,roll] degrees"},
                "scale": {"type": "array", "items": {"type": "number"}, "minItems": 3, "description": "[x,y,z]"},
                "mesh": {"type": "string", "description": "Mesh asset path (auto-adds MeshRenderer with MESH_PBR shader)"},
                "color": {"type": "array", "items": {"type": "number"}, "minItems": 4, "description": "[r,g,b,a] 0-1 range"},
                "components": {
                    "type": "array",
                    "description": "Components to add. Each item is a type name string or {type, properties} object.",
                    "items": {
                        "oneOf": [
                            {"type": "string"},
                            {
                                "type": "object",
                                "properties": {
                                    "type": {"type": "string"},
                                    "properties": {"type": "object"}
                                },
                                "required": ["type"]
                            }
                        ]
                    }
                }
            },
            "required": ["name"]
        }
    },
    {
        "name": "dsengine_entity_delete",
        "description": "Delete an entity by ID",
        "inputSchema": {
            "type": "object",
            "properties": {
                "entity_id": {"type": "integer", "description": "Entity ID (uint32)"}
            },
            "required": ["entity_id"]
        }
    },
    {
        "name": "dsengine_script_create",
        "description": "Create or overwrite a Lua script file. Editor will hot-reload automatically.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Script path (relative to project root)"},
                "content": {"type": "string", "description": "Lua script content"}
            },
            "required": ["path", "content"]
        }
    },
    {
        "name": "dsengine_editor_get_state",
        "description": "Get editor state (edit/play/pause) and entity count",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    },
    {
        "name": "dsengine_entity_modify",
        "description": "Modify an existing entity's name, transform, and/or component properties. "
                       "Use modify_component for a single component, modify_components for batch updates.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "entity_id": {"type": "integer", "description": "Entity ID (uint32)"},
                "name": {"type": "string", "description": "New entity name"},
                "position": {"type": "array", "items": {"type": "number"}, "minItems": 3, "description": "[x,y,z]"},
                "rotation": {"type": "array", "items": {"type": "number"}, "minItems": 3, "description": "[pitch,yaw,roll] degrees"},
                "scale": {"type": "array", "items": {"type": "number"}, "minItems": 3, "description": "[x,y,z]"},
                "modify_component": {
                    "type": "object",
                    "description": "Modify a single component's properties: {type, properties}",
                    "properties": {
                        "type": {"type": "string"},
                        "properties": {"type": "object"}
                    },
                    "required": ["type", "properties"]
                },
                "modify_components": {
                    "type": "array",
                    "description": "Batch modify multiple components: [{type, properties}, ...]",
                    "items": {
                        "type": "object",
                        "properties": {
                            "type": {"type": "string"},
                            "properties": {"type": "object"}
                        },
                        "required": ["type", "properties"]
                    }
                }
            },
            "required": ["entity_id"]
        }
    },
    {
        "name": "dsengine_entity_add_component",
        "description": "Add a component to an existing entity. "
                       "Supported types: MeshRenderer, Camera3D, DirectionalLight, PointLight, SpotLight, "
                       "RigidBody3D, BoxCollider3D, SphereCollider3D, AudioSource, AudioListener, SkyLight, Skybox, PostProcess.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "entity_id": {"type": "integer", "description": "Entity ID (uint32)"},
                "type": {"type": "string", "description": "Component type name"},
                "properties": {"type": "object", "description": "Optional component properties"}
            },
            "required": ["entity_id", "type"]
        }
    },
    {
        "name": "dsengine_entity_get_components",
        "description": "Get all components on an entity with optional property details",
        "inputSchema": {
            "type": "object",
            "properties": {
                "entity_id": {"type": "integer", "description": "Entity ID (uint32)"},
                "detailed": {"type": "boolean", "default": True, "description": "Include property values (default true)"}
            },
            "required": ["entity_id"]
        }
    },
    {
        "name": "dsengine_editor_play",
        "description": "Enter Play mode (run the game in editor)",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    },
    {
        "name": "dsengine_editor_stop",
        "description": "Exit Play mode (return to Edit mode, restoring scene state)",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    },
    {
        "name": "dsengine_editor_undo",
        "description": "Undo the last editor action",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    },
    {
        "name": "dsengine_editor_redo",
        "description": "Redo the last undone editor action",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    },
    {
        "name": "dsengine_editor_screenshot",
        "description": "Capture a screenshot of the scene or game viewport, returns base64 PNG",
        "inputSchema": {
            "type": "object",
            "properties": {
                "target": {
                    "type": "string",
                    "enum": ["scene", "game"],
                    "default": "scene",
                    "description": "Which viewport to capture"
                }
            }
        }
    },
    {
        "name": "dsengine_scene_save",
        "description": "Save the current scene to a file",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "File path to save (optional, uses current if omitted)"}
            }
        }
    },
    {
        "name": "dsengine_scene_load",
        "description": "Load a scene from a file",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Scene file path to load"}
            },
            "required": ["path"]
        }
    }
]

# ─── WebSocket bridge ────────────────────────────────────────────────────────

class WsBridge:
    def __init__(self, port=9527):
        self.url = f"ws://127.0.0.1:{port}"
        self.ws = None
        self._id_counter = 0
        self._lock = threading.Lock()

    def connect(self):
        try:
            self.ws = websocket.create_connection(self.url, timeout=5)
            return True
        except Exception as e:
            sys.stderr.write(f"[MCP] WS connect failed: {e}\n")
            return False

    def call(self, method, params=None):
        """Send JSON-RPC call to Control Server, return result dict or raise."""
        if not self.ws:
            if not self.connect():
                return {"error": "Cannot connect to DSEngine Editor (ws://127.0.0.1:9527). Is the editor running?"}

        with self._lock:
            self._id_counter += 1
            req_id = self._id_counter

        request = {
            "jsonrpc": "2.0",
            "id": req_id,
            "method": method,
            "params": params or {}
        }

        try:
            self.ws.send(json.dumps(request))
            raw = self.ws.recv()
            resp = json.loads(raw)
        except Exception as e:
            self.ws = None  # force reconnect next time
            return {"error": f"WS communication error: {e}"}

        if "error" in resp:
            return {"error": f"[{resp['error'].get('code')}] {resp['error'].get('message')}"}
        return resp.get("result", {})

    def close(self):
        if self.ws:
            try:
                self.ws.close()
            except Exception:
                pass

# ─── MCP stdio protocol ─────────────────────────────────────────────────────

def read_message():
    """Read a JSON-RPC message from stdin (line-delimited)."""
    line = sys.stdin.readline()
    if not line:
        return None
    return json.loads(line.strip())


def write_message(msg):
    """Write a JSON-RPC message to stdout (line-delimited)."""
    sys.stdout.write(json.dumps(msg) + "\n")
    sys.stdout.flush()


def make_response(req_id, result):
    return {"jsonrpc": "2.0", "id": req_id, "result": result}


def make_error(req_id, code, message):
    return {"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}}


def handle_initialize(msg):
    """MCP initialize handshake."""
    return make_response(msg["id"], {
        "protocolVersion": "2024-11-05",
        "capabilities": {
            "tools": {}
        },
        "serverInfo": {
            "name": "dsengine-mcp",
            "version": "1.0.0"
        }
    })


def handle_tools_list(msg):
    """Return available tools."""
    return make_response(msg["id"], {"tools": MCP_TOOLS})


def handle_tools_call(msg, bridge):
    """Execute a tool call by forwarding to Control Server."""
    params = msg.get("params", {})
    tool_name = params.get("name", "")
    arguments = params.get("arguments", {})

    result = bridge.call(tool_name, arguments)

    if isinstance(result, dict) and "error" in result:
        return make_response(msg["id"], {
            "content": [{"type": "text", "text": result["error"]}],
            "isError": True
        })

    text = json.dumps(result, indent=2, ensure_ascii=False)
    return make_response(msg["id"], {
        "content": [{"type": "text", "text": text}]
    })


# ─── Main loop ──────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="DSEngine MCP stdio adapter")
    parser.add_argument("--port", type=int, default=9527, help="Control Server port")
    args = parser.parse_args()

    bridge = WsBridge(port=args.port)
    sys.stderr.write(f"[MCP] DSEngine MCP adapter started (port={args.port})\n")

    try:
        while True:
            msg = read_message()
            if msg is None:
                break  # stdin closed

            method = msg.get("method", "")

            if method == "initialize":
                write_message(handle_initialize(msg))
            elif method == "notifications/initialized":
                pass  # no response needed
            elif method == "tools/list":
                write_message(handle_tools_list(msg))
            elif method == "tools/call":
                write_message(handle_tools_call(msg, bridge))
            elif method == "ping":
                write_message(make_response(msg.get("id"), {}))
            else:
                if "id" in msg:
                    write_message(make_error(msg["id"], -32601, f"Method not found: {method}"))
    except KeyboardInterrupt:
        pass
    finally:
        bridge.close()
        sys.stderr.write("[MCP] Adapter stopped\n")


if __name__ == "__main__":
    main()
