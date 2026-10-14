#!/bin/bash
# Build zh_client.exe (the game client and, with UM_SERVER set, the dedicated server), zh_launcher.exe and zh_host.exe into build/cl. Run scripts/setup.sh first.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
. "$here/build/env.sh"; export LLVM_MINGW PATH="$HOME/.local/bin:$PATH"
cmake -S "$here" -B "$here/build/cl" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$here/toolchain-mingw32.cmake" -DUM_CLIENT_BUILD=ON
ninja -C "$here/build/cl" zh_client zh_launcher zh_host
ls -la "$here/build/cl/"*.exe
