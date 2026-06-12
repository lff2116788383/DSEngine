#!/usr/bin/env python3
"""Print detailed arg/return info for given binding functions (ground truth)."""
import os, re, sys, json

BIND_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..",
                           "engine", "scripting", "lua", "bindings"))

reg_pats = [
    re.compile(r'set_fn\(\s*"([a-zA-Z0-9_]+)"\s*,\s*([A-Za-z0-9_]+)\)'),
    re.compile(r'\{\s*"([a-zA-Z0-9_]+)"\s*,\s*(L_[A-Za-z0-9_]+)\s*\}'),
    re.compile(r'RegisterFn\(\s*L\s*,\s*"([a-zA-Z0-9_]+)"\s*,\s*([A-Za-z0-9_]+)\)'),
]

ARG = re.compile(
    r'(luaL_check\w+|luaL_opt\w+|lua_to\w+|helper::Check\w+|helper::Opt\w+)'
    r'\s*\(\s*L\s*,\s*(\d+)\s*(?:,\s*([^);]+))?\)')
PUSH = re.compile(r'lua_push(\w+)\s*\(')

def bodies(text):
    out = {}
    for m in re.finditer(r'\bint\s+(L_[A-Za-z0-9_]+)\s*\(\s*lua_State\s*\*\s*L\s*\)', text):
        s = text.find('{', m.end())
        if s < 0: continue
        d=0; i=s
        while i < len(text):
            if text[i]=='{': d+=1
            elif text[i]=='}':
                d-=1
                if d==0: break
            i+=1
        out[m.group(1)] = text[s:i+1]
    return out

def main(targets):
    for fn, names in targets.items():
        path = os.path.join(BIND_DIR, fn)
        text = open(path, encoding="utf-8").read()
        n2f={}
        for p in reg_pats:
            for m in p.finditer(text):
                n2f[m.group(1)]=m.group(2)
        bs=bodies(text)
        print(f"\n##### {fn}")
        for n in sorted(names):
            f=n2f.get(n)
            b=bs.get(f,"")
            args={}
            for m in ARG.finditer(b):
                idx=int(m.group(2))
                kind=m.group(1).split("::")[-1]
                dflt=(m.group(3) or "").strip()
                if idx not in args:
                    args[idx]=(kind,dflt)
            argstr=", ".join(f"{i}:{args[i][0]}{('='+args[i][1]) if args[i][1] else ''}"
                             for i in sorted(args))
            pushes=PUSH.findall(b)
            rets=re.findall(r'return\s+(\d+)\s*;', b)
            ret=rets[-1] if rets else "?"
            print(f"  {n}  [{argstr}]  ret={ret} pushes={pushes[:6]}")

if __name__=="__main__":
    main(json.load(open(sys.argv[1],encoding="utf-8")))
