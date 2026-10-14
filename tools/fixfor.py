#!/usr/bin/env python3
"""Hoist VC6-style leaked for-loop variables: for errors 'use of undeclared identifier X' at file:line, find the
nearest earlier 'for (T X =' and rewrite it as 'T X; for (X ='. Prints what it changed."""
import re,glob,sys,collections
errs=collections.OrderedDict()
for log in glob.glob('/home/dnlwtsn/Code/zh-living-world/build/syn/*.log')+['/home/dnlwtsn/Code/zh-living-world/build/ninja_all.log']:
    for m in re.finditer(r"^(/\S+?):(\d+):\d+: error: use of undeclared identifier '(\w+)'",open(log,errors='replace').read(),re.M):
        errs[(m.group(1),int(m.group(2)),m.group(3))]=1
done=set();changed=0
for (f,line,x) in errs:
    if (f,x) in done: continue
    L=open(f,encoding='latin-1').read().split('\n')
    pat=re.compile(r'\bfor\s*\(\s*((?:const\s+)?[A-Za-z_][\w:<>\*&]*(?:\s*\*)?)\s+%s\s*=' % x)
    for i in range(min(line-1,len(L)-1),max(line-80,-1),-1):
        m=pat.search(L[i])
        if m:
            L[i]=L[i][:m.start()]+m.group(1).strip()+' '+x+'; for ('+x+' ='+L[i][m.end():]
            open(f,'w',encoding='latin-1').write('\n'.join(L)); done.add((f,x)); changed+=1
            print('hoisted',x,f.split('Code/')[-1],i+1); break
    else: print('NOFIND',x,f.split('Code/')[-1],line)
print(changed,'changes')
