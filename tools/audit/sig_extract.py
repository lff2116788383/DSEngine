#!/usr/bin/env python3
"""Extract approximate Lua signatures for given binding functions.

For each registered Lua name, find the mapped C function and scan its body
for argument accessors (by stack index) and return count.
"""
import os, re, sys, json

BIND_DIR = os.path.join(os.path.dirname(__file__), "..", "..",
                        "engine", "scripting", "lua", "bindings")
BIND_DIR = os.path.abspath(BIND_DIR)

reg_pats = [
    re.compile(r'set_fn\(\s*"([a-zA-Z0-9_]+)"\s*,\s*([A-Za-z0-9_]+)\)'),
    re.compile(r'\{\s*"([a-zA-Z0-9_]+)"\s*,\s*(L_[A-Za-z0-9_]+)\s*\}'),
    re.compile(r'RegisterFn\(\s*L\s*,\s*"([a-zA-Z0-9_]+)"\s*,\s*([A-Za-z0-9_]+)\)'),
]

arg_pat = re.compile(
    r'(?:luaL_check(integer|number|string|udata)|luaL_opt(integer|number|number|string)'
    r'|lua_to(boolean|integer|number|string)|helper::Check(Entity|Bool|Float|Int|String|Number|UInt)'
    r'|helper::Opt(Float|Int|Bool|Number))\s*\(\s*L\s*,\s*(\d+)'
)
optnum_pat = re.compile(r'luaL_opt(?:integer|number)\s*\(\s*L\s*,\s*(\d+)\s*,\s*([^)]+)\)')

def funcs_in(text):
    """Return dict func_name -> body."""
    out = {}
    for m in re.finditer(r'\bint\s+(L_[A-Za-z0-9_]+)\s*\(\s*lua_State\s*\*\s*L\s*\)', text):
        name = m.group(1)
        start = text.find('{', m.end())
        if start < 0:
            continue
        depth = 0
        i = start
        while i < len(text):
            c = text[i]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        out[name] = text[start:i+1]
    return out

def sig_for(body):
    idxs = [int(m.group(m.lastindex)) for m in arg_pat.finditer(body)]
    maxarg = max(idxs) if idxs else 0
    # return count: last 'return N;'
    rets = re.findall(r'return\s+(\d+)\s*;', body)
    retcount = rets[-1] if rets else "?"
    return maxarg, retcount

def main(targets_by_file):
    for fn, names in targets_by_file.items():
        path = os.path.join(BIND_DIR, fn)
        text = open(path, encoding="utf-8").read()
        name2fn = {}
        for pat in reg_pats:
            for m in pat.finditer(text):
                name2fn[m.group(1)] = m.group(2)
        bodies = funcs_in(text)
        print(f"\n===== {fn} =====")
        for n in sorted(names):
            f = name2fn.get(n)
            if not f or f not in bodies:
                print(f"  {n}: <unresolved fn={f}>")
                continue
            maxarg, ret = sig_for(bodies[f])
            print(f"  {n}(args<= {maxarg}) -> {ret}")

if __name__ == "__main__":
    targets = json.load(open(sys.argv[1], encoding="utf-8"))
    main(targets)
