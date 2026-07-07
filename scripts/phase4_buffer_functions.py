#!/usr/bin/env python3
"""Phase 4: Add 12 buffer+capacity functions to function_defs.json."""
import json
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
DEFS_PATH = os.path.join(REPO_ROOT, "tools", "codegen", "function_defs.json")

# 12 buffer functions mapped to their target groups
BUFFER_FUNCTIONS = {
    # group_name -> list of function defs
    "ecs_phys3d": [
        {
            "c_name": "dse_physics3d_overlap_sphere",
            "lua_name": "physics3d_overlap_sphere",
            "params": [
                {"name": "cx", "type": "float"},
                {"name": "cy", "type": "float"},
                {"name": "cz", "type": "float"},
                {"name": "radius", "type": "float"}
            ],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 256,
                "stride": 1,
                "mode": "flat_int"
            }
        },
        {
            "c_name": "dse_physics3d_overlap_box",
            "lua_name": "physics3d_overlap_box",
            "params": [
                {"name": "min_x", "type": "float"},
                {"name": "min_y", "type": "float"},
                {"name": "min_z", "type": "float"},
                {"name": "max_x", "type": "float"},
                {"name": "max_y", "type": "float"},
                {"name": "max_z", "type": "float"}
            ],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 256,
                "stride": 1,
                "mode": "flat_int"
            }
        },
        {
            "c_name": "dse_physics3d_get_collision_events",
            "lua_name": "physics3d_get_collision_events",
            "params": [],
            "buffer_output": {
                "elem_type": "float",
                "max_count": 256,
                "stride": 11,
                "mode": "collision_11f"
            }
        },
        {
            "c_name": "dse_physics3d_get_trigger_events",
            "lua_name": "physics3d_get_trigger_events",
            "params": [],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 256,
                "stride": 2,
                "mode": "trigger_dual"
            }
        }
    ],
    "ecs_gameplay3d": [
        {
            "c_name": "dse_rope_get_positions",
            "lua_name": "rope_get_positions",
            "params": [
                {"name": "e", "type": "entity"}
            ],
            "buffer_output": {
                "elem_type": "float",
                "max_count": 256,
                "stride": 3,
                "mode": "vec3"
            }
        }
    ],
    "open_world_p2p5": [
        {
            "c_name": "dse_physics_lod_evaluate",
            "lua_name": "physics_lod_evaluate",
            "params": [
                {"name": "cam_x", "type": "float"},
                {"name": "cam_y", "type": "float"},
                {"name": "cam_z", "type": "float"},
                {"name": "frame", "type": "uint32"}
            ],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 512,
                "stride": 1,
                "mode": "flat_int"
            }
        }
    ],
    "http": [
        {
            "c_name": "dse_http_poll",
            "lua_name": "poll",
            "params": [],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 64,
                "stride": 1,
                "mode": "flat_int"
            }
        }
    ],
    "ecs_core": [
        {
            "c_name": "dse_ecs_find_entities_by_mesh_path",
            "lua_name": "find_entities_by_mesh_path",
            "params": [
                {"name": "mesh_path", "type": "string"}
            ],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 512,
                "stride": 1,
                "mode": "flat_int"
            }
        },
        {
            "c_name": "dse_ecs_find_entities_with",
            "lua_name": "find_entities_with",
            "params": [
                {"name": "component", "type": "string"}
            ],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 1024,
                "stride": 1,
                "mode": "flat_int"
            }
        }
    ],
    "navigation": [
        {
            "c_name": "dse_nav_find_path",
            "lua_name": "find_path",
            "params": [
                {"name": "sx", "type": "float"},
                {"name": "sy", "type": "float"},
                {"name": "sz", "type": "float"},
                {"name": "ex", "type": "float"},
                {"name": "ey", "type": "float"},
                {"name": "ez", "type": "float"}
            ],
            "buffer_output": {
                "elem_type": "float",
                "max_count": 256,
                "stride": 3,
                "mode": "vec3"
            }
        }
    ],
    "ui_full": [
        {
            "c_name": "dse_ui_load_from_file",
            "lua_name": "uiloadfromfile",
            "params": [
                {"name": "path", "type": "string"}
            ],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 256,
                "stride": 1,
                "mode": "flat_int"
            }
        },
        {
            "c_name": "dse_ui_load_from_json",
            "lua_name": "uiloadfromjson",
            "params": [
                {"name": "json_str", "type": "string"}
            ],
            "buffer_output": {
                "elem_type": "uint32_t",
                "max_count": 256,
                "stride": 1,
                "mode": "flat_int"
            }
        }
    ]
}


def main():
    with open(DEFS_PATH, "r", encoding="utf-8") as f:
        defs = json.load(f)

    groups = defs["function_groups"]
    added_count = 0

    for group_name, new_funcs in BUFFER_FUNCTIONS.items():
        # Find the target group
        target = None
        for g in groups:
            if g["group"] == group_name:
                target = g
                break

        if target is None:
            print(f"WARNING: group '{group_name}' not found, skipping {len(new_funcs)} functions")
            continue

        # Check which functions already exist
        existing_names = {fn["c_name"] for fn in target["functions"]}

        for func in new_funcs:
            if func["c_name"] in existing_names:
                print(f"  SKIP (exists): {func['c_name']} in {group_name}")
                continue
            target["functions"].append(func)
            added_count += 1
            print(f"  ADD: {func['c_name']} -> {group_name}")

    with open(DEFS_PATH, "w", encoding="utf-8") as f:
        json.dump(defs, f, indent=2, ensure_ascii=False)

    print(f"\nAdded {added_count} buffer functions to function_defs.json")


if __name__ == "__main__":
    main()
