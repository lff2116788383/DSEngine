#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成中文位图字体图集 + scripts/font_atlas.lua。

引擎自带的 dse.font.load_cjk 只打包 U+4E00 起的连续 800 个码位，
常用字大量缺失；模板改用「自绘 SDF 无关的位图图集 + Lua 逐字排版」，
字形覆盖由本脚本扫描模板文本保证。
"""
import os
import re

from PIL import Image, ImageDraw, ImageFont

FONT_CANDIDATES = [
    os.path.join(os.path.dirname(__file__), "..", "assets", "font", "NotoSansSC-Regular.ttf"),
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "apps", "editor_cpp", "fonts",
                 "NotoSansSC-Regular.ttf"),
]

CJK_PUNCT = "　、。，．《》〈〉【】（）！？：；「」～％＋－＝"


def collect_chars(extra_texts):
    chars = set(chr(c) for c in range(0x20, 0x7F))
    chars.update(CJK_PUNCT)
    for t in extra_texts:
        for ch in t:
            if ord(ch) >= 0x80:
                chars.add(ch)
    return sorted(chars)


def _find_font():
    for p in FONT_CANDIDATES:
        p = os.path.abspath(p)
        if os.path.exists(p):
            return p
    raise SystemExit("找不到 OFL 中文字体 NotoSansSC-Regular.ttf，请放到 assets/font/ 或 apps/editor_cpp/fonts/ 下")


def build_atlas(chars, size, out_png, out_lua, font_path, name):
    font = ImageFont.truetype(font_path, size)
    ascent, descent = font.getmetrics()
    cells = []
    max_w = max_h = 1
    for ch in chars:
        bbox = font.getbbox(ch)
        w = max(1, bbox[2] - bbox[0])
        h = max(1, bbox[3] - bbox[1])
        adv = max(1, int(round(font.getlength(ch))))
        cells.append((ch, bbox, w, h, adv))
        max_w = max(max_w, w)
        max_h = max(max_h, h)
    pad = 1
    cw, chh = max_w + pad * 2, max_h + pad * 2
    cols = 16
    rows = (len(cells) + cols - 1) // cols
    img = Image.new("RGBA", (cols * cw, rows * chh), (255, 255, 255, 0))
    d = ImageDraw.Draw(img)
    entries = []
    for i, (ch, bbox, gw, gh, adv) in enumerate(cells):
        col, row = i % cols, i // cols
        x0, y0 = col * cw + pad, row * chh + pad
        d.text((x0 - bbox[0], y0 - bbox[1]), ch, font=font, fill=(255, 255, 255, 255))
        W, H = img.size
        px, py = x0 - bbox[0] + bbox[0], y0  # 字形左上角像素
        u0 = px / W
        v0 = 1.0 - (py + gh) / H
        u1 = (px + gw) / W
        v1 = 1.0 - py / H
        entries.append((ord(ch), u0, v0, u1, v1, gw, gh, adv,
                        bbox[0], ascent + bbox[1]))
    img.save(out_png)
    lines = ["-- 由 tools/gen_font.py 自动生成（%s, %dpx）：位图字体图集度量" % (name, size),
             "return {",
             "  texture = \"%s\"," % os.path.basename(out_png),
             "  size = %d, ascent = %d, descent = %d, line = %d," % (size, ascent, descent, ascent + descent),
             "  glyphs = {"]
    for (cp, u0, v0, u1, v1, gw, gh, adv, ox, oy) in entries:
        ch = chr(cp)
        if ch == '"':
            ch = '\\"'
        elif ch == "\\":
            ch = "\\\\"
        lines.append(('    [%d]={u0=%.5f,v0=%.5f,u1=%.5f,v1=%.5f,w=%d,h=%d,adv=%d,ox=%d,oy=%d}, -- %s'
                      % (cp, u0, v0, u1, v1, gw, gh, adv, ox, oy, ch)).rstrip())
    lines.append("  },")
    lines.append("}")
    with open(out_lua, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("[font] %s: %d glyphs -> %s" % (name, len(entries), out_png))
    return len(entries)


def generate_fonts(out_root, extra_texts):
    ui_dir = os.path.join(out_root, "assets", "ui")
    script_dir = os.path.join(out_root, "scripts")
    os.makedirs(ui_dir, exist_ok=True)
    os.makedirs(script_dir, exist_ok=True)
    font_path = _find_font()
    chars = collect_chars(extra_texts)
    build_atlas(chars, 18, os.path.join(ui_dir, "font_small.png"),
                os.path.join(script_dir, "font_small.lua"), font_path, "small")
    build_atlas(chars, 34, os.path.join(ui_dir, "font_big.png"),
                os.path.join(script_dir, "font_big.lua"), font_path, "big")
    return chars