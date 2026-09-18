#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Import selected CC0 Ninja Adventure assets into games/wuxia_arpg/assets/external.

Source asset pack: Ninja Adventure Asset Pack by Pixel-Boy / AAA.
Published as CC0 1.0; see SOURCE.md written by this script.
This script only uses the local unpacked source and Pillow.
"""
from __future__ import annotations
import argparse
import json
import shutil
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "games" / "wuxia_arpg" / "assets" / "external" / "ninja_adventure"
ACTOR_OUT = OUT / "actor"
AUDIO_OUT = OUT / "audio"
CANDIDATES = [
    ROOT / "third_party" / "assets" / "ninja_adventure",
    ROOT / "tmp" / "kenney_packs" / "NinjaAdventure-main" / "NinjaAdventure-main",
]
ACTORS = {
    "hero": "content/character/samurai_blue/sprite.png",
    "bandit": "content/character/samurai_green/samurai_green.png",
    "boss": "content/character/ninja_blue/sprite.png",
}
MUSIC = {
    "theme_plain.ogg": "audio/music/theme_plain.ogg",
    "theme_swamp.ogg": "audio/music/theme_swamp.ogg",
    "theme_lost_village.ogg": "audio/music/theme_lost_village.ogg",
    "theme_dream.ogg": "audio/music/theme_dream.ogg",
}
DIRS = ["d", "u", "l", "r"]  # down, up, left, right (Ninja Adventure column order)
ACTIONS = {
    "idle": ([0], 4.0, True),
    "walk": ([0, 1, 2, 3], 8.0, True),
    "attack": ([4], 12.0, False),
    "dodge": ([1, 2, 3], 10.0, False),
    "hurt": ([6], 4.0, False),
    "die": ([6], 8.0, False),
}
FRAME = 16


def find_source(root: Path | None) -> Path:
    for p in ([root] if root else []) + CANDIDATES:
        if p and (p / "content" / "character" / "samurai_blue" / "sprite.png").exists():
            return p
    raise SystemExit("找不到 Ninja Adventure 解包目录，请通过 --source 指定")


def build_actor_atlas(src: Path, actor: str, rel: str):
    sheet = Image.open(src / rel).convert("RGBA")
    if sheet.width % FRAME or sheet.height % FRAME:
        raise SystemExit(f"{rel} 尺寸不是 {FRAME} 的整数倍: {sheet.size}")
    for dir_idx, d in enumerate(DIRS):
        frames = []
        clips = {}
        for action, (rows, fps, loop) in ACTIONS.items():
            indices = []
            for row in rows:
                frame = sheet.crop((dir_idx * FRAME, row * FRAME,
                                    (dir_idx + 1) * FRAME, (row + 1) * FRAME))
                indices.append(len(frames))
                frames.append((action, len(indices) - 1, frame))
            clips[action] = {"frames": indices, "fps": fps, "loop": loop}
        atlas = Image.new("RGBA", (FRAME * len(frames), FRAME), (0, 0, 0, 0))
        for i, (_, _, frame) in enumerate(frames):
            atlas.alpha_composite(frame, (i * FRAME, 0))
        png = f"{actor}_{d}_atlas.png"
        js = f"{actor}_{d}_atlas.dsprite.json"
        atlas.save(ACTOR_OUT / png)
        payload = {"version": 1, "texture": png, "width": atlas.width, "height": atlas.height,
                   "frames": [], "clips": clips}
        cursor = 0
        for action, _, _ in frames:
            payload["frames"].append({
                "name": f"{action}_{cursor}", "index": cursor,
                "pixel_rect": {"x": cursor * FRAME, "y": 0, "w": FRAME, "h": FRAME},
                "uv_rect": {"x": cursor * FRAME / atlas.width, "y": 0.0,
                            "w": FRAME / atlas.width, "h": 1.0},
                "pivot": {"x": 0.5, "y": 0.0},
            })
            cursor += 1
        (ACTOR_OUT / js).write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")
        print("[ninja] actor", actor, d, atlas.size, len(frames), "frames")


def copy_music(src: Path):
    AUDIO_OUT.mkdir(parents=True, exist_ok=True)
    for out_name, rel in MUSIC.items():
        shutil.copyfile(src / rel, AUDIO_OUT / out_name)
        print("[ninja] music", out_name)


def write_source_note():
    note = """# Ninja Adventure Asset Pack source note

- Pack: Ninja Adventure Asset Pack
- Author: Pixel-Boy / AAA
- Source page: https://pixel-boy.itch.io/ninja-adventure-asset-pack
- Mirror page: https://opengameart.org/content/ninja-adventure-asset-pack
- GitHub mirror used for download: https://github.com/pixel-boy/NinjaAdventure
- License: CC0 1.0 (as published by the pack author; no separate license file is present in the GitHub mirror)
- Imported files: selected character atlases derived from the ninja/samurai/pig sheets, and selected OGG music tracks
- Modifications: character sheets sliced into 16x16 directional/action atlases and repacked as .dsprite.json; music copied unmodified
- Commercial game rips: none
"""
    (OUT / "SOURCE.md").write_text(note, encoding="utf-8")
    print("[ninja] SOURCE.md written")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", default=None)
    args = ap.parse_args()
    src = find_source(Path(args.source).resolve() if args.source else None)
    ACTOR_OUT.mkdir(parents=True, exist_ok=True)
    for actor, rel in ACTORS.items():
        build_actor_atlas(src, actor, rel)
    copy_music(src)
    write_source_note()
    print("[ninja] done", OUT)


if __name__ == "__main__":
    main()