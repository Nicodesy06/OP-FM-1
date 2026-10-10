#!/usr/bin/env python3
"""Recover functions from a bftool linear disassembly of OP1_vdk code.

usage: funcmap.py DIS_FILE OUT.json [--extra DIS_FILE ...]

Method (no symbols exist in the binary):
  1. every direct `call 0xADDR` target that lands on a decoded instruction
     is a candidate entry;
  2. from each entry a CFG walk follows fall-through and direct branches,
     stops at rts / rti / jump (Px) (VisualDSP returns via
     `P0=[FP+4]; unlink; jump (P0)`) and treats `jump.l` / `jump` to another
     entry as a tail call;
  3. an entry whose walk reaches an undecodable word is discarded (data
     mis-decoded as a call);
  4. per function: size, callees, callers, indirect calls, hardware loops,
     immediate high-halves loaded into P/R registers (0xffc0..0xffff = MMR,
     0xff80..0xffbf = L1 SRAM, 0x0100..0x03ff = SDRAM globals).
"""
import sys, re, json, collections
LINE = re.compile(r'^([0-9a-f]{8}): ((?:[0-9a-f]{4} )+)\s*(.*)$')
TGT = re.compile(r'0x([0-9a-f]+)')
def load(path, ins):
    for l in open(path):
        m = LINE.match(l.rstrip('\n'))
        if not m: continue
        a = int(m.group(1), 16); n = len(m.group(2).split()) * 2
        ins[a] = (n, m.group(3).strip())
def main():
    ins = {}
    load(sys.argv[1], ins)
    i = 3
    while i < len(sys.argv):
        if sys.argv[i] == '--extra': load(sys.argv[i+1], ins); i += 2
        else: i += 1
    calls = collections.Counter()
    for a, (n, t) in ins.items():
        if t.startswith('call 0x'): calls[int(TGT.search(t).group(1), 16)] += 1
    entries = {e for e in calls if e in ins}
    funcs = {}; covered = set()
    for e in sorted(entries):
        seen = set(); work = [e]; bad = False
        info = dict(callees=set(), indirect_calls=0, hwloops=0, imm_hi=set(), returns=0, tailcalls=set(),
                    excpt=0, idle=0, insn=0)
        while work and not bad:
            a = work.pop()
            while a not in seen:
                if a not in ins: bad = True; break
                n, t = ins[a]
                if t == 'illegal' or t.startswith('ILLEGAL'): bad = True; break
                seen.add(a); info['insn'] += 1
                if len(seen) > 20000: bad = True; break
                for m in re.finditer(r'\.h = 0x([0-9a-f]+)', t): info['imm_hi'].add(int(m.group(1), 16))
                if t.startswith('lsetup') or t.startswith('LSETUP'): info['hwloops'] += 1
                if t.startswith('excpt') or t.startswith('raise'): info['excpt'] += 1
                if t == 'idle': info['idle'] += 1
                nxt = a + n
                if t.startswith('call 0x'):
                    info['callees'].add(int(TGT.search(t).group(1), 16))
                elif t.startswith('call ('):
                    info['indirect_calls'] += 1
                elif t in ('rts', 'rti', 'rtx', 'rtn', 'rte') or re.match(r'jump \((P\d|pc \+ P\d)\)', t):
                    info['returns'] += 1; break
                elif re.match(r'jump(\.[sl])? 0x', t):
                    tg = int(TGT.search(t).group(1), 16)
                    if tg in entries and tg != e: info['tailcalls'].add(tg); break
                    work.append(tg); break
                elif re.match(r'if !?cc jump 0x', t):
                    work.append(int(TGT.search(t).group(1), 16))
                a = nxt
        if bad: continue
        lo, hi = min(seen), max(seen) + ins[max(seen)][0]
        nbytes = sum(ins[x][0] for x in seen); covered.update(x for x in seen)
        hi_imms = info['imm_hi']
        funcs[e] = dict(entry=e, lo=lo, hi=hi, span=hi - lo, bytes=nbytes, insn=info['insn'], ncallers=calls[e],
                        callees=sorted(info['callees'] | info['tailcalls']), indirect_calls=info['indirect_calls'],
                        hwloops=info['hwloops'], returns=info['returns'], excpt=info['excpt'], idle=info['idle'],
                        mmr=sorted(h for h in hi_imms if h >= 0xffc0),
                        l1=sorted(h for h in hi_imms if 0xff80 <= h < 0xffc0),
                        globals=sorted(h for h in hi_imms if 0x100 <= h < 0x400))
    callers = collections.defaultdict(set)
    for e, f in funcs.items():
        for c in f['callees']: callers[c].add(e)
    for e, f in funcs.items(): f['callers'] = sorted(callers[e])
    json.dump({f"{e:08x}": f for e, f in sorted(funcs.items())}, open(sys.argv[2], 'w'))
    cov = sum(ins[x][0] for x in covered)
    print(f"candidate entries={len(entries)} functions={len(funcs)} union_code_bytes={cov}")
if __name__ == '__main__':
    main()
