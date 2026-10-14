#!/bin/bash
# run.sh [seconds] [args...]: run the headless exe from the Zero Hour install dir, capture stdout/stderr + engine log tail.
export WSLENV="UM_SKIRMISH_MAP:UM_SKIRMISH_AI:UM_SYNTH_STARTS:UM_FRAMES_MAX:UM_UNCAPPED:UM_SERVER:UM_CLIENT:UM_SEED:UM_DEEPCRC_FRAME:UM_SAVE_AT:UM_LOADSAVE:UM_WATCHDOG"
SEC=${1:-120}; shift
GAME="/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour"
OUT=/home/dnlwtsn/Code/zh-living-world/build/last_run.txt
cd "$GAME" && timeout $SEC /home/dnlwtsn/Code/zh-living-world/build/cm/zh_headless.exe "$@" > $OUT 2>&1; echo "exit=$?" >> $OUT
tail -${TAILN:-8} $OUT | cut -c1-240; echo "--- engine log"; tail -${TAILN:-8} /home/dnlwtsn/Code/zh-living-world/build/cm/DebugLogFile.txt | cut -c1-240
