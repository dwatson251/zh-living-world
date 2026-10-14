#!/bin/bash
# net_test.sh <numAI> <serverSecs> <clientDelay> <clientSecs>: server + one late-joining replica; prints both summaries.
N=${1:-16}; SS=${2:-90}; CD=${3:-30}; CS=${4:-45}
R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
mkdir -p $R/build/cm/client && cp -f $R/build/cm/zh_headless.exe $R/build/cm/client/zh_client.exe
export UM_DEEPCRC_FRAME=${UM_DEEPCRC_FRAME:-0}
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_FRAMES_MAX:UM_UNCAPPED:UM_SERVER:UM_CLIENT:UM_SEED:UM_EPOCH:UM_FILL:UM_XFERTRACE:UM_HUMAN_SLOTS:UM_BOT:UM_KEY:UM_TEST_SELFDESTRUCT:UM_LASTHIT:UM_START_CASH:UM_WORLD_DIR:UM_RESUME:UM_STAY_AFTER_KICK:UM_TRACE_BATCH:UM_TRACE_KILL:UM_TRACE_AI:UM_AI_POOL:UM_AI_MAX:UM_AI_JOIN_FRAMES:UM_ABSENT_FRAMES:UM_COMBAT_FRAMES:UM_RENT_FRAMES:UM_RENT_PCT:UM_RENT_FLOOR:UM_RENT_DMG_FRAMES:UM_AI_CASH:UM_TRACE_CENSUS:UM_DEEPCRC_FRAME:UM_SAVE_AT:UM_LOADSAVE:UM_WATCHDOG"
cd "$GAME"
( export UM_HUMAN_SLOTS=${HUMANS:-0} ${TESTSD:+UM_TEST_SELFDESTRUCT=$TESTSD} UM_EPOCH=${EPOCH:-0} UM_SERVER=7777 ${SERVER_UNCAPPED:+UM_UNCAPPED=1} UM_AI_POOL=${POOL:-0} UM_SYNTH_STARTS=$((N+${HUMANS:-0}+${POOL:-0})) UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=$N
  timeout $SS $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/server_out.txt 2>&1 ) &
sleep $CD
( export ${BOT:+UM_BOT=1} UM_KEY=${KEY:-alice} UM_CLIENT=127.0.0.1:7777; timeout $CS $R/build/cm/client/zh_client.exe -noshellmap -nomusic -noaudio > $R/build/client_out.txt 2>&1 )
wait
echo "--- server"; grep -E "^\[um\] (server|scenario)" $R/build/server_out.txt | tail -8 | cut -c1-200
echo "--- client"; grep -E "^\[um\] (client|frame)" $R/build/client_out.txt | tail -6 | cut -c1-200
grep -c "MISMATCH" $R/build/server_out.txt
