#!/bin/bash
# fill_test.sh <numAI> <secs>: same world twice with different allocation poison bytes; any CRC difference = uninitialised read.
N=${1:-16}; SEC=${2:-45}; R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_UNCAPPED:UM_FILL:UM_CRC_EVERY:UM_CRC_FROM"
cd "$GAME"
for f in 0 165; do
  ( export UM_FILL=$f UM_CRC_EVERY=${EVERY:-30} UM_UNCAPPED=1 UM_SYNTH_STARTS=$N UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=$N
    timeout $SEC $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/fill_$f.txt 2>&1 )
  grep "^\[um\] frame" $R/build/fill_$f.txt | sed 's/ t=.*//' > $R/build/fill_$f.f
done
python3 - <<'P'
a={l.split()[2]:l for l in open('/home/dnlwtsn/Code/zh-living-world/build/fill_0.f')}
b={l.split()[2]:l for l in open('/home/dnlwtsn/Code/zh-living-world/build/fill_165.f')}
c=sorted(set(a)&set(b),key=int); bad=[k for k in c if a[k]!=b[k]]
print(len(a),len(b),len(c),'common; mismatches',len(bad),'first',bad[:3])
P
