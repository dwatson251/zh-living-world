#!/usr/bin/env python3
"""bigcat.py <dir with .big> <path inside> [--list]: print a file from the game's BIG archives (later archives do not override here; first hit wins)."""
import struct,sys,glob,os
d,want=sys.argv[1],sys.argv[2].lower().replace('/','\\')
for big in sorted(glob.glob(os.path.join(d,'*.big'))):
    f=open(big,'rb'); h=f.read(16)
    if h[:4]!=b'BIGF': continue
    n=struct.unpack('>I',h[8:12])[0]
    ents=[]
    for i in range(n):
        off,size=struct.unpack('>II',f.read(8)); nm=b''
        while True:
            c=f.read(1)
            if c==b'\0':break
            nm+=c
        ents.append((nm.decode('latin-1'),off,size))
    for nm,off,size in ents:
        if nm.lower()==want:
            f.seek(off); sys.stderr.write('from %s\n'%os.path.basename(big)); sys.stdout.buffer.write(f.read(size)); sys.exit(0)
sys.exit('not found')
