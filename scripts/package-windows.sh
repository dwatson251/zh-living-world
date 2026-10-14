#!/bin/bash
# Build the release zip: build/dist/LivingWorld-<version>-win-x86.zip (version from LW_VERSION, the release tag). Run scripts/build.sh first.
# players point the launcher at their own Zero Hour install. Expects build/cl/{zh_client,zh_launcher,zh_host}.exe (cmake -DUM_CLIENT_BUILD=ON).
set -euo pipefail
V=${LW_VERSION:-${1:-dev}}; R="$(cd "$(dirname "$0")/.." && pwd)"; D=$R/build/dist/LivingWorld; B=$R/build/cl
rm -rf "$R/build/dist"; mkdir -p "$D"
cp "$B/zh_client.exe" "$B/zh_launcher.exe" "$B/zh_host.exe" "$D/"
cp "$R/host/um_server.ini" "$D/um_server.ini"
cp "$R/dist/README.txt" "$D/README.txt"
{ echo "Living World $V"; echo "commit $(git -C "$R" rev-parse --short HEAD 2>/dev/null || echo unknown)"; echo "EA source $(cat "$R/EA_SOURCE_SHA" 2>/dev/null || echo unknown)"; } > "$D/version.txt"
sed -i 's/$/\r/' "$D/README.txt" "$D/version.txt"
(cd "$R/build/dist" && python3 -c "import shutil; shutil.make_archive('LivingWorld-$V-win-x86','zip','.','LivingWorld')")
ls -la "$R/build/dist/LivingWorld-$V-win-x86.zip"
