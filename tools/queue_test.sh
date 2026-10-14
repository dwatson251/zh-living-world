#!/bin/bash
# queue_test.sh: one human slot, client alice joins and leaves, client bob waits in the queue and must be promoted when alice's absent base expires.
R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
mkdir -p $R/build/cm/client && cp -f $R/build/cm/zh_headless.exe $R/build/cm/client/zh_client.exe; cp -f $R/build/cm/zh_headless.exe $R/build/cm/client/zh_client2.exe
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_UNCAPPED:UM_SERVER:UM_CLIENT:UM_HUMAN_SLOTS:UM_BOT:UM_KEY:UM_ABSENT_FRAMES:UM_COMBAT_FRAMES:UM_RENT_FRAMES"
cd "$GAME"
( export UM_HUMAN_SLOTS=1 UM_SERVER=7777 UM_UNCAPPED=1 UM_ABSENT_FRAMES=600 UM_COMBAT_FRAMES=100 UM_RENT_FRAMES=1000000 UM_SYNTH_STARTS=5 UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=4
  timeout 140 $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/server_out.txt 2>&1 ) &
sleep 20
( export UM_BOT=1 UM_KEY=alice UM_CLIENT=127.0.0.1:7777; timeout 25 $R/build/cm/client/zh_client.exe -noshellmap -nomusic -noaudio > $R/build/client_out.txt 2>&1 ) &
sleep 8
( export UM_BOT=1 UM_KEY=bobby UM_CLIENT=127.0.0.1:7777; timeout 90 $R/build/cm/client/zh_client2.exe -noshellmap -nomusic -noaudio > $R/build/client2_out.txt 2>&1 ) &
wait
grep -E "^\[um\] server: (client|slot|alice|bobby|absent)" $R/build/server_out.txt | cut -c1-160
echo "mismatches: $(grep -c MISMATCH $R/build/server_out.txt)"
