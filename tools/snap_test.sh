#!/bin/bash
# snap_test.sh <numAI> <saveFrame> <loadSecs>: process A runs a world and saves at <saveFrame>; process B resumes the save; compare CRCs after the save frame.
RELOAD=${RELOAD:-}
N=${1:-16}; AT=${2:-900}; LS=${3:-50}
R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_UNCAPPED:UM_SAVE_AT:UM_LOADSAVE:UM_WATCHDOG:UM_DEEPCRC_FRAME:UM_DEEPCRC_TAG:UM_RELOAD:UM_CRC_EVERY:UM_CRC_FROM:UM_FILL"
cd "$GAME"
( ${RELOAD:+export UM_RELOAD=1}; export ${FILLA:+UM_FILL=$FILLA} UM_CRC_EVERY=${EVERY:-30} UM_CRC_FROM=${FROM:-0} UM_DEEPCRC_FRAME=${DEEP:-930} UM_DEEPCRC_TAG=A UM_SAVE_AT=$AT UM_UNCAPPED=1 UM_SYNTH_STARTS=$N UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=$N
  timeout $((AT/20+25)) $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/snapA.txt 2>&1 )
( export ${FILLB:+UM_FILL=$FILLB} UM_CRC_EVERY=${EVERY:-30} UM_CRC_FROM=${FROM:-0} UM_DEEPCRC_FRAME=${DEEP:-930} UM_DEEPCRC_TAG=B UM_LOADSAVE=um_snap.sav UM_UNCAPPED=1 UM_SYNTH_STARTS=$N UM_WATCHDOG=$((LS-5)); timeout $LS $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/snapB.txt 2>&1 )
grep -E "saveGame|CRASH" $R/build/snapA.txt | cut -c1-160; grep -E "loadGame|CRASH|WATCHDOG" $R/build/snapB.txt | cut -c1-160
for x in A B; do grep "^\[um\] frame" $R/build/snap$x.txt | sed 's/ t=.*//' > $R/build/snap$x.f; done
python3 - <<'P'
a={l.split()[2]:l for l in open('/home/dnlwtsn/Code/zh-living-world/build/snapA.f')}
b={l.split()[2]:l for l in open('/home/dnlwtsn/Code/zh-living-world/build/snapB.f')}
c=sorted(set(a)&set(b),key=int); bad=[k for k in c if a[k]!=b[k]]
print(len(a),len(b),len(c),'common frames; mismatches',len(bad),'first',bad[:3])
P
