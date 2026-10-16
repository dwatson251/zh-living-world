#!/bin/bash
# spectate_test.sh: one human slot. alice takes it and leaves; carol arrives while it is full (no spectate flag) and must watch, then ask for the queue
# (UM_TEST_JOIN_FRAME) and be promoted when alice's absent base expires; dave asks to spectate and must stay unqueued the whole time.
R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
mkdir -p $R/build/cm/client; for n in "" 2 3; do cp -f $R/build/cm/zh_headless.exe $R/build/cm/client/zh_client$n.exe; done
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_UNCAPPED:UM_SERVER:UM_CLIENT:UM_HUMAN_SLOTS:UM_BOT:UM_KEY:UM_ABSENT_FRAMES:UM_COMBAT_FRAMES:UM_RENT_FRAMES:UM_SPECTATE:UM_TEST_JOIN_FRAME:UM_STAY_AFTER_KICK"
cd "$GAME"
( export UM_HUMAN_SLOTS=1 UM_SERVER=7777 UM_UNCAPPED=1 UM_ABSENT_FRAMES=600 UM_COMBAT_FRAMES=100 UM_RENT_FRAMES=1000000 UM_SYNTH_STARTS=5 UM_SKIRMISH_MAP='Maps\Tournament Continent\Tournament Continent.map' UM_SKIRMISH_AI=4
  timeout 140 $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/server_out.txt 2>&1 ) &
sleep 20
( export UM_BOT=1 UM_KEY=alice UM_CLIENT=127.0.0.1:7777; timeout 25 $R/build/cm/client/zh_client.exe -noshellmap -nomusic -noaudio > $R/build/client_out.txt 2>&1 ) &
sleep 6
( export UM_KEY=carol UM_CLIENT=127.0.0.1:7777 UM_TEST_JOIN_FRAME=300 UM_STAY_AFTER_KICK=1; timeout 100 $R/build/cm/client/zh_client2.exe -noshellmap -nomusic -noaudio > $R/build/client2_out.txt 2>&1 ) &
( export UM_KEY=dave UM_SPECTATE=1 UM_CLIENT=127.0.0.1:7777 UM_STAY_AFTER_KICK=1; timeout 100 $R/build/cm/client/zh_client3.exe -noshellmap -nomusic -noaudio > $R/build/client3_out.txt 2>&1 ) &
wait
echo "--- server"; grep -E "^\[um\] server: (client|slot|absent)" $R/build/server_out.txt | cut -c1-160
echo "--- carol"; grep -E "^\[um\] client: (welcome|asked|KICKED)" $R/build/client2_out.txt
echo "--- dave"; grep -E "^\[um\] client: (welcome|asked|KICKED)" $R/build/client3_out.txt
echo "mismatches: $(grep -c MISMATCH $R/build/server_out.txt)"
