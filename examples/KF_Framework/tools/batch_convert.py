#!/usr/bin/env python3
"""
batch_convert.py — 批量转换 KF 资产到 DSEngine 原生格式

流程:
  1. 调用 kf_to_gltf.py 将 KF 二进制 → .glb
  2. 调用 AssetBuilder 将 .glb → .dmesh / .dskel / .danim / .dmat

用法:
    python tools/batch_convert.py --actors knight mutant zombie
    python tools/batch_convert.py --all
    python tools/batch_convert.py --actors knight --asset-builder path/to/AssetBuilder.exe
"""

import argparse
import subprocess
import sys
from pathlib import Path

# 默认 AssetBuilder 路径 (相对于 DSEngine 根目录)
DEFAULT_ASSET_BUILDER_PATHS = [
    "bin/AssetBuilder.exe",
    "bin/Release/AssetBuilder.exe",
    "bin/Debug/AssetBuilder.exe",
    "build_vs2022/apps/tools/asset_builder/Release/AssetBuilder.exe",
    "build_vs2022/apps/tools/asset_builder/Debug/AssetBuilder.exe",
    "build_vs2022/Release/AssetBuilder.exe",
    "build_vs2022/Debug/AssetBuilder.exe",
]


def find_asset_builder(engine_root: Path) -> Path:
    """查找 AssetBuilder.exe"""
    for rel in DEFAULT_ASSET_BUILDER_PATHS:
        p = engine_root / rel
        if p.exists():
            return p
    return None


def run_kf_to_gltf(work_dir: Path, actor: str, raw_dir: Path, gltf_dir: Path):
    """调用 kf_to_gltf.py 转换一个 actor"""
    script = work_dir / "tools" / "kf_to_gltf.py"
    cmd = [
        sys.executable, str(script),
        "--actor", actor,
        "--raw-dir", str(raw_dir),
        "--out-dir", str(gltf_dir),
    ]
    print(f"\n{'='*60}")
    print(f"[batch] kf_to_gltf: {actor}")
    print(f"{'='*60}")
    result = subprocess.run(cmd, cwd=str(work_dir))
    if result.returncode != 0:
        print(f"[ERROR] kf_to_gltf 失败: {actor}")
        return False
    return True


def run_asset_builder(asset_builder: Path, glb_path: Path, out_dir: Path):
    """调用 AssetBuilder 将 .glb 转换为 DSEngine 原生格式"""
    cmd = [
        str(asset_builder),
        str(glb_path),
        "--out-dir", str(out_dir),
    ]
    print(f"  [AssetBuilder] {glb_path.name} → {out_dir}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  [ERROR] AssetBuilder 失败:")
        print(f"    stdout: {result.stdout}")
        print(f"    stderr: {result.stderr}")
        return False
    else:
        if result.stdout.strip():
            for line in result.stdout.strip().split('\n'):
                print(f"    {line}")
    return True


def get_actor_list(raw_dir: Path) -> list:
    """从 raw/model/actor/ 目录获取所有 actor 名称"""
    actor_dir = raw_dir / "model" / "actor"
    if not actor_dir.exists():
        return []
    return [f.stem for f in actor_dir.glob("*.model")]


def main():
    parser = argparse.ArgumentParser(description="批量转换 KF 资产到 DSEngine 原生格式")
    parser.add_argument("--actors", nargs="+", help="要转换的 actor 名称列表")
    parser.add_argument("--all", action="store_true", help="转换所有 actor")
    parser.add_argument("--asset-builder", type=str, default=None,
                        help="AssetBuilder.exe 路径")
    parser.add_argument("--skip-gltf", action="store_true",
                        help="跳过 glTF 生成 (仅运行 AssetBuilder)")
    parser.add_argument("--skip-cook", action="store_true",
                        help="跳过 AssetBuilder (仅生成 glTF)")
    args = parser.parse_args()

    # 路径设置
    work_dir = Path(__file__).resolve().parent.parent
    engine_root = work_dir.parent.parent  # DSEngine 根目录
    raw_dir = work_dir / "assets" / "raw"
    gltf_dir = work_dir / "assets" / "gltf"
    cooked_dir = work_dir / "cooked"

    gltf_dir.mkdir(parents=True, exist_ok=True)
    cooked_dir.mkdir(parents=True, exist_ok=True)

    # 查找 AssetBuilder
    asset_builder = None
    if not args.skip_cook:
        if args.asset_builder:
            asset_builder = Path(args.asset_builder)
            if not asset_builder.exists():
                print(f"[ERROR] AssetBuilder 不存在: {asset_builder}")
                sys.exit(1)
        else:
            asset_builder = find_asset_builder(engine_root)
            if not asset_builder:
                print("[WARN] 未找到 AssetBuilder.exe，将跳过 cooking 步骤")
                print("       请先编译 DSEngine，或使用 --asset-builder 指定路径")
                print(f"       搜索位置: {engine_root}")
                args.skip_cook = True

    if asset_builder:
        print(f"[batch] AssetBuilder: {asset_builder}")

    # 确定要转换的 actor
    if args.all:
        actors = get_actor_list(raw_dir)
        if not actors:
            print("[ERROR] 在 assets/raw/model/actor/ 中未找到 .model 文件")
            print("        请先运行: python tools/setup_assets.py")
            sys.exit(1)
    elif args.actors:
        actors = args.actors
    else:
        print("[ERROR] 请指定 --actors 或 --all")
        parser.print_help()
        sys.exit(1)

    print(f"[batch] 工作目录: {work_dir}")
    print(f"[batch] 待转换 actors: {actors}")

    success_count = 0
    fail_count = 0

    for actor in actors:
        # Step 1: KF → glTF
        if not args.skip_gltf:
            ok = run_kf_to_gltf(work_dir, actor, raw_dir, gltf_dir)
            if not ok:
                fail_count += 1
                continue

        # Step 2: glTF → DSEngine (AssetBuilder)
        if not args.skip_cook:
            # 找到该 actor 的所有 .glb 文件
            glb_files = sorted(gltf_dir.glob(f"{actor}*.glb"))
            if not glb_files:
                print(f"[WARN] 未找到 {actor} 的 .glb 文件")
                fail_count += 1
                continue

            for glb in glb_files:
                ok = run_asset_builder(asset_builder, glb, cooked_dir)
                if not ok:
                    fail_count += 1
                else:
                    success_count += 1
        else:
            success_count += 1

    print(f"\n{'='*60}")
    print(f"[batch] 完成!")
    print(f"[batch] 成功: {success_count}, 失败: {fail_count}")
    if not args.skip_cook and cooked_dir.exists():
        cooked_files = list(cooked_dir.glob("*"))
        if cooked_files:
            print(f"[batch] 输出目录 ({cooked_dir}):")
            for f in sorted(cooked_files):
                print(f"         {f.name} ({f.stat().st_size} bytes)")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
