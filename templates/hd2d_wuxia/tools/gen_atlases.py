#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Build .dsprite atlases for every generated PNG sequence.

This is content-pipeline glue for the HD-2D B+ presenter.  The 2D runtime keeps
using the existing per-frame PNG files; the B+ presenter loads the packed
atlases through dse.assets.load_sprite_atlas so Sprite3D clip animation can run
without changing the battle/AI/quest scripts.
"""
import json
import os
import re
import sys

from PIL import Image

ACTOR_SPECS = {
    "hero": {
        "dirs": ("d", "u", "l", "r"),
        "path": "char/hero",
        "prefix": "hero",
        "actions": ("idle", "walk", "attack", "dodge", "cast", "hurt", "die"),
    },
    "bandit": {
        "dirs": ("d", "u", "l", "r"),
        "path": "enemy/bandit",
        "prefix": "bandit",
        "actions": ("idle", "walk", "attack", "hurt", "die"),
    },
    "boss": {
        "dirs": ("d", "u", "l", "r"),
        "path": "enemy/boss",
        "prefix": "boss",
        "actions": ("idle", "walk", "attack", "hurt", "die"),
    },
    "wolf": {
        "dirs": ("l", "r"),
        "path": "enemy",
        "prefix": "wolf",
        "actions": ("idle", "walk", "attack", "hurt", "die"),
    },
    "ghost": {
        "dirs": ("l", "r"),
        "path": "enemy",
        "prefix": "ghost",
        "actions": ("idle", "walk", "attack", "hurt", "die"),
    },
}

NPC_SPECS = {
    "villager": {"dirs": ("d", "u", "r"), "path": "npc/villager"},
    "smith": {"dirs": ("d", "u", "r"), "path": "npc/smith"},
    "elder": {"dirs": ("d", "u", "r"), "path": "npc/elder"},
}

FX_SEQUENCES = ("slash", "hit", "dust", "qi", "heal", "levelup", "leaf")
FX_SINGLE = ("firefly",)

ACTION_FPS = {
    "idle": 8.0,
    "walk": 10.0,
    "attack": 16.0,
    "dodge": 8.0,
    "cast": 9.0,
    "hurt": 8.0,
    "die": 8.0,
    "play": 20.0,
}
ACTION_LOOP = {
    "idle": True,
    "walk": True,
    "cast": False,
    "dodge": False,
    "attack": False,
    "hurt": False,
    "die": False,
    "play": False,
}

_ATLAS_CACHE = {}


def _open_rgba(path):
    img = Image.open(path)
    return img.convert("RGBA")


def build_atlas(frame_paths, out_png, clip_name, fps=10.0, loop=True, pivot_y=0.0):
    """Pack same-height frames horizontally and emit .dsprite JSON.

    All generated animation frames are anchored at their feet; pixel-art frames
    may differ by a few pixels, so the atlas keeps each frame's real rect and
    the runtime Sprite3D path scales the quad by size_w/size_h as before.
    """
    if not frame_paths:
        return False
    imgs = [_open_rgba(p) for p in frame_paths]
    max_h = max(im.height for im in imgs)
    total_w = sum(im.width for im in imgs)
    atlas = Image.new("RGBA", (total_w, max_h), (0, 0, 0, 0))
    frames = []
    x = 0
    for i, im in enumerate(imgs):
        atlas.paste(im, (x, 0), im)
        u0 = x / float(total_w)
        v0 = 0.0
        u1 = (x + im.width) / float(total_w)
        v1 = im.height / float(max_h)
        frames.append({
            "name": "%s_%d" % (clip_name, i),
            "index": i,
            "pixel_rect": {"x": x, "y": 0, "w": im.width, "h": im.height},
            "uv_rect": {"x": u0, "y": v0, "w": u1 - u0, "h": v1},
            "pivot": {"x": 0.5, "y": pivot_y},
        })
        x += im.width

    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    atlas.save(out_png)
    data = {
        "version": 1,
        "texture": os.path.basename(out_png),
        "width": total_w,
        "height": max_h,
        "frames": frames,
        "clips": {
            clip_name: {
                "frames": list(range(len(frames))),
                "fps": float(fps),
                "loop": bool(loop),
            }
        },
    }
    # Preserve optional normal/emissive maps when a matching sibling exists.
    base = os.path.splitext(out_png)[0]
    normal_path = base + "_normal.png"
    emissive_path = base + "_emissive.png"
    if os.path.exists(normal_path):
        data["normal"] = os.path.basename(normal_path)
    if os.path.exists(emissive_path):
        data["emissive"] = os.path.basename(emissive_path)
    with open(os.path.splitext(out_png)[0] + ".dsprite.json", "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")
    return True


def _actor_files(out_root, spec, d, action):
    root = os.path.join(out_root, "assets", spec["path"])
    prefix = spec["prefix"]
    pat = re.compile(re.escape(prefix) + r"_" + re.escape(d) + r"_" + re.escape(action) + r"_(\d+)\.png$")
    found = []
    for name in os.listdir(root):
        m = pat.match(name)
        if m:
            found.append((int(m.group(1)), os.path.join(root, name)))
    found.sort(key=lambda item: item[0])
    return [p for _, p in found]


def generate_actor_atlases(out_root):
    total = 0
    for key, spec in ACTOR_SPECS.items():
        for d in spec["dirs"]:
            for action in spec["actions"]:
                frames = _actor_files(out_root, spec, d, action)
                if not frames:
                    continue
                root = os.path.join(out_root, "assets", spec["path"])
                out_png = os.path.join(root, "%s_%s_%s_atlas.png" % (spec["prefix"], d, action))
                ok = build_atlas(frames, out_png, action,
                                 ACTION_FPS.get(action, 10.0),
                                 ACTION_LOOP.get(action, True))
                if ok:
                    total += 1
    for key, spec in NPC_SPECS.items():
        for d in spec["dirs"]:
            root = os.path.join(out_root, "assets", spec["path"])
            frames = _actor_files(out_root, {"path": spec["path"], "prefix": key}, d, "idle")
            if not frames:
                continue
            out_png = os.path.join(root, "%s_%s_idle_atlas.png" % (key, d))
            if build_atlas(frames, out_png, "idle", 8.0, True):
                total += 1
    print("[atlas] actor/npc atlases=%d" % total)
    return total


def generate_fx_atlases(out_root):
    root = os.path.join(out_root, "assets", "fx")
    total = 0
    for kind in FX_SEQUENCES:
        frames = []
        i = 0
        while True:
            p = os.path.join(root, "%s_%d.png" % (kind, i))
            if not os.path.exists(p):
                break
            frames.append(p)
            i += 1
        if not frames:
            continue
        out_png = os.path.join(root, "%s_atlas.png" % kind)
        if build_atlas(frames, out_png, kind, ACTION_FPS.get("play", 20.0), False, 0.5):
            total += 1
    for kind in FX_SINGLE:
        p = os.path.join(root, "%s.png" % kind)
        if os.path.exists(p):
            out_png = os.path.join(root, "%s_atlas.png" % kind)
            if build_atlas([p], out_png, kind, 1.0, True, 0.5):
                total += 1
    print("[atlas] fx atlases=%d" % total)
    return total


def generate_all(out_root):
    return generate_actor_atlases(out_root) + generate_fx_atlases(out_root)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_root = os.path.dirname(here)
    if len(sys.argv) > 1:
        out_root = os.path.abspath(sys.argv[1])
    generate_all(out_root)


if __name__ == "__main__":
    main()
