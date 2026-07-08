#!/usr/bin/env python3
"""Split dse_api.h into module-specific headers."""
import re
from pathlib import Path
from collections import OrderedDict

repo_root = Path(__file__).resolve().parent.parent.parent
api_dir = repo_root / "engine/scripting/native_api"
header_path = api_dir / "dse_api.h"

content = header_path.read_text(encoding="utf-8")
lines = content.split("\n")

# Find section boundaries: lines that are "// SectionName — description"
# Pattern: // SectionName — description  OR  // SectionName
section_pattern = re.compile(r'^// ([A-Z][A-Za-z0-9_/ ]+?)(?:\s*[—–-]|$)')

# Header preamble (lines 1-23): file comment, include guard, includes, DSE_CAPI, extern "C"
# We need to extract: lines 1-23

# Find the end of the preamble (after extern "C" {)
preamble_end = 0
for i, line in enumerate(lines):
    if 'extern "C" {' in line:
        preamble_end = i + 1  # Include the extern "C" { line
        break

# Find the end of file (before #endif)
ending_start = len(lines)
for i in range(len(lines) - 1, -1, -1):
    if lines[i].strip() == '#endif':
        ending_start = i
        break

# Collect sections
sections = []  # List of (section_name, start_line, end_line)
current_section = None
current_start = None

for i in range(preamble_end, ending_start):
    line = lines[i].strip()
    m = section_pattern.match(line)
    if m and line.startswith('// ') and not line.startswith('// DSE_'):
        # New section
        if current_section:
            sections.append((current_section, current_start, i))
        current_section = m.group(1).strip()
        current_start = i
    elif line == '' and current_section is None:
        continue  # Skip blank lines before first section

if current_section:
    sections.append((current_section, current_start, ending_start))

# Map sections to modules
# Format: module_name -> list of section name patterns
MODULE_MAP = OrderedDict([
    ("core", [
        r"API Version", r"Context Setup", r"Entity", r"TransformComponent",
        r"ECS Core", r"Input", r"App", r"Metrics", r"Floating Origin",
        r"UUID", r"Extended Context", r"Assets",
    ]),
    ("render", [
        r"Camera3D", r"MeshRenderer", r"DirectionalLight", r"PointLight",
        r"SpotLight", r"SkyLight", r"Tree", r"PostProcess", r"Animator3D",
        r"Render ", r"Rendering", r"2D Systems", r"Particles 3D",
        r"LineRenderer", r"Parallax", r"Light2D", r"Sprite", r"Atlas",
        r"Camera2D", r"Trail", r"Decal",
    ]),
    ("physics", [
        r"Physics3D", r"Physics2D", r"RigidBody", r"CharacterController",
        r"Joint3D", r"TerrainHeightmap", r"NavMeshAutoRebake",
        r"DynamicObstacle", r"TerrainTile",
    ]),
    ("world", [
        r"Weather", r"SnowCover", r"Atmosphere", r"DayNight", r"VolumetricCloud",
        r"Open World", r"Streaming", r"Scene", r"SubScene",
    ]),
    ("services", [
        r"Audio", r"Localization", r"UI", r"Navigation",
    ]),
    ("gameplay", [
        r"Gameplay3D", r"Animation", r"Steering", r"LOD", r"Hair",
    ]),
])

def match_module(section_name):
    for module, patterns in MODULE_MAP.items():
        for p in patterns:
            if re.search(p, section_name, re.IGNORECASE):
                return module
    return "core"  # Default to core

# Group sections by module
module_sections = {m: [] for m in MODULE_MAP.keys()}
for name, start, end in sections:
    module = match_module(name)
    module_sections[module].append((name, start, end))

# Also add lines between sections to the appropriate module
# (lines that are part of the previous section's function declarations)

# Preamble (shared by all modules)
preamble = """/**
 * @file {filename}
 * @brief DSEngine Native C ABI — {module_name} module
 *
 * Split from dse_api.h for maintainability. Include dse_api.h for all modules.
 */

#ifndef {guard}
#define {guard}

#include <stdint.h>

#ifndef DSE_CAPI
#  ifdef _WIN32
#    define DSE_CAPI __declspec(dllexport)
#  else
#    define DSE_CAPI __attribute__((visibility("default")))
#  endif
#endif

#ifdef __cplusplus
extern "C" {{
#endif

"""

footer = """

#ifdef __cplusplus
}}
#endif

#endif // {guard}
"""

MODULE_NAMES = {
    "core": "Core (Entity/Transform/ECS/Input/App/Metrics)",
    "render": "Rendering (Camera/Mesh/Light/PostProcess/Particles)",
    "physics": "Physics (3D/2D/Colliders/Joints)",
    "world": "World (Terrain/Water/Weather/Scene/Streaming/OpenWorld)",
    "services": "Services (Audio/Localization/UI/Navigation)",
    "gameplay": "Gameplay3D (Fracture/Cloth/Fluid/Vehicle/Animation)",
}

MODULE_GUARDS = {
    "core": "DSE_API_CORE_H",
    "render": "DSE_API_RENDER_H",
    "physics": "DSE_API_PHYSICS_H",
    "world": "DSE_API_WORLD_H",
    "services": "DSE_API_SERVICES_H",
    "gameplay": "DSE_API_GAMEPLAY_H",
}

# Write module files
written_modules = []
for module, sects in module_sections.items():
    if not sects:
        continue

    filename = f"dse_api_{module}.h"
    guard = MODULE_GUARDS[module]
    module_name = MODULE_NAMES[module]

    header_text = preamble.format(filename=filename, module_name=module_name, guard=guard)

    body = ""
    for name, start, end in sects:
        section_lines = lines[start:end]
        body += "\n".join(section_lines) + "\n\n"

    footer_text = footer.format(guard=guard)

    full_content = header_text + body + footer_text
    out_path = api_dir / filename
    out_path.write_text(full_content, encoding="utf-8")
    written_modules.append(module)
    print(f"  Wrote {filename}: {len(sects)} sections, {len(full_content)} bytes")

# Write new dse_api.h as aggregate
aggregate = """/**
 * @file dse_api.h
 * @brief DSEngine Native C ABI — Lua 与 C# 共享的底层引擎接口
 *
 * 纯 C 函数导出，消除 Lua / C# 两套绑定的重复逻辑。
 * C# 侧通过 Mono InternalCall 或 P/Invoke 调用。
 * Lua 侧 lua_binding_ecs_*.cpp 逐步迁移为调用本层函数。
 *
 * 本文件为聚合头文件，按模块拆分为：
 *   dse_api_core.h      — Entity/Transform/ECS/Input/App/Metrics
 *   dse_api_render.h    — Camera/Mesh/Light/PostProcess/Particles
 *   dse_api_physics.h   — Physics 3D/2D/Colliders/Joints
 *   dse_api_world.h     — Terrain/Water/Weather/Scene/Streaming/OpenWorld
 *   dse_api_services.h  — Audio/Localization/UI/Navigation
 *   dse_api_gameplay.h  — Gameplay3D/Animation
 *
 * 修改单个模块只需编辑对应的 dse_api_<module>.h。
 */

#ifndef DSE_API_H
#define DSE_API_H

#include <stdint.h>

#ifdef _WIN32
#  define DSE_CAPI __declspec(dllexport)
#else
#  define DSE_CAPI __attribute__((visibility("default")))
#endif

// API Version — must be visible before module includes
#define DSE_API_VERSION 10000u   /* v1.0.0 — MMNNPP format */

#ifdef __cplusplus
extern "C" {
#endif

// ── Module includes ──────────────────────────────────────────────
"""

for module in ["core", "render", "physics", "world", "services", "gameplay"]:
    if module in written_modules:
        aggregate += f'#include "dse_api_{module}.h"\n'

aggregate += """

#ifdef __cplusplus
}
#endif

#endif // DSE_API_H
"""

header_path.write_text(aggregate, encoding="utf-8")
print(f"\n  Wrote dse_api.h (aggregate, {len(aggregate)} bytes)")
print(f"  Modules: {', '.join(written_modules)}")
