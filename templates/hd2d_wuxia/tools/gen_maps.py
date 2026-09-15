#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""HD-2D 武侠模板：地图布局 + 分层地图渲染 + mapdata.lua 导出。

地图用「特征列表」描述（水面/崖壁/竹林/树/房屋/道路/篱笆/灯笼），
同一份数据既用于绘制美术，也用于生成 Lua 侧的碰撞格子与实体摆放，
保证"看到的就是能撞到的"。
"""
import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from gen_lib import (C, PAL, disc, fx_glow, new_img, noise_img, px, px_ellipse,
                     px_line, px_poly, px_rect, shade, soft_shadow, thick_line,
                     tint_noise)

TILE = 32

# ---------------------------------------------------------------------------
# 地图特征描述
# ---------------------------------------------------------------------------
MAP_SPECS = [
    {
        "id": "village",
        "name": "暮色青溪村",
        "w": 44, "h": 30,
        "music": "field",
        "ambient": (0.42, 0.46, 0.62, 0.55),
        "water_rows": 27,                # 从该行到底部为溪流
        "bridge": (19, 23),
        "cliffs": [(0, 0, 44, 2), (0, 0, 3, 30), (41, 0, 3, 30)],
        "gate": (20, 0),                 # 北侧通往山寨的谷口
        "bamboo": [(5, 5, 3), (12, 3, 2), (36, 4, 3), (39, 12, 2), (4, 20, 3), (33, 22, 3)],
        "trees": [(9, 20), (14, 21), (29, 19), (34, 16), (17, 8), (27, 24), (7, 14)],
        "houses": [
            (6, 9, 7, 5, "home"),
            (25, 8, 8, 5, "shop"),
            (15, 15, 8, 5, "temple"),
        ],
        "paths": [
            [(21, 26), (21, 21), (19, 18), (14, 17), (11, 14), (11, 12), (9, 11)],
            [(21, 21), (24, 18), (26, 14), (29, 12)],
            [(21, 18), (21, 12), (21, 4)],
        ],
        "fences": [((4, 14), (4, 19)), ((34, 12), (34, 17)), ((23, 19), (30, 19))],
        "lanterns": [(3, 13), (15, 13), (24, 12), (32, 11), (20, 26), (20, 22)],
        "wells": [(19, 20)],
        "chests": [(33, 5)],
        "stones": [(13, 6), (30, 21), (8, 17), (26, 6)],
        "flowers": [(10, 7), (18, 7), (22, 15), (28, 20), (16, 5), (37, 18), (6, 24), (31, 25)],
        "spawn": (20.5, 25.5),
        "exits": [{"x": 20.0, "y": 0.4, "w": 4.0, "h": 1.6, "to": "stronghold", "entry": "south"}],
        "npcs": [{"id": "elder", "x": 18.5, "y": 14.6, "dir": "r", "dialog": "elder"},
                 {"id": "smith", "x": 27.5, "y": 13.4, "dir": "d", "dialog": "smith"},
                 {"id": "villager", "x": 10.5, "y": 17.5, "dir": "u", "dialog": "villager"}],
        "items": [{"kind": "hp_potion", "x": 12.5, "y": 21.5},
                  {"kind": "mp_potion", "x": 35.5, "y": 15.5},
                  {"kind": "coin", "x": 6.6, "y": 11.6},
                  {"kind": "coin", "x": 30.6, "y": 25.6}],
        "enemies": [{"kind": "bandit", "x": 9.5, "y": 8.5, "patrol": 2.5},
                    {"kind": "bandit", "x": 33.5, "y": 9.5, "patrol": 2.0},
                    {"kind": "wolf", "x": 15.5, "y": 24.5, "patrol": 3.0},
                    {"kind": "wolf", "x": 37.5, "y": 20.5, "patrol": 2.5},
                    {"kind": "ghost", "x": 26.5, "y": 22.5, "patrol": 1.5}],
        "boss": None,
    },
    {
        "id": "stronghold",
        "name": "夜雨黑风寨",
        "w": 44, "h": 30,
        "music": "boss",
        "ambient": (0.52, 0.38, 0.44, 0.6),
        "water_rows": None,
        "bridge": None,
        "cliffs": [(0, 0, 44, 3), (0, 0, 4, 30), (40, 0, 4, 30), (0, 26, 44, 4)],
        "gate": None,
        "bamboo": [(6, 6, 2), (37, 6, 2)],
        "trees": [(9, 9), (35, 10), (8, 20), (36, 20)],
        "houses": [
            (11, 12, 9, 6, "hall"),
            (30, 14, 7, 5, "barrack"),
            (18, 8, 6, 4, "store"),
        ],
        "paths": [
            [(21, 27), (21, 22), (20, 18), (21, 14), (22, 10), (22, 5)],
            [(21, 18), (13, 18), (11, 15)],
            [(21, 16), (31, 17), (33, 16)],
        ],
        "fences": [((15, 20), (27, 20)), ((29, 22), (29, 25))],
        "lanterns": [(14, 13), (27, 12), (19, 19), (24, 19), (21, 24), (16, 24), (28, 24)],
        "wells": [(23, 21)],
        "chests": [(19, 9), (34, 16)],
        "stones": [(12, 24), (32, 24), (6, 14)],
        "flowers": [(17, 25), (26, 26), (10, 11), (35, 11)],
        "spawn": (21.5, 26.5),
        "exits": [{"x": 20.0, "y": 28.2, "w": 4.0, "h": 1.6, "to": "village", "entry": "north"}],
        "npcs": [],
        "items": [{"kind": "hp_potion", "x": 17.5, "y": 19.5},
                  {"kind": "mp_potion", "x": 25.5, "y": 19.5}],
        "enemies": [{"kind": "bandit", "x": 13.5, "y": 22.5, "patrol": 2.5},
                    {"kind": "bandit", "x": 29.5, "y": 21.5, "patrol": 2.5},
                    {"kind": "bandit", "x": 20.5, "y": 13.5, "patrol": 2.0},
                    {"kind": "wolf", "x": 34.5, "y": 23.5, "patrol": 2.0},
                    {"kind": "ghost", "x": 12.5, "y": 16.5, "patrol": 1.5},
                    {"kind": "bandit", "x": 24.5, "y": 10.5, "patrol": 1.5}],
        "boss": {"kind": "boss", "x": 21.5, "y": 6.5, "patrol": 3.0},
    },
]

SOLID_CHARS = set("#=frT BglxcwW h".replace(" ", ""))
WALK_CHARS = set(".ps*hD%")


def build_grid(spec):
    """由特征列表生成字符网格（碰撞 + 地形类型）。"""
    W, H = spec["w"], spec["h"]
    g = [["." for _ in range(W)] for _ in range(H)]
    for (x, y, w, h) in spec.get("cliffs", []):
        for yy in range(max(0, y), min(H, y + h)):
            for xx in range(max(0, x), min(W, x + w)):
                g[yy][xx] = "#"
    wr = spec.get("water_rows")
    if wr is not None:
        for yy in range(wr, H):
            for xx in range(W):
                if g[yy][xx] == "#":
                    continue
                edge = 1 if yy == wr and (xx + int(2 * math.sin(xx * 0.7))) % 5 == 0 else 0
                g[yy][xx] = "W" if yy >= wr + 2 else "w"
                if edge:
                    g[yy][xx] = "."
    for (cx, cy, r) in spec.get("bamboo", []):
        for yy in range(cy - r, cy + r + 1):
            for xx in range(cx - r, cx + r + 1):
                if 0 <= xx < W and 0 <= yy < H and g[yy][xx] in (".", ","):
                    if (xx - cx) ** 2 + (yy - cy) ** 2 <= r * r + 1:
                        g[yy][xx] = "B"
    for (tx, ty) in spec.get("trees", []):
        g[ty][tx] = "T"
    for (hx, hy, hw, hh, _style) in spec.get("houses", []):
        for yy in range(hy, min(H, hy + hh)):
            for xx in range(hx, min(W, hx + hw)):
                g[yy][xx] = "="
    for path in spec.get("paths", []):
        for i in range(len(path) - 1):
            x0, y0 = path[i]
            x1, y1 = path[i + 1]
            steps = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
            for k in range(steps + 1):
                t = k / steps
                cx = x0 + (x1 - x0) * t
                cy = y0 + (y1 - y0) * t
                for ox, oy in ((0, 0), (1, 0), (0, 1)):
                    xx, yy = int(cx) + ox, int(cy) + oy
                    if 0 <= xx < W and 0 <= yy < H and g[yy][xx] in (".", ",", "*", "%", "h"):
                        g[yy][xx] = "p"
    for (a, b) in spec.get("fences", []):
        x0, y0 = a
        x1, y1 = b
        steps = max(abs(x1 - x0), abs(y1 - y0))
        for k in range(steps + 1):
            t = k / max(1, steps)
            xx, yy = int(round(x0 + (x1 - x0) * t)), int(round(y0 + (y1 - y0) * t))
            if 0 <= xx < W and 0 <= yy < H and g[yy][xx] in (".", ",", "p"):
                g[yy][xx] = "f"
    for (lx, ly) in spec.get("lanterns", []):
        g[ly][lx] = "l"
    for (wx, wy) in spec.get("wells", []):
        g[wy][wx] = "o"
        if wx + 1 < W:
            g[wy][wx + 1] = "o"
    for (cx, cy) in spec.get("chests", []):
        g[cy][cx] = "c"
    for (sx, sy) in spec.get("stones", []):
        g[sy][sx] = "x"
    for (fx, fy) in spec.get("flowers", []):
        if g[fy][fx] in (".", ","):
            g[fy][fx] = "*"
    br = spec.get("bridge")
    if br:
        x0, x1 = br
        for yy in range(wr or (H - 3), H):
            for xx in range(x0, x1 + 1):
                g[yy][xx] = "D"
    if spec.get("gate"):
        gx, gy = spec["gate"]
        for xx in range(gx, gx + 4):
            for yy in range(gy, gy + 3):
                if 0 <= xx < W and 0 <= yy < H and g[yy][xx] == "#":
                    g[yy][xx] = "G"
    return ["".join(row) for row in g]


def is_solid_char(ch):
    return ch in "=frTBloxc#"
# ---------------------------------------------------------------------------
# 地形绘制
# ---------------------------------------------------------------------------
def _hash(*args):
    h = 2166136261
    for a in args:
        h = ((h ^ (int(a) & 0xFFFFFFFF)) * 16777619) & 0xFFFFFFFF
    return h


def _grass_tile(img, x0, y0, seed, dark=False):
    base = 0.82 + ((_hash(seed, 7) % 100) / 100.0) * 0.34
    col = shade("grass_d" if dark else "grass", base)
    px_rect(img, x0, y0, TILE, TILE, col)
    rnd = random.Random(seed * 977 + 13)
    for _ in range(9):
        gx = x0 + rnd.randint(1, TILE - 2)
        gy = y0 + rnd.randint(2, TILE - 1)
        ln = rnd.randint(2, 4)
        c = shade("grass_l" if rnd.random() < 0.6 else "grass_h", 0.9 + rnd.random() * 0.3)
        px_line(img, gx, gy, gx + rnd.choice((-1, 0, 1)), gy - ln, c)
    for _ in range(2):
        gx = x0 + rnd.randint(0, TILE - 1)
        gy = y0 + rnd.randint(0, TILE - 1)
        px(img, gx, gy, shade("grass_sh", 1.0))


def _dirt_tile(img, x0, y0, seed):
    base = 0.86 + ((_hash(seed, 3) % 100) / 100.0) * 0.28
    px_rect(img, x0, y0, TILE, TILE, shade("dirt", base))
    rnd = random.Random(seed * 331 + 7)
    for _ in range(6):
        gx = x0 + rnd.randint(1, TILE - 2)
        gy = y0 + rnd.randint(1, TILE - 2)
        px(img, gx, gy, shade("dirt_l", 0.9 + rnd.random() * 0.3))
    for _ in range(4):
        gx = x0 + rnd.randint(1, TILE - 2)
        gy = y0 + rnd.randint(1, TILE - 2)
        px(img, gx, gy, shade("dirt_d", 0.9))


def _water_tile(img, x0, y0, seed, deep=False):
    px_rect(img, x0, y0, TILE, TILE, C("water_d" if deep else "water"))
    rnd = random.Random(seed * 131 + 3)
    for k in range(3):
        yy = y0 + 6 + k * 9 + rnd.randint(-2, 2)
        ph = rnd.random() * 6.28
        for xx in range(TILE):
            y2 = yy + int(2.2 * math.sin((xx + seed) * 0.35 + ph))
            px(img, x0 + xx, y2, shade("water_l", 0.75 + 0.25 * rnd.random()))
            if rnd.random() < 0.25:
                px(img, x0 + xx, y2 + 1, shade("water", 1.15))


def _rock_tile(img, x0, y0, seed, top=False):
    px_rect(img, x0, y0, TILE, TILE, C("rock_d"))
    rnd = random.Random(seed * 457 + 11)
    px_rect(img, x0, y0, TILE, TILE - 6, C("rock"))
    for k in range(4):
        yy = y0 + 5 + k * 6
        px_line(img, x0, yy, x0 + TILE - 1, yy + rnd.randint(-1, 1), shade("rock_d", 0.92))
    for _ in range(7):
        gx = x0 + rnd.randint(1, TILE - 2)
        gy = y0 + rnd.randint(2, TILE - 3)
        px(img, gx, gy, shade("rock_l", 0.85 + rnd.random() * 0.4))
    if top:
        px_rect(img, x0, y0, TILE, 4, shade("rock_l", 1.05))
        for _ in range(5):
            px(img, x0 + rnd.randint(0, TILE - 1), y0, shade("grass_l", 0.9))


def _floor_tile(img, x0, y0, seed):
    px_rect(img, x0, y0, TILE, TILE, C("wood_d"))
    for k in range(0, TILE, 8):
        px_line(img, x0, y0 + k, x0 + TILE - 1, y0 + k, shade("wood", 0.8))
    rnd = random.Random(seed * 61 + 5)
    for _ in range(5):
        px(img, x0 + rnd.randint(0, TILE - 1), y0 + rnd.randint(0, TILE - 1), shade("wood", 1.1))


# ---------------------------------------------------------------------------
# 物件
# ---------------------------------------------------------------------------
def draw_tree(ground, fg, px_x, px_y, scale=1.0, seed=0):
    """px_x/px_y 为树根底部中心（像素坐标，y 向下）。"""
    rnd = random.Random(seed)
    trunk_w = max(5, int(9 * scale))
    trunk_h = int(52 * scale)
    px_rect(ground, px_x - trunk_w // 2, px_y - trunk_h, trunk_w, trunk_h, C("wood_d"))
    px_rect(ground, px_x - trunk_w // 2 + 1, px_y - trunk_h, max(2, trunk_w // 3), trunk_h,
            shade("wood", 1.0))
    cx, cy = px_x, px_y - trunk_h
    rad = int(30 * scale)
    for i, (ox, oy, rr) in enumerate([(-rad * 0.55, rad * 0.18, rad * 0.72),
                                      (rad * 0.55, rad * 0.10, rad * 0.68),
                                      (0, -rad * 0.30, rad * 0.85)]):
        c = shade("bamboo_d" if i % 2 == 0 else "grass_d", 1.0 - i * 0.06)
        disc(ground, cx + ox, cy + oy, rr, c)
    for i in range(46):
        a = rnd.random() * math.tau
        r = rad * (0.35 + rnd.random() * 0.85)
        x = cx + math.cos(a) * r
        y = cy + math.sin(a) * r * 0.85
        col = shade("bamboo_l" if y < cy else "bamboo", 0.8 + rnd.random() * 0.55)
        disc(ground, x, y, 1.6 + rnd.random() * 2.0, col)
    # 上层树冠画进前景层，玩家走到树后可被遮挡
    for i in range(26):
        a = rnd.random() * math.tau
        r = rad * (0.30 + rnd.random() * 0.62)
        x = cx + math.cos(a) * r
        y = cy + math.sin(a) * r * 0.6 - rad * 0.55
        col = shade("bamboo_d" if rnd.random() < 0.5 else "grass_d", 0.85 + rnd.random() * 0.4)
        disc(fg, x, y, 2.2 + rnd.random() * 2.6, col)
    for i in range(14):
        a = rnd.random() * math.tau
        r = rad * (0.22 + rnd.random() * 0.5)
        disc(fg, cx + math.cos(a) * r, cy + math.sin(a) * r * 0.5 - rad * 0.6,
             1.6, shade("bamboo_l", 0.95 + rnd.random() * 0.3))


def draw_bamboo(ground, fg, px_x, px_y, count=3, seed=0):
    rnd = random.Random(seed)
    for k in range(count):
        bx = px_x + rnd.randint(-12, 12)
        h = rnd.randint(66, 104)
        wdt = 4
        px_rect(ground, bx, px_y - h, wdt, h, shade("bamboo_d", 0.9))
        px_rect(ground, bx, px_y - h, 2, h, shade("bamboo", 1.1))
        for seg in range(0, h, 14):
            px_line(ground, bx, px_y - h + seg, bx + wdt, px_y - h + seg,
                    shade("bamboo_l", 0.8))
        ly = px_y - h
        for _ in range(9):
            ln = rnd.randint(6, 14)
            ang = rnd.uniform(-2.6, -0.5)
            ex = bx + wdt // 2 + math.cos(ang) * ln
            ey = ly + math.sin(ang) * ln + rnd.randint(-6, 6)
            for layer, col in ((ground, "bamboo"), (fg, "bamboo_l")):
                thick_line(layer, bx + wdt // 2, ly + rnd.randint(0, 10), ex, ey, 2,
                           shade(col, 0.85 + rnd.random() * 0.45))


def draw_house(ground, fg, spec_h, style):
    hx, hy, hw, hh = spec_h[0] * TILE, spec_h[1] * TILE, spec_h[2] * TILE, spec_h[3] * TILE
    wall_top = hy + int(hh * 0.42)
    wall_h = hh - (wall_top - hy)
    # 墙体
    px_rect(ground, hx + 4, wall_top, hw - 8, wall_h, C("wall"))
    px_rect(ground, hx + 4, wall_top + wall_h - 6, hw - 8, 6, C("wall_d"))
    for bx in range(hx + 4, hx + hw - 8, 18):
        px_rect(ground, bx, wall_top, 3, wall_h, C("wood_d"))
    px_rect(ground, hx + 4, wall_top, hw - 8, 3, C("wood_d"))
    # 门窗
    door_w, door_h = 22, 34
    dx = hx + hw // 2 - door_w // 2
    px_rect(ground, dx, hy + hh - door_h - 4, door_w, door_h, C("wood_d"))
    px_rect(ground, dx + 3, hy + hh - door_h, door_w - 6, door_h - 4, shade("wood", 0.85))
    px_rect(ground, dx + door_w - 8, hy + hh - door_h // 2, 3, 3, C("gold"))
    for wx in (hx + 14, hx + hw - 34):
        wy = wall_top + 10
        px_rect(ground, wx, wy, 18, 16, C("wood_d"))
        px_rect(ground, wx + 2, wy + 2, 14, 12, C("paper"))
        px_line(ground, wx + 9, wy + 2, wx + 9, wy + 13, C("wood_d"), 1)
        px_line(ground, wx + 2, wy + 8, wx + 15, wy + 8, C("wood_d"), 1)
    # 屋顶（瓦片）
    eave = 8
    roof_bot = wall_top + 4
    roof_top = hy
    px_poly(ground, [(hx - eave, roof_bot), (hx + hw + eave, roof_bot),
                     (hx + hw - 6, roof_top + 6), (hx + 6, roof_top + 6)],
            C("roof"))
    for k in range(0, hw + eave * 2, 8):
        px_line(ground, hx - eave + k, roof_bot, hx - eave + k - 3, roof_top + 8,
                shade("roof_d", 1.0))
    px_poly(ground, [(hx + 6, roof_top + 6), (hx + hw - 6, roof_top + 6),
                     (hx + hw - 10, roof_top), (hx + 10, roof_top)], C("roof_l"))
    px_line(ground, hx - eave, roof_bot, hx + hw + eave, roof_bot, C("roof_d"), 2)
    # 檐下阴影 + 门前台阶
    px_rect(ground, hx + 4, wall_top, hw - 8, 5, (10, 10, 16, 90))
    px_rect(ground, dx - 4, hy + hh - 4, door_w + 8, 4, C("stone_d"))
    # 前檐画入前景（角色经过门前会被轻微遮挡）
    px_poly(fg, [(hx - eave, roof_bot), (hx + hw + eave, roof_bot),
                 (hx + hw + eave - 2, roof_bot + 5), (hx - eave + 2, roof_bot + 5)],
            shade("roof_d", 0.9, 235))


def draw_fence(ground, x, y):
    px_rect(ground, x + 8, y + 10, 4, 20, C("wood_d"))
    px_rect(ground, x + 8, y + 14, TILE, 3, shade("wood", 0.95))
    px_rect(ground, x + 8, y + 22, TILE, 3, shade("wood", 0.8))


def draw_lantern(ground, x, y, color=(255, 186, 94)):
    px_rect(ground, x + 13, y + 12, 5, 20, C("wood_d"))
    px_rect(ground, x + 10, y + 4, 11, 10, C("lantern"))
    px_rect(ground, x + 11, y + 5, 4, 8, C("lantern_hot"))
    px_rect(ground, x + 9, y + 2, 13, 3, C("ink2"))
    px_rect(ground, x + 9, y + 14, 13, 2, C("ink2"))
    return (x + 15.5, y + 8.0, color)


def draw_well(ground, x, y):
    px_ellipse(ground, x + TILE, y + 20, 22, 14, C("stone_d"))
    px_ellipse(ground, x + TILE, y + 18, 18, 11, C("stone"))
    px_ellipse(ground, x + TILE, y + 18, 13, 7, (18, 22, 30, 255))
    px_rect(ground, x + 12, y - 10, 4, 30, C("wood_d"))
    px_rect(ground, x + TILE + 2, y - 10, 4, 30, C("wood_d"))
    px_poly(ground, [(x + 4, y - 10), (x + TILE * 2 - 4, y - 10), (x + TILE, y - 24)],
            C("roof"))


def draw_chest(ground, x, y):
    px_rect(ground, x + 6, y + 12, 20, 16, C("wood"))
    px_rect(ground, x + 6, y + 12, 20, 5, shade("wood_l", 1.0))
    px_rect(ground, x + 14, y + 12, 4, 16, C("gold_d"))
    px_rect(ground, x + 6, y + 20, 20, 2, C("gold_d"))


def draw_stone(ground, x, y):
    px_ellipse(ground, x + TILE // 2, y + 22, 11, 8, C("stone_d"))
    px_ellipse(ground, x + TILE // 2 - 1, y + 20, 8, 6, C("stone"))


def draw_flowers(ground, x, y, seed):
    rnd = random.Random(seed)
    for _ in range(3):
        fx = x + rnd.randint(4, TILE - 4)
        fy = y + rnd.randint(6, TILE - 4)
        col = rnd.choice(["robe_l", "white", "gold", "ghost_glow"])
        px_line(ground, fx, fy, fx, fy - 4, C("grass_l"), 1)
        px(ground, fx, fy - 5, C(col))
        px(ground, fx - 1, fy - 5, C(col))
        px(ground, fx, fy - 6, C(col))


def draw_cliff_face(ground, spec, grid):
    W, H = spec["w"], spec["h"]
    for y in range(H):
        for x in range(W):
            if grid[y][x] not in ("#", "G"):
                continue
            top = (y == 0 or grid[y - 1][x] not in ("#", "G"))
            _rock_tile(ground, x * TILE, y * TILE, x * 131 + y * 17, top)


def bake_lights(img, w, h, lights, ambient=(0.55, 0.58, 0.72)):
    lm = Image.new("RGB", (w, h), tuple(int(c * 255) for c in ambient))
    for (lx, ly, col, radius) in lights:
        size = int(radius * 2)
        g = fx_glow(size, col, 2.0)
        g = g.convert("RGB")
        tmp = Image.new("RGB", (w, h), (0, 0, 0))
        tmp.paste(g, (int(lx - radius), int(ly - radius)))
        lm = ImageChops.add(lm, tmp, scale=1.0, offset=0)
    return lm


def bake_shadows(ground, w, h, shadows):
    mask = new_img(w, h, (0, 0, 0, 0))
    d = ImageDraw.Draw(mask)
    for (sx, sy, sw, sh, alpha) in shadows:
        d.ellipse([sx - sw / 2 + 4, sy - sh / 2 + 6, sx + sw / 2 + 4, sy + sh / 2 + 6],
                  fill=(6, 8, 14, alpha))
    mask = mask.filter(ImageFilter.GaussianBlur(3.0))
    return Image.alpha_composite(ground, mask)
# ---------------------------------------------------------------------------
# 天空 / 远景
# ---------------------------------------------------------------------------
def render_sky(spec):
    W, H = spec["w"], spec["h"]
    w, h = W * TILE, H * TILE
    img = new_img(w, h, (0, 0, 0, 255))
    for y in range(h):
        t = y / max(1, h - 1)
        c0, c1 = PAL["sky_top"], PAL["sky_bot"]
        col = tuple(int(c0[i] + (c1[i] - c0[i]) * (t ** 0.8)) for i in range(3)) + (255,)
        px_rect(img, 0, y, w, 1, col)
    rnd = random.Random(4242)
    for _ in range(220):
        x, y = rnd.randint(0, w - 1), rnd.randint(0, int(h * 0.62))
        b = rnd.randint(120, 240)
        a = rnd.randint(60, 190)
        px(img, x, y, (b, b, min(255, b + 16), a))
    # 月
    mx, my = int(w * 0.74), int(h * 0.20)
    disc(img, mx, my, 34, (250, 246, 216, 255))
    disc(img, mx, my, 30, PAL["moon"] + (255,))
    px_ellipse(img, mx - 10, my - 6, 6, 5, (222, 216, 188, 255))
    px_ellipse(img, mx + 8, my + 7, 8, 6, (226, 220, 190, 255))
    glow = fx_glow(220, (250, 240, 200), 2.4)
    img.alpha_composite(glow, (mx - 110, my - 110))
    # 云
    for k in range(9):
        cx = rnd.randint(0, w)
        cy = rnd.randint(30, int(h * 0.55))
        cw = rnd.randint(90, 260)
        layer = new_img(w, h)
        for j in range(rnd.randint(4, 8)):
            disc(layer, cx + rnd.randint(-cw // 2, cw // 2), cy + rnd.randint(-10, 10),
                 rnd.randint(16, 34), PAL["cloud"] + (rnd.randint(40, 90),))
        layer = layer.filter(ImageFilter.GaussianBlur(6))
        img.alpha_composite(layer)
    return img


def render_far(spec):
    W, H = spec["w"], spec["h"]
    w, h = W * TILE, H * TILE
    img = new_img(w, h)
    horizon = int(h * 0.62)
    rnd = random.Random(777)
    # 远山（两层）
    for layer, (col, base, amp) in enumerate([("mtn_far", horizon - 40, 46),
                                              ("mtn", horizon + 10, 62)]):
        pts = [(0, h)]
        x = 0
        while x <= w:
            y = base + math.sin(x * 0.004 + layer * 2.0) * amp + rnd.randint(-10, 10)
            pts.append((x, y))
            x += rnd.randint(40, 90)
        pts.append((w, h))
        px_poly(img, pts, C(col))
        for _ in range(30):
            px_line(img, rnd.randint(0, w), base + rnd.randint(-10, 20),
                    rnd.randint(0, w), base + rnd.randint(0, 40), shade(col, 0.9, 120), 2)
    # 山脚雾带
    mist = new_img(w, h)
    for k in range(6):
        disc(mist, rnd.randint(0, w), horizon + rnd.randint(-20, 40), rnd.randint(60, 160),
             PAL["mist"] + (60,))
    img.alpha_composite(mist.filter(ImageFilter.GaussianBlur(12)))
    # 竹影剪影
    for k in range(26):
        bx = rnd.randint(0, w)
        bh = rnd.randint(90, 220)
        by = horizon + rnd.randint(-10, 30)
        thick_line(img, bx, by, bx + rnd.randint(-6, 6), by - bh, 3,
                   shade("bamboo_d", 0.85, 200))
        for _ in range(4):
            ly = by - bh + rnd.randint(-30, 20)
            thick_line(img, bx, ly, bx + rnd.randint(-26, 26), ly + rnd.randint(-16, 8), 2,
                       shade("bamboo_d", 0.9, 190))
    return img


# ---------------------------------------------------------------------------
# 主渲染
# ---------------------------------------------------------------------------
def render_map(spec, out_dir):
    W, H = spec["w"], spec["h"]
    w, h = W * TILE, H * TILE
    grid = build_grid(spec)
    ground = new_img(w, h)
    fg = new_img(w, h)
    lights, shadows = [], []

    for y in range(H):
        for x in range(W):
            ch = grid[y][x]
            x0, y0 = x * TILE, y * TILE
            seed = x * 977 + y * 131
            if ch in ("w", "W"):
                _water_tile(ground, x0, y0, seed, deep=(ch == "W"))
            elif ch in ("#", "G"):
                continue
            elif ch == "=":
                _floor_tile(ground, x0, y0, seed)
            elif ch in ("p", "D"):
                _dirt_tile(ground, x0, y0, seed)
            else:
                _grass_tile(ground, x0, y0, seed, dark=(ch == ","))
            if ch == "*":
                draw_flowers(ground, x0, y0, seed)
            elif ch == "x":
                draw_stone(ground, x0, y0)
                shadows.append((x0 + TILE // 2, y0 + 24, 30, 14, 70))
            elif ch == "f":
                draw_fence(ground, x0, y0)
                shadows.append((x0 + 16, y0 + 26, 40, 10, 60))
            elif ch == "l":
                lx, ly, col = draw_lantern(ground, x0, y0)
                lights.append((lx, ly, col, 96.0))
                shadows.append((x0 + 16, y0 + 30, 26, 10, 70))
            elif ch == "o":
                if x == 0 or grid[y][x - 1] != "o":
                    draw_well(ground, x0, y0)
            elif ch == "c":
                draw_chest(ground, x0, y0)
                shadows.append((x0 + 16, y0 + 28, 34, 12, 80))

    draw_cliff_face(ground, spec, grid)

    # 水岸浪花
    for y in range(H):
        for x in range(W):
            if grid[y][x] not in ("w", "W"):
                continue
            x0, y0 = x * TILE, y * TILE
            if y > 0 and grid[y - 1][x] not in ("w", "W"):
                px_rect(ground, x0, y0, TILE, 2, (200, 216, 214, 190))
                px_rect(ground, x0, y0 + 2, TILE, 1, shade("water_l", 1.1, 200))

    # 房屋 / 树 / 竹
    for hs in spec.get("houses", []):
        draw_house(ground, fg, hs, hs[4])
        hx, hy, hw, hh = hs[0] * TILE, hs[1] * TILE, hs[2] * TILE, hs[3] * TILE
        shadows.append((hx + hw / 2 + 6, hy + hh - 6, hw + 18, 22, 90))
        lights.append((hx + 14.0, hy + hh * 0.6, (255, 196, 120), 64.0))
        lights.append((hx + hw - 34.0, hy + hh * 0.6, (255, 196, 120), 64.0))
    for i, (tx, ty) in enumerate(spec.get("trees", [])):
        draw_tree(ground, fg, tx * TILE + TILE // 2, ty * TILE + TILE - 2, 1.0, i * 31 + 5)
        shadows.append((tx * TILE + TILE // 2 + 6, ty * TILE + TILE - 8, 74, 24, 80))
    for i, (bx, by, cnt) in enumerate(spec.get("bamboo", [])):
        draw_bamboo(ground, fg, bx * TILE + TILE // 2, by * TILE + TILE - 2, cnt, i * 17 + 3)
        shadows.append((bx * TILE + TILE // 2, by * TILE + TILE - 8, 60, 18, 70))

    ground = bake_shadows(ground, w, h, shadows)
    lm = bake_lights(ground, w, h, lights, ambient=spec.get("ambient", (0.55, 0.58, 0.72))[:3])
    rgb = ImageChops.multiply(ground.convert("RGB"), lm)
    a = ground.getchannel("A")
    ground = rgb.convert("RGBA")
    ground.putalpha(a)

    # 手绘质感 + 边缘压暗
    ground = tint_noise(ground, 0.16, 5, seed=9, smooth=False)
    ground = tint_noise(ground, 0.12, 27, seed=11, smooth=True)
    vig = new_img(w, h, (0, 0, 0, 0))
    dv = ImageDraw.Draw(vig)
    for k in range(6):
        dv.rectangle([k * 8, k * 8, w - 1 - k * 8, h - 1 - k * 8], outline=(0, 0, 0, 26))
    ground = Image.alpha_composite(ground, vig.filter(ImageFilter.GaussianBlur(14)))

    sky = render_sky(spec)
    far = render_far(spec)

    # 导出
    os.makedirs(out_dir, exist_ok=True)
    names = {}
    for key, im in (("sky", sky), ("far", far), ("ground", ground), ("fg", fg)):
        fn = "%s_%s.png" % (spec["id"], key)
        im.save(os.path.join(out_dir, fn))
        names[key] = fn

    collide = []
    for y in range(H):
        row = "".join("#" if (grid[y][x] in "#=frTBloxc" or grid[y][x] in "wW") else "."
                      for x in range(W))
        collide.append(row)

    def wx(px_x):
        return round(px_x / TILE, 3)

    def wy(px_y):
        return round(H - px_y / TILE, 3)

    data = {
        "id": spec["id"], "name": spec["name"], "w": W, "h": H, "tile": TILE,
        "bg": names, "parallax": {"sky": 0.10, "far": 0.42},
        "spawn": {"x": spec["spawn"][0], "y": spec["spawn"][1]}, "collide": collide,
        "exits": spec.get("exits", []), "npcs": spec.get("npcs", []),
        "items": spec.get("items", []), "enemies": spec.get("enemies", []),
        "boss": spec.get("boss"),
        "music": spec.get("music", "field"),
        "lights": [{"x": wx(lx), "y": wy(ly), "r": col[0] / 255.0, "g": col[1] / 255.0,
                    "b": col[2] / 255.0, "size": round(radius / TILE, 3)}
                   for (lx, ly, col, radius) in lights],
    }
    return data


def _lua_val(v, indent=0):
    pad = "  " * indent
    if v is None:
        return "nil"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        return ("%g" % v)
    if isinstance(v, str):
        return '"%s"' % v.replace("\\", "\\\\").replace('"', '\\"')
    if isinstance(v, (list, tuple)):
        if not v:
            return "{}"
        parts = ["{"]
        for it in v:
            if isinstance(it, dict):
                parts.append(pad + "  " + _lua_val(it, indent + 1) + ",")
            else:
                parts.append(pad + "  " + _lua_val(it, indent + 1) + ",")
        parts.append(pad + "}")
        return "\n".join(parts)
    if isinstance(v, dict):
        if not v:
            return "{}"
        parts = ["{"]
        for k, val in v.items():
            parts.append("%s  %s = %s," % (pad, k, _lua_val(val, indent + 1)))
        parts.append(pad + "}")
        return "\n".join(parts)
    return "nil"


def generate_maps(out_root):
    maps_dir = os.path.join(out_root, "assets", "maps")
    script_dir = os.path.join(out_root, "scripts")
    os.makedirs(script_dir, exist_ok=True)
    entries = []
    for spec in MAP_SPECS:
        entries.append(render_map(spec, maps_dir))
        print("[maps] %s -> %s" % (spec["id"], maps_dir))
    lines = ["-- 由 tools/gen_maps.py 自动生成，请勿手改（改地图请改 MAP_SPECS 后重新生成）",
             "return {"]
    for e in entries:
        lines.append(_lua_val(e, 1) + ",")
    lines.append("}")
    with open(os.path.join(script_dir, "mapdata.lua"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("[maps] mapdata.lua -> %s" % script_dir)