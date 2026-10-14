#!/usr/bin/env python3
"""sym.py <module base hex> <addr hex>... : map runtime addresses to symbols using build/cm/zh.map (lld map, RVAs)."""
import sys,re,bisect
rows=[]
for l in open('/home/dnlwtsn/Code/zh-living-world/build/cm/zh.map',errors='replace'):
    m=re.match(r'^([0-9a-f]{8})\s+([0-9a-f]{8})\s+\d+\s{9,}(\S.*)$',l)
    if m: rows.append((int(m.group(1),16),m.group(3).strip()))
rows.sort(); keys=[r[0] for r in rows]
base=int(sys.argv[1],16)
for a in sys.argv[2:]:
    rva=int(a,16)-base
    i=bisect.bisect_right(keys,rva)-1
    print(a,hex(rva),rows[i][1][:160] if i>=0 else '?')
