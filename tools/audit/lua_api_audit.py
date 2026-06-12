#!/usr/bin/env python3
"""Audit Lua bindings vs docs/api/LUA_API.md.

Extracts every Lua-visible function name registered in
engine/scripting/lua/bindings/*.cpp and compares against the names
documented in docs/api/LUA_API.md.
"""
import os
import re
import sys
import json

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIND_DIR = os.path.join(REPO, "engine", "scripting", "lua", "bindings")
DOC = os.path.join(REPO, "docs", "api", "LUA_API.md")

# Names that are module/sub-table names set via lua_setfield, not functions.
# We still capture them but tag separately.
MODULE_SETFIELDS = set()

# regex patterns for registration of a Lua-visible *function* name
PAT_SET_FN = re.compile(r'set_fn\(\s*"([a-zA-Z0-9_]+)"')
PAT_REGISTER_FN = re.compile(r'RegisterFn\(\s*L\s*,\s*"([a-zA-Z0-9_]+)"')
PAT_BINDING_ENTRY = re.compile(r'\{\s*"([a-zA-Z0-9_]+)"\s*,\s*L_')
# lua_setfield with a following lua_pushcfunction is a function; but many
# lua_setfield calls assign subtables. We capture them all then post-filter.
PAT_SETFIELD = re.compile(r'lua_setfield\(\s*L\s*,\s*-?\d+\s*,\s*"([a-zA-Z0-9_]+)"')
PAT_PUSHCFUNC_SETFIELD = re.compile(
    r'lua_pushcfunction\(\s*L\s*,\s*[A-Za-z0-9_]+\s*\)\s*;\s*lua_setfield\(\s*L\s*,\s*-?\d+\s*,\s*"([a-zA-Z0-9_]+)"'
)

bound = {}  # name -> set(files)

def add(name, f):
    bound.setdefault(name, set()).add(os.path.basename(f))

for fn in sorted(os.listdir(BIND_DIR)):
    if not fn.endswith(".cpp"):
        continue
    path = os.path.join(BIND_DIR, fn)
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    for pat in (PAT_SET_FN, PAT_REGISTER_FN, PAT_BINDING_ENTRY, PAT_PUSHCFUNC_SETFIELD):
        for m in pat.finditer(text):
            add(m.group(1), path)
    # collapse whitespace to catch multi-line pushcfunction;setfield
    flat = re.sub(r'\s+', ' ', text)
    for m in PAT_PUSHCFUNC_SETFIELD.finditer(flat):
        add(m.group(1), path)

# documented names: every snake/camel identifier token present anywhere in doc
with open(DOC, encoding="utf-8") as fh:
    doc = fh.read()
doc_tokens = set(re.findall(r'[A-Za-z_][A-Za-z0-9_]*', doc))
# also collect names used as `module.func(` for phantom detection
doc_call_names = set()
for m in re.finditer(r'([a-zA-Z_][a-zA-Z0-9_]*)\.([a-zA-Z0-9_]+)\s*\(', doc):
    doc_call_names.add(m.group(2))

bound_names = set(bound.keys())

undocumented = sorted(n for n in bound_names if n not in doc_tokens)
phantom = sorted(n for n in doc_call_names
                 if n not in bound_names and re.match(r'^[a-z]', n))

print("== TOTAL bound function names:", len(bound_names))
print("== TOTAL doc tokens:", len(doc_tokens))
print()
print("== BOUND but NOT in doc (", len(undocumented), ") ==")
for n in undocumented:
    print(f"  {n:40s} <- {sorted(bound[n])}")
print()
print("== In doc but NOT bound (possible phantom/stale) (", len(phantom), ") ==")
for n in phantom:
    print(f"  {n}")

# dump full bound inventory per file
inv = {}
for fn in sorted(os.listdir(BIND_DIR)):
    if not fn.endswith(".cpp"):
        continue
    names = sorted(n for n, fs in bound.items() if fn in fs)
    if names:
        inv[fn] = names
with open(os.path.join(os.path.dirname(__file__), "bound_inventory.json"), "w", encoding="utf-8") as fh:
    json.dump(inv, fh, indent=2, ensure_ascii=False)
print("\nwrote bound_inventory.json")
