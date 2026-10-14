#!/usr/bin/env python3
"""lsym.py <module base hex> <addr hex>...: file:line symbolication with llvm-symbolizer (needs -gline-tables-only build)."""
import sys,subprocess
base=int(sys.argv[1],16); import os; exe=os.environ.get('UM_EXE','/home/dnlwtsn/Code/zh-living-world/build/cm/zh_headless.exe')
IMAGE=0x400000
args=['%#x'%(int(a,16)-base+IMAGE) for a in sys.argv[2:]]
r=subprocess.run(['/home/dnlwtsn/tools/llvm-mingw-20261006/bin/llvm-symbolizer','--obj='+exe,'--inlines','--demangle']+args,capture_output=True,text=True)
print(r.stdout.replace('/home/dnlwtsn/Code/zh-living-world/src-zh/Code/',''))
