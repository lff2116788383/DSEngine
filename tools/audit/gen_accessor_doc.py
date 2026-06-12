#!/usr/bin/env python3
"""Generate Markdown reference for codegen component field accessors.

Source of truth: tools/codegen/binding_defs.json
Output: stdout (markdown), intended to be embedded in docs/api/LUA_API.md.
"""
import json, os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFS = os.path.join(ROOT, "tools", "codegen", "binding_defs.json")

# type -> (getter return desc, setter arg desc)
TYPE_MAP = {
    "vec3":       ("x, y, z",    "x, y, z"),
    "vec4":       ("x, y, z, w", "x, y, z, w"),
    "euler_quat": ("x, y, z",    "x, y, z"),
    "float":      ("number",     "value:number"),
    "int":        ("integer",    "value:int"),
    "enum_int":   ("integer",    "value:int"),
    "bool":       ("boolean",    "value:bool"),
    "string":     ("string",     "value:string"),
}

def main():
    d = json.load(open(DEFS, encoding="utf-8"))
    comps = d["components"]
    out = []
    out.append("## 18. ECS 组件字段访问器（Codegen 自动生成）\n")
    out.append(
        "> 本节由 `tools/codegen/binding_defs.json` 自动派生，"
        "对应 `engine/scripting/lua/bindings/lua_binding_ecs_*.gen.cpp`。\n"
        "> 这些是逐字段的底层 getter/setter，统一注册在 `dse.ecs` 表下，"
        "与上文高层封装函数互补。\n"
        "> 调用约定：getter 形如 `ecs.get_<prefix>_<field>(e)`，"
        "setter 形如 `ecs.set_<prefix>_<field>(e, ...)`。\n")
    total = 0
    for i, c in enumerate(comps, start=1):
        prefix = c["prefix"]
        out.append(f"\n### 18.{i} {c['name']} — 前缀 `{prefix}`\n")
        out.append("| 字段 | 类型 | Getter → 返回 | Setter(参数) | 默认值 |")
        out.append("|------|------|--------------|--------------|--------|")
        for f in c["fields"]:
            t = f["type"]
            getr, setr = TYPE_MAP.get(t, ("value", "value"))
            g = f.get("lua_getter")
            s = f.get("lua_setter")
            default = f.get("default", "—")
            gcell = f"`ecs.{g}(e)` → {getr}" if g else "—"
            scell = f"`ecs.{s}(e, {setr})`" if s else "—"
            out.append(f"| `{f['name']}` | {t} | {gcell} | {scell} | `{default}` |")
            if g: total += 1
            if s: total += 1
    out.insert(2, f"> 合计：{len(comps)} 个组件，{total} 个访问器函数。\n")
    print("\n".join(out))

if __name__ == "__main__":
    main()
