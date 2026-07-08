#!/usr/bin/env python3
"""Audit function coverage: dse_api.h vs function_defs.json + binding_defs.json"""
import re, json, sys
from pathlib import Path

script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parent.parent

# Extract all DSE_CAPI function names from dse_api.h + dse_api.gen.h
h = ""
for p in [
    repo_root / "engine/scripting/native_api/dse_api.h",
    repo_root / "engine/scripting/native_api/dse_api.gen.h",
]:
    if p.exists():
        h += p.read_text(encoding="utf-8") + "\n"

funcs_in_header = set()
for m in re.finditer(r'DSE_CAPI\s+\S+\s+(\w+)\s*\(', h):
    funcs_in_header.add(m.group(1))

# Remove init/internal functions
funcs_in_header.discard("dse_native_api_init")
funcs_in_header.discard("dse_get_world_ptr")

# Extract all c_name entries from function_defs.json
# Handle both direct functions (c_name) and composite functions (calls[].c_name)
defs_path = script_dir / "function_defs.json"
funcs_in_defs = set()
if defs_path.exists():
    defs = json.loads(defs_path.read_text(encoding="utf-8"))
    for group in defs.get("function_groups", []):
        for fn in group.get("functions", []):
            # Direct function
            if "c_name" in fn:
                funcs_in_defs.add(fn["c_name"])
            # Composite function — extract all called C functions
            for call in fn.get("calls", []):
                if "c_name" in call:
                    funcs_in_defs.add(call["c_name"])

# Also check binding_defs.json for component field functions
bdefs_path = script_dir / "binding_defs.json"
funcs_in_binding = set()
if bdefs_path.exists():
    bdefs = json.loads(bdefs_path.read_text(encoding="utf-8"))
    for comp in bdefs.get("components", []):
        prefix = comp["prefix"]
        for field in comp.get("fields", []):
            funcs_in_binding.add(f"dse_{prefix}_get_{field['name']}")
            if not field.get("readonly", False):
                if field.get("capi_setter") != "manual":
                    funcs_in_binding.add(f"dse_{prefix}_set_{field['name']}")
        # Also check for add functions
        if comp.get("capi_add"):
            funcs_in_binding.add(comp["capi_add"])

all_codegen = funcs_in_defs | funcs_in_binding
missing = sorted(funcs_in_header - all_codegen)

print(f"Total functions in dse_api.h (+.gen.h): {len(funcs_in_header)}")
print(f"Functions in function_defs.json: {len(funcs_in_defs)}")
print(f"Functions in binding_defs.json (component fields): {len(funcs_in_binding)}")
print(f"Combined codegen coverage: {len(all_codegen)}")
print(f"Missing from codegen: {len(missing)}")
if missing:
    print("\nFunctions in dse_api.h but NOT in function_defs.json or binding_defs.json:")
    for f in missing:
        print(f"  - {f}")
else:
    print("\nAll functions covered!")
