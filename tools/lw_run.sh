#!/bin/bash
# lw_run.sh <seconds-before-screenshot> [client args...]: kill leftovers of its own earlier runs (exact PIDs of build\\cl exes only: never the user's playtest processes), start a W3D server on a small map and one graphical client, screenshot the client.
R=/home/dnlwtsn/Code/zh-living-world; G="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"; cd $R
for p in $(powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='zh_client.exe' or Name='zh_headless.exe'\" | Where-Object { \$_.ExecutablePath -like '*zh-living-world\\build\\cl\\*' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'); do /home/dnlwtsn/Code/universal-modder/bin/um win kill $p > /dev/null; done
sleep 3; rm -f "$G"/um_*log.txt "$G"/um_client_crash.* "$G"/um_client_trace.txt
timeout 30 tools/winproc.sh build/cl/zh_client.exe "$G" server UM_SERVER=7792 UM_NULLD3D=1 UM_LOGFILE=um_server_log.txt UM_HUMAN_SLOTS=${HUMANS:-4} UM_SYNTH_STARTS=${SYNTH:-8} "UM_SKIRMISH_MAP=${MAP:-Maps\\Tournament Desert\\Tournament Desert.map}" UM_SKIRMISH_AI=${AI:-4} ${EXTRA_SERVER_ENV} -- -win -noshellmap -xres 800 -yres 600 > /dev/null
for i in $(seq 1 60); do grep -aq "listening on port" "$G/um_server_log.txt" 2>/dev/null && break; sleep 1; done; sleep 3
timeout 30 tools/winproc.sh build/cl/zh_client.exe "$G" client UM_CLIENT=127.0.0.1:7792 UM_NAME=${NAME:-Tester} UM_FACTION=${FACTION:-FactionChina} UM_COLOR=${COLOR:-ff8800} UM_KEY=${KEY:-tester-key-$RANDOM} ${EXTRA_CLIENT_ENV} -- ${CLIENT_ARGS:--win -xres ${XRES:-1024} -yres ${YRES:-768}} > /dev/null
sleep ${1:-35}
/home/dnlwtsn/Code/universal-modder/bin/um win shot --exe zh_client.exe ${SHOT:-/tmp/claude-1000/-home-dnlwtsn-Code-universal-modder/0f40306b-2aef-429e-98c6-3a9f5b6a7e68/scratchpad/shot.png} 2>&1 | tail -1
grep -a "client frame\|DIVERGED\|KICK\|now playing\|welcome" "$G/um_client_log.txt" | tail -4 | cut -c1-140
ls "$G"/um_client_crash.* 2>&1 | tail -1
