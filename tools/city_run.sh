#!/bin/bash
# city_run.sh <map> <seconds-before-screenshot> [ENV=VAL ...]: a lab server (zh_city.exe, null Direct3D) on <map> (e.g. city:london) and one graphical
# client (zh_cityc.exe), then a screenshot of the client and the tail of both logs. Uses its own exe names and port 7796, and kills only the
# processes it started, so it never touches other sessions' servers or the user's own playtest.
# Extra ENV=VAL words go to both processes; SERVER_ENV / CLIENT_ENV add to one side. AI=<n> computer players (default 4).
R=/home/dnlwtsn/Code/zh-living-world; G="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"; cd $R
MAP=$1; WAIT=${2:-90}; shift 2
SHOT=${SHOT:-$R/build/city_shot.png}
mkdir -p build/citybench; cp build/cl/zh_client.exe build/citybench/zh_city.exe; cp build/cl/zh_client.exe build/citybench/zh_cityc.exe
pids() { powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='$1'\" | % { \$_.ProcessId }" | tr -d '\r'; }
for p in $(pids zh_city.exe) $(pids zh_cityc.exe); do /home/dnlwtsn/Code/universal-modder/bin/um win kill $p > /dev/null 2>&1; done
rm -f "$G/um_city_server.txt" "$G/um_city_client.txt"
CD="UM_CITY_DIR=$(wslpath -w cities)"
(setsid tools/winproc.sh build/citybench/zh_city.exe "$G" city_srv UM_SERVER=7796 UM_NULLD3D=1 UM_LOGFILE=um_city_server.txt UM_HUMAN_SLOTS=${HUMANS:-4} UM_SYNTH_STARTS=${AI:-4} "UM_SKIRMISH_MAP=$MAP" UM_SKIRMISH_AI=${AI:-4} "$CD" "$@" $SERVER_ENV -- -win -noshellmap -xres 800 -yres 600 > /dev/null 2>&1 < /dev/null &)
for i in $(seq 1 600); do grep -aq "listening on port" "$G/um_city_server.txt" 2>/dev/null && break; sleep 1; done
echo "server listening after ~${i}s"; sleep ${CLIENT_DELAY:-0}
(setsid tools/winproc.sh build/citybench/zh_cityc.exe "$G" city_cli UM_CLIENT=127.0.0.1:7796 UM_NAME=${NAME:-Tester} UM_FACTION=${FACTION:-FactionAmerica} UM_COLOR=${COLOR:-3080ff} UM_KEY=${KEY:-city-key-$RANDOM} UM_LOGFILE=um_city_client.txt "$CD" "$@" $CLIENT_ENV -- ${CLIENT_ARGS:--win -xres ${XRES:-1280} -yres ${YRES:-800}} > /dev/null 2>&1 < /dev/null &)
sleep $WAIT
/home/dnlwtsn/Code/universal-modder/bin/um win shot --exe zh_cityc.exe "$SHOT" 2>&1 | tail -1
for p in $(pids zh_cityc.exe); do powershell.exe -NoProfile -Command "\$p=Get-Process -Id $p; 'client ws/priv MB: {0} {1}' -f [int](\$p.WorkingSet64/1MB),[int](\$p.PrivateMemorySize64/1MB)" | tr -d '\r'; done
echo "--- server"; grep -a "city map\|partition\|server frame\|bind slot\|rror" "$G/um_city_server.txt" | tail -4 | cut -c1-200
echo "--- client"; grep -a "city map\|client frame\|now playing\|DIVERGED\|MISMATCH\|KICK\|rror" "$G/um_city_client.txt" | tail -5 | cut -c1-200
[ -n "$KEEP" ] || for p in $(pids zh_city.exe) $(pids zh_cityc.exe); do /home/dnlwtsn/Code/universal-modder/bin/um win kill $p > /dev/null 2>&1; done
