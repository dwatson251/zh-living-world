#!/bin/bash
# local_build.sh: rebuild the three programs and refresh build/local/LivingWorld, the folder to playtest from before tagging.
# build/ is already excluded from Defender on the dev machine, and your um_server.ini and world/ in that folder are kept between refreshes.
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"; D=$R/build/local/LivingWorld; B=$R/build/cl
timeout 3300 ~/.local/bin/ninja -C "$B" zh_client zh_launcher zh_host | tail -2
mkdir -p "$D"
cp "$B/zh_client.exe" "$B/zh_launcher.exe" "$B/zh_host.exe" "$D/"
[ -f "$D/um_server.ini" ] || cp "$R/host/um_server.ini" "$D/um_server.ini"
cp "$R/dist/README.txt" "$D/README.txt"
{ echo "Living World local build"; echo "commit $(git -C "$R" rev-parse --short HEAD)$(git -C "$R" diff --quiet HEAD -- . && echo '' || echo ' + uncommitted changes')"; date; } > "$D/version.txt"
sed -i 's/$/\r/' "$D/README.txt" "$D/version.txt"
echo "Playtest folder: $(wslpath -w "$D")"
