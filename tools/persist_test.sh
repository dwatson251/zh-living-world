#!/bin/bash
# persist_test.sh <numAI>: run a persisted server, kill it, restart with UM_RESUME, compare CRCs of the overlapping frames.
N=${1:-16}; R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
WD='\\wsl.localhost\Ubuntu\home\dnlwtsn\Code\zh-living-world\worlds\t1'; rm -rf $R/worlds/t1; mkdir -p $R/worlds
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_UNCAPPED:UM_SERVER:UM_EPOCH:UM_WORLD_DIR:UM_RESUME"
cd "$GAME"
( export UM_WORLD_DIR="$WD" UM_EPOCH=1500 UM_SERVER=7779 UM_UNCAPPED=1 UM_SYNTH_STARTS=$N UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=$N
  timeout ${1:+75} $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/p1.txt 2>&1 )
ls -la $R/worlds/t1 | tail -6; cat $R/worlds/t1/frame.txt; echo
( export UM_WORLD_DIR="$WD" UM_RESUME=1 UM_EPOCH=1500 UM_SERVER=7779 UM_UNCAPPED=1 UM_SYNTH_STARTS=$N UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=$N
  timeout 75 $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/p2.txt 2>&1 )
grep -E "resuming|restored|RESTORE|persist|epoch" $R/build/p2.txt | cut -c1-170
for x in 1 2; do grep "^\[um\] frame" $R/build/p$x.txt | sed 's/ t=.*//' > $R/build/p$x.f; done
python3 - <<'P'
a={l.split()[2]:l for l in open('/home/dnlwtsn/Code/zh-living-world/build/p1.f')}
b={l.split()[2]:l for l in open('/home/dnlwtsn/Code/zh-living-world/build/p2.f')}
c=sorted(set(a)&set(b),key=int); bad=[k for k in c if a[k]!=b[k]]
print(len(a),len(b),len(c),'common frames; mismatches',len(bad),'first',bad[:3], 'range',(c[0],c[-1]) if c else None)
P
