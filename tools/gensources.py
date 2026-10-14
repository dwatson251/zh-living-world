#!/usr/bin/env python3
"""Write dsp_sources.cmake from the original VC6 projects so we build exactly what the game built."""
import subprocess,os
R=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..')); C=R+'/src-zh/Code'; W=C+'/Libraries/Source/WWVegas'
sets={'WW_DSP':[W+'/WWLib/wwlib.dsp',W+'/WWMath/wwmath.dsp',W+'/WWDebug/wwdebug.dsp',W+'/WWSaveLoad/wwsaveload.dsp',W+'/Wwutil/wwutil.dsp',W+'/WWDownload/WWDownload.dsp'],
      'GE_DSP':[C+'/GameEngine/GameEngine.dsp'],
      'WW3D_DSP':[W+'/WW3D2/ww3d2.dsp',W+'/wwshade/wwshade.dsp'],
      'DEV_DSP':[C+'/GameEngineDevice/GameEngineDevice.dsp']}
out=[]
for name,dsps in sets.items():
    fs=[]
    for d in dsps:
        fs+=[l for l in subprocess.check_output(['python3',R+'/tools/dspfiles.py',d]).decode().split('\n') if l]
    def resolve(f):
        if os.path.exists(f): return f
        d=os.path.dirname(f)
        if os.path.isdir(d):
            for n in os.listdir(d):
                if n.lower()==os.path.basename(f).lower(): return os.path.join(d,n)
        print('MISSING',f); return None
    fs=[r for r in (resolve(f) for f in fs) if r]
    out.append('set(%s\n  %s)'%(name,'\n  '.join(sorted(set(fs)))))
open(R+'/dsp_sources.cmake','w').write('\n'.join(out)+'\n')
print({k:len(v) for k,v in sets.items()})
