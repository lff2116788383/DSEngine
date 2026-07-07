#!/usr/bin/env python3
"""Patch lua_binding_modules.h: replace old world systems declarations with codegen ones."""
import pathlib

p = pathlib.Path("engine/scripting/lua/bindings/lua_binding_modules.h")
t = p.read_text("utf-8")

old = """// 6\u5927\u4e16\u754c\u7cfb\u7edf\uff08Spline / Ocean / EditorTools / VSM / EQS / Distribution\uff09
void RegisterWorldSystemsBindings(lua_State* L);
void ShutdownWorldSystemsBindings();"""

new = """// 6\u5927\u4e16\u754c\u7cfb\u7edf\uff08Spline / Ocean / EditorTools / VSM / EQS / Distribution\uff09\u2014 codegen \u751f\u6210
void RegisterFreeWorldSplineBindings(lua_State* L);
void RegisterFreeWorldOceanBindings(lua_State* L);
void RegisterFreeWorldEditorBindings(lua_State* L);
void RegisterFreeWorldVsmBindings(lua_State* L);
void RegisterFreeWorldEqsBindings(lua_State* L);
void RegisterFreeWorldDistBindings(lua_State* L);"""

if old in t:
    t = t.replace(old, new)
    p.write_text(t, "utf-8")
    print("OK: patched modules.h")
else:
    print("SKIP: old text not found (may already be patched)")
