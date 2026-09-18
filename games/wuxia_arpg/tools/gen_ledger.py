#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Regenerate docs/design/WUXIA_ARPG_NEW_ASSET_LEDGER.{md,csv} for games/wuxia_arpg."""
from __future__ import annotations
import csv
import hashlib
from pathlib import Path

TOOL = Path(__file__).resolve()
GAME = TOOL.parents[1]
ROOT = TOOL.parents[3]
OUT_MD = ROOT / "docs" / "design" / "WUXIA_ARPG_NEW_ASSET_LEDGER.md"
OUT_CSV = ROOT / "docs" / "design" / "WUXIA_ARPG_NEW_ASSET_LEDGER.csv"

GENERATED_ACTOR = "gen_assets.py"
GENERATED_PROP = "gen_assets.py"
GENERATED_AUDIO = "gen_assets.py"
GENERATED_FONT = "gen_assets.py + apps/editor_cpp/fonts/NotoSansSC-Regular.ttf"


def rel(p: Path) -> str:
    return p.relative_to(ROOT).as_posix()


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def classify(path: Path):
    r = rel(path)
    if r.endswith(".gitignore") or r.endswith("README.md"):
        return ("文档/仓库配置", "仓库手写", "DSEngine Authors", "Apache-2.0", "HAND")
    if "/assets/actor/" in r:
        return ("新生成角色图集", GENERATED_ACTOR, "DSEngine Authors", "Apache-2.0", "G1")
    if "/assets/props/" in r:
        return ("新生成 3D 道具贴图", GENERATED_PROP, "DSEngine Authors", "Apache-2.0", "G1")
    if "/assets/audio/" in r:
        return ("新生成音频", GENERATED_AUDIO, "DSEngine Authors", "Apache-2.0", "G1")
    if r.endswith("/assets/ui/font.png") or r.endswith("/scripts/font.lua"):
        return ("新生成字体图集/度量", GENERATED_FONT,
                "DSEngine Authors / Noto CJK contributors",
                "Apache-2.0 (生成输出) + SIL OFL 1.1 (字形来源)", "G1")
    if "/tools/" in r and r.endswith(".py"):
        return ("生成/验收工具源码", "仓库手写", "DSEngine Authors", "Apache-2.0", "HAND")
    if "/scripts/" in r and r.endswith(".lua"):
        return ("新游戏 Lua 源码", "仓库手写", "DSEngine Authors", "Apache-2.0", "HAND")
    return ("其他源文件", "仓库手写", "DSEngine Authors", "Apache-2.0", "HAND")


def main():
    files = [p for p in GAME.rglob("*") if p.is_file()
             and "__pycache__" not in p.parts
             and p.name != "wuxia_arpg_save.dat"
             and ".git" not in p.parts]
    files.sort()
    lines = []
    lines.append("# 青溪问剑（新游戏）资产许可台账")
    lines.append("")
    lines.append("> 由 `games/wuxia_arpg/tools/gen_ledger.py` 自动生成。")
    lines.append("> 本工程不使用 `templates/hd2d_wuxia` 的代码或素材。")
    lines.append("")
    lines.append("## 外部输入")
    lines.append("")
    lines.append("| 输入 | 路径 | 作者/来源 | 许可 | 说明 |")
    lines.append("|---|---|---|---|---|")
    lines.append("| Noto Sans SC Regular | `apps/editor_cpp/fonts/NotoSansSC-Regular.ttf` | Google Noto CJK contributors | SIL OFL 1.1 | 仅用于生成新字体图集 |")
    lines.append("| Pillow | 生成期环境（不随仓库提交） | Python Pillow contributors | HPND License | 仅素材生成/像素统计 |")
    lines.append("")
    lines.append("## 逐条清单")
    lines.append("")
    lines.append("| # | 相对路径 | 类型 | 来源 | 作者 | 许可 | 命令 | sha256 |")
    lines.append("|---:|---|---|---|---|---|---|---|")
    with OUT_CSV.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["path", "type", "source", "author", "license", "generation_command", "sha256"])
        for i, p in enumerate(files, 1):
            typ, src, author, lic, cmd = classify(p)
            digest = sha256(p)
            lines.append(f"| {i} | `{rel(p)}` | {typ} | {src} | {author} | {lic} | {cmd} | `{digest}` |")
            w.writerow([rel(p), typ, src, author, lic, cmd, digest])
    OUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[ledger] {len(files)} files -> {OUT_MD}, {OUT_CSV}")


if __name__ == "__main__":
    main()