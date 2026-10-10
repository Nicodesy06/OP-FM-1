#!/usr/bin/env python3
"""Attach string cross-references and source-module attribution to funcmap.

usage: xref.py IMAGE DIS_FILE FUNCS.json OUT.json [--extra DIS]
A constant is recovered when a function loads REG.l = lo and REG.h = hi
(either order, VisualDSP emits both) and the address holds a NUL-terminated
printable string of length >= 4. Module = the source path quoted in an
assert string ("..\\src\\<module>:<line> <expr>") referenced by the function.
"""
import sys, re, json, collections
LINE = re.compile(r'^([0-9a-f]{8}): ((?:[0-9a-f]{4} )+)\s*(.*)$')
img = open(sys.argv[1], 'rb').read()
ins = {}
def load(p):
    for l in open(p):
        m = LINE.match(l.rstrip('\n'))
        if m: ins[int(m.group(1), 16)] = (len(m.group(2).split()) * 2, m.group(3).strip())
load(sys.argv[2])
if '--extra' in sys.argv: load(sys.argv[sys.argv.index('--extra') + 1])
funcs = json.load(open(sys.argv[3]))
def cstr(a):
    if not (0 <= a < len(img)): return None
    e = img.find(b'\0', a, a + 300)
    if e < 0 or e - a < 4: return None
    s = img[a:e]
    if all(32 <= c < 127 or c in (9, 10) for c in s): return s.decode()
    return None
RL = re.compile(r'\b([RP][0-7])\.l = 0x([0-9a-f]+)'); RH = re.compile(r'\b([RP][0-7])\.h = 0x([0-9a-f]+)')
SRC = re.compile(r'\\src\\([\w\\]+\.(?:cpp|c|h)):(\d+)')
for k, f in funcs.items():
    lo, hi = f['lo'], f['hi']
    regl, regh = {}, {}; consts = set()
    a = lo
    # linear pass over the function's address range (good enough: VisualDSP
    # keeps a function's blocks contiguous; tail-merged code is rare)
    while a < hi and (hi - lo) < 0x40000:
        if a not in ins: a += 2; continue
        n, t = ins[a]
        for m in RL.finditer(t):
            regl[m.group(1)] = int(m.group(2), 16)
            if m.group(1) in regh: consts.add((regh[m.group(1)] << 16) | regl[m.group(1)])
        for m in RH.finditer(t):
            regh[m.group(1)] = int(m.group(2), 16)
            if m.group(1) in regl: consts.add((regh[m.group(1)] << 16) | regl[m.group(1)])
        a += n
    strs = []
    for c in sorted(consts):
        s = cstr(c)
        if s: strs.append([c, s])
    mods = collections.Counter()
    for c, s in strs:
        m = SRC.search(s)
        if m: mods[m.group(1).replace('\\', '/')] += 1
    f['strings'] = strs[:40]
    f['modules'] = dict(mods)
    f['module'] = mods.most_common(1)[0][0] if mods else None
    f['data_refs'] = sorted(c for c in consts if 0x01000000 <= c < 0x04000000 and not cstr(c))[:64]
json.dump(funcs, open(sys.argv[4], 'w'))
n = sum(1 for f in funcs.values() if f['module'])
print(f"functions={len(funcs)} with_module={n} with_strings={sum(1 for f in funcs.values() if f['strings'])}")
