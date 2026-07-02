#!/usr/bin/env python3
"""Generate C# LibraryImport declarations for the hand-written C ABI in dse_api.h.

Codegen'd C ABI (dse_api.gen.h) is covered by Native.gen.cs (from binding_defs.json).
This script covers the *manual* functions declared in dse_api.h, emitting
GameScripts/DSEngine.Runtime/Generated/NativeManual.gen.cs (partial class Native).

Skips: functions already declared in Native.gen.cs, and internal context-setup
functions that are not scriptable API (dse_native_api_init, dse_get_*_ptr).
"""
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HEADER = os.path.join(ROOT, "engine", "scripting", "native_api", "dse_api.h")
EXISTING = os.path.join(ROOT, "GameScripts", "DSEngine.Runtime", "Generated", "Native.gen.cs")
OUT = os.path.join(ROOT, "GameScripts", "DSEngine.Runtime", "Generated", "NativeManual.gen.cs")

SKIP = {"dse_native_api_init", "dse_get_world_ptr", "dse_get_asset_manager_ptr",
        "dse_get_audio_system_ptr"}

RET_MAP = {"void": "void", "int": "int", "float": "float", "uint32_t": "uint"}

# out-pointer params that are arrays (not scalar out) — matched by parameter name
ARRAY_OUT_NAMES = {
    "out_xyz", "out_vel", "out_origin", "out_dir", "out_point", "out_normal",
    "out_velocity", "out", "buf", "out_hit_xyz", "out_entities",
}


CS_KEYWORDS = {"out", "in", "ref", "params", "base", "event", "object", "string", "float",
               "int", "lock", "this", "fixed", "checked", "class", "new", "delegate", "default"}


def snake_to_camel(name: str) -> str:
    parts = name.split("_")
    n = parts[0] + "".join(p.capitalize() for p in parts[1:])
    return "@" + n if n in CS_KEYWORDS else n


def map_param(ctype: str, name: str, func: str):
    """Return (csharp_param, needs_utf8) for one C parameter."""
    ctype = ctype.strip()
    n = snake_to_camel(name)
    needs_utf8 = False
    if ctype == "const char* const*":
        cs = f"string[] {n}"
        needs_utf8 = True
    elif ctype == "const char*":
        cs = f"string {n}"
        needs_utf8 = True
    elif ctype == "char*":
        cs = f"[Out] byte[] {n}"
    elif ctype in ("const uint32_t*",):
        cs = f"uint[] {n}"
    elif ctype in ("const float*",):
        cs = f"float[] {n}"
    elif ctype in ("const int*",):
        cs = f"int[] {n}"
    elif ctype.endswith("*"):
        base = RET_MAP[ctype[:-1].strip()]
        if name in ARRAY_OUT_NAMES:
            cs = f"[Out] {base}[] {n}"
        else:
            cs = f"out {base} {n}"
    else:
        cs = f"{RET_MAP[ctype]} {n}"
    return cs, needs_utf8


def parse_header(text: str):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    decls = []
    for m in re.finditer(
            r"DSE_CAPI\s+([A-Za-z0-9_]+\s*\*?)\s*(dse_[a-z0-9_]+)\s*\(([^;]*?)\)\s*;",
            text, flags=re.S):
        ret, name, args = m.group(1).strip(), m.group(2), m.group(3).strip()
        decls.append((ret, name, args))
    return decls


def parse_args(args: str):
    args = re.sub(r"\s+", " ", args).strip()
    if args in ("", "void"):
        return []
    out = []
    for a in args.split(","):
        a = a.strip()
        mm = re.match(r"^(.*?)([A-Za-z_][A-Za-z0-9_]*)$", a)
        ctype, name = mm.group(1).strip(), mm.group(2)
        # normalize pointer spacing: "const char * const *" -> "const char* const*"
        ctype = re.sub(r"\s*\*\s*", "* ", ctype).strip()
        ctype = re.sub(r"\*\s+$", "*", ctype)
        ctype = ctype.replace("* const*", "* const*")
        out.append((ctype, name))
    return out


# 公开门面分组：前缀 -> (类名, 需剥离的前缀)
GROUPS = [
    ("input_", "Input"),
    ("app_", "App"),
    ("assets_", "Assets"),
    ("metrics_", "Metrics"),
    ("render_", "Render"),
    ("physics3d_", "Physics3D"),
    ("rigidbody3d_", "RigidBody3D"),
    ("collision_", "Collision"),
    ("collider_", "Collider"),
    ("joint3d_", "Joint3D"),
    ("character_controller3d_", "CharacterController3D"),
    ("terrain_heightmap_", "Terrain"),
    ("terrain_", "Terrain"),
    ("box_collider3d_", "BoxCollider3D"),
    ("sphere_collider3d_", "SphereCollider3D"),
    ("capsule_collider3d_", "CapsuleCollider3D"),
    ("mesh_collider3d_", "MeshCollider3D"),
    ("anim2d_", "Anim2D"),
    ("anim3d_", "Anim3D"),
    ("animlayer_", "AnimLayer"),
    ("ik_", "Ik"),
    ("foot_ik_", "FootIk"),
    ("bone_attach_", "BoneAttachment"),
    ("morph_", "MorphTarget"),
    ("fracture_", "Fracture"),
    ("cloth_", "Cloth"),
    ("fluid_", "Fluid"),
    ("ragdoll_", "Ragdoll"),
    ("softbody_", "SoftBody"),
    ("vehicle_", "Vehicle"),
    ("rope_", "Rope"),
    ("buoyancy_", "Buoyancy"),
    ("weather_", "Weather"),
    ("snow_cover_", "Snow"),
    ("snow_", "Snow"),
    ("atmosphere_", "Atmosphere"),
    ("day_night_", "DayNight"),
    ("volumetric_cloud_", "Cloud"),
    ("cloud_", "Cloud"),
    ("audio_source_", "AudioSource"),
    ("audio_listener_", "AudioListener"),
    ("audio_", "Audio"),
    ("nav_agent_", "NavAgent"),
    ("nav_", "Nav"),
    ("l10n_", "Localization"),
    ("scene_", "Scene"),
    ("ui_", "Ui"),
]

FACADE_OUT = os.path.join(ROOT, "GameScripts", "DSEngine.Runtime", "Generated", "ApiManual.gen.cs")


def group_for(short: str):
    for prefix, cls in GROUPS:
        if short.startswith(prefix):
            method = short[len(prefix):]
            # 保留子系统区分度：同类多前缀时以剥离前缀后的 Pascal 名为方法名，
            # 若冲突风险高（如 physics3d 组多前缀），保留前缀词
            return cls, prefix, method
    return "Components", "", short


def pascal(snake: str) -> str:
    return "".join(p.capitalize() for p in snake.split("_"))


def main():
    existing = set(re.findall(r'EntryPoint = "(dse_[a-z0-9_]+)"',
                              open(EXISTING, encoding="utf-8").read()))
    decls = parse_header(open(HEADER, encoding="utf-8").read())

    lines = [
        "// <auto-generated>",
        "//   来源：tools/codegen/gen_csharp_manual.py ← engine/scripting/native_api/dse_api.h（手写 C ABI）",
        "//   勿手动修改；新增手写 C ABI 后重新运行生成脚本。",
        "// </auto-generated>",
        "",
        "using System.Runtime.InteropServices;",
        "",
        "namespace DSEngine;",
        "",
        "internal static partial class Native {",
    ]
    count = 0
    facade = {}  # class -> [method lines]
    for ret, name, args in decls:
        if name in SKIP or name in existing:
            continue
        try:
            params = []
            calls = []
            utf8 = False
            for ctype, pname in parse_args(args):
                cs, n8 = map_param(ctype, pname, name)
                params.append(cs)
                pn = cs.split()[-1]
                calls.append(f"out {pn}" if cs.startswith("out ") else pn)
                utf8 = utf8 or n8
            csret = RET_MAP[ret] if not ret.endswith("*") else "nint"
        except KeyError as e:
            raise SystemExit(f"unmapped type {e} in {name}({args})")
        attr = f'[LibraryImport(Lib, EntryPoint = "{name}"'
        if utf8:
            attr += ", StringMarshalling = StringMarshalling.Utf8"
        attr += ")]"
        lines.append(f"    {attr}")
        lines.append(f"    internal static partial {csret} {name}({', '.join(params)});")
        lines.append("")
        count += 1

        cls, _prefix, remainder = group_for(name[len("dse_"):])
        method = pascal(remainder)
        body_ret = "" if csret == "void" else "return "
        facade.setdefault(cls, []).append(
            f"    public static {csret} {method}({', '.join(params)}) "
            f"{{ {body_ret}Native.{name}({', '.join(calls)}); }}")
    lines.append("}")
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {OUT}: {count} functions")

    flines = [
        "// <auto-generated>",
        "//   来源：tools/codegen/gen_csharp_manual.py ← engine/scripting/native_api/dse_api.h（手写 C ABI）",
        "//   公开门面：按子系统分组的静态类，转发到 Native P/Invoke 声明。勿手动修改。",
        "// </auto-generated>",
        "",
        "using System.Runtime.InteropServices;",
        "",
        "namespace DSEngine.Api;",
        "",
    ]
    for cls in sorted(facade):
        flines.append(f"public static class {cls} {{")
        flines.extend(facade[cls])
        flines.append("}")
        flines.append("")
    with open(FACADE_OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(flines) + "\n")
    print(f"wrote {FACADE_OUT}: {len(facade)} classes")


if __name__ == "__main__":
    main()
