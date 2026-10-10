#!/usr/bin/env python3
"""Export a funcmap/xref JSON as a TSV for research/data (no code bytes,
no string contents except the source-module name).

usage: export_tsv.py FUNCS_X.json OUT.tsv
Columns: entry, reached_bytes, insns, n_direct_callers, n_callees,
indirect_calls, hwloops, mmr_hi, l1_hi, module, module_evidence, near_module
module_evidence: 'assert' when the function itself references an assert
string naming the source file (high confidence), 'near' when only the
closest attributed functions before/after share a module (low confidence).
"""
import sys, json, bisect
f = json.load(open(sys.argv[1]))
att = sorted((v['entry'], v['module']) for v in f.values() if v['module'])
keys = [a for a, _ in att]
def near(a):
    i = bisect.bisect(keys, a)
    lo = att[i-1][1] if i > 0 else None
    hi = att[i][1] if i < len(att) else None
    return lo if lo == hi else ''
with open(sys.argv[2], 'w') as o:
    o.write('entry\treached_bytes\tinsns\tdirect_callers\tcallees\tindirect_calls\thwloops\tmmr_hi\tl1_hi\tmodule\tmodule_evidence\tnear_module\n')
    for k, v in sorted(f.items()):
        nm = near(v['entry']) if not v['module'] else ''
        o.write(f"0x{k}\t{v['bytes']}\t{v['insn']}\t{v['ncallers']}\t{len(v['callees'])}\t{v['indirect_calls']}\t{v['hwloops']}\t"
                f"{','.join(hex(x) for x in v['mmr'])}\t{','.join(hex(x) for x in v['l1'])}\t{v['module'] or ''}\t"
                f"{'assert' if v['module'] else ('near' if nm else '')}\t{nm or ''}\n")
