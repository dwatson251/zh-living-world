#!/bin/bash
# lw_killall.sh: stop every lab (build\cl) zh_client.exe process, by exact PID. Never touches the user's own copies elsewhere.
for p in $(powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='zh_client.exe'\" | Where-Object { \$_.ExecutablePath -like '*zh-living-world\\build\\cl\\*' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'); do /home/dnlwtsn/Code/universal-modder/bin/um win kill $p > /dev/null; done
sleep 3
