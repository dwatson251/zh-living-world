#!/bin/bash
# return_test.sh: alice joins, leaves, rejoins within the absence window and must get the same slot and base back.
R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
mkdir -p $R/build/cm/client && cp -f $R/build/cm/zh_headless.exe $R/build/cm/client/zh_client.exe
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_UNCAPPED:UM_SERVER:UM_CLIENT:UM_HUMAN_SLOTS:UM_BOT:UM_KEY:UM_ABSENT_FRAMES:UM_COMBAT_FRAMES:UM_RENT_FRAMES:UM_IMMUNE_FRAMES"
cd "$GAME"
( export UM_HUMAN_SLOTS=2 UM_SERVER=7777 UM_UNCAPPED=1 UM_ABSENT_FRAMES=100000 UM_RENT_FRAMES=1000000 UM_IMMUNE_FRAMES=10000000 UM_SYNTH_STARTS=4 UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=2
  timeout 110 $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/server_out.txt 2>&1 ) &
sleep 15
( export UM_BOT=1 UM_KEY=alice UM_CLIENT=127.0.0.1:7777; timeout 15 $R/build/cm/client/zh_client.exe -noshellmap -nomusic -noaudio > $R/build/client_out.txt 2>&1 )
sleep 5
( export UM_BOT=1 UM_KEY=alice UM_CLIENT=127.0.0.1:7777; timeout 25 $R/build/cm/client/zh_client.exe -noshellmap -nomusic -noaudio > $R/build/client2_out.txt 2>&1 )
wait
grep -E "^\[um\] server: (client|slot|alice|absent)" $R/build/server_out.txt | cut -c1-160
echo "mismatches: $(grep -c MISMATCH $R/build/server_out.txt)  client2 frames: $(grep -c '^\[um\] frame' $R/build/client2_out.txt)"
