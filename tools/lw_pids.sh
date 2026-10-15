#!/bin/bash
# lw_pids.sh server|client: print the PID of the lab (build\cl) server or client process (server: -xres 800, client: anything else)
if [ "$1" = server ]; then F="-like '*xres 800*'"; else F="-notlike '*xres 800*'"; fi
powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='zh_client.exe'\" | Where-Object { \$_.ExecutablePath -like '*zh-living-world\\build\\cl\\*' -and \$_.CommandLine $F } | ForEach-Object { \$_.ProcessId }" | tr -d '\r' | head -1
