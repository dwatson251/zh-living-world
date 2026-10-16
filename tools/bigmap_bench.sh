#!/bin/bash
# bigmap_bench.sh <map> <ai> <seconds> [ENV=VAL ...]: run a lab W3D server (null Direct3D, uncapped) on <map> with <ai> computer players,
# then print load time, logic frames per second and the process memory (working set / private bytes, peak) every 10 s.
# <map> is a map path, or a number N for the generated "Living World Grand N" (N cells per side). Kills only the PID it started.
R=/home/dnlwtsn/Code/zh-living-world; G="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"; cd $R
MAP=$1; AI=${2:-8}; SEC=${3:-120}; shift 3
case "$MAP" in 800) MAP="Maps\\Living World Grand\\Living World Grand.map";; [0-9]*) MAP="Maps\\Living World Grand $MAP\\Living World Grand $MAP.map";; esac
LOG=um_bench_log.txt; rm -f "$G/$LOG"
mkdir -p build/citybench; cp build/cl/zh_client.exe build/citybench/zh_city.exe   # own exe name: other sessions' lw_killall.sh (build\cl zh_client.exe) cannot hit it, and this script never kills theirs
(setsid tools/winproc.sh build/citybench/zh_city.exe "$G" bench UM_SERVER=${PORT:-7795} UM_NULLD3D=1 UM_UNCAPPED=1 UM_LOGFILE=$LOG UM_HUMAN_SLOTS=${HUMANS:-1} UM_SYNTH_STARTS=${SYNTH:-$AI} "UM_SKIRMISH_MAP=$MAP" UM_SKIRMISH_AI=$AI "$@" -- -win -noshellmap -xres 800 -yres 600 > /dev/null 2>&1 < /dev/null &)
sleep 8; PID=$(powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='zh_city.exe'\" | Sort-Object CreationDate -Descending | Select -First 1 | % { \$_.ProcessId }" | tr -d '\r')   # winproc does not return while the process lives (WSL interop), and killing it kills the server: leave it in the background
echo "pid $PID map '$MAP' ai $AI"
T0=$SECONDS; loaded=""
while [ $((SECONDS - T0)) -lt $SEC ]; do
  sleep 10
  m=$(powershell.exe -NoProfile -Command "\$p=Get-Process -Id $PID -ErrorAction SilentlyContinue; if(\$p){'{0} {1} {2}' -f [int](\$p.WorkingSet64/1MB),[int](\$p.PrivateMemorySize64/1MB),[int](\$p.PeakWorkingSet64/1MB)}else{'gone'}" | tr -d '\r')
  f=$(grep -a "server frame" "$G/$LOG" 2>/dev/null | tail -1 | awk '{print $4}')
  [ -z "$loaded" ] && grep -aq "listening on port" "$G/$LOG" 2>/dev/null && loaded=$((SECONDS - T0)) && echo "listening after ~${loaded}s"
  echo "t=$((SECONDS - T0))s frame=${f:-0} ws/priv/peak MB=$m"
  [ "$m" = gone ] && break
done
grep -a "map generator\|CRASH\|Ran out\|rror" "$G/$LOG" | head -5 | cut -c1-200
/home/dnlwtsn/Code/universal-modder/bin/um win kill $PID > /dev/null 2>&1; sleep 2
