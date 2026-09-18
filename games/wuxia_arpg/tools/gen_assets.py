#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""New HD-2D wuxia ARPG assets (R3, not derived from templates/hd2d_wuxia).

Outputs:
  games/wuxia_arpg/assets/actor/{hero,bandit}_{d,u,l,r}_atlas.{png,dsprite.json}
  games/wuxia_arpg/assets/props/*.png
  games/wuxia_arpg/assets/ui/font.png
  games/wuxia_arpg/scripts/font.lua
  games/wuxia_arpg/assets/audio/sfx_*.wav, bgm_loop.wav

Font input: apps/editor_cpp/fonts/NotoSansSC-Regular.ttf (SIL OFL 1.1).
All generated files are original project output under Apache-2.0.
"""
import json
import math
import os
import random
import struct
import wave
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")
ACTOR_DIR = os.path.join(ASSETS, "actor")
PROP_DIR = os.path.join(ASSETS, "props")
UI_DIR = os.path.join(ASSETS, "ui")
AUDIO_DIR = os.path.join(ASSETS, "audio")
SCRIPT_DIR = os.path.join(ROOT, "scripts")
NOTO = os.path.abspath(os.path.join(ROOT, "..", "..", "apps", "editor_cpp", "fonts",
                                                "NotoSansSC-Regular.ttf"))


def ensure_dirs():
    for d in (ACTOR_DIR, PROP_DIR, UI_DIR, AUDIO_DIR, SCRIPT_DIR):
        os.makedirs(d, exist_ok=True)


def img(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def px(d, x, y, c):
    if 0 <= x < d._image.size[0] and 0 <= y < d._image.size[1]:
        d.point((x, y), fill=c)


def rect(d, x0, y0, x1, y1, c):
    d.rectangle([x0, y0, x1, y1], fill=c)


def poly(d, pts, c):
    d.polygon(pts, fill=c)


HERO = dict(robe=(54, 96, 162, 255), robe_d=(30, 54, 104, 255), trim=(226, 226, 214, 255),
            belt=(198, 172, 106, 255), hair=(18, 18, 24, 255), skin=(232, 188, 150, 255),
            blade=(214, 224, 232, 255), boot=(42, 42, 54, 255))
BANDIT = dict(robe=(126, 54, 48, 255), robe_d=(74, 32, 30, 255), trim=(206, 174, 130, 255),
              belt=(96, 66, 38, 255), hair=(28, 20, 18, 255), skin=(226, 178, 142, 255),
              blade=(198, 206, 214, 255), boot=(48, 40, 38, 255))


def human(kind, direction, action, i, n):
    w, h = (32, 48) if kind == "hero" else (30, 44)
    im = img(w, h)
    d = ImageDraw.Draw(im)
    pal = HERO if kind == "hero" else BANDIT
    cx = w // 2
    foot = h - 2
    bob = -1 if action == "idle" and i % 2 else 0
    if action == "walk":
        bob = -1 if i % 2 else 0
    if action == "attack":
        lean = 1 if i >= n // 2 else 0
    else:
        lean = 0
    if action == "dodge":
        bob = 2
    if action == "hurt":
        bob = 1

    # legs / boots
    step = 0
    if action == "walk":
        step = -2 if i % 2 == 0 else 2
    if action == "die":
        # fallen body: horizontal silhouette
        rect(d, 5, foot - 10, w - 6, foot - 6, pal["robe_d"])
        rect(d, 7, foot - 15, w - 9, foot - 10, pal["robe"])
        rect(d, w - 10, foot - 20, w - 4, foot - 14, pal["skin"])
        rect(d, w - 12, foot - 22, w - 5, foot - 18, pal["hair"])
        return im
    rect(d, cx - 6, foot - 12 + bob, cx - 2, foot, pal["boot"])
    rect(d, cx + 1 + step, foot - 12 + bob, cx + 5 + step, foot, pal["boot"])
    # robe / body
    rect(d, cx - 8, foot - 32 + bob, cx + 8, foot - 10 + bob, pal["robe"])
    rect(d, cx - 8, foot - 32 + bob, cx + 8, foot - 29 + bob, pal["robe_d"])
    rect(d, cx - 6, foot - 25 + bob, cx + 6, foot - 23 + bob, pal["belt"])
    rect(d, cx - 2, foot - 32 + bob, cx + 3, foot - 10 + bob, pal["trim"])
    # head / hair
    rect(d, cx - 6, foot - 44 + bob, cx + 6, foot - 32 + bob, pal["skin"])
    rect(d, cx - 7, foot - 46 + bob, cx + 7, foot - 40 + bob, pal["hair"])
    if direction == "u":
        rect(d, cx - 6, foot - 43 + bob, cx + 6, foot - 34 + bob, pal["hair"])
    elif direction == "l":
        rect(d, cx - 7, foot - 43 + bob, cx - 2, foot - 35 + bob, pal["hair"])
    elif direction == "r":
        rect(d, cx + 2, foot - 43 + bob, cx + 7, foot - 35 + bob, pal["hair"])
    # arms / weapon
    ax = cx + (7 if direction != "l" else -7)
    if action == "attack" and i >= n // 2:
        if direction == "l":
            rect(d, cx - 14, foot - 30 + bob, cx - 2, foot - 27 + bob, pal["blade"])
            rect(d, cx - 16, foot - 32 + bob, cx - 13, foot - 25 + bob, pal["blade"])
        elif direction == "r":
            rect(d, cx + 2, foot - 30 + bob, cx + 14, foot - 27 + bob, pal["blade"])
        else:
            rect(d, cx - 10, foot - 36 + bob, cx + 10, foot - 33 + bob, pal["blade"])
    else:
        rect(d, ax - 2, foot - 31 + bob, ax + 2, foot - 18 + bob, pal["skin"])
    return im


def gen_actor(kind):
    actions = {"idle": 2, "walk": 4, "attack": 4, "dodge": 3, "hurt": 1, "die": 4}
    fps = {"idle": 4.0, "walk": 8.0, "attack": 12.0, "dodge": 10.0, "hurt": 4.0, "die": 8.0}
    for direction in ("d", "u", "l", "r"):
        frames = []
        for action, n in actions.items():
            for i in range(n):
                frames.append((action, i, human(kind, direction, action, i, n)))
        cw = max(im.width for _, _, im in frames)
        ch = max(im.height for _, _, im in frames)
        atlas = Image.new("RGBA", (cw * len(frames), ch), (0, 0, 0, 0))
        for idx, (_, _, im) in enumerate(frames):
            atlas.alpha_composite(im, (idx * cw, 0))
        png = f"{kind}_{direction}_atlas.png"
        jsonp = f"{kind}_{direction}_atlas.dsprite.json"
        atlas.save(os.path.join(ACTOR_DIR, png))
        jframes = []
        jclips = {}
        cursor = 0
        for action, n in actions.items():
            indices = []
            for i in range(n):
                x = cursor * cw
                jframes.append({
                    "name": f"{action}_{i}", "index": cursor,
                    "pixel_rect": {"x": x, "y": 0, "w": cw, "h": ch},
                    "uv_rect": {"x": x / atlas.width, "y": 0.0,
                                "w": cw / atlas.width, "h": 1.0},
                    "pivot": {"x": 0.5, "y": 0.0},
                })
                indices.append(cursor)
                cursor += 1
            jclips[action] = {"frames": indices, "fps": fps[action],
                              "loop": action in ("idle", "walk")}
        payload = {"version": 1, "texture": png, "width": atlas.width, "height": atlas.height,
                   "frames": jframes, "clips": jclips}
        with open(os.path.join(ACTOR_DIR, jsonp), "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False)
        print("[actor]", png, atlas.size, len(frames), "frames")


def prop_tree():
    im = img(48, 64)
    d = ImageDraw.Draw(im)
    rect(d, 21, 34, 27, 62, (96, 66, 38, 255))
    rect(d, 22, 34, 24, 62, (132, 94, 52, 255))
    for (x, y, r, c) in [(24, 22, 19, (48, 112, 62, 255)), (13, 30, 13, (38, 92, 54, 255)),
                         (35, 31, 13, (58, 128, 68, 255)), (24, 12, 13, (72, 146, 76, 255))]:
        d.ellipse([x-r, y-r, x+r, y+r], fill=c)
    return im


def prop_bamboo():
    im = img(32, 64)
    d = ImageDraw.Draw(im)
    for x, h in ((8, 60), (16, 56), (24, 62)):
        rect(d, x, 64-h, x+3, 63, (82, 136, 58, 255))
        for yy in range(64-h, 64, 12):
            rect(d, x-2, yy, x+5, yy+2, (58, 104, 46, 255))
    return im


def prop_house():
    im = img(96, 80)
    d = ImageDraw.Draw(im)
    rect(d, 12, 34, 84, 76, (154, 126, 92, 255))
    rect(d, 12, 34, 84, 40, (92, 70, 50, 255))
    poly(d, [(4, 36), (48, 6), (92, 36)], (100, 52, 44, 255))
    rect(d, 40, 52, 56, 76, (72, 48, 36, 255))
    rect(d, 18, 46, 32, 58, (214, 200, 156, 255))
    rect(d, 64, 46, 78, 58, (214, 200, 156, 255))
    return im


def prop_lantern():
    im = img(16, 32)
    d = ImageDraw.Draw(im)
    rect(d, 7, 0, 8, 30, (68, 48, 36, 255))
    rect(d, 3, 6, 12, 22, (196, 54, 44, 255))
    rect(d, 5, 8, 10, 20, (238, 92, 58, 255))
    rect(d, 6, 22, 9, 25, (220, 190, 96, 255))
    return im


def prop_rock():
    im = img(32, 24)
    d = ImageDraw.Draw(im)
    poly(d, [(2, 22), (7, 8), (15, 3), (26, 7), (30, 22)], (104, 108, 112, 255))
    poly(d, [(8, 20), (12, 10), (22, 8), (26, 20)], (138, 142, 146, 255))
    return im


def gen_props():
    for name, fn in (("tree", prop_tree), ("bamboo", prop_bamboo), ("house", prop_house),
                     ("lantern", prop_lantern), ("rock", prop_rock)):
        fn().save(os.path.join(PROP_DIR, f"{name}.png"))
        print("[prop]", name)


def gen_font():
    chars = set(chr(i) for i in range(32, 127))
    text = ("青溪问剑 生命 内力 体力 等级 金币 攻击 防御 闪避 技能 连招 山贼 侠客 "
            "开始游戏 读取存档 已保存 读档成功 获得 升级 消灭 暂停 继续 退出 按 WASD 移动 J 攻击 K 闪避 U 技能 I 治疗 "
            "铁剑 布衣 玉佩 稀有 魔法 传奇 普通 铜钱 掉落 伤害 暴击 未命中 已装备 背包 天气 落叶 村庄 夜雨")
    chars.update(text)
    chars = sorted(chars)
    font_path = NOTO
    if not os.path.exists(font_path):
        raise SystemExit("missing OFL font: " + font_path)
    font = ImageFont.truetype(font_path, 20)
    ascent, descent = font.getmetrics()
    cells = []
    max_w = max_h = 1
    for ch in chars:
        bbox = font.getbbox(ch)
        gw = max(1, bbox[2] - bbox[0]); gh = max(1, bbox[3] - bbox[1])
        adv = max(1, int(round(font.getlength(ch))))
        cells.append((ch, bbox, gw, gh, adv))
        max_w = max(max_w, gw); max_h = max(max_h, gh)
    pad = 1; cw = max_w + pad*2; chh = max_h + pad*2; cols = 16
    rows = (len(cells) + cols - 1)//cols
    atlas = img(cols*cw, rows*chh)
    d = ImageDraw.Draw(atlas)
    entries = []
    for i, (ch, bbox, gw, gh, adv) in enumerate(cells):
        col, row = i % cols, i // cols
        x0, y0 = col*cw+pad, row*chh+pad
        d.text((x0-bbox[0], y0-bbox[1]), ch, font=font, fill=(255, 255, 255, 255))
        u0 = x0/atlas.width; v0 = 1.0-(y0+gh)/atlas.height
        u1 = (x0+gw)/atlas.width; v1 = 1.0-y0/atlas.height
        entries.append((ord(ch), u0, v0, u1, v1, gw, gh, adv, bbox[0], ascent+bbox[1]))
    atlas.save(os.path.join(UI_DIR, "font.png"))
    lines = ["-- Generated by games/wuxia_arpg/tools/gen_assets.py", "return {",
             '  texture = "font.png",', f"  size = 20, ascent = {ascent}, descent = {descent}, line = {ascent+descent},",
             "  glyphs = {"]
    for (cp, u0, v0, u1, v1, gw, gh, adv, ox, oy) in entries:
        ch = chr(cp)
        if ch == '"': ch = '\\"'
        elif ch == "\\": ch = "\\\\"
        lines.append(("    [%d]={u0=%.6f,v0=%.6f,u1=%.6f,v1=%.6f,w=%d,h=%d,adv=%d,ox=%d,oy=%d}, -- %s"
                      % (cp, u0, v0, u1, v1, gw, gh, adv, ox, oy, ch)).rstrip())
    lines += ["  },", "}", ""]
    with open(os.path.join(SCRIPT_DIR, "font.lua"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("[font]", len(cells), "glyphs", atlas.size)


def _wav(path, samples, sr=22050):
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
        frames = b"".join(struct.pack("<h", max(-32767, min(32767, int(s*32767)))) for s in samples)
        w.writeframes(frames)


def _tone(freq, dur, vol=0.5, sr=22050, decay=6.0):
    n = int(dur*sr)
    return [vol*math.sin(2*math.pi*freq*i/sr)*math.exp(-decay*i/sr) for i in range(n)]


def gen_audio():
    sr = 22050
    fx = {
        "slash": [0.45*math.sin(2*math.pi*(900-400*i/6000)*i/sr)*math.exp(-9*i/sr) for i in range(6000)],
        "hit": [0.5*math.sin(2*math.pi*(180-80*i/4000)*i/sr)*math.exp(-10*i/sr) for i in range(4000)],
        "pickup": _tone(880,0.16,0.45,sr,8)+_tone(1320,0.16,0.35,sr,8),
        "levelup": _tone(523,0.18,0.45,sr,4)+_tone(659,0.18,0.45,sr,4)+_tone(784,0.30,0.5,sr,4),
        "click": _tone(420,0.06,0.35,sr,12),
    }
    for name, samples in fx.items():
        _wav(os.path.join(AUDIO_DIR, f"sfx_{name}.wav"), samples, sr)
        print("[audio]", name)
    # simple 4s loop bed (pentatonic)
    notes = [220, 261.63, 293.66, 329.63, 392.0]
    samples = [0.0]*(sr*4)
    for k, f in enumerate(notes):
        seg = _tone(f, 1.0, 0.16, sr, 1.6)
        for i, s in enumerate(seg):
            idx = (k*int(sr*0.7)+i) % len(samples)
            samples[idx] += s
    _wav(os.path.join(AUDIO_DIR, "bgm_loop.wav"), samples, sr)
    print("[audio] bgm_loop")


def main():
    ensure_dirs()
    random.seed(20260918)
    gen_actor("hero")
    gen_actor("bandit")
    gen_props()
    gen_font()
    gen_audio()
    print("[done] new wuxia assets")


if __name__ == "__main__":
    main()