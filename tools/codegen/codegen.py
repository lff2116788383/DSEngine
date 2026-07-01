#!/usr/bin/env python3
"""
DSEngine Binding Codegen
用法:
    python codegen.py [--defs <path>] [--out <repo_root>] [--dry-run]

依赖:
    pip install jinja2

输入:
    binding_defs.json  — 组件字段定义（唯一数据源）

输出（均写入 <repo_root>/generated/）:
    engine/scripting/native_api/dse_api.gen.h
    engine/scripting/native_api/dse_api_<prefix>.gen.cpp           (每组件一个)
    engine/scripting/lua/bindings/lua_binding_ecs_<prefix>.gen.cpp  (每组件一个)
    GameScripts/DSEngine/Native.gen.cs
"""

import argparse
import json
import os
import sys
from pathlib import Path
from copy import deepcopy

# 控制台可能为非 UTF-8 编码（如 Windows cp1252），统一输出编码避免摘要打印崩溃。
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

try:
    from jinja2 import Environment, FileSystemLoader, StrictUndefined
except ImportError:
    print("[codegen] ERROR: jinja2 not found. Run: pip install jinja2")
    sys.exit(1)


# ── Helpers ──────────────────────────────────────────────────────────────────


def write_if_changed(path: Path, content: str, dry_run: bool) -> bool:
    """内容变化时才写文件，返回是否实际写入。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    if not dry_run:
        path.write_text(content, encoding="utf-8")
    return True


# ── Replication Codec preprocessing ──────────────────────────────────────────

# 可复制的字段类型（字符串不走高频复制路径）
_REPL_TYPES = {"float", "int", "bool", "vec3", "vec4", "euler_quat"}


def _field_write_code(f: dict) -> str:
    """生成 ByteWriter 写入代码（缩进 4 空格）。"""
    n = f["name"]
    t = f["type"]
    if t == "float":
        return f"    w.WriteF32(c.{n});"
    elif t == "int":
        return f"    w.WriteU32(static_cast<uint32_t>(c.{n}));"
    elif t == "bool":
        return f"    w.WriteU8(c.{n} ? 1 : 0);"
    elif t == "vec3":
        return f"    w.WriteF32(c.{n}.x); w.WriteF32(c.{n}.y); w.WriteF32(c.{n}.z);"
    elif t in ("vec4", "euler_quat"):
        return f"    w.WriteF32(c.{n}.x); w.WriteF32(c.{n}.y); w.WriteF32(c.{n}.z); w.WriteF32(c.{n}.w);"
    return ""


def _field_write_inline(f: dict) -> str:
    """单行写入（用于 delta 分支内）。"""
    return _field_write_code(f).strip()


def _field_read_code(f: dict) -> str:
    """生成 ByteReader 读取代码（缩进 4 空格）。"""
    n = f["name"]
    t = f["type"]
    if t == "float":
        return f"    c.{n} = r.ReadF32();"
    elif t == "int":
        return f"    c.{n} = static_cast<int>(r.ReadU32());"
    elif t == "bool":
        return f"    c.{n} = r.ReadU8() != 0;"
    elif t == "vec3":
        return f"    c.{n}.x = r.ReadF32(); c.{n}.y = r.ReadF32(); c.{n}.z = r.ReadF32();"
    elif t in ("vec4", "euler_quat"):
        return f"    c.{n}.x = r.ReadF32(); c.{n}.y = r.ReadF32(); c.{n}.z = r.ReadF32(); c.{n}.w = r.ReadF32();"
    return ""


def _field_read_inline(f: dict) -> str:
    """单行读取。"""
    return _field_read_code(f).strip()


def _field_diff_expr(f: dict) -> str:
    """生成比较表达式。"""
    n = f["name"]
    t = f["type"]
    if t in ("float", "int", "bool"):
        return f"cur.{n} != base.{n}"
    elif t == "vec3":
        return f"cur.{n}.x != base.{n}.x || cur.{n}.y != base.{n}.y || cur.{n}.z != base.{n}.z"
    elif t in ("vec4", "euler_quat"):
        return (f"cur.{n}.x != base.{n}.x || cur.{n}.y != base.{n}.y || "
                f"cur.{n}.z != base.{n}.z || cur.{n}.w != base.{n}.w")
    return "false"


def preprocess_repl_components(components: list) -> list:
    """为复制层模板预处理组件数据，添加 repl_fields / qualified_name 等字段。"""
    result = []
    for comp in components:
        c = deepcopy(comp)
        # 过滤出可复制字段
        repl_fields = []
        for f in c.get("fields", []):
            if f["type"] in _REPL_TYPES:
                repl_fields.append({
                    "name": f["name"],
                    "type": f["type"],
                    "write_code": _field_write_code(f),
                    "write_code_inline": _field_write_inline(f),
                    "read_code": _field_read_code(f),
                    "read_code_inline": _field_read_inline(f),
                    "diff_expr": _field_diff_expr(f),
                })
        c["repl_fields"] = repl_fields
        # All mask
        if repl_fields:
            c["repl_all_mask"] = hex((1 << len(repl_fields)) - 1)
        else:
            c["repl_all_mask"] = "0x0"
        # Qualified C++ name
        ns = c.get("namespace", "")
        c["qualified_name"] = f"{ns}::{c['name']}" if ns else c["name"]
        # dirty flag
        dirty = None
        for f in c.get("fields", []):
            if "dirty_flag" in f:
                dirty = f["dirty_flag"]
                break
        c["dirty_flag"] = dirty
        result.append(c)
    return result


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="DSEngine binding codegen")
    parser.add_argument("--defs", default=None, help="binding_defs.json 路径")
    parser.add_argument("--out",  default=None, help="仓库根目录（生成文件写入此处）")
    parser.add_argument("--dry-run", action="store_true", help="只打印，不写文件")
    args = parser.parse_args()

    script_dir  = Path(__file__).resolve().parent
    defs_path   = Path(args.defs) if args.defs else script_dir / "binding_defs.json"
    repo_root   = Path(args.out)  if args.out  else script_dir.parent.parent
    template_dir = script_dir / "templates"

    if not defs_path.exists():
        print(f"[codegen] ERROR: binding_defs.json not found: {defs_path}")
        sys.exit(1)

    with open(defs_path, encoding="utf-8") as f:
        defs = json.load(f)

    components = defs["components"]

    env = Environment(
        loader=FileSystemLoader(str(template_dir)),
        undefined=StrictUndefined,
        keep_trailing_newline=True,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    written = []
    skipped = []

    def render(template_name: str, out_rel: str, **ctx):
        tpl     = env.get_template(template_name)
        content = tpl.render(**ctx)
        out_abs = repo_root / out_rel
        changed = write_if_changed(out_abs, content, args.dry_run)
        (written if changed else skipped).append(out_rel)

    # ── dse_api.gen.h ────────────────────────────────────────────────────────
    render(
        "dse_api.h.j2",
        "engine/scripting/native_api/dse_api.gen.h",
        components=components,
    )

    # ── dse_api_<prefix>.gen.cpp (每组件，与 Lua 拆分边界对齐) ────────────────
    for comp in components:
        render(
            "dse_api.cpp.j2",
            f"engine/scripting/native_api/dse_api_{comp['prefix']}.gen.cpp",
            comp=comp,
        )

    # ── lua_binding_ecs_<prefix>.gen.cpp (每组件) ────────────────────────────
    for comp in components:
        render(
            "lua_binding.cpp.j2",
            f"engine/scripting/lua/bindings/lua_binding_ecs_{comp['prefix']}.gen.cpp",
            comp=comp,
        )

    # ── Native.gen.cs ────────────────────────────────────────────────────────
    # C# 绑定为构建时生成产物，不纳入版本库（见 .gitignore: /GameScripts/）；
    # 通过 `cmake --build <build> --target dse_codegen` 按需生成。
    render(
        "csharp_native.cs.j2",
        "GameScripts/DSEngine.Runtime/Generated/Native.gen.cs",
        components=components,
    )

    # ── repl_codec.gen.h ─────────────────────────────────────────────────────
    # 复制层统一编解码 — 由 binding_defs.json 单一数据源驱动，消除双序列化漂移。
    repl_components = preprocess_repl_components(components)
    render(
        "repl_codec.gen.h.j2",
        "engine/net/replication/repl_codec.gen.h",
        components=repl_components,
    )

    # ── 输出摘要 ──────────────────────────────────────────────────────────────
    tag = "[DRY-RUN] " if args.dry_run else ""
    print(f"[codegen] {tag}完成")
    for p in written:
        print(f"  ✔ 写入: {p}")
    for p in skipped:
        print(f"  ─ 无变化: {p}")
    print(f"[codegen] 共 {len(written)} 个文件更新，{len(skipped)} 个无变化")


if __name__ == "__main__":
    main()
