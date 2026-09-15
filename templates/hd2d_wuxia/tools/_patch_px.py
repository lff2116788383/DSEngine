import re
for fn in ("gen_lib.py", "gen_maps.py", "generate_assets.py", "gen_font.py", "gen_audio.py"):
    src = open(fn, encoding="utf-8").read()
    out, n = [], 0
    for line in src.split("\n"):
        if "def px(" in line:
            out.append(line)
            continue
        new = re.sub(r"(?<![\w.])px\(\s*(?!img[\s,])", "px(img, ", line)
        if new != line:
            n += 1
        out.append(new)
    if n:
        open(fn, "w", encoding="utf-8").write("\n".join(out))
    print(fn, "patched", n)