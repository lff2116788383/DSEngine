#!/usr/bin/env python3
"""
Phase 1: Generate function_defs.json entries for all 86 world_systems functions.
Outputs the 6 groups as JSON to be merged into function_defs.json.
"""

import json
import os

def build_world_systems_groups():
    groups = []

    # ── §1 dse.spline — 14 functions ──
    spline = {
        "group": "world_spline",
        "lua_path": ["dse", "spline"],
        "register_func": "RegisterFreeWorldSplineBindings",
        "functions": [
            {
                "c_name": "dse_spline_init",
                "lua_name": "init",
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_spline_shutdown",
                "lua_name": "shutdown",
                "params": []
            },
            {
                "c_name": "dse_spline_create",
                "lua_name": "create",
                "params": [{"name": "name", "type": "string"}],
                "returns": [{"type": "uint32"}]
            },
            {
                "c_name": "dse_spline_destroy",
                "lua_name": "destroy",
                "params": [{"name": "id", "type": "uint32"}]
            },
            {
                "c_name": "dse_spline_add_point",
                "lua_name": "add_point",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "x", "type": "float"},
                    {"name": "y", "type": "float"},
                    {"name": "z", "type": "float"},
                    {"name": "width", "type": "float", "default": 4.0}
                ]
            },
            {
                "c_name": "dse_spline_set_point",
                "lua_name": "set_point",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "index", "type": "int"},
                    {"name": "x", "type": "float"},
                    {"name": "y", "type": "float"},
                    {"name": "z", "type": "float"},
                    {"name": "width", "type": "float", "default": 4.0}
                ]
            },
            {
                "c_name": "dse_spline_remove_point",
                "lua_name": "remove_point",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "index", "type": "int"}
                ]
            },
            {
                "c_name": "dse_spline_get_point_count",
                "lua_name": "get_point_count",
                "params": [{"name": "id", "type": "uint32"}],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_spline_get_length",
                "lua_name": "get_length",
                "params": [{"name": "id", "type": "uint32"}],
                "returns": [{"type": "float"}]
            },
            {
                "c_name": "dse_spline_evaluate",
                "lua_name": "evaluate",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "t", "type": "float"}
                ],
                "out_params": [{"name": "xyz", "type": "float3"}]
            },
            {
                "c_name": "dse_spline_evaluate_distance",
                "lua_name": "evaluate_distance",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "dist", "type": "float"}
                ],
                "out_params": [{"name": "xyz", "type": "float3"}]
            },
            {
                "c_name": "dse_spline_find_nearest",
                "lua_name": "find_nearest",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "x", "type": "float"},
                    {"name": "y", "type": "float"},
                    {"name": "z", "type": "float"}
                ],
                "returns": [{"type": "float"}]
            },
            {
                "c_name": "dse_spline_gen_road",
                "lua_name": "gen_road",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "step", "type": "float", "default": 1.0},
                    {"name": "segments", "type": "int", "default": 4}
                ],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_spline_gen_river",
                "lua_name": "gen_river",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "width", "type": "float", "default": 2.0},
                    {"name": "depth", "type": "float", "default": 2.0}
                ],
                "returns": [{"type": "int"}]
            }
        ]
    }
    groups.append(spline)

    # ── §2 dse.ocean — 10 functions ──
    ocean = {
        "group": "world_ocean",
        "lua_path": ["dse", "ocean"],
        "register_func": "RegisterFreeWorldOceanBindings",
        "functions": [
            {
                "c_name": "dse_ocean_init",
                "lua_name": "init",
                "table_input": {
                    "fields": [
                        {"key": "fft_resolution", "name": "fft_res", "type": "int", "default": 256},
                        {"key": "tile_size", "name": "tile_size", "type": "float", "default": 512.0},
                        {"key": "wind_speed", "name": "wind_speed", "type": "float", "default": 8.0},
                        {"key": "choppiness", "name": "choppiness", "type": "float", "default": 1.0}
                    ]
                },
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_ocean_shutdown",
                "lua_name": "shutdown",
                "params": []
            },
            {
                "c_name": "dse_ocean_update",
                "lua_name": "update",
                "params": [
                    {"name": "dt", "type": "float"},
                    {"name": "cam_x", "type": "float"},
                    {"name": "cam_y", "type": "float"},
                    {"name": "cam_z", "type": "float"}
                ]
            },
            {
                "c_name": "dse_ocean_get_height",
                "lua_name": "get_height",
                "params": [
                    {"name": "x", "type": "float"},
                    {"name": "z", "type": "float"}
                ],
                "returns": [{"type": "float"}]
            },
            {
                "c_name": "dse_ocean_get_normal",
                "lua_name": "get_normal",
                "params": [
                    {"name": "x", "type": "float"},
                    {"name": "z", "type": "float"}
                ],
                "out_params": [{"name": "xyz", "type": "float3"}]
            },
            {
                "c_name": "dse_ocean_get_foam",
                "lua_name": "get_foam",
                "params": [
                    {"name": "x", "type": "float"},
                    {"name": "z", "type": "float"}
                ],
                "returns": [{"type": "float"}]
            },
            {
                "c_name": "dse_ocean_set_wind",
                "lua_name": "set_wind",
                "params": [
                    {"name": "dx", "type": "float"},
                    {"name": "dz", "type": "float"},
                    {"name": "speed", "type": "float"}
                ]
            },
            {
                "c_name": "dse_ocean_set_choppiness",
                "lua_name": "set_choppiness",
                "params": [{"name": "c", "type": "float"}]
            },
            {
                "c_name": "dse_ocean_get_stats",
                "lua_name": "get_stats",
                "params": [],
                "out_params": [
                    {"name": "total", "type": "int"},
                    {"name": "visible", "type": "int"},
                    {"name": "fft_res", "type": "int"},
                    {"name": "max_height", "type": "float"}
                ],
                "table_return": {
                    "fields": [
                        {"key": "total_tiles", "source": "total", "push": "integer"},
                        {"key": "visible_tiles", "source": "visible", "push": "integer"},
                        {"key": "fft_resolution", "source": "fft_res", "push": "integer"},
                        {"key": "max_height", "source": "max_height", "push": "number"}
                    ]
                }
            },
            {
                "c_name": "dse_ocean_get_lod_count",
                "lua_name": "get_lod_count",
                "params": [],
                "returns": [{"type": "int"}]
            }
        ]
    }
    groups.append(ocean)

    # ── §3 dse.editor — 14 functions ──
    editor = {
        "group": "world_editor",
        "lua_path": ["dse", "editor"],
        "register_func": "RegisterFreeWorldEditorBindings",
        "functions": [
            {
                "c_name": "dse_editor_init",
                "lua_name": "init",
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_editor_shutdown",
                "lua_name": "shutdown",
                "params": []
            },
            {
                "c_name": "dse_editor_terrain_brush",
                "lua_name": "terrain_brush",
                "params": [
                    {"name": "mode", "type": "int"},
                    {"name": "cx", "type": "float"},
                    {"name": "cy", "type": "float"},
                    {"name": "radius", "type": "float"},
                    {"name": "strength", "type": "float"},
                    {"name": "falloff", "type": "float", "default": 0.5},
                    {"name": "opacity", "type": "float", "default": 0.5}
                ],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_editor_brush_preview",
                "lua_name": "brush_preview",
                "params": [
                    {"name": "cx", "type": "float"},
                    {"name": "cy", "type": "float"},
                    {"name": "radius", "type": "float"},
                    {"name": "strength", "type": "float"}
                ],
                "out_params": [
                    {"name": "min_x", "type": "float"},
                    {"name": "min_y", "type": "float"},
                    {"name": "max_x", "type": "float"},
                    {"name": "max_y", "type": "float"}
                ]
            },
            {
                "c_name": "dse_editor_place_foliage",
                "lua_name": "place_foliage",
                "params": [
                    {"name": "x", "type": "float"},
                    {"name": "y", "type": "float"},
                    {"name": "radius", "type": "float"},
                    {"name": "density", "type": "float"},
                    {"name": "scale", "type": "float", "default": 0.5},
                    {"name": "mesh_name", "type": "string", "default": "\"default_tree\""}
                ],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_editor_erase_foliage",
                "lua_name": "erase_foliage",
                "params": [
                    {"name": "x", "type": "float"},
                    {"name": "y", "type": "float"},
                    {"name": "radius", "type": "float"},
                    {"name": "strength", "type": "float"}
                ],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_editor_get_foliage_count",
                "lua_name": "get_foliage_count",
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_editor_begin_road",
                "lua_name": "begin_road",
                "params": [
                    {"name": "width", "type": "float", "default": 4.0}
                ],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_editor_add_road_point",
                "lua_name": "add_road_point",
                "params": [
                    {"name": "road_id", "type": "uint32"},
                    {"name": "x", "type": "float"},
                    {"name": "y", "type": "float"},
                    {"name": "z", "type": "float"}
                ]
            },
            {
                "c_name": "dse_editor_end_road",
                "lua_name": "end_road",
                "params": [{"name": "road_id", "type": "uint32"}]
            },
            {
                "c_name": "dse_editor_update_partition_vis",
                "lua_name": "update_partition_vis",
                "params": [
                    {"name": "cx", "type": "float"},
                    {"name": "cy", "type": "float"},
                    {"name": "cz", "type": "float"},
                    {"name": "radius", "type": "float", "default": 256.0}
                ]
            },
            {
                "c_name": "dse_editor_get_cell_count",
                "lua_name": "get_cell_count",
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_editor_undo",
                "lua_name": "undo",
                "params": [],
                "returns": [{"type": "bool"}]
            },
            {
                "c_name": "dse_editor_redo",
                "lua_name": "redo",
                "params": [],
                "returns": [{"type": "bool"}]
            }
        ]
    }
    groups.append(editor)

    # ── §4 dse.vsm — 12 functions ──
    vsm = {
        "group": "world_vsm",
        "lua_path": ["dse", "vsm"],
        "register_func": "RegisterFreeWorldVsmBindings",
        "functions": [
            {
                "c_name": "dse_vsm_init",
                "lua_name": "init",
                "table_input": {
                    "fields": [
                        {"key": "virtual_resolution", "name": "virtual_res", "type": "uint32", "default": 16384},
                        {"key": "page_size", "name": "page_size", "type": "uint32", "default": 128},
                        {"key": "pool_pages", "name": "pool_pages", "type": "uint32", "default": 512},
                        {"key": "clipmap_levels", "name": "clipmap_levels", "type": "uint32", "default": 6}
                    ]
                },
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_vsm_shutdown",
                "lua_name": "shutdown",
                "params": []
            },
            {
                "c_name": "dse_vsm_register_light",
                "lua_name": "register_light",
                "params": [
                    {"name": "entity", "type": "uint32"},
                    {"name": "is_directional", "type": "bool"},
                    {"name": "dx", "type": "float", "default": 0},
                    {"name": "dy", "type": "float", "default": -1},
                    {"name": "dz", "type": "float", "default": 0}
                ],
                "returns": [{"type": "uint32"}]
            },
            {
                "c_name": "dse_vsm_unregister_light",
                "lua_name": "unregister_light",
                "params": [{"name": "light", "type": "uint32"}]
            },
            {
                "c_name": "dse_vsm_begin_frame",
                "lua_name": "begin_frame",
                "params": [
                    {"name": "light", "type": "uint32"},
                    {"name": "cx", "type": "float"},
                    {"name": "cy", "type": "float"},
                    {"name": "cz", "type": "float"}
                ]
            },
            {
                "c_name": "dse_vsm_end_frame",
                "lua_name": "end_frame",
                "params": []
            },
            {
                "c_name": "dse_vsm_invalidate",
                "lua_name": "invalidate",
                "params": [
                    {"name": "light", "type": "uint32"},
                    {"name": "min_x", "type": "float"},
                    {"name": "min_y", "type": "float"},
                    {"name": "min_z", "type": "float"},
                    {"name": "max_x", "type": "float"},
                    {"name": "max_y", "type": "float"},
                    {"name": "max_z", "type": "float"}
                ]
            },
            {
                "c_name": "dse_vsm_mark_page_rendered",
                "lua_name": "mark_rendered",
                "params": [
                    {"name": "light", "type": "uint32"},
                    {"name": "px", "type": "uint32"},
                    {"name": "py", "type": "uint32"},
                    {"name": "level", "type": "uint32"}
                ]
            },
            {
                "c_name": "dse_vsm_get_pages_to_render",
                "lua_name": "get_pages_to_render",
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_vsm_lookup_page",
                "lua_name": "lookup_page",
                "params": [
                    {"name": "light", "type": "uint32"},
                    {"name": "vx", "type": "uint32"},
                    {"name": "vy", "type": "uint32"},
                    {"name": "level", "type": "uint32"}
                ],
                "out_params": [
                    {"name": "px", "type": "uint32"},
                    {"name": "py", "type": "uint32"}
                ],
                "conditional_return": {
                    "check_var": "_ret",
                    "on_true_extra": ["px", "py"]
                },
                "returns": [{"type": "bool"}]
            },
            {
                "c_name": "dse_vsm_get_stats",
                "lua_name": "get_stats",
                "params": [],
                "out_params": [
                    {"name": "total", "type": "int"},
                    {"name": "mapped", "type": "int"},
                    {"name": "dirty", "type": "int"},
                    {"name": "rendered", "type": "int"},
                    {"name": "cache_hit", "type": "int"},
                    {"name": "pool_usage", "type": "int"}
                ],
                "table_return": {
                    "fields": [
                        {"key": "total_pages", "source": "total", "push": "integer"},
                        {"key": "mapped_pages", "source": "mapped", "push": "integer"},
                        {"key": "dirty_pages", "source": "dirty", "push": "integer"},
                        {"key": "rendered_this_frame", "source": "rendered", "push": "integer"},
                        {"key": "cache_hit_percent", "source": "cache_hit", "push": "integer"},
                        {"key": "pool_usage_percent", "source": "pool_usage", "push": "integer"}
                    ]
                }
            },
            {
                "c_name": "dse_vsm_get_clipmap_levels",
                "lua_name": "get_clipmap_levels",
                "params": [],
                "returns": [{"type": "int"}]
            }
        ]
    }
    groups.append(vsm)

    # ── §5 dse.eqs — 12 functions ──
    eqs = {
        "group": "world_eqs",
        "lua_path": ["dse", "eqs"],
        "register_func": "RegisterFreeWorldEqsBindings",
        "functions": [
            {
                "c_name": "dse_eqs_init",
                "lua_name": "init",
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_eqs_shutdown",
                "lua_name": "shutdown",
                "params": []
            },
            {
                "c_name": "dse_eqs_create_template",
                "lua_name": "create_template",
                "params": [{"name": "name", "type": "string"}],
                "returns": [{"type": "uint32"}]
            },
            {
                "c_name": "dse_eqs_destroy_template",
                "lua_name": "destroy_template",
                "params": [{"name": "id", "type": "uint32"}]
            },
            {
                "c_name": "dse_eqs_set_generator",
                "lua_name": "set_generator",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "gen_type", "type": "int"},
                    {"name": "radius", "type": "float", "default": 20.0},
                    {"name": "spacing", "type": "float", "default": 2.0},
                    {"name": "max_items", "type": "int", "default": 200}
                ]
            },
            {
                "c_name": "dse_eqs_add_scorer",
                "lua_name": "add_scorer",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "scorer_type", "type": "int"},
                    {"name": "weight", "type": "float", "default": 1.0},
                    {"name": "invert", "type": "bool", "default": False},
                    {"name": "max_dist", "type": "float", "default": 100.0}
                ]
            },
            {
                "c_name": "dse_eqs_clear_scorers",
                "lua_name": "clear_scorers",
                "params": [{"name": "id", "type": "uint32"}]
            },
            {
                "c_name": "dse_eqs_set_combine_mode",
                "lua_name": "set_combine_mode",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "mode", "type": "int"}
                ]
            },
            {
                "c_name": "dse_eqs_set_max_results",
                "lua_name": "set_max_results",
                "params": [
                    {"name": "id", "type": "uint32"},
                    {"name": "max_results", "type": "uint32"}
                ]
            },
            {
                "c_name": "dse_eqs_execute",
                "lua_name": "execute",
                "params": [
                    {"name": "template_id", "type": "uint32"},
                    {"name": "cx", "type": "float"},
                    {"name": "cy", "type": "float"},
                    {"name": "cz", "type": "float"}
                ],
                "out_params": [
                    {"name": "out", "type": "float_array", "size": 7}
                ],
                "table_return": {
                    "fields": [
                        {"key": "best_x", "source": "out[0]", "push": "number"},
                        {"key": "best_y", "source": "out[1]", "push": "number"},
                        {"key": "best_z", "source": "out[2]", "push": "number"},
                        {"key": "best_score", "source": "out[3]", "push": "number"},
                        {"key": "total_generated", "source": "out[4]", "push": "integer_cast"},
                        {"key": "valid_count", "source": "out[5]", "push": "integer_cast"},
                        {"key": "query_time_ms", "source": "out[6]", "push": "number"}
                    ]
                }
            },
            {
                "c_name": "dse_eqs_get_template_count",
                "lua_name": "get_template_count",
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_eqs_execute_at",
                "lua_name": "execute_at",
                "params": [
                    {"name": "template_id", "type": "uint32"},
                    {"name": "cx", "type": "float"},
                    {"name": "cy", "type": "float"},
                    {"name": "cz", "type": "float"},
                    {"name": "tx", "type": "float"},
                    {"name": "ty", "type": "float"},
                    {"name": "tz", "type": "float"}
                ],
                "out_params": [
                    {"name": "out", "type": "float_array", "size": 5}
                ],
                "table_return": {
                    "fields": [
                        {"key": "best_x", "source": "out[0]", "push": "number"},
                        {"key": "best_y", "source": "out[1]", "push": "number"},
                        {"key": "best_z", "source": "out[2]", "push": "number"},
                        {"key": "best_score", "source": "out[3]", "push": "number"},
                        {"key": "valid_count", "source": "out[4]", "push": "integer_cast"}
                    ]
                }
            }
        ]
    }
    groups.append(eqs)

    # ── §6 dse.distribution — 14 functions ──
    distribution = {
        "group": "world_distribution",
        "lua_path": ["dse", "distribution"],
        "register_func": "RegisterFreeWorldDistBindings",
        "functions": [
            {
                "c_name": "dse_dist_init",
                "lua_name": "init",
                "table_input": {
                    "fields": [
                        {"key": "cell_size", "name": "cell_size", "type": "float", "default": 512.0},
                        {"key": "max_downloads", "name": "max_downloads", "type": "int", "default": 4},
                        {"key": "cdn_url", "name": "cdn_url", "type": "string", "default": "nullptr"}
                    ]
                },
                "params": [],
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_dist_shutdown",
                "lua_name": "shutdown",
                "params": []
            },
            {
                "c_name": "dse_dist_load_manifest",
                "lua_name": "load_manifest",
                "params": [{"name": "path", "type": "string"}],
                "returns": [{"type": "bool"}]
            },
            {
                "c_name": "dse_dist_save_manifest",
                "lua_name": "save_manifest",
                "params": [{"name": "path", "type": "string"}],
                "returns": [{"type": "bool"}]
            },
            {
                "c_name": "dse_dist_package_cell",
                "lua_name": "package_cell",
                "params": [
                    {"name": "cx", "type": "int"},
                    {"name": "cz", "type": "int"},
                    {"name": "lod", "type": "int", "default": 0}
                ],
                "string_array_param": {
                    "name": "assets",
                    "stack_index": 4
                },
                "returns": [{"type": "int"}]
            },
            {
                "c_name": "dse_dist_request_download",
                "lua_name": "request_download",
                "params": [{"name": "package_id", "type": "string"}]
            },
            {
                "c_name": "dse_dist_cancel_download",
                "lua_name": "cancel_download",
                "params": [{"name": "package_id", "type": "string"}]
            },
            {
                "c_name": "dse_dist_update_priorities",
                "lua_name": "update_priorities",
                "params": [
                    {"name": "x", "type": "float"},
                    {"name": "y", "type": "float"},
                    {"name": "z", "type": "float"}
                ]
            },
            {
                "c_name": "dse_dist_tick",
                "lua_name": "tick",
                "params": [
                    {"name": "dt", "type": "float", "default": 0.016}
                ]
            },
            {
                "c_name": "dse_dist_is_installed",
                "lua_name": "is_installed",
                "params": [{"name": "package_id", "type": "string"}],
                "returns": [{"type": "bool"}]
            },
            {
                "c_name": "dse_dist_get_stats",
                "lua_name": "get_stats",
                "params": [],
                "out_params": [
                    {"name": "total", "type": "int"},
                    {"name": "installed", "type": "int"},
                    {"name": "downloading", "type": "int"},
                    {"name": "pending", "type": "int"},
                    {"name": "dl_bytes", "type": "double"},
                    {"name": "speed", "type": "double"}
                ],
                "table_return": {
                    "fields": [
                        {"key": "total_packages", "source": "total", "push": "integer"},
                        {"key": "installed", "source": "installed", "push": "integer"},
                        {"key": "downloading", "source": "downloading", "push": "integer"},
                        {"key": "pending", "source": "pending", "push": "integer"},
                        {"key": "downloaded_bytes", "source": "dl_bytes", "push": "number"},
                        {"key": "speed_bps", "source": "speed", "push": "number"}
                    ]
                }
            },
            {
                "c_name": "dse_dist_get_missing",
                "lua_name": "get_missing",
                "params": [
                    {"name": "x", "type": "float"},
                    {"name": "y", "type": "float"},
                    {"name": "z", "type": "float"},
                    {"name": "radius", "type": "float", "default": 512.0}
                ],
                "null_sep_buffer_return": {
                    "buf_size": 8192
                }
            },
            {
                "c_name": "dse_dist_verify",
                "lua_name": "verify",
                "params": [{"name": "package_id", "type": "string"}],
                "returns": [{"type": "bool"}]
            },
            {
                "c_name": "dse_dist_get_disk_usage",
                "lua_name": "get_disk_usage",
                "params": [],
                "returns": [{"type": "uint32"}]
            }
        ]
    }
    groups.append(distribution)

    return groups


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)
    func_defs_path = os.path.join(repo_root, "tools", "codegen", "function_defs.json")

    with open(func_defs_path, encoding="utf-8") as f:
        data = json.load(f)

    new_groups = build_world_systems_groups()

    # Remove any existing world_* groups (idempotent)
    existing_names = {g["group"] for g in new_groups}
    data["function_groups"] = [g for g in data["function_groups"]
                                if g["group"] not in existing_names]

    # Append new groups
    data["function_groups"].extend(new_groups)

    total_funcs = sum(len(g["functions"]) for g in new_groups)
    print(f"Added {len(new_groups)} groups with {total_funcs} functions")

    with open(func_defs_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    print(f"Updated {func_defs_path}")


if __name__ == "__main__":
    main()
