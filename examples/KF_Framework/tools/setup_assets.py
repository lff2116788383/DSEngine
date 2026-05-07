#!/usr/bin/env python3
"""
setup_assets.py — 复制 KF_Framework 资产到本地 assets/ 目录

用法:
    python tools/setup_assets.py
    python tools/setup_assets.py --source C:/path/to/KF_Framework/data --fbx-dir C:/path/to/KF_ModelAnalyzer/data/FBX
"""

import argparse
import shutil
import sys
from pathlib import Path

# 默认路径
DEFAULT_SOURCE = r"C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data"
DEFAULT_FBX_DIR = r"C:\Users\wenbilin\Desktop\temp_analysis\KF_ModelAnalyzer\data\FBX"

def copy_files(src_dir: Path, dst_dir: Path, extensions: list, recursive=False):
    """复制指定扩展名的文件"""
    dst_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    if recursive:
        files = [f for ext in extensions for f in src_dir.rglob(f"*{ext}")]
    else:
        files = [f for ext in extensions for f in src_dir.glob(f"*{ext}")]
    
    for f in files:
        dst = dst_dir / f.name
        if not dst.exists():
            shutil.copy2(f, dst)
            count += 1
        else:
            pass  # 已存在，跳过
    return count


def copy_directory(src_dir: Path, dst_dir: Path):
    """递归复制整个目录"""
    dst_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    if not src_dir.exists():
        return 0
    for f in src_dir.rglob("*"):
        if f.is_file():
            rel = f.relative_to(src_dir)
            dst = dst_dir / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            if not dst.exists():
                shutil.copy2(f, dst)
                count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description="复制 KF_Framework 资产到本地 assets/ 目录")
    parser.add_argument("--source", type=str, default=DEFAULT_SOURCE,
                        help="KF_Framework/data 目录路径")
    parser.add_argument("--fbx-dir", type=str, default=DEFAULT_FBX_DIR,
                        help="KF_ModelAnalyzer/data/FBX 目录路径")
    parser.add_argument("--force", action="store_true",
                        help="强制覆盖已有文件")
    args = parser.parse_args()

    source = Path(args.source)
    fbx_dir = Path(args.fbx_dir)

    if not source.exists():
        print(f"[ERROR] 源目录不存在: {source}")
        sys.exit(1)

    # 工作目录: examples/KF_Framework/
    work_dir = Path(__file__).resolve().parent.parent
    assets_dir = work_dir / "assets"

    print(f"[setup_assets] 纹理/音频源: {source}")
    print(f"[setup_assets] FBX 源: {fbx_dir}")
    print(f"[setup_assets] 目标目录: {assets_dir}")
    print()

    # 1. 纹理: data/TEXTURE/*.jpg,*.png,*.tga → assets/textures/
    tex_src = source / "TEXTURE"
    tex_dst = assets_dir / "textures"
    n = copy_files(tex_src, tex_dst, [".jpg", ".png", ".tga"])
    print(f"[textures] 复制 {n} 个文件 → {tex_dst}")

    # 2. BGM: data/bgm/*.wav → assets/audio/bgm/
    bgm_src = source / "bgm"
    bgm_dst = assets_dir / "audio" / "bgm"
    n = copy_files(bgm_src, bgm_dst, [".wav"])
    print(f"[bgm] 复制 {n} 个文件 → {bgm_dst}")

    # 3. SE: data/se/*.wav → assets/audio/se/
    se_src = source / "se"
    se_dst = assets_dir / "audio" / "se"
    n = copy_files(se_src, se_dst, [".wav"])
    print(f"[se] 复制 {n} 个文件 → {se_dst}")

    # 4. FBX: KF_ModelAnalyzer/data/FBX/ → assets/fbx/ (保持子目录结构)
    if fbx_dir.exists():
        fbx_dst = assets_dir / "fbx"
        n = copy_directory(fbx_dir, fbx_dst)
        print(f"[fbx] 复制 {n} 个文件 → {fbx_dst}")
    else:
        print(f"[fbx] 跳过: FBX 源目录不存在 ({fbx_dir})")

    print()
    print("[setup_assets] 完成!")


if __name__ == "__main__":
    main()
