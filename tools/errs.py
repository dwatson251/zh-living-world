#!/usr/bin/env python3
"""usage: errs.py <file-suffix> [N] -- re-run one translation unit's compile (syntax only) and show first N errors."""
import json,subprocess,shlex,sys
cc=json.load(open('/home/dnlwtsn/Code/zh-living-world/build/cm/compile_commands.json'))
e=[c for c in cc if c['file'].endswith(sys.argv[1])][0]
a=shlex.split(e['command']); i=a.index('-o'); a=a[:i]+['-fsyntax-only']+a[i+2:]
for k in ('-MT','-MF'):
    while k in a: j=a.index(k); del a[j:j+2]
a=[x for x in a if x!='-MD']
r=subprocess.run(a,capture_output=True,text=True,cwd=e['directory'])
n=int(sys.argv[2]) if len(sys.argv)>2 else 10
lines=[l.replace('/home/dnlwtsn/Code/zh-living-world/src-zh/Code/','')[:220] for l in r.stderr.split('\n') if 'error' in l]
print('\n'.join(lines[:n]))
