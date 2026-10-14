#!/bin/bash
# soak.sh <N...>: for each AI count run the scenario and summarise logic fps / memory.
for n in "$@"; do
  TAILN=1 tools/scen.sh $n ${SOAK_SEC:-120} > /dev/null 2>&1
  python3 - "$n" <<'P'
import re,sys
n=sys.argv[1]; L=[l for l in open('/home/dnlwtsn/Code/zh-living-world/build/last_run.txt') if l.startswith('[um] frame')]
crash=any('CRASH' in l for l in open('/home/dnlwtsn/Code/zh-living-world/build/last_run.txt'))
if not L: print(f'N={n}: no frames (crash={crash})'); sys.exit()
def p(l):
    m=re.search(r'frame (\d+) objects (\d+) crc (\w+) t=(\d+)ms mem=(\d+)MB',l); return tuple(int(x) if x.isdigit() else x for x in m.groups())
a,b=p(L[0]),p(L[-1])
fps=(b[0]-a[0])/max((b[3]-a[3])/1000.0,0.001)
print(f'N={n}: frames={b[0]} objects={b[1]} fps={fps:.1f} mem={b[4]}MB crash={crash}')
P
done
