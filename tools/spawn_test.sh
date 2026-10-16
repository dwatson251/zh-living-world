#!/bin/bash
# spawn_test.sh [map] [deadzone-units]: spawn lots + deadzone on a city map. The AI pool fills every lot it can (deadzone large so that happens fast),
# then the test checks that every spawn kept the deadzone to the earlier ones, and that a human joining afterwards is refused with the
# "No space" message. Runs its own exes (build/citybench/zh_city*.exe, port 7796), kills only its own processes.
MAP=${1:-city:london}; DZ=${2:-12000}
R=/home/dnlwtsn/Code/zh-living-world; G="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"; cd $R
AI=4 HUMANS=4 WAITS=${WAITS:-240} SERVER_ENV="UM_UNCAPPED=1 UM_AI_POOL=60 UM_AI_MAX=60 UM_AI_JOIN_FRAMES=45 UM_SPAWN_DEADZONE=$DZ UM_AI_RESERVE_LOTS=0" \
KEEP=1 SHOT=$R/build/spawn_shot.png tools/city_run.sh "$MAP" ${WAITS:-240} > build/spawn_test_run.txt 2>&1
# a second player arrives once the AI has taken every lot it could
cp build/citybench/zh_cityc.exe build/citybench/zh_cityc2.exe; rm -f "$G/um_city_client2.txt"
(setsid tools/winproc.sh build/citybench/zh_cityc2.exe "$G" city_cli2 UM_CLIENT=127.0.0.1:7796 UM_NAME=Late UM_FACTION=FactionChina UM_COLOR=ff3030 UM_KEY=late-key-$RANDOM UM_LOGFILE=um_city_client2.txt UM_STAY_AFTER_KICK=1 "UM_CITY_DIR=$(wslpath -w cities)" -- -win -xres 800 -yres 600 > /dev/null 2>&1 < /dev/null &)
for i in $(seq 1 180); do grep -aq "KICKED\|now playing" "$G/um_city_client2.txt" 2>/dev/null && break; sleep 1; done; sleep 5
/home/dnlwtsn/Code/universal-modder/bin/um win shot --exe zh_cityc2.exe "$R/build/spawn_refused.png" > /dev/null 2>&1
for n in zh_city.exe zh_cityc.exe zh_cityc2.exe; do for p in $(powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='$n'\" | % { \$_.ProcessId }" | tr -d '\r'); do /home/dnlwtsn/Code/universal-modder/bin/um win kill $p > /dev/null 2>&1; done; done
python3 - "$G/um_city_server.txt" "$G/um_city_client2.txt" "$DZ" <<'P'
import re, sys, math
srv = open(sys.argv[1], encoding='latin1').read(); cli = open(sys.argv[2], encoding='latin1').read(); dz = float(sys.argv[3])
lots = re.search(r'(\d+) spawn lots, deadzone (\d+)', srv)
sp = [(int(s), float(x), float(y)) for s, x, y in re.findall(r'slot (\d+) spawns at lot \d+ \(([-\d.]+), ([-\d.]+)\)', srv)]
print(f'lots: {lots.group(1) if lots else "?"}, spawns: {len(sp)}')
bad = 0
for i, (s, x, y) in enumerate(sp):
    for s2, x2, y2 in sp[:i]:
        d = math.hypot(x - x2, y - y2)
        if d < dz * 0.95: bad += 1; print(f'  too close: slot {s} and slot {s2}: {d:.0f} < {dz:.0f}')
refused = 'refused: no free spawn lot' in srv
kicked = re.search(r'KICKED \((No space[^)]*)\)', cli)
print('deadzone kept between spawns:', 'yes' if bad == 0 else f'NO ({bad} pairs)')
print('human join refused when full:', 'yes' if refused and kicked else 'no', '|', kicked.group(1) if kicked else '')
print('PASS' if bad == 0 and refused and kicked and len(sp) > 0 else 'FAIL')
P
