#!/bin/bash
# client_test.sh [serverSecs] [clientSecs]: headless server on a small map + one graphical client; logs in build/. Screenshot via `um win shot`.
SS=${1:-120}; CS=${2:-60}
R=/home/dnlwtsn/Code/zh-living-world; GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_UNCAPPED:UM_SERVER:UM_CLIENT:UM_HUMAN_SLOTS:UM_AI_POOL:UM_AI_MAX:UM_NAME:UM_FACTION:UM_COLOR:UM_KEY:UM_BOT"
cd "$GAME"
( export UM_HUMAN_SLOTS=4 UM_AI_POOL=0 UM_SERVER=7790 UM_SYNTH_STARTS=${SYNTH:-8} UM_SKIRMISH_MAP='Maps\Tournament Desert\Tournament Desert.map' UM_SKIRMISH_AI=${AI:-4}
  timeout $SS $R/build/cm/zh_headless.exe -noshellmap -nomusic -noaudio > $R/build/server_out.txt 2>&1 ) &
sleep 15
( export UM_CLIENT=127.0.0.1:7790 UM_NAME=${NAME:-Tester} UM_FACTION=${FACTION:-FactionChina} UM_COLOR=${COLOR:-ff8800} UM_KEY=${KEY:-tester-key-0001}
  timeout $CS $R/build/cl/zh_client.exe -win -xres 1024 -yres 768 -nosound > $R/build/zhclient_out.txt 2>&1 ) &
wait
