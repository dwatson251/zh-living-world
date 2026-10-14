#!/bin/bash
# cycle.sh [seconds]: incremental build, run headless, print crash line / exit status.
cd /home/dnlwtsn/Code/zh-living-world
timeout 1500 ~/.local/bin/ninja -C build/cm zh_headless 2>&1 | grep -E "error:|FAILED" | head -5
TAILN=${TAILN:-5} tools/run.sh ${1:-150} | tail -${TAILN:-5}
A=$(grep -o "addr=[0-9A-Fa-fx]*" build/last_run.txt | head -1 | cut -d= -f2); B=$(grep -o "image base [0-9A-Fa-fx]*" build/last_run.txt | head -1 | awk '{print $3}')
if [ -n "$A" ]; then grep CRASH build/last_run.txt; python3 tools/lsym.py ${B#0x} ${A#0x} | head -10; fi
grep -E "^exit=" build/last_run.txt
