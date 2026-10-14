#!/usr/bin/env python3
"""List .cpp files a VC6 .dsp builds (SOURCE= entries not excluded from every config). usage: dspfiles.py file.dsp"""
import sys,re,os
p=sys.argv[1]; base=os.path.dirname(p)
t=open(p,encoding='latin-1').read().replace('\r','')
out=[]
for blk in t.split('# Begin Source File')[1:]:
    blk=blk.split('# End Source File')[0]
    m=re.search(r'SOURCE=(.+)',blk)
    if not m: continue
    f=m.group(1).strip().replace('\\','/')
    if not f.lower().endswith(('.cpp','.c')): continue
    cfgs=len(re.findall(r'!(?:IF|ELSEIF)\s+"\$\(CFG\)"',blk))
    exc=blk.count('PROP Exclude_From_Build 1')
    if cfgs and exc>=cfgs: continue
    out.append(os.path.normpath(os.path.join(base,f)))
print('\n'.join(out))
