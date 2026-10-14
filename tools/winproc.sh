#!/bin/bash
# winproc.sh <exe (WSL path)> <workdir (WSL path)> <logname> [ENV=VAL ...] -- [args...]
# Starts a Windows process detached (stdout/stderr to build/<logname>.out/.err), environment from the ENV=VAL words (passed through WSLENV); prints its PID.
R=/home/dnlwtsn/Code/zh-living-world
exe=$(wslpath -w "$1"); wd=$(wslpath -w "$2"); name=$3; shift 3
names=""
while [ "$1" != "--" ] && [ -n "$1" ]; do k=${1%%=*}; export "$k=${1#*=}"; names="$names:$k"; shift; done; shift
export WSLENV="${names#:}"
args="$*"; pidf=$(mktemp); pidw=$(wslpath -w "$pidf")
powershell.exe -NoProfile -Command "\$p = Start-Process -FilePath '$exe' -WorkingDirectory '$wd' -ArgumentList '$args' -RedirectStandardOutput '$(wslpath -w $R/build/$name.out)' -RedirectStandardError '$(wslpath -w $R/build/$name.err)' -PassThru; \$p.Id | Out-File -Encoding ascii '$pidw'" > /dev/null 2>&1 < /dev/null
tr -d '\r\n ' < "$pidf"; echo; rm -f "$pidf"
