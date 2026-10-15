#!/bin/bash
# Living World server container entrypoint: checks the Zero Hour files the admin mounted, builds a private working folder of links to them
# (the engine writes logs into its working folder; the mount can stay read-only), maps LW_* settings, and runs zh_host.exe --cli under Wine.
set -euo pipefail
LWHOME=${LW_HOME:-/opt/livingworld}
GAME=${ZH_GAME_DIR:-/data/zerohour}; WORLD=${ZH_WORLD_DIR:-/data/world}; RUN=${ZH_RUN_DIR:-/tmp/zh-run}
say() { echo "[entrypoint] $*"; }

if [ ! -d "$GAME" ]; then say "ERROR: $GAME is not mounted. Mount your own Command & Conquer Generals Zero Hour install folder there (see docker/README.md)."; exit 2; fi
missing=""
for f in INIZH.big MapsZH.big W3DZH.big TerrainZH.big EnglishZH.big; do
	[ -n "$(find "$GAME" -maxdepth 1 -iname "$f" -print -quit)" ] || missing="$missing $f"
done
if [ -n "$missing" ]; then
	say "ERROR: $GAME does not look like a Zero Hour install. Missing:$missing"
	say "This server does not ship any game files: mount the folder of a Zero Hour copy you own at $GAME."
	exit 2
fi
mkdir -p "$WORLD" 2>/dev/null || true
if ! touch "$WORLD/.write-test" 2>/dev/null; then say "ERROR: $WORLD is not writable by uid $(id -u). Fix the volume owner (chown $(id -u) <dir>) or use a named volume."; exit 2; fi
rm -f "$WORLD/.write-test"

rm -rf "$RUN"; mkdir -p "$RUN"
shopt -s dotglob nullglob
for e in "$GAME"/*; do ln -s "$e" "$RUN/$(basename "$e")"; done
shopt -u dotglob nullglob

export LW_GAME_DIR='Z:'"$(echo "$RUN" | tr / '\\')" LW_WORLD_DIR='Z:'"$(echo "$WORLD" | tr / '\\')"
export WINEPREFIX=${WINEPREFIX:-/data/world/.wineprefix} WINEDEBUG=${WINEDEBUG:--all}
unset DISPLAY WAYLAND_DISPLAY
# the Wine prefix lives in the world volume and is created on first start (about a minute); a half-made one is thrown away
if [ ! -f "$WINEPREFIX/.lw-ready" ]; then
	say "creating the Wine prefix (first start only); host kernel $(uname -r), $(id -un) uid $(id -u), HOME=${HOME:-unset}"
	rm -rf "$WINEPREFIX"
	WLOG=$(mktemp)
	if WINEDEBUG=err+all timeout 300 wineboot -i >"$WLOG" 2>&1; then rc=0; else rc=$?; fi
	timeout 60 wineserver -w >>"$WLOG" 2>&1 || true
	if [ $rc -eq 0 ] && [ -f "$WINEPREFIX/system.reg" ]; then touch "$WINEPREFIX/.lw-ready"
	else
		say "ERROR: could not create the Wine prefix (wineboot exit code $rc; 124 = timed out). Wine output:"; tail -n 40 "$WLOG" | sed 's/^/[wine] /'
		say "free space in $WORLD: $(df -h "$WORLD" | tail -1)"; exit 3
	fi
fi
cd "$LWHOME"

args=(--cli)
[ -f /data/config/um_server.ini ] && args+=(--config 'Z:\data\config\um_server.ini')

: > "$WORLD/host_log.txt"; : > "$WORLD/server_log.txt" 2>/dev/null || true
tail -n +1 -F "$WORLD/host_log.txt" "$WORLD/server_log.txt" 2>/dev/null &
TAILPID=$!
say "starting Living World under Wine (game files: $GAME, world: $WORLD)"
wine "$LWHOME/zh_host.exe" "${args[@]}" "$@" > /dev/null 2>&1 &
WPID=$!
trap 'say "stopping"; kill $WPID 2>/dev/null; wineserver -k 2>/dev/null; kill $TAILPID 2>/dev/null; exit 0' TERM INT
wait $WPID || true
say "host exited"; kill $TAILPID 2>/dev/null; exit 1
