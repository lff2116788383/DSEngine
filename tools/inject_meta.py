"""One-shot tool: inject _meta tables into all 3D demo .lua files.

Reads config.lua to extract per-demo config, then inserts a _meta block
right after the module table declaration (e.g. `local FractureDemo = {}`).

Usage:  python tools/inject_meta.py [--dry-run]
"""

import re, sys, pathlib, textwrap

DRY_RUN = "--dry-run" in sys.argv

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEMO_DIR = ROOT / "samples" / "lua" / "3d"
CONFIG_PATH = ROOT / "samples" / "lua" / "config.lua"

# ---- Parse config.lua to build demo_name -> config dict string ----
def parse_config_lua():
    """Extract Config.demo_3d_xxx = { ... } blocks as raw Lua strings."""
    text = CONFIG_PATH.read_text(encoding="utf-8")
    mapping = {}
    # Match: Config.demo_3d_xxx = { ... }  or  Config.basic_3d = { ... }
    pattern = re.compile(
        r'Config\.(demo_3d_\w+|basic_3d)\s*=\s*\{([^}]*)\}',
        re.DOTALL
    )
    for m in pattern.finditer(text):
        key = m.group(1)  # e.g. "demo_3d_fracture"
        body = m.group(2).strip()
        if key.startswith("demo_"):
            demo_name = key[5:]  # "3d_fracture"
        else:
            demo_name = key      # "basic_3d"
        mapping[demo_name] = body
    return mapping

# ---- Category inference from filename / first comment ----
CATEGORY_KEYWORDS = {
    "physics": ["physics", "fracture", "ragdoll", "softbody", "buoyancy", "vehicle", "rope", "fluid", "cloth", "jolt"],
    "rendering": ["material", "lighting", "shadow", "skybox", "postprocess", "render_quality", "pbr", "texture",
                   "static_model", "textured_cube", "instancing", "transparency", "lod", "morph", "reflection",
                   "gi_probe", "light_probe", "hair", "decal", "fog", "water"],
    "animation": ["animation", "character", "ik_layers"],
    "audio": ["audio"],
    "scene": ["scene", "terrain", "asset_pack", "streaming"],
    "ui": ["hud", "input", "metrics", "camera_showcase"],
    "compute": ["compute", "gpu_culling"],
    "ai": ["navmesh", "steering"],
    "basic": ["triangle", "square", "cube"],
    "procedural": ["procedural", "particles"],
}

def infer_category(filename):
    stem = filename.replace(".lua", "").replace("3d_", "")
    for cat, keywords in CATEGORY_KEYWORDS.items():
        for kw in keywords:
            if kw in stem:
                return cat
    return "misc"

# ---- Extract human-readable name from first comment line ----
def extract_name(lines):
    for line in lines[:5]:
        stripped = line.strip()
        if stripped.startswith("--"):
            # Remove leading "-- " and any "3D Px sample: " prefix
            desc = re.sub(r'^--\s*', '', stripped)
            desc = re.sub(r'^3D\s+P\d+\s+sample:\s*', '', desc)
            # Trim to reasonable length
            if len(desc) > 60:
                desc = desc[:57] + "..."
            return desc
    return None

# ---- Extract module table name (e.g. "FractureDemo") ----
def find_module_table(lines):
    """Find `local <Name> = {}` near the top of file and return (name, line_index)."""
    for i, line in enumerate(lines[:20]):
        m = re.match(r'^local\s+(\w+)\s*=\s*\{\s*\}', line)
        if m:
            return m.group(1), i
    return None, None

def build_meta_block(table_name, demo_name, name_str, category, config_body):
    """Build the _meta assignment Lua string."""
    lines = [f'{table_name}._meta = {{']
    if name_str:
        escaped = name_str.replace('"', '\\"')
        lines.append(f'    name     = "{escaped}",')
    lines.append(f'    category = "{category}",')
    if config_body:
        lines.append(f'    config   = {{ {config_body} }},')
    lines.append('}')
    return '\n'.join(lines)

def process_file(fpath, config_map):
    text = fpath.read_text(encoding="utf-8")
    lines = text.split('\n')

    table_name, decl_idx = find_module_table(lines)
    if table_name is None:
        print(f"  SKIP {fpath.name}: no module table found")
        return False

    # Already has _meta?
    if f'{table_name}._meta' in text:
        print(f"  SKIP {fpath.name}: _meta already exists")
        return False

    stem = fpath.stem  # e.g. "3d_fracture"
    category = infer_category(stem)
    name_str = extract_name(lines)
    config_body = config_map.get(stem, None)

    meta_block = build_meta_block(table_name, stem, name_str, category, config_body)

    # Insert after the `local <Name> = {}` line
    insert_idx = decl_idx + 1
    # Skip any blank lines right after declaration
    while insert_idx < len(lines) and lines[insert_idx].strip() == '':
        insert_idx += 1

    # Insert meta block with a blank line before and after
    new_lines = lines[:insert_idx] + ['', meta_block, ''] + lines[insert_idx:]

    new_text = '\n'.join(new_lines)

    if DRY_RUN:
        print(f"  DRY  {fpath.name}: would insert _meta after line {decl_idx+1}")
        print(f"       {meta_block.split(chr(10))[0]} ...")
    else:
        fpath.write_text(new_text, encoding="utf-8")
        print(f"  OK   {fpath.name}: _meta inserted ({category})")
    return True

def main():
    config_map = parse_config_lua()
    print(f"Parsed {len(config_map)} config entries from config.lua")

    lua_files = sorted(DEMO_DIR.glob("*.lua"))
    print(f"Found {len(lua_files)} demo files in {DEMO_DIR}")
    if DRY_RUN:
        print("=== DRY RUN MODE ===")
    print()

    modified = 0
    for fpath in lua_files:
        if process_file(fpath, config_map):
            modified += 1

    print(f"\nDone: {modified} files {'would be ' if DRY_RUN else ''}modified")

if __name__ == "__main__":
    main()
