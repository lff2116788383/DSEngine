#!/usr/bin/env python3
"""Patch lua_binding_registry.cpp: replace RegisterWorldSystemsBindings with 6 codegen calls."""
import pathlib

p = pathlib.Path("engine/scripting/lua/bindings/lua_binding_registry.cpp")
t = p.read_text("utf-8")

old = "    RegisterWorldSystemsBindings(L);"
new = """    RegisterFreeWorldSplineBindings(L);
    RegisterFreeWorldOceanBindings(L);
    RegisterFreeWorldEditorBindings(L);
    RegisterFreeWorldVsmBindings(L);
    RegisterFreeWorldEqsBindings(L);
    RegisterFreeWorldDistBindings(L);"""

if old in t:
    t = t.replace(old, new)
    p.write_text(t, "utf-8")
    print("OK: patched registry")
else:
    print("SKIP: old text not found")
