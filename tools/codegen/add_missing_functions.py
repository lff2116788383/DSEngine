#!/usr/bin/env python3
"""Add missing dse_api.h functions to function_defs.json."""
import re, json
from pathlib import Path
from collections import defaultdict

script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parent.parent

# Read dse_api.h
header_path = repo_root / "engine/scripting/native_api/dse_api.h"
header = header_path.read_text(encoding="utf-8")

# Functions that should NOT have Lua bindings
SKIP_FUNCS = {
    "dse_get_asset_manager_ptr",
    "dse_get_audio_system_ptr",
    "dse_get_floating_origin_ptr",
    "dse_native_api_init_ext",
}

# Missing functions that need to be added
MISSING_FUNCS = [
    "dse_anim2d_pop_event",
    "dse_anim3d_add_transition",
    "dse_anim3d_get_state",
    "dse_anim3d_init_fsm",
    "dse_anim3d_pop_event",
    "dse_animlayer_set_blend_tree_1d",
    "dse_audio_bus_get_names",
    "dse_audio_snapshot_list",
    "dse_audio_source_get_state",
    "dse_ecs_get_queryable_components",
    "dse_ecs_get_script_path",
    "dse_http_get_response",
    "dse_line_renderer_set_points",
    "dse_mesh_renderer_add_procedural",
    "dse_mesh_renderer_set_normals",
    "dse_mesh_renderer_set_tangents",
    "dse_mesh_renderer_set_uvs",
    "dse_meshlet_build",
    "dse_meshlet_cull_add_instance",
    "dse_meshlet_cull_execute_cpu",
    "dse_meshlet_cull_prepare",
    "dse_morph_simple_add_target",
    "dse_nav_bake",
    "dse_open_world_p2p5_shutdown",
    "dse_particle_system_3d_get_state",
    "dse_physics2d_add_polygon_collider",
    "dse_scene_get_active",
    "dse_scene_get_loaded_subs",
    "dse_streaming_add_assets",
    "dse_ui_get_dropdown_value",
    "dse_ui_get_text_input_text",
    "dse_uuid_get",
    "dse_uuid_set",
    "dse_weather_add",
]

# Map function prefix to group name and lua_path
# Based on existing function_defs.json group structure
FUNC_GROUP_MAP = {
    "dse_anim": ("ecs_animation", ["dse", "animation"]),
    "dse_audio": ("audio", ["dse", "audio"]),
    "dse_ecs": ("ecs_gap", ["dse", "ecs"]),
    "dse_http": ("http", ["dse", "http"]),
    "dse_line_renderer": ("rendering", ["dse", "rendering"]),
    "dse_mesh_renderer": ("rendering", ["dse", "rendering"]),
    "dse_meshlet": ("meshlet", ["dse", "meshlet"]),
    "dse_morph": ("ecs_animation", ["dse", "animation"]),
    "dse_nav": ("navigation", ["dse", "navigation"]),
    "dse_open_world": ("open_world", ["dse", "open_world"]),
    "dse_particle": ("ecs_gap", ["dse", "ecs"]),
    "dse_physics2d": ("physics2d", ["dse", "physics2d"]),
    "dse_scene": ("scene", ["dse", "scene"]),
    "dse_streaming": ("streaming", ["dse", "streaming"]),
    "dse_ui": ("ui", ["dse", "ui"]),
    "dse_uuid": ("api_core", ["dse"]),
    "dse_weather": ("world", ["dse", "world"]),
}

# C type to function_defs.json type mapping
TYPE_MAP = {
    "void": "void",
    "int": "int",
    "int64_t": "int64",
    "uint32_t": "uint32",
    "uint64_t": "uint64",
    "float": "float",
    "double": "double",
    "const char*": "string",
    "char*": "string",
}

def parse_function_signature(header: str, func_name: str):
    """Parse a DSE_CAPI function declaration from the header."""
    # Match: DSE_CAPI <ret_type> func_name(<params>);
    # Handle multi-line declarations
    pattern = rf'DSE_CAPI\s+([\w\s\*]+?)\s+{re.escape(func_name)}\s*\(([^)]+)\)'
    m = re.search(pattern, header, re.DOTALL)
    if not m:
        return None

    ret_type = m.group(1).strip()
    params_str = m.group(2).strip()

    # Parse return type
    if ret_type == "void":
        returns = []
    elif ret_type == "float":
        returns = [{"type": "float"}]
    elif ret_type == "int":
        returns = [{"type": "int"}]
    elif ret_type == "uint32_t":
        returns = [{"type": "uint32"}]
    elif ret_type == "int64_t":
        returns = [{"type": "int64"}]
    elif ret_type == "uint64_t":
        returns = [{"type": "uint64"}]
    elif ret_type == "double":
        returns = [{"type": "double"}]
    elif ret_type == "void*":
        returns = [{"type": "uint64"}]  # Handle as opaque pointer
    else:
        returns = [{"type": "int"}]  # Default

    # Parse parameters
    params = []
    if params_str and params_str != "void":
        # Split by comma, handling nested parens
        parts = []
        depth = 0
        current = ""
        for c in params_str:
            if c == '(':
                depth += 1
                current += c
            elif c == ')':
                depth -= 1
                current += c
            elif c == ',' and depth == 0:
                parts.append(current.strip())
                current = ""
            else:
                current += c
        if current.strip():
            parts.append(current.strip())

        for part in parts:
            # Parse: "const char* path" or "float x" or "uint32_t e" etc.
            # Handle function pointer params: void (*quit_fn)(void)
            if '(' in part:
                # Function pointer parameter - skip for now
                params.append({"name": "fn_ptr", "type": "uint64"})
                continue

            # Split into type and name
            tokens = part.split()
            if len(tokens) >= 2:
                name = tokens[-1]
                # Remove array brackets from name
                name = name.replace("[]", "").replace("*", "")
                # Type is everything except the last token
                type_str = " ".join(tokens[:-1])

                # Map type
                if "float" in type_str:
                    ptype = "float"
                elif "const char" in type_str:
                    ptype = "string"
                elif "char" in type_str and "*" in type_str:
                    ptype = "string"
                elif "int32_t" in type_str or "int " in type_str or type_str.startswith("int"):
                    ptype = "int"
                elif "uint32_t" in type_str:
                    ptype = "uint32"
                elif "int64_t" in type_str:
                    ptype = "int64"
                elif "uint64_t" in type_str:
                    ptype = "uint64"
                elif "double" in type_str:
                    ptype = "double"
                elif "void*" in type_str:
                    ptype = "uint64"
                else:
                    ptype = "int"

                # Skip output pointer params (float* x) - they become out params
                if "*" in type_str and ptype in ("float", "int", "uint32"):
                    # These are out params - for Lua binding we handle differently
                    # For now, include as regular params
                    params.append({"name": name, "type": ptype})
                else:
                    params.append({"name": name, "type": ptype})

    return {"params": params, "returns": returns}


def snake_to_camel(name: str) -> str:
    """Convert dse_xx_yy to xx_yy (remove dse_ prefix for lua_name)."""
    if name.startswith("dse_"):
        name = name[4:]
    return name


def main():
    # Read function_defs.json
    defs_path = script_dir / "function_defs.json"
    defs = json.loads(defs_path.read_text(encoding="utf-8"))

    # Build a map of group_name -> group index
    group_map = {}
    for i, group in enumerate(defs["function_groups"]):
        group_map[group["group"]] = i

    # Track existing c_names to avoid duplicates
    existing_c_names = set()
    for group in defs["function_groups"]:
        for fn in group.get("functions", []):
            if "c_name" in fn:
                existing_c_names.add(fn["c_name"])

    # Parse and add each missing function
    added = 0
    for func_name in MISSING_FUNCS:
        if func_name in SKIP_FUNCS:
            continue
        if func_name in existing_c_names:
            print(f"  SKIP (already exists): {func_name}")
            continue

        sig = parse_function_signature(header, func_name)
        if not sig:
            print(f"  WARNING: Could not parse signature for {func_name}")
            continue

        # Determine group
        group_name = None
        lua_path = None
        for prefix, (gn, lp) in FUNC_GROUP_MAP.items():
            if func_name.startswith(prefix):
                group_name = gn
                lua_path = lp
                break

        if not group_name:
            print(f"  WARNING: No group mapping for {func_name}")
            continue

        if group_name not in group_map:
            # Create new group
            new_group = {
                "group": group_name,
                "lua_path": lua_path,
                "functions": [],
                "register_func": f"RegisterFreeFn_{group_name}"
            }
            defs["function_groups"].append(new_group)
            group_map[group_name] = len(defs["function_groups"]) - 1

        # Create function entry
        entry = {
            "c_name": func_name,
            "lua_name": snake_to_camel(func_name),
            "params": sig["params"],
            "returns": sig["returns"],
        }

        group_idx = group_map[group_name]
        defs["function_groups"][group_idx]["functions"].append(entry)
        existing_c_names.add(func_name)
        added += 1
        print(f"  ADDED: {func_name} -> group '{group_name}'")

    # Write back
    defs_path.write_text(
        json.dumps(defs, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8"
    )
    print(f"\nTotal added: {added}")
    print(f"Total function groups: {len(defs['function_groups'])}")


if __name__ == "__main__":
    main()
