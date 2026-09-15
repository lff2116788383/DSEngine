#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""HD-2D 武侠模板素材生成库：调色板 / 绘图工具 / 角色与特效画笔。

坐标与尺寸约定（与 Lua 运行时一致）：
  * 1 个地图格子 = 32 px 美术 = 1.0 世界单位
  * 角色图按「脚底为原点」绘制，导出 PNG 的像素尺寸即精灵尺寸
"""
import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter

random.seed(20260915)

# ---------------------------------------------------------------------------
# 调色板（暮色武侠：青灰冷调 + 暖灯火）
# ---------------------------------------------------------------------------
PAL = {
    "sky_top": (24, 28, 52), "sky_mid": (52, 54, 84), "sky_bot": (128, 104, 104),
    "moon": (238, 232, 200), "cloud": (86, 88, 118), "cloud_lit": (128, 122, 142),
    "mtn_far": (58, 64, 92), "mtn": (44, 48, 72), "mtn_lit": (86, 90, 122),
    "mist": (132, 138, 164),
    "bamboo_d": (24, 48, 40), "bamboo": (46, 88, 64), "bamboo_l": (104, 148, 92),
    "grass_d": (34, 56, 38), "grass": (56, 90, 52), "grass_l": (94, 134, 74),
    "grass_h": (146, 176, 100), "grass_sh": (26, 42, 32),
    "dirt": (98, 76, 54), "dirt_d": (70, 54, 40), "dirt_l": (132, 106, 74),
    "stone": (108, 108, 116), "stone_d": (74, 74, 84), "stone_l": (152, 152, 158),
    "rock": (92, 88, 96), "rock_d": (58, 56, 66), "rock_l": (134, 130, 138),
    "wood": (98, 66, 42), "wood_d": (62, 42, 28), "wood_l": (146, 106, 68),
    "roof": (48, 46, 66), "roof_l": (86, 82, 110), "roof_d": (30, 30, 44),
    "wall": (198, 184, 152), "wall_d": (150, 138, 112), "wall_s": (110, 100, 82),
    "paper": (250, 214, 140), "lantern": (255, 178, 84), "lantern_hot": (255, 240, 196),
    "torch": (255, 132, 52), "fire": (255, 196, 96),
    "water_d": (26, 48, 66), "water": (44, 84, 104), "water_l": (104, 156, 162),
    "foam": (196, 214, 214),
    "ink": (16, 16, 24), "ink2": (34, 32, 48), "ink3": (56, 54, 72),
    "gold": (214, 176, 96), "gold_d": (150, 116, 56),
    "skin": (228, 182, 142), "skin_d": (188, 134, 102), "skin_s": (156, 104, 78),
    "hair": (32, 28, 38), "hair_l": (66, 56, 66),
    "robe": (172, 58, 52), "robe_d": (118, 34, 36), "robe_l": (212, 100, 82),
    "cloth_blue": (58, 84, 132), "cloth_blue_d": (38, 54, 92), "cloth_grey": (118, 116, 124),
    "leather": (110, 78, 48), "leather_d": (74, 52, 32),
    "steel": (186, 192, 204), "steel_d": (124, 132, 148), "steel_l": (232, 238, 246),
    "ghost": (150, 214, 214), "ghost_d": (86, 150, 158), "ghost_glow": (206, 246, 246),
    "hp": (198, 56, 50), "mp": (66, 122, 202), "stam": (122, 182, 88),
    "xp": (206, 170, 84), "white": (236, 236, 228), "black": (12, 12, 18),
}


def C(name, a=255):
    """取调色板颜色，可覆盖透明度。"""
    r, g, b = PAL[name]
    return (r, g, b, a)


def shade(name, f, a=255):
    r, g, b = PAL[name]
    return (max(0, min(255, int(r * f))), max(0, min(255, int(g * f))),
            max(0, min(255, int(b * f))), a)


# ---------------------------------------------------------------------------
# 小工具
# ---------------------------------------------------------------------------
def new_img(w, h, color=(0, 0, 0, 0)):
    return Image.new("RGBA", (int(w), int(h)), color)


def cls_img(c):
    """由调色板名或元组生成纯色图。"""
    return c if isinstance(c, tuple) else C(c)


def px(img, x, y, color):
    x, y = int(x), int(y)
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), color if isinstance(color, tuple) else C(color))


def fill_rect(img, x, y, w, h, color):
    c = color if isinstance(color, tuple) else C(color)
    img.paste(c, (int(x), int(y), int(x + w), int(y + h)))


def px_rect(img, x, y, w, h, color):
    for j in range(int(y), int(y + h)):
        for i in range(int(x), int(x + w)):
            px(img, i, j, color)


def px_line(img, x0, y0, x1, y1, color, width=1):
    d = ImageDraw.Draw(img)
    d.line([(x0, y0), (x1, y1)], fill=(color if isinstance(color, tuple) else C(color)),
           width=int(width))


def px_ellipse(img, cx, cy, rx, ry, color):
    c = color if isinstance(color, tuple) else C(color)
    d = ImageDraw.Draw(img)
    d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=c)


def px_poly(img, pts, color):
    c = color if isinstance(color, tuple) else C(color)
    ImageDraw.Draw(img).polygon([(float(x), float(y)) for x, y in pts], fill=c)


def disc(img, cx, cy, r, color):
    d = ImageDraw.Draw(img)
    c = color if isinstance(color, tuple) else C(color)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=c)


def outline(img, color, alpha=255, keep_alpha=True):
    """给非透明像素描一圈边（像素风格描边）。"""
    c = color if isinstance(color, tuple) else C(color)
    c = (c[0], c[1], c[2], alpha)
    alpha_ch = img.getchannel("A")
    grown = alpha_ch.filter(ImageFilter.MaxFilter(3))
    border = ImageChops.subtract(grown, alpha_ch)
    layer = Image.new("RGBA", img.size, c)
    layer.putalpha(border)
    return Image.alpha_composite(layer, img)


def soft_shadow(w, h, rx=0.5, ry=0.5, alpha=110):
    """生成柔边椭圆阴影（用于角色脚下的接地阴影）。"""
    img = new_img(w, h)
    d = ImageDraw.Draw(img)
    d.ellipse([1, 1, w - 2, h - 2], fill=(8, 10, 14, alpha))
    return img.filter(ImageFilter.GaussianBlur(radius=max(0.6, min(w, h) * 0.12)))


def noise_img(w, h, cell=8, seed=0, smooth=True):
    """基于小图放大的值噪声（L 通道）。"""
    rnd = random.Random(seed)
    gw, gh = max(2, int(w // cell) + 2), max(2, int(h // cell) + 2)
    small = Image.new("L", (gw, gh))
    small.putdata([rnd.randint(0, 255) for _ in range(gw * gh)])
    return small.resize((int(w), int(h)),
                        Image.BICUBIC if smooth else Image.NEAREST)


def tint_noise(img, amount=0.22, cell=8, seed=1, smooth=True):
    """在图上叠加明暗噪声，得到手绘质感。"""
    nl = noise_img(img.width, img.height, cell, seed, smooth)
    n = Image.merge("RGBA", (nl, nl, nl, Image.new("L", img.size, 255)))
    # 以 128 为中性色做乘算
    return Image.blend(img, ImageChops.multiply(img, n), amount)


def hline_gradient(img, y0, y1, top, bot):
    """竖直渐变填充。"""
    r0, g0, b0 = (top if isinstance(top, tuple) else PAL[top])[:3]
    r1, g1, b1 = (bot if isinstance(bot, tuple) else PAL[bot])[:3]
    h = max(1, int(y1 - y0))
    d = ImageDraw.Draw(img)
    for i in range(h):
        t = i / max(1, h - 1)
        d.line([(0, y0 + i), (img.width, y0 + i)],
               fill=(int(r0 + (r1 - r0) * t), int(g0 + (g1 - g0) * t),
                     int(b0 + (b1 - b0) * t), 255))


def dither_blend(a, b, t, seed=0):
    """两张同尺寸图做抖动混合（像素风过渡）。"""
    n = noise_img(a.width, a.height, 2, seed, False).convert("L")
    mask = n.point(lambda v: 255 if v < int(t * 255) else 0)
    out = a.copy()
    out.paste(b, (0, 0), mask)
    return out
# ---------------------------------------------------------------------------
# 肢体 / 人形画笔
# ---------------------------------------------------------------------------
def thick_line(img, x0, y0, x1, y1, width, color):
    """画一条有宽度的线段（旋转矩形），用于四肢与刀剑。"""
    c = color if isinstance(color, tuple) else C(color)
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy)
    if ln < 1e-6:
        px_rect(img, x0, y0, max(1, width), max(1, width), c)
        return
    ux, uy = dx / ln, dy / ln
    px_, py_ = -uy, ux
    hw = width * 0.5
    pts = [(x0 + px_ * hw, y0 + py_ * hw), (x1 + px_ * hw, y1 + py_ * hw),
           (x1 - px_ * hw, y1 - py_ * hw), (x0 - px_ * hw, y0 - py_ * hw)]
    px_poly(img, pts, c)


def limb(img, ox, oy, ang_deg, length, width, color):
    """从 (ox,oy) 以 ang 度（0=向右，-90=向上）画肢体，返回末端坐标。"""
    a = math.radians(ang_deg)
    ex, ey = ox + math.cos(a) * length, oy + math.sin(a) * length
    thick_line(img, ox, oy, ex, ey, width, color)
    return ex, ey


def _proportions(h):
    """按总高推导各部位尺寸（脚底在 h-1）。"""
    head_r = max(4, int(h * 0.105))
    head_cy = int(h * 0.145) + head_r // 2
    torso_top = head_cy + head_r - 1
    hip_y = int(h * 0.60)
    return head_r, head_cy, torso_top, hip_y


def paint_humanoid(h, pose, look):
    """通用人形绘制。

    look 字段：robe / robe_d / robe_l / skin / hair / trim / style / hat / cape /
              weapon(sword|blade|staff|None) / scale
    pose 字段：dir / bob / leg_swing / arm_swing / lean / sword_ang / crouch / hurt
    """
    w = max(18, int(h * 0.78))
    img = new_img(w, h)
    cx = w // 2
    head_r, head_cy, torso_top, hip_y = _proportions(h)
    bob = pose.get("bob", 0)
    lean = pose.get("lean", 0.0)
    crouch = pose.get("crouch", 0)
    d = pose.get("dir", "d")
    style = look.get("style", "martial")
    weapon = look.get("weapon", "sword")
    flip = (d == "l")
    dd = "r" if d == "l" else d

    head_cy += bob + crouch
    torso_top += bob + crouch
    hip_y += crouch
    feet = h - 1
    lean_px = int(lean * 2)

    robe, robe_d, robe_l = look.get("robe"), look.get("robe_d"), look.get("robe_l")
    skin, skin_d = look.get("skin", "skin"), look.get("skin_d", "skin_d")
    hair, trim = look.get("hair", "hair"), look.get("trim", "gold")

    leg_w = max(3, int(h * 0.075))
    torso_w = max(8, int(w * 0.62))
    swing = pose.get("leg_swing", 0.0)
    aswing = pose.get("arm_swing", 0.0)
    sw_ang = pose.get("sword_ang", None)
    leg_len = feet - hip_y

    # ---- 后侧肢体 ----
    if dd == "d":
        limb(img, cx - torso_w * 0.22, hip_y, 90 + swing * 16, leg_len, leg_w,
             shade(robe_d, 0.85))
        limb(img, cx + torso_w * 0.22, hip_y, 90 - swing * 16, leg_len, leg_w,
             shade(robe_d, 0.7))
    elif dd == "u":
        limb(img, cx - torso_w * 0.22, hip_y, 90 - swing * 14, leg_len, leg_w, robe_d)
        limb(img, cx + torso_w * 0.22, hip_y, 90 + swing * 14, leg_len, leg_w,
             shade(robe_d, 0.75))
    else:  # 侧面
        limb(img, cx - 1, hip_y, 90 - 18 * swing, leg_len, leg_w + 1, shade(robe_d, 0.8))
        limb(img, cx + 1, hip_y, 90 + 18 * swing, leg_len, leg_w + 1, shade(robe_d, 0.65))

    # ---- 躯干（长袍 / 短打）----
    top = torso_top
    bot = hip_y + (2 if style == "elder" else 6)
    tw_top = int(torso_w * 0.78)
    px_poly(img, [(cx - tw_top // 2 + lean_px, top), (cx + tw_top // 2 + lean_px, top),
                  (cx + torso_w // 2, bot), (cx - torso_w // 2, bot)], C(robe))
    # 侧面收窄
    if dd in ("l", "r"):
        px_poly(img, [(cx - tw_top // 2 + lean_px, top), (cx + tw_top // 2 + lean_px, top),
                      (cx + torso_w // 2 - 2, bot), (cx - torso_w // 2 + 2, bot)], C(robe_d))
        px_poly(img, [(cx - tw_top // 2 + lean_px, top), (cx - tw_top // 2 + 3 + lean_px, top),
                      (cx - torso_w // 2 + 3, bot), (cx - torso_w // 2 + 1, bot)], C(robe_l))
    # 交领 / 腰带
    px_line(img, cx - tw_top // 2 + 1 + lean_px, top + 2, cx + lean_px, top + int((bot - top) * 0.45),
            C(trim), 1)
    px_line(img, cx + tw_top // 2 - 1 + lean_px, top + 2, cx + lean_px, top + int((bot - top) * 0.45),
            C(trim), 1)
    belt_y = top + int((bot - top) * 0.55)
    px_rect(img, cx - torso_w // 2, belt_y, torso_w, max(2, int(h * 0.05)), C(robe_d))
    px_rect(img, cx - torso_w // 2 + 1, belt_y, torso_w - 2, 1, C(trim))
    if style == "elder":
        px_poly(img, [(cx - 3 + lean_px, top + 2), (cx + 3 + lean_px, top + 2),
                      (cx + 1, bot + 2), (cx - 1, bot + 2)], C("white"))
    if pose.get("cape"):
        px_poly(img, [(cx - tw_top // 2 - 1, top + 1), (cx + tw_top // 2 + 1, top + 1),
                      (cx + torso_w, bot + 2), (cx - torso_w, bot + 2)], shade(robe_d, 0.7, 220))

    # ---- 前侧手臂 ----
    sh_y = top + max(2, int((bot - top) * 0.18))
    if dd == "d":
        limb(img, cx - torso_w * 0.5, sh_y, 96 + aswing * 22, int(leg_len * 0.78),
             max(3, leg_w - 1), C(robe))
        fh = limb(img, cx + torso_w * 0.5, sh_y, 84 - aswing * 22, int(leg_len * 0.78),
                  max(3, leg_w - 1), C(robe_l))
    elif dd == "u":
        limb(img, cx - torso_w * 0.5, sh_y, 96 - aswing * 18, int(leg_len * 0.78),
             max(3, leg_w - 1), shade(robe, 0.85))
        fh = limb(img, cx + torso_w * 0.5, sh_y, 84 + aswing * 18, int(leg_len * 0.78),
                  max(3, leg_w - 1), shade(robe, 0.8))
    else:
        limb(img, cx - 2, sh_y, 100 - aswing * 26, int(leg_len * 0.8),
             max(3, leg_w - 1), shade(robe_d, 0.9))
        fh = limb(img, cx + 2, sh_y, 80 + aswing * 26, int(leg_len * 0.8),
                  max(3, leg_w - 1), C(robe_l))

    # ---- 头部 ----
    px_ellipse(img, cx + lean_px, head_cy, head_r, head_r, C(skin))
    # 头发（帽 / 发髻 / 头巾）
    if look.get("hat"):
        px_rect(img, cx - head_r - 1 + lean_px, head_cy - head_r, head_r * 2 + 2, 3, C(trim))
        px_ellipse(img, cx + lean_px, head_cy - head_r, head_r + 1, 2, shade("wood_d", 1.1))
    else:
        px_ellipse(img, cx + lean_px, head_cy - 1, head_r, head_r, C(hair))
        px_rect(img, cx - head_r + lean_px, head_cy - 1, head_r * 2, head_r, C(hair))
        if dd != "u":
            px_ellipse(img, cx + lean_px, head_cy - 1, head_r - 2, head_r - 3, C(skin))
    # 发髻
    disc(img, cx + lean_px, head_cy - head_r - 1, max(1, head_r // 3), C(hair))
    px_rect(img, cx - 1 + lean_px, head_cy - head_r - 3, 3, 3, C(hair))
    # 头带
    if look.get("headband"):
        px_rect(img, cx - head_r + lean_px, head_cy - 2, head_r * 2, 2, C(look["headband"]))
    # 五官
    if dd == "d":
        px(img, cx - 2 + lean_px, head_cy, C("ink"))
        px(img, cx + 2 + lean_px, head_cy, C("ink"))
        if style == "elder":
            px_rect(img, cx - 2 + lean_px, head_cy + 2, 5, 2, C("white"))
    elif dd == "r":
        px(img, cx + head_r - 3 + lean_px, head_cy - 1, C("ink"))
        px(img, cx + head_r - 1 + lean_px, head_cy + 1, C("skin_d"))
    # 表情（受击）
    if pose.get("hurt") and dd == "d":
        px_line(img, cx - 3 + lean_px, head_cy - 2, cx - 1 + lean_px, head_cy - 1, C("ink"), 1)
        px_line(img, cx + 1 + lean_px, head_cy - 1, cx + 3 + lean_px, head_cy - 2, C("ink"), 1)

    # ---- 武器 ----
    if weapon and sw_ang is not None:
        blade_len = {52: 20, 46: 17, 84: 30}.get(h, max(14, int(h * 0.4)))
        blade_len = int(blade_len * look.get("scale", 1.0))
        bx, by = (fh if dd != "u" else (cx, sh_y))
        hx = bx + math.cos(math.radians(sw_ang)) * 3
        hy = by + math.sin(math.radians(sw_ang)) * 3
        ex = hx + math.cos(math.radians(sw_ang)) * blade_len
        ey = hy + math.sin(math.radians(sw_ang)) * blade_len
        thick_line(img, hx, hy, ex, ey, 3, C("steel"))
        thick_line(img, hx, hy, ex, ey, 1, C("steel_l"))
        px_rect(img, hx - 2, hy - 2, 4, 4, C("gold_d"))
    elif weapon == "sword" and dd == "u":
        # 背后剑柄
        px_line(img, cx - 6, torso_top + 2, cx + 8, torso_top - 6, C("steel"), 2)
        px_rect(img, cx + 6, torso_top - 8, 3, 4, C("gold_d"))

    if flip:
        img = img.transpose(Image.FLIP_LEFT_RIGHT)
    return img


# ---------------------------------------------------------------------------
# 姿态表：idle / walk / attack / dodge / cast / hurt / die
# ---------------------------------------------------------------------------
def pose_for(d, action, i, n):
    ph = i / max(1, n)
    p = {"dir": d, "bob": 0, "leg_swing": 0.0, "arm_swing": 0.0, "lean": 0.0,
         "sword_ang": None, "crouch": 0, "hurt": False}
    side = d in ("l", "r")
    if action == "idle":
        p["bob"] = 0 if i % 2 == 0 else -1
        p["arm_swing"] = 0.05 if i % 2 == 0 else -0.05
        p["sword_ang"] = 118 if side else None
        if not side:
            p["sword_ang"] = 70 if d == "d" else None
    elif action == "walk":
        s = math.sin(ph * math.tau)
        p["leg_swing"] = s
        p["arm_swing"] = -s
        p["bob"] = -1 if abs(math.sin(ph * math.tau)) > 0.6 else 0
        p["sword_ang"] = 108 + s * 8 if side else (66 if d == "d" else None)
    elif action == "attack":
        t = i / max(1, n - 1)
        ang = -120 + 190 * t            # 上撩  下劈
        p["sword_ang"] = ang
        p["lean"] = 0.6 * math.sin(math.pi * t)
        p["arm_swing"] = 0.6 * math.sin(math.pi * t)
        p["leg_swing"] = 0.5 if 0.2 < t < 0.8 else 0.0
        p["bob"] = -1 if t > 0.25 else 0
    elif action == "dodge":
        t = i / max(1, n - 1)
        p["crouch"] = 4 - int(t * 3)
        p["lean"] = 0.9 * (1 - t)
        p["leg_swing"] = 0.9
        p["arm_swing"] = -0.8
        p["sword_ang"] = 150
    elif action == "cast":
        t = i / max(1, n - 1)
        p["arm_swing"] = -0.9
        p["sword_ang"] = -80 - 30 * math.sin(math.pi * t) if side else None
        p["bob"] = -1 if 0.3 < t < 0.9 else 0
        p["lean"] = 0.3
    elif action == "hurt":
        p["hurt"] = True
        p["lean"] = -0.9
        p["arm_swing"] = -0.6
        p["leg_swing"] = -0.5
        p["sword_ang"] = 160
    elif action == "die":
        t = i / max(1, n - 1)
        p["crouch"] = int(2 + 6 * t)
        p["lean"] = 1.2 * t
        p["leg_swing"] = -0.8
        p["hurt"] = True
    return p


HERO_LOOK = dict(robe="robe", robe_d="robe_d", robe_l="robe_l", skin="skin",
                 skin_d="skin_d", hair="hair", trim="gold", style="martial",
                 headband="robe_d", weapon="sword")
BANDIT_LOOK = dict(robe="cloth_grey", robe_d="ink2", robe_l="stone", skin="skin_d",
                   skin_d="skin_s", hair="ink2", trim="leather", style="martial",
                   headband="robe", weapon="blade")
GUARD_LOOK = dict(robe="cloth_blue", robe_d="cloth_blue_d", robe_l="stone_l",
                  skin="skin", skin_d="skin_d", hair="hair", trim="steel_d",
                  style="martial", weapon="blade")
ELDER_LOOK = dict(robe="cloth_grey", robe_d="ink3", robe_l="stone", skin="skin_d",
                  skin_d="skin_s", hair="white", trim="gold_d", style="elder",
                  weapon="staff", hat=True)
VILLAGER_LOOK = dict(robe="leather", robe_d="leather_d", robe_l="wall_d", skin="skin",
                     skin_d="skin_d", hair="hair", trim="wood_d", style="villager",
                     hat=True, weapon=None)
SMITH_LOOK = dict(robe="leather", robe_d="leather_d", robe_l="wall_d", skin="skin_d",
                  skin_d="skin_s", hair="ink2", trim="steel_d", style="villager",
                  weapon=None)
# ---------------------------------------------------------------------------
# 四足（狼）画笔
# ---------------------------------------------------------------------------
def paint_wolf(h, pose, fur="cloth_grey", fur_d="ink3", fur_l="stone_l"):
    w = int(h * 1.5)
    img = new_img(w, h)
    bob = pose.get("bob", 0)
    swing = pose.get("leg_swing", 0.0)
    lunge = pose.get("lunge", 0.0)
    body_y = int(h * 0.42) + bob + int(lunge * 3)
    body_h = max(6, int(h * 0.30))
    x0, x1 = int(w * 0.14), int(w * 0.80)
    limb(img, x0 + 2, body_y, 165 - 12 * swing, int(h * 0.34), max(3, int(h * 0.11)), C(fur_d))
    px_ellipse(img, (x0 + x1) // 2, body_y + body_h // 2, (x1 - x0) // 2, body_h // 2, C(fur))
    px_ellipse(img, (x0 + x1) // 2 - 2, body_y + body_h // 2 - 1, (x1 - x0) // 2 - 2,
               body_h // 2 - 2, C(fur_l))
    leg_len = h - (body_y + body_h) - 1
    for k, (lx, ph) in enumerate([(x0 + 4, swing), (x0 + 8, -swing),
                                  (x1 - 8, -swing), (x1 - 3, swing)]):
        ang = 90 + ph * 22
        limb(img, lx, body_y + body_h - 1, ang, leg_len, max(3, int(h * 0.10)),
             C(fur_d) if k % 2 == 0 else C(fur))
    hx = x1 + int(w * 0.10)
    hy = body_y - int(h * 0.05)
    px_ellipse(img, hx, hy, int(h * 0.20), int(h * 0.17), C(fur))
    px_poly(img, [(hx + int(h * 0.12), hy - 2), (hx + int(h * 0.30), hy + 2),
                  (hx + int(h * 0.12), hy + 5)], C(fur_d))
    px_poly(img, [(hx - 4, hy - int(h * 0.16)), (hx - 1, hy - int(h * 0.30)),
                  (hx + 2, hy - int(h * 0.14))], C(fur_d))
    px_poly(img, [(hx + 2, hy - int(h * 0.15)), (hx + 6, hy - int(h * 0.28)),
                  (hx + 8, hy - int(h * 0.12))], C(fur_d))
    if lunge > 0.4:
        px(img, hx + 1, hy - 1, C("fire"))
        px_poly(img, [(hx + int(h * 0.16), hy + 2), (hx + int(h * 0.34), hy + 6),
                      (hx + int(h * 0.14), hy + 7)], C("robe_d"))
        px_line(img, hx + 4, hy + 3, hx + int(h * 0.30), hy + 5, C("white"), 1)
    else:
        px(img, hx + 1, hy - 1, C("fire"))
        px(img, hx + int(h * 0.26), hy + 3, C("ink"))
    return img


# ---------------------------------------------------------------------------
# 幽魂画笔
# ---------------------------------------------------------------------------
def paint_ghost(h, pose):
    w = int(h * 0.85)
    img = new_img(w, h)
    cx = w // 2
    bob = pose.get("bob", 0)
    top = int(h * 0.16) + bob
    body_bot = int(h * 0.80) + bob
    for k in range(4):
        y = body_bot + k * 3
        off = int(math.sin(k * 1.3 + pose.get("phase", 0)) * 3)
        px_poly(img, [(cx - 9 + k, y), (cx + 9 - k, y),
                      (cx + 7 - k + off, y + 4), (cx - 7 + k + off, y + 4)],
                (PAL["ghost_d"][0], PAL["ghost_d"][1], PAL["ghost_d"][2], max(60, 190 - k * 40)))
    px_poly(img, [(cx - 5, top), (cx + 5, top), (cx + 11, body_bot),
                  (cx - 11, body_bot)], (PAL["ghost"][0], PAL["ghost"][1], PAL["ghost"][2], 210))
    px_poly(img, [(cx - 5, top), (cx + 2, top), (cx + 5, body_bot),
                  (cx - 4, body_bot)], (PAL["ghost_glow"][0], PAL["ghost_glow"][1],
                                       PAL["ghost_glow"][2], 120))
    px_ellipse(img, cx, top + 3, 8, 7, (PAL["ghost_d"][0], PAL["ghost_d"][1],
                                        PAL["ghost_d"][2], 235))
    px_ellipse(img, cx, top + 4, 5, 4, (10, 14, 20, 235))
    px(img, cx - 2, top + 4, C("ghost_glow"))
    px(img, cx + 2, top + 4, C("ghost_glow"))
    atk = pose.get("attack", 0.0)
    for sgn in (-1, 1):
        hx = cx + sgn * int(10 + atk * 6)
        hy = top + int(12 - atk * 8)
        ang = math.degrees(math.atan2(hy - (top + 10), hx - (cx + sgn * 6)))
        limb(img, cx + sgn * 6, top + 10, ang, 9 + atk * 3, 3,
             (PAL["ghost_glow"][0], PAL["ghost_glow"][1], PAL["ghost_glow"][2], 200))
    return img


# ---------------------------------------------------------------------------
# 特效
# ---------------------------------------------------------------------------
def fx_slash(n=5, size=56):
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        img = new_img(size, size)
        cx = cy = size / 2
        r = size * (0.28 + 0.16 * t)
        a0 = -120 + 150 * t
        a1 = a0 + 110 - 60 * t
        alpha = int(230 * (1.0 - 0.55 * t))
        for k in range(24):
            ang = math.radians(a0 + (a1 - a0) * k / 23.0)
            x = cx + math.cos(ang) * r
            y = cy + math.sin(ang) * r
            w = max(1, int(5 * (1.0 - t) + 1))
            thick_line(img, x - math.cos(ang) * w, y - math.sin(ang) * w, x, y, w,
                       (236, 244, 255, alpha))
        for k in range(16):
            ang = math.radians(a0 + (a1 - a0) * k / 15.0)
            r2 = r * 0.86
            x = cx + math.cos(ang) * r2
            y = cy + math.sin(ang) * r2
            thick_line(img, x, y, x + math.cos(ang) * 4, y + math.sin(ang) * 4, 2,
                       (150, 220, 255, int(alpha * 0.8)))
        out.append(img.filter(ImageFilter.GaussianBlur(0.4)))
    return out


def fx_hit(n=4, size=40):
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        img = new_img(size, size)
        cx = cy = size / 2
        for k in range(10):
            ang = k * math.tau / 10 + 0.2
            ln = size * (0.24 + 0.22 * (1 - t)) * (1.0 if k % 2 == 0 else 0.62)
            x1, y1 = cx + math.cos(ang) * ln, cy + math.sin(ang) * ln
            col = (255, 236, 170, int(240 * (1 - t))) if k % 2 == 0 else (255, 150, 60, int(200 * (1 - t)))
            thick_line(img, cx, cy, x1, y1, max(1, int(4 * (1 - t) + 1)), col)
        disc(img, cx, cy, max(1, int(5 * (1 - t))), (255, 255, 236, int(250 * (1 - t))))
        out.append(img)
    return out


def fx_dust(n=4, size=28):
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        img = new_img(size, size)
        for ox, oy, rr in [(-4, 0, 4), (3, -1, 3.4), (0, -3, 2.6)]:
            rr2 = rr * (0.6 + 1.1 * t)
            disc(img, size / 2 + ox * (0.6 + t), size * 0.66 + oy - 4 * t, rr2,
                 (196, 190, 176, int(150 * (1 - t))))
        out.append(img.filter(ImageFilter.GaussianBlur(0.5)))
    return out


def fx_qi(n=5, size=72, col="ghost"):
    base = PAL[col]
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        img = new_img(size, size)
        r = size * (0.18 + 0.34 * t)
        a = int(210 * (1 - t))
        for k in range(64):
            ang = k * math.tau / 64
            x = size / 2 + math.cos(ang) * r
            y = size / 2 + math.sin(ang) * r * 0.72
            disc(img, x, y, 1.6 + 1.4 * (1 - t), (base[0], base[1], base[2], a))
        out.append(img.filter(ImageFilter.GaussianBlur(0.6)))
    return out


def fx_heal(n=5, size=36):
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        img = new_img(size, size)
        rnd = random.Random(1000 + i)
        for _ in range(7):
            x = size / 2 + rnd.uniform(-8, 8)
            y = size - 2 - (t * size * 0.8) - rnd.uniform(0, 10)
            a = int(230 * (1 - t) * rnd.uniform(0.6, 1.0))
            disc(img, x, y, rnd.uniform(1.2, 2.2), (150, 236, 150, a))
            px(img, x, y - 2, (220, 255, 200, a))
        out.append(img)
    return out


def fx_levelup(n=6, size=(72, 108)):
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        img = new_img(*size)
        w, h = size
        a = int(220 * (1 - t))
        px_poly(img, [(w // 2 - int(10 * (1 - t)), h), (w // 2 + int(10 * (1 - t)), h),
                      (w // 2 + int(4 * (1 - t)), 0), (w // 2 - int(4 * (1 - t)), 0)],
                (255, 236, 170, a))
        for k in range(48):
            ang = k * math.tau / 48
            r = w * (0.22 + 0.28 * t)
            x = w / 2 + math.cos(ang) * r
            y = h * 0.72 + math.sin(ang) * r * 0.3
            disc(img, x, y, 1.5, (255, 246, 200, int(a * 0.9)))
        out.append(img.filter(ImageFilter.GaussianBlur(0.7)))
    return out


def fx_leaf(n=3, size=12):
    out = []
    for i in range(n):
        img = new_img(size, size)
        px_poly(img, [(2, 6), (6, 1 + i), (10, 6 - i), (6, 10)], (206, 168, 74, 235))
        px_line(img, 2, 6, 10, 6 - i, (150, 116, 44, 235), 1)
        out.append(img)
    return out


def fx_glow(size=160, color=(255, 186, 94), power=2.1):
    img = new_img(size, size)
    cx = cy = size / 2
    px_ = img.load()
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - cx, y - cy) / (size / 2)
            if d >= 1.0:
                continue
            a = int(232 * ((1.0 - d) ** power))
            px_[x, y] = (color[0], color[1], color[2], a)
    return img.filter(ImageFilter.GaussianBlur(size * 0.06))


def fx_firefly(size=14):
    img = new_img(size, size)
    disc(img, size / 2, size / 2, size * 0.22, (255, 250, 210, 255))
    return Image.alpha_composite(fx_glow(size, (255, 226, 150), 1.6), img)


def fx_shadow(w=52, h=18):
    return soft_shadow(w, h)