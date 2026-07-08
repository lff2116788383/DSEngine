#!/usr/bin/env python3
"""Full audit: check which dse_api.h functions have NO binding at all."""
import re, json, os
from pathlib import Path

repo = Path(__file__).resolve().parent.parent.parent

# Get all DSE_CAPI function names from dse_api.h + dse_api.gen.h
h = ""
for p in ["engine/scripting/native_api/dse_api.h", "engine/scripting/native_api/dse_api.gen.h"]:
    fp = repo / p
    if fp.exists():
        h += fp.read_text(encoding="utf-8") + "\n"

funcs_in_header = set()
for m in re.finditer(r'DSE_CAPI\s+\S+\s+(\w+)\s*\(', h):
    funcs_in_header.add(m.group(1))
funcs_in_header.discard("dse_native_api_init")
funcs_in_header.discard("dse_get_world_ptr")
funcs_in_header.discard("dse_native_api_init_ext")

# Get functions from function_defs.json
defs = json.loads((repo / "tools/codegen/function_defs.json").read_text(encoding="utf-8"))
funcs_in_defs = set()
for group in defs.get("function_groups", []):
    for fn in group.get("functions", []):
        if "c_name" in fn:
            funcs_in_defs.add(fn["c_name"])
        for call in fn.get("calls", []):
            if "c_name" in call:
                funcs_in_defs.add(call["c_name"])

# Get functions from binding_defs.json
bdefs = json.loads((repo / "tools/codegen/binding_defs.json").read_text(encoding="utf-8"))
funcs_in_binding = set()
for comp in bdefs.get("components", []):
    prefix = comp["prefix"]
    for field in comp.get("fields", []):
        funcs_in_binding.add(f'dse_{prefix}_get_{field["name"]}')
        if not field.get("readonly", False) and field.get("capi_setter") != "manual":
            funcs_in_binding.add(f'dse_{prefix}_set_{field["name"]}')

all_codegen = funcs_in_defs | funcs_in_binding
missing = sorted(funcs_in_header - all_codegen)

# Check which missing functions have hand-written Lua bindings
lua_dir = repo / "engine/scripting/lua/bindings"
lua_files = list(lua_dir.glob("*.cpp")) + list(lua_dir.glob("*.gen.cpp"))
lua_content = ""
for f in lua_files:
    lua_content += f.read_text(encoding="utf-8") + "\n"

truly_missing = []
has_lua = []
for fn in missing:
    if fn in lua_content:
        has_lua.append(fn)
    else:
        truly_missing.append(fn)

print(f"Missing from codegen: {len(missing)}")
print(f"  - Has hand-written Lua binding: {len(has_lua)}")
print(f"  - Truly missing (no binding at all): {len(truly_missing)}")
if truly_missing:
    print("\nTruly missing functions (no Lua binding, no codegen):")
    for f in truly_missing:
        print(f"  - {f}")
else:
    print("\nAll functions have at least one form of binding!")
