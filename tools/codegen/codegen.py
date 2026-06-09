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
    render(
        "csharp_native.cs.j2",
        "GameScripts/DSEngine/Native.gen.cs",
        components=components,
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
