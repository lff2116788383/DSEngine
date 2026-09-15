#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""HD-2D 武侠模板：一键生成全部素材。

用法（仓库根目录下）：
    wsl -d Ubuntu2204 -- python3 /mnt/e/Engine/DSEngine/templates/hd2d_wuxia/tools/generate_assets.py
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from gen_audio import generate_audio
from gen_font import generate_fonts
from gen_lib import (BANDIT_LOOK, C, ELDER_LOOK, HERO_LOOK, PAL, SMITH_LOOK,
                     VILLAGER_LOOK, disc, fx_dust, fx_firefly, fx_glow, fx_heal,
                     fx_hit, fx_leaf, fx_levelup, fx_qi, fx_shadow, fx_slash,
                     new_img, paint_humanoid, paint_wolf, paint_ghost, pose_for,
                     px, px_ellipse, px_line, px_poly, px_rect, shade, thick_line)
from gen_maps import generate_maps

random.seed(20260915)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 角色动画帧数表
ANIM = {"idle": 4, "walk": 6, "attack": 5, "dodge": 4, "cast": 5, "hurt": 2, "die": 5}
NPC_ANIM = {"idle": 3, "walk": 0, "attack": 0, "dodge": 0, "cast": 0, "hurt": 0, "die": 0}

BOSS_LOOK = dict(HERO_LOOK, robe="robe_d", robe_d="ink2", robe_l="robe", trim="gold",
                 headband="gold_d", cape=True, weapon="blade")

CHARS = [
    ("char/hero", HERO_LOOK, 52, ANIM, "dulr", None),
    ("enemy/bandit", BANDIT_LOOK, 46, {"idle": 3, "walk": 4, "attack": 3, "hurt": 1, "die": 4}, "dulr", None),
    ("enemy/boss", BOSS_LOOK, 84, {"idle": 4, "walk": 6, "attack": 4, "hurt": 1, "die": 6}, "dulr", {"cape": True}),
    ("npc/villager", VILLAGER_LOOK, 46, NPC_ANIM, "dur", None),
    ("npc/smith", SMITH_LOOK, 46, NPC_ANIM, "dur", None),
    ("npc/elder", ELDER_LOOK, 48, NPC_ANIM, "dur", None),
]


def gen_chars(out_root):
    base = os.path.join(out_root, "assets")
    total = 0
    for (sub, look, h, anims, dirs, extra) in CHARS:
        d = os.path.join(base, sub)
        os.makedirs(d, exist_ok=True)
        for dr in dirs:
            for action, n in anims.items():
                if n <= 0:
                    continue
                for i in range(n):
                    pose = pose_for(dr, action, i, n)
                    if extra:
                        pose.update(extra)
                    img = paint_humanoid(h, pose, look)
                    img.save(os.path.join(d, "%s_%s_%s_%d.png" % (os.path.basename(sub), dr, action, i)))
                    total += 1
    # 狼 / 幽魂（侧视，左右各一套）
    for kind, painter, h, acts in (("wolf", paint_wolf, 34, {"idle": 3, "walk": 6, "attack": 3, "hurt": 1, "die": 4}),
                                   ("ghost", paint_ghost, 46, {"idle": 4, "walk": 6, "attack": 4, "hurt": 1, "die": 4})):
        d = os.path.join(base, "enemy")
        os.makedirs(d, exist_ok=True)
        for dr in ("r", "l"):
            for action, n in acts.items():
                for i in range(n):
                    pose = {"dir": dr, "bob": 0 if i % 2 else -1, "phase": i * 1.1}
                    if action == "walk":
                        pose["leg_swing"] = math.sin(i / n * math.tau)
                    if action == "attack":
                        pose["lunge"] = 0.9 if i >= n // 2 else 0.2
                        pose["attack"] = 1.0 if i >= n // 2 else 0.3
                    if action == "hurt":
                        pose["lunge"] = -0.4
                    if action == "die":
                        pose["lunge"] = 1.0
                        pose["phase"] = 6.0
                    img = painter(h, pose) if kind == "ghost" else painter(h, pose)
                    img.save(os.path.join(d, "%s_%s_%s_%d.png" % (kind, dr, action, i)))
                    total += 1
    print("[chars] %d frames" % total)


def gen_fx(out_root):
    d = os.path.join(out_root, "assets", "fx")
    os.makedirs(d, exist_ok=True)
    for i, im in enumerate(fx_slash(5)):
        im.save(os.path.join(d, "slash_%d.png" % i))
    for i, im in enumerate(fx_hit(4)):
        im.save(os.path.join(d, "hit_%d.png" % i))
    for i, im in enumerate(fx_dust(4)):
        im.save(os.path.join(d, "dust_%d.png" % i))
    for i, im in enumerate(fx_qi(5)):
        im.save(os.path.join(d, "qi_%d.png" % i))
    for i, im in enumerate(fx_heal(5)):
        im.save(os.path.join(d, "heal_%d.png" % i))
    for i, im in enumerate(fx_levelup(6)):
        im.save(os.path.join(d, "levelup_%d.png" % i))
    for i, im in enumerate(fx_leaf(3)):
        im.save(os.path.join(d, "leaf_%d.png" % i))
    fx_firefly(16).save(os.path.join(d, "firefly.png"))
    fx_glow(192, (255, 186, 94), 2.1).save(os.path.join(d, "glow_warm.png"))
    fx_glow(192, (140, 226, 226), 2.1).save(os.path.join(d, "glow_cool.png"))
    fx_glow(192, (255, 120, 80), 2.0).save(os.path.join(d, "glow_torch.png"))
    fx_shadow(44, 15).save(os.path.join(d, "shadow_small.png"))
    fx_shadow(64, 20).save(os.path.join(d, "shadow_mid.png"))
    fx_shadow(104, 30).save(os.path.join(d, "shadow_big.png"))
    print("[fx] ok")


# ---------------------------------------------------------------------------
# UI
# ---------------------------------------------------------------------------
def _ink_panel(w, h, fill=(16, 16, 24, 216), border="gold", inner="gold_d", r=10):
    img = new_img(w, h)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=r, fill=fill,
                        outline=C(border), width=2)
    d.rounded_rectangle([3, 3, w - 4, h - 4], radius=max(2, r - 3),
                        outline=shade(inner, 1.0, 120), width=1)
    for (cx, cy) in ((6, 6), (w - 7, 6), (6, h - 7), (w - 7, h - 7)):
        disc(img, cx, cy, 2, C(border))
    return img


def _bar(w, h, c0, c1):
    img = new_img(w, h)
    for x in range(w):
        t = x / max(1, w - 1)
        col = tuple(int(c0[i] + (c1[i] - c0[i]) * t) for i in range(3)) + (255,)
        px_rect(img, x, 0, 1, h, col)
    px_rect(img, 0, 0, w, 1, (255, 255, 255, 90))
    px_rect(img, 0, h - 1, w, 1, (0, 0, 0, 120))
    return img


def gen_ui(out_root):
    d = os.path.join(out_root, "assets", "ui")
    os.makedirs(d, exist_ok=True)
    _ink_panel(768, 176, r=14).save(os.path.join(d, "panel_dialog.png"))
    _ink_panel(576, 428, r=12).save(os.path.join(d, "panel_menu.png"))
    _ink_panel(320, 84, r=8).save(os.path.join(d, "panel_hud.png"))
    _ink_panel(184, 40, r=6).save(os.path.join(d, "name_plate.png"))
    _ink_panel(204, 152, r=8, fill=(12, 14, 20, 200)).save(os.path.join(d, "minimap_frame.png"))
    _ink_panel(168, 44, r=6, fill=(26, 24, 34, 230)).save(os.path.join(d, "button.png"))
    _ink_panel(560, 140, r=12, fill=(14, 14, 22, 200)).save(os.path.join(d, "title_plate.png"))
    # 进度条
    _bar(240, 12, PAL["hp"], (238, 120, 96)).save(os.path.join(d, "bar_hp.png"))
    _bar(240, 12, PAL["mp"], (128, 190, 246)).save(os.path.join(d, "bar_mp.png"))
    _bar(240, 10, PAL["stam"], (196, 230, 140)).save(os.path.join(d, "bar_stam.png"))
    frame = new_img(252, 22)
    ImageDraw.Draw(frame).rounded_rectangle([0, 0, 251, 21], radius=5, outline=C("gold"), width=2)
    frame.save(os.path.join(d, "bar_frame.png"))
    # 技能槽 + 图标
    slot = new_img(56, 56)
    ImageDraw.Draw(slot).rounded_rectangle([0, 0, 55, 55], radius=8, fill=(12, 14, 20, 215),
                                           outline=C("gold_d"), width=2)
    slot.save(os.path.join(d, "skill_slot.png"))
    def icon_sword(w=44, h=44):
        img = new_img(w, h)
        thick_line(img, 8, 36, 34, 10, 4, C("steel"))
        thick_line(img, 8, 36, 34, 10, 2, C("steel_l"))
        thick_line(img, 6, 34, 14, 26, 4, C("gold_d"))
        px_rect(img, 32, 6, 5, 5, C("gold"))
        return img
    def icon_palm(w=44, h=44):
        img = new_img(w, h)
        px_ellipse(img, 22, 26, 11, 12, C("skin_d"))
        for k, ox in enumerate((-8, -3, 2, 7)):
            thick_line(img, 22 + ox, 20, 22 + ox, 6 + abs(k - 1) * 2, 3, C("skin"))
        thick_line(img, 12, 26, 4, 20, 3, C("skin"))
        disc(img, 22, 24, 6, C("ghost_glow")[0:3] + (140,))
        return img
    def icon_step(w=44, h=44):
        img = new_img(w, h)
        for k in range(4):
            a0 = -40 + k * 80
            thick_line(img, 22, 22, 22 + math.cos(math.radians(a0)) * 15,
                       22 + math.sin(math.radians(a0)) * 15, 3,
                       C("ghost")[0:3] + (200 - k * 30,))
        disc(img, 22, 22, 5, C("ghost_glow")[0:3] + (220,))
        return img
    def icon_qi(w=44, h=44):
        img = new_img(w, h)
        for rr, aa in ((16, 90), (12, 130), (7, 190)):
            d2 = ImageDraw.Draw(img)
            d2.ellipse([22 - rr, 22 - rr, 22 + rr, 22 + rr], outline=C("gold")[0:3] + (aa,), width=3)
        disc(img, 22, 22, 5, C("lantern_hot")[0:3] + (240,))
        return img
    icon_sword().save(os.path.join(d, "skill_icon_0.png"))
    icon_palm().save(os.path.join(d, "skill_icon_1.png"))
    icon_step().save(os.path.join(d, "skill_icon_2.png"))
    icon_qi().save(os.path.join(d, "skill_icon_3.png"))
    # 道具图标
    def icon_coin(w=22, h=22):
        img = new_img(w, h)
        disc(img, w // 2, h // 2, w // 2 - 2, C("gold"))
        disc(img, w // 2, h // 2, w // 2 - 5, C("gold_d"))
        px_rect(img, w // 2 - 1, 5, 3, h - 10, C("gold"))
        return img
    def icon_potion(w=26, h=26, col="robe"):
        img = new_img(w, h)
        px_rect(img, 9, 2, 8, 5, C("wood_d"))
        px_poly(img, [(9, 7), (17, 7), (21, 22), (5, 22)], C(col))
        px_rect(img, 8, 12, 10, 2, (255, 255, 255, 70))
        px_rect(img, 10, 15, 6, 4, shade(col, 1.25))
        return img
    icon_coin().save(os.path.join(d, "icon_coin.png"))
    icon_potion(col="robe").save(os.path.join(d, "icon_hp_potion.png"))
    icon_potion(col="cloth_blue").save(os.path.join(d, "icon_mp_potion.png"))
    sw = new_img(26, 26)
    thick_line(sw, 4, 22, 20, 6, 3, C("steel"))
    thick_line(sw, 3, 20, 9, 14, 3, C("gold_d"))
    sw.save(os.path.join(d, "icon_sword.png"))
    ar = new_img(26, 26)
    px_poly(ar, [(13, 2), (23, 7), (21, 20), (13, 24), (5, 20), (3, 7)], C("leather"))
    px_poly(ar, [(13, 6), (19, 9), (18, 18), (13, 20), (8, 18), (7, 9)], C("leather_d"))
    ar.save(os.path.join(d, "icon_armor.png"))
    mn = new_img(26, 26)
    px_rect(mn, 5, 3, 16, 20, C("wall"))
    for k in range(4):
        px_line(mn, 7 + (k % 2) * 2, 6 + k * 5, 19, 6 + k * 5, C("ink3"))
    mn.save(os.path.join(d, "icon_manual.png"))
    # 光标
    cur = new_img(24, 24)
    thick_line(cur, 3, 21, 17, 5, 3, C("steel_l"))
    thick_line(cur, 2, 20, 7, 15, 3, C("gold"))
    cur.save(os.path.join(d, "cursor.png"))
    # 任务标记
    qm = new_img(20, 20)
    px_poly(qm, [(10, 1), (19, 10), (10, 19), (1, 10)], C("gold"))
    px_poly(qm, [(10, 4), (16, 10), (10, 16), (4, 10)], C("ink2"))
    px_rect(qm, 9, 6, 2, 5, C("lantern_hot"))
    px_rect(qm, 9, 12, 2, 2, C("lantern_hot"))
    qm.save(os.path.join(d, "quest_marker.png"))
    # 底部渐变 / 暗角
    grad = new_img(1280, 260)
    for y in range(260):
        a = int(210 * (y / 259.0) ** 1.6)
        px_rect(grad, 0, y, 1280, 1, (6, 8, 14, a))
    grad.save(os.path.join(d, "grad_bottom.png"))
    vig = new_img(1280, 720)
    dv = ImageDraw.Draw(vig)
    for k in range(90):
        a = int(120 * (1 - k / 90.0) ** 2)
        dv.rectangle([k, k, 1279 - k, 719 - k], outline=(4, 6, 12, a))
    vig.filter(ImageFilter.GaussianBlur(22)).save(os.path.join(d, "vignette.png"))
    # 主角头像（半身）
    pt = _ink_panel(96, 96, r=8, fill=(14, 16, 24, 230))
    body = new_img(96, 96)
    look = dict(HERO_LOOK)
    head = paint_humanoid(150, {"dir": "d", "sword_ang": None}, look)
    head = head.resize((int(head.width * 1.05), int(head.height * 1.05)), Image.NEAREST)
    body.alpha_composite(head.crop((max(0, head.width // 2 - 40), 10,
                                    min(head.width, head.width // 2 + 40), 118)), (8, 22))
    pt.alpha_composite(body)
    pt.save(os.path.join(d, "portrait_hero.png"))
    print("[ui] ok")


TEXT_SAMPLES = [
    "青溪问剑", "HD-2D 武侠模板", "按 任意键 开始", "退出", "继续游戏", "新游戏", "读取存档",
    "操作：WASD/方向键 移动　J 攻击　K 闪避　U/I 技能　1/2 用药　E 交互　Esc 暂停",
    "青溪村", "黑风寨", "暮色青溪村", "夜雨黑风寨",
    "生命", "内力", "体力", "等级", "经验", "金钱", "境界", "攻击", "防御", "身法",
    "青溪剑法", "分花拂柳", "踏雪无痕", "紫霞真气", "剑意", "命中", "暴击",
    "金创药", "回气散", "铜钱", "铁剑", "皮甲", "剑谱残页", "寨主令",
    "任务", "支线", "主线", "未完成", "已完成", "目标", "奖励", "获得",
    "老者", "铁匠", "村民", "寨主", "刀客", "野狼", "怨魂",
    "少侠留步，黑风寨的贼人近日又在山口出没，村中人心惶惶。",
    "若要上山，先在此处歇息。老夫这儿有几句要紧的话，你且听好。",
    "剑走轻灵，心要静。你手中这柄铁剑虽钝，却也够斩几个毛贼。",
    "铁料不够了，改日再来。",
    "这把刀是我打的，便宜卖你哎，客官别走啊！",
    "多谢少侠出手相救！那伙贼人抢了我们的粮车，往寨子里去了。",
    "哼，区区一个毛头小子，也敢闯我黑风寨？",
    "既然来了，就把命留下！",
    "少侠好身手山寨的宝藏，都在后堂",
    "击破黑风寨，救出被掳的村民！", "前往黑风寨，击败寨主。",
    "救出村民", "击败寨主", "探索青溪村", "寻访村中长者",
    "升级！", "等级提升", "学会新招：分花拂柳（K 闪避后按 J）",
    "暂停", "返回游戏", "保存进度", "读取进度", "设置", "音量",
    "存档成功", "读档成功", "没有存档", "游戏结束", "按 R 重新开始",
    "通关！", "青溪问剑  完", "感谢游玩 HD-2D 武侠模板",
    "已拾取", "背包已满", "内力不足", "冷却中", "金钱不足",
    "第 1 章　暮色青溪", "第 2 章　夜雨黑风",
    "连击", "闪避", "格挡", "破防", "击杀", "逃脱",
]


def scan_lua_texts(root):
    texts = []
    for dirpath, _dirs, files in os.walk(os.path.join(root, "scripts")):
        for fn in files:
            if fn.endswith(".lua"):
                try:
                    with open(os.path.join(dirpath, fn), "r", encoding="utf-8") as f:
                        texts.append(f.read())
                except (OSError, UnicodeDecodeError):
                    pass
    return texts


def main():
    out = ROOT
    gen_chars(out)
    gen_fx(out)
    gen_ui(out)
    generate_fonts(out, TEXT_SAMPLES + scan_lua_texts(out))
    generate_maps(out)
    generate_audio(out)
    print("[done] assets -> %s" % os.path.join(out, "assets"))


if __name__ == "__main__":
    main()