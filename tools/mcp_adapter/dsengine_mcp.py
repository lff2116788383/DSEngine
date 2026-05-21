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

try:
    import requests
except ImportError:
    requests = None  # optional: needed only for asset generation tools

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
                },
                "add_components": {
                    "type": "array",
                    "description": "Add components to entity. Items: type string or {type, properties} object.",
                    "items": {
                        "oneOf": [
                            {"type": "string"},
                            {
                                "type": "object",
                                "properties": {"type": {"type": "string"}, "properties": {"type": "object"}},
                                "required": ["type"]
                            }
                        ]
                    }
                },
                "remove_components": {
                    "type": "array",
                    "description": "Remove components by type name.",
                    "items": {"type": "string"}
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
        "name": "dsengine_entity_remove_component",
        "description": "Remove a component from an existing entity by type name",
        "inputSchema": {
            "type": "object",
            "properties": {
                "entity_id": {"type": "integer", "description": "Entity ID (uint32)"},
                "type": {"type": "string", "description": "Component type name to remove"}
            },
            "required": ["entity_id", "type"]
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
    },
    {
        "name": "dsengine_asset_import",
        "description": "Import an asset file (texture/mesh/audio/material) into the engine. "
                       "Type is auto-detected from extension, or specify explicitly.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Asset file path (relative to data root or absolute)"},
                "type": {
                    "type": "string",
                    "enum": ["texture", "mesh", "audio", "material", "auto"],
                    "default": "auto",
                    "description": "Asset type (auto-detected from extension if omitted)"
                },
                "material_index": {"type": "integer", "default": 0, "description": "Material index for .dmat files"}
            },
            "required": ["path"]
        }
    },
    {
        "name": "dsengine_material_create",
        "description": "Create a PBR material (.dmat file) with specified properties and load it into the engine.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "name": {"type": "string", "description": "Material name"},
                "save_path": {"type": "string", "description": "File path to save (optional, defaults to data/materials/<name>.dmat)"},
                "shader_variant": {"type": "string", "default": "MESH_PBR", "description": "Shader variant"},
                "base_color": {"type": "array", "items": {"type": "number"}, "description": "[r,g,b,a] 0-1"},
                "emissive": {"type": "array", "items": {"type": "number"}, "description": "[r,g,b] emissive color"},
                "metallic": {"type": "number", "description": "Metallic factor 0-1"},
                "roughness": {"type": "number", "description": "Roughness factor 0-1"},
                "base_color_texture": {"type": "string", "description": "Albedo texture path"},
                "normal_texture": {"type": "string", "description": "Normal map path"},
                "metallic_roughness_texture": {"type": "string", "description": "Metallic-roughness texture path"},
                "double_sided": {"type": "boolean", "default": False}
            },
            "required": ["name"]
        }
    },
    {
        "name": "dsengine_asset_generate_texture",
        "description": "Generate a texture using AI (DALL-E 3 or compatible API) and import it into the engine. "
                       "Requires OPENAI_API_KEY environment variable.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "prompt": {"type": "string", "description": "Texture description (e.g. 'seamless concrete floor PBR texture')"},
                "save_path": {"type": "string", "description": "Save path relative to data/ (e.g. 'textures/concrete.png')"},
                "size": {"type": "string", "enum": ["1024x1024", "512x512", "1792x1024", "1024x1792"], "default": "1024x1024"},
                "quality": {"type": "string", "enum": ["standard", "hd"], "default": "standard"},
                "style": {"type": "string", "enum": ["natural", "vivid"], "default": "natural"}
            },
            "required": ["prompt", "save_path"]
        }
    },
    {
        "name": "dsengine_asset_generate_model",
        "description": "Generate a 3D model using Meshy.ai API and download it. "
                       "Requires MESHY_API_KEY environment variable. Returns a task ID for async generation.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "prompt": {"type": "string", "description": "3D model description"},
                "save_path": {"type": "string", "description": "Save path for downloaded model (e.g. 'models/chair.glb')"},
                "art_style": {
                    "type": "string",
                    "enum": ["realistic", "cartoon", "low-poly", "sculpture", "pbr"],
                    "default": "realistic"
                },
                "topology": {"type": "string", "enum": ["triangle", "quad"], "default": "triangle"}
            },
            "required": ["prompt", "save_path"]
        }
    },
    {
        "name": "dsengine_asset_generate_sfx",
        "description": "Generate a sound effect using ElevenLabs API. "
                       "Requires ELEVENLABS_API_KEY environment variable.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "prompt": {"type": "string", "description": "Sound effect description (e.g. 'sword clash metallic impact')"},
                "save_path": {"type": "string", "description": "Save path (e.g. 'audio/sfx/sword_clash.wav')"},
                "duration_seconds": {"type": "number", "default": 2.0, "description": "Desired duration in seconds"}
            },
            "required": ["prompt", "save_path"]
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
    try:
        return json.loads(line.strip())
    except json.JSONDecodeError as e:
        sys.stderr.write(f"[MCP] JSON decode error: {e} | line={line!r}\n")
        return None


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


# ─── AI asset generation helpers ─────────────────────────────────────────────

def _ensure_requests():
    if requests is None:
        return "Missing dependency: pip install requests"
    return None


def _ensure_parent_dir(filepath):
    dirpath = os.path.dirname(filepath)
    if dirpath:
        os.makedirs(dirpath, exist_ok=True)


def _resolve_data_path(save_path, bridge):
    """Resolve a relative save_path to absolute using engine data root."""
    if os.path.isabs(save_path):
        return save_path
    # Ask engine for data root via editor state
    state = bridge.call("dsengine_editor_get_state")
    data_root = "data"
    if isinstance(state, dict) and "data_root" in state:
        data_root = state["data_root"]
    return os.path.join(data_root, save_path)


def _generate_texture_dalle(arguments, bridge):
    """Generate texture via OpenAI DALL-E 3 API."""
    err = _ensure_requests()
    if err:
        return {"error": err}

    api_key = os.environ.get("OPENAI_API_KEY")
    if not api_key:
        return {"error": "OPENAI_API_KEY environment variable not set"}

    prompt = arguments.get("prompt", "")
    save_path = arguments.get("save_path", "textures/generated.png")
    size = arguments.get("size", "1024x1024")
    quality = arguments.get("quality", "standard")
    style = arguments.get("style", "natural")

    abs_path = _resolve_data_path(save_path, bridge)
    _ensure_parent_dir(abs_path)

    sys.stderr.write(f"[MCP] Generating texture: {prompt} -> {abs_path}\n")

    try:
        resp = requests.post(
            "https://api.openai.com/v1/images/generations",
            headers={"Authorization": f"Bearer {api_key}", "Content-Type": "application/json"},
            json={
                "model": "dall-e-3",
                "prompt": prompt,
                "n": 1,
                "size": size,
                "quality": quality,
                "style": style,
                "response_format": "url"
            },
            timeout=120
        )
        resp.raise_for_status()
        image_url = resp.json()["data"][0]["url"]

        img_resp = requests.get(image_url, timeout=60)
        img_resp.raise_for_status()

        with open(abs_path, "wb") as f:
            f.write(img_resp.content)

        sys.stderr.write(f"[MCP] Texture saved: {abs_path} ({len(img_resp.content)} bytes)\n")
    except Exception as e:
        return {"error": f"DALL-E API error: {e}"}

    # Import into engine
    import_result = bridge.call("dsengine_asset_import", {"path": save_path, "type": "texture"})

    return {
        "file_path": abs_path,
        "save_path": save_path,
        "file_size": os.path.getsize(abs_path),
        "import_result": import_result
    }


def _generate_model_meshy(arguments, bridge):
    """Generate 3D model via Meshy.ai API."""
    err = _ensure_requests()
    if err:
        return {"error": err}

    api_key = os.environ.get("MESHY_API_KEY")
    if not api_key:
        return {"error": "MESHY_API_KEY environment variable not set"}

    prompt = arguments.get("prompt", "")
    save_path = arguments.get("save_path", "models/generated.glb")
    art_style = arguments.get("art_style", "realistic")
    topology = arguments.get("topology", "triangle")

    abs_path = _resolve_data_path(save_path, bridge)
    _ensure_parent_dir(abs_path)

    sys.stderr.write(f"[MCP] Generating 3D model: {prompt}\n")

    headers = {"Authorization": f"Bearer {api_key}", "Content-Type": "application/json"}

    try:
        # Create text-to-3d task
        resp = requests.post(
            "https://api.meshy.ai/v2/text-to-3d",
            headers=headers,
            json={
                "mode": "preview",
                "prompt": prompt,
                "art_style": art_style,
                "topology": topology
            },
            timeout=30
        )
        resp.raise_for_status()
        task_id = resp.json().get("result")
        if not task_id:
            return {"error": "Meshy API did not return task_id"}

        sys.stderr.write(f"[MCP] Meshy task created: {task_id}, polling...\n")

        # Poll for completion (up to 5 minutes)
        import time
        for _ in range(60):
            time.sleep(5)
            status_resp = requests.get(
                f"https://api.meshy.ai/v2/text-to-3d/{task_id}",
                headers=headers, timeout=30
            )
            status_resp.raise_for_status()
            status_data = status_resp.json()
            status = status_data.get("status", "")

            if status == "SUCCEEDED":
                model_url = status_data.get("model_urls", {}).get("glb")
                if not model_url:
                    return {"error": "Meshy task succeeded but no GLB URL found"}

                model_resp = requests.get(model_url, timeout=120)
                model_resp.raise_for_status()
                with open(abs_path, "wb") as f:
                    f.write(model_resp.content)

                sys.stderr.write(f"[MCP] Model saved: {abs_path} ({len(model_resp.content)} bytes)\n")
                return {
                    "task_id": task_id,
                    "file_path": abs_path,
                    "save_path": save_path,
                    "file_size": os.path.getsize(abs_path),
                    "status": "completed"
                }
            elif status == "FAILED":
                return {"error": f"Meshy task failed: {status_data.get('task_error', 'unknown')}"}

        return {"error": "Meshy task timed out after 5 minutes", "task_id": task_id}

    except Exception as e:
        return {"error": f"Meshy API error: {e}"}


def _generate_sfx_elevenlabs(arguments, bridge):
    """Generate sound effect via ElevenLabs API."""
    err = _ensure_requests()
    if err:
        return {"error": err}

    api_key = os.environ.get("ELEVENLABS_API_KEY")
    if not api_key:
        return {"error": "ELEVENLABS_API_KEY environment variable not set"}

    prompt = arguments.get("prompt", "")
    save_path = arguments.get("save_path", "audio/sfx/generated.wav")
    duration = arguments.get("duration_seconds", 2.0)

    abs_path = _resolve_data_path(save_path, bridge)
    _ensure_parent_dir(abs_path)

    sys.stderr.write(f"[MCP] Generating SFX: {prompt} -> {abs_path}\n")

    try:
        resp = requests.post(
            "https://api.elevenlabs.io/v1/sound-generation",
            headers={"xi-api-key": api_key, "Content-Type": "application/json"},
            json={
                "text": prompt,
                "duration_seconds": duration
            },
            timeout=60
        )
        resp.raise_for_status()

        with open(abs_path, "wb") as f:
            f.write(resp.content)

        sys.stderr.write(f"[MCP] SFX saved: {abs_path} ({len(resp.content)} bytes)\n")
    except Exception as e:
        return {"error": f"ElevenLabs API error: {e}"}

    # Import into engine
    import_result = bridge.call("dsengine_asset_import", {"path": save_path, "type": "audio"})

    return {
        "file_path": abs_path,
        "save_path": save_path,
        "file_size": os.path.getsize(abs_path),
        "import_result": import_result
    }


# Map of locally-handled tools (not forwarded to C++ Control Server)
LOCAL_TOOL_HANDLERS = {
    "dsengine_asset_generate_texture": _generate_texture_dalle,
    "dsengine_asset_generate_model": _generate_model_meshy,
    "dsengine_asset_generate_sfx": _generate_sfx_elevenlabs,
}


def handle_tools_call(msg, bridge):
    """Execute a tool call by forwarding to Control Server or handling locally."""
    params = msg.get("params", {})
    tool_name = params.get("name", "")
    arguments = params.get("arguments", {})

    # Check if this is a locally-handled tool (AI generation)
    if tool_name in LOCAL_TOOL_HANDLERS:
        result = LOCAL_TOOL_HANDLERS[tool_name](arguments, bridge)
    else:
        # Forward to C++ Control Server
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
                if sys.stdin.closed:
                    break  # stdin closed
                continue  # JSON 解析失败，跳过该行

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
