#!/bin/bash
# economy_test.sh [inflation 0|1]: money pools and optional inflation. A server with AI players runs short economy cycles with a small pool seed, a bot-driven replica joins late.
# Checks: the economy commands run (log), generators run dry (status.json budgets reach 0 within a cycle), pools refill from rent, and server and replica never diverge (0 mismatches).
INF=${1:-0}
R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
WD="E:\\um-econ-test"; WDL=/mnt/e/um-econ-test; rm -rf $WDL
mkdir -p $R/build/cm/client && cp -f $R/build/cm/zh_headless.exe $R/build/cm/client/zh_client.exe
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_UNCAPPED:UM_SERVER:UM_CLIENT:UM_HUMAN_SLOTS:UM_BOT:UM_KEY:UM_WORLD_DIR:UM_RENT_FRAMES:UM_RENT_FLOOR:UM_ECON_FRAMES:UM_POOL_SEED:UM_POOL_JOIN:UM_INFLATION:UM_GDP_BASELINE:UM_DEFLATION_PERMILLE:UM_TRACE_CMD:UM_POOLS:UM_DEEPCRC_FRAME:UM_TRACE_ECON"
cd "$GAME"
( export UM_WORLD_DIR="$WD" UM_HUMAN_SLOTS=1 UM_SERVER=7781 UM_UNCAPPED=1 UM_RENT_FRAMES=1500 UM_ECON_FRAMES=1500 UM_RENT_FLOOR=800 UM_POOLS=${POOLS:-1} UM_POOL_SEED=1500 UM_POOL_JOIN=500 UM_INFLATION=$INF UM_GDP_BASELINE=20000 UM_DEFLATION_PERMILLE=20 UM_TRACE_CMD=1 UM_SYNTH_STARTS=$((${AI:-4}+1)) UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=${AI:-4}
  timeout 150 $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/server_out.txt 2>&1 ) &
sleep 60
( export UM_BOT=1 UM_KEY=alice UM_CLIENT=127.0.0.1:7781; timeout 60 $R/build/cm/client/zh_client.exe -noshellmap -nomusic -noaudio > $R/build/client_out.txt 2>&1 ) &
wait
echo "--- economy ticks (server)"; grep -c "economy tick" $R/build/server_out.txt; grep "economy tick" $R/build/server_out.txt | head -3 | cut -c1-160; grep "economy tick" $R/build/server_out.txt | tail -2 | cut -c1-160
echo "--- status.json economy"; python3 -c "import json;d=json.load(open('$WDL/'+__import__('os').listdir('$WDL')[0]+'/status.json'));print(json.dumps(d.get('economy')))" 2>&1 | cut -c1-400
echo "--- replica"; grep -E "^\[um\] client: (welcome|connected)" $R/build/client_out.txt | head -3
echo "mismatches: $(grep -c MISMATCH $R/build/server_out.txt)  (replica said diverged: $(grep -c 'DIVERGED' $R/build/client_out.txt))"
