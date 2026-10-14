#!/usr/bin/env python3
"""Generate a case-insensitive clang VFS overlay: /vroot/<path> -> src-zh/Code/<path> (explicit file entries,
because directory-remap does not honour case-insensitivity). Use with -ivfsoverlay and -I/vroot/..."""
import os,json
ROOT=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..'))
root=ROOT+'/src-zh/Code'
roots=[]
for d,_,fs in os.walk(root):
    rel=os.path.relpath(d,root); v='/vroot'+('' if rel=='.' else '/'+rel)
    ents=[{'name':f,'type':'file','external-contents':os.path.join(d,f)} for f in fs if f.lower().endswith(('.h','.inl','.cpp','.c','.hpp','.fx','.rc'))]
    if ents: roots.append({'name':v,'type':'directory','contents':ents})
json.dump({'version':0,'case-sensitive':False,'roots':roots},open(ROOT+'/build/vfs.yaml','w'))
print(len(roots),'dirs')
