#!/bin/bash
# scen.sh <numAI> [seconds] [synth starts]: build, run the N-AI skirmish on Tournament Continent, print the tail + crash line.
N=${1:-8}; SEC=${2:-90}
cd /home/dnlwtsn/Code/zh-living-world
timeout 2500 ~/.local/bin/ninja -C build/cm zh_headless 2>&1 | grep -E "error:|FAILED" | head -5
export UM_UNCAPPED=1 UM_SYNTH_STARTS=${3:-$N} UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=$N
TAILN=${TAILN:-6} tools/run.sh $SEC -noshellmap -nomusic -noaudio -noFPSLimit | cut -c1-170
A=$(grep -o "addr=[0-9A-Fa-fx]*" build/last_run.txt | head -1 | cut -d= -f2); B=$(grep -o "image base [0-9A-Fa-fx]*" build/last_run.txt | head -1 | awk '{print $3}')
[ -n "$A" ] && { grep CRASH build/last_run.txt; python3 tools/lsym.py ${B#0x} ${A#0x} | head -8; }
