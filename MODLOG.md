# MODLOG: Zero Hour living world

## Route (decided 2026-10-13)
Engine-level mod of EA's released source (/home/dnlwtsn/Code/CnC_Generals_Zero_Hour, untouched; work in a copy here).
Server-managed state is the system of record (no witness/anchor). Widen player masks first, then headless build, server, late join.
Data: /mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour (read-only, never shipped).

## Recon
- MAX_PLAYER_COUNT=16, PlayerMaskType=UnsignedShort, net MAX_SLOTS=8; P2P lockstep, no headless mode.
- No clang/mingw/cmake in WSL.

## Lab (2026-10-13)
- No docker/sudo. Portable toolchain, nothing system-wide: ~/tools/llvm-mingw-20261006 (clang 23, i686/x86_64 mingw, msvcrt), cmake+ninja via `uv tool`.
- Verified: clang++ -> i686 Windows exe runs from WSL.
- Source copied to src-zh/ (GeneralsMD only; Generals base dropped for now). Original clone untouched.

## Phase 0 progress: syntax-clean porting (2026-10-13)
Method: `tools/syntax.sh <dir>` clang -fsyntax-only (i686 mingw, gnu++98, -fms-extensions -fms-compatibility -U_MSC_VER -D__GNUC__=4 -fasm-blocks,
force-included shim/compat.h), `tools/autoshim.sh` resolves case-mismatched/missing includes into shim/, `tools/fixfor.py` hoists VC6 leaked for-vars.
Patch set vs original: `diff -ru` of CnC_Generals_Zero_Hour/GeneralsMD vs src-zh (src-zh not committed here).
Fix families found: (1) forward-declared enums -> `: int` on fwd decl + definition (63 enums); (2) DLINK macro needs `&Class::method`;
(3) `template<>` on BitFlags::s_bitNameList specialisations; (4) VC6 for-scope leaks; (5) STLport vector iterators were raw pointers
(libc++ wrap_iter: use &*it); (6) functions used before declaration; (7) GameSpy/udp.h need replacement (network phase).
MSVC inline asm (BaseType.h float->int) kept via -fasm-blocks to preserve exact rounding (determinism).
Result: GameLogic 249/259 files parse. Remaining: iterator-as-pointer sites, GameSpy headers, a few const-char issues.

## Phase 0 status after GameLogic/Common/GameClient/GameNetwork parse passes (2026-10-13)
`tools/syntax.sh GameEngine/Source/<Module>` (parse only, no link). ok/total: GameLogic 258/259, Common 122/132, GameClient 122/147, GameNetwork 32/47.
Remaining failures are almost all things a headless dedicated build drops: GameSpy (Peer/GP/ghttp stubs in shim/GameSpy), WOLBrowser (ATL COM; FEBDispatch.h replaced by a placeholder),
WWDebug profile.h, plus a few real fixes (UserPreferences for-scope, GameState const char*, GameMemory sizeof, ProcessAnimateWindow class tag).
New flags: -DWIN32 -D_WINDOWS -DNDEBUG -D_MBCS -DZ_PREFIX (Release config from GameEngine.dsp); `typedef unsigned short wchar_t` in shim/compat.h (VC6 semantics).
Replaced: WWLib/mutex.h inline-asm spinlock -> __sync_lock_test_and_set; D3DX8Math.h -> shim/D3DX8Math.h (Vec4/Matrix only, for Bezier).
Core network layer (Network, ConnectionManager, FrameData*, Transport, DisconnectManager, NetPacket, Connection, udp) parses clean.
Findings for the headless design: the sim needs three device-layer classes: GameEngineDevice/Source/W3DDevice/GameLogic/{W3DTerrainLogic,W3DGameLogic,W3DGhostObject}.cpp
(heightmap load via WorldHeightMap in W3DDevice/GameClient). Those must be kept or reimplemented in a Null device; everything else under W3DDevice/Miles/Bink can be stubbed.
Missing third-party sources (empty dirs in the EA drop): zlib 1.1.4, LZH-Light (CompLibSource), GameSpy, Miles, Bink, DirectX. Next: zlib+LZH replacements for .big/.ref decompression, device layer, link.

## Phase 0: engine compiles and nearly links (2026-10-13)
Build: `cmake -S . -B build/cm -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw32.cmake && ninja -C build/cm zh_headless` (llvm-mingw clang, i686, gnu++98 + ms extensions).
Source lists come from the original VC6 .dsp files (tools/gensources.py -> dsp_sources.cmake) so we build what the game built.
Case-insensitive includes: clang VFS overlay (tools/genvfs.py -> build/vfs.yaml, mounted at /vroot); regenerate after adding files.
All ~700 objects compile (GameEngine Common/GameLogic/GameClient/GameNetwork minus GameSpy+WOL, WWLib/WWMath/WWDebug/WWSaveLoad/Wwutil/WWDownload, zlib 1.3.1,
WorldHeightMap+TileData from the W3D device). Replaced: RefPack codec (src/um_refpack.cpp, decoder follows the public format; encoder = literal-only),
NOX/LZH codec (stub: fails), D3DX math, GameSpy SDK headers (shim/GameSpy), embedded browser (inert), Except.cpp thread-id hooks.
Undefined at link: only CreateCDManager, CreateGameEngine, WinMain. Next: headless GameEngine subclass (Null client/display/audio/input, W3D draw-module stand-ins), then run INI/BIG load.
Gotchas worth a field note: StringClass passed through printf varargs (VC6 tolerated; clang aborts), explicit-specialisation static members need template<>,
wchar_t must be unsigned short (VC6) -> typedef in compat.h and explicit basic_string<unsigned short> instantiation, DLINK macros need &Class::member,
vector iterators were raw pointers under STLport, AnimateWindow clashes with winuser.h.

## Phase 0 EXIT: headless sim runs (2026-10-13)
`build/cm/zh_headless.exe`, run with cwd = the Zero Hour install (tools/run.sh, tools/cycle.sh): loads all INI + BIG data (ZH and base Generals via registry),
boots the shell map (305 objects, AI tanks/aircraft fighting) and steps the logic at 30 fps, printing `[um] frame N objects M crc XXXXXXXX` every 30 frames.
Two independent runs gave identical CRC sequences over 1620 frames (deterministic run-to-run). Engine log: build/cm/DebugLogFile.txt (beside the exe, not in the install).
Headless client = src/headless_engine.cpp + generated Null* classes (tools/gennull.py via clang JSON AST) + 19 W3D draw-module stand-ins (src/null_draw.cpp: parse-and-discard INI blocks).
Terrain heights/LOS/cliff come from src/um/um_terrain.* (copied from BaseHeightMap.cpp); bridges NOT loaded yet.

### Bugs found that any libc++/clang port of this code base will hit (worth a KB note)
- STLport `hash<const char*>` hashes characters; libc++'s hashes the pointer -> AsciiString hash_map lookups failed (ThingFactory). Fixed in STLTypedefs.h (h=5h+c).
- `while(*it)` over a std::list relying on the sentinel node holding NULL (Contain classes): UB under libc++. Fixed to compare with end().
- Global operator new/delete link self-test expects 6 calls (STLport/VC6 chaining); libc++ gives 4.
- Dead debug-only code hides latent bugs: with DEBUG_CRASH enabled, StringClass/AsciiString passed through `...` aborts at runtime under clang.
- Engine writes ReleaseCrashInfo.txt + DebugLogFile.txt next to the exe / in Documents\...Data; back up user data before runs (backups/).
- ERROR_BAD_INI etc. are plain enum throws; wrap __cxa_throw (-Wl,--wrap) + _Unwind_Backtrace + llvm-symbolizer (-gline-tables-only) gives line-accurate traces without a debugger.

### Known gaps / TODO before the sim can match real clients
- ObjectDrawInterface (bones, barrels, launch offsets, animation completion) is stubbed: "ProjectileLaunchPos not found" asserts. 31 GameLogic call sites depend on it.
  Real fix = a W3D bone/HLOD/HAnim reader feeding the same ModelConditionInfo logic as W3DModelDraw.cpp, using the same WWMath code for bit-identical floats.
- Bridges (W3DBridgeBuffer::loadBridges), roads, trees/props (logic-visible via terrain blocking?) not loaded.
- setFPMode(): INI::load sets x87 precision for determinism; confirm it stays in effect on the headless thread model.
- Frame limiter: runs at real-time 30 fps; add an uncapped mode for soak/catch-up.

## Phase 1: player cap widened (2026-10-13/14)
- `UM_MAX_PLAYERS` (CMake, default 16 in source) now drives MAX_PLAYER_COUNT; `PlayerMaskType` is a multiword class (GameCommon.h) with safe-bool and legacy `== 0` spellings.
  Network `MAX_PLAYER` scales (slots = UM_MAX_PLAYERS-8; 120 at 128). Script SIDE parameters cache player index+1 instead of a 16-bit mask.
  MultiplayerSettings::extendColors() pads the 8-colour palette (golden-angle hues); colour assignment loop guarded.
- Regression: at cap 16 and at cap 128 the shell map reproduces the original object counts for 1500 frames.
- Stress scenario (headless): `UM_SKIRMISH_MAP`, `UM_SKIRMISH_AI=N`, `UM_SYNTH_STARTS=N`, `UM_UNCAPPED=1` (env forwarded via WSLENV); one observer + N medium AI on Tournament Continent
  (map defines 4 starts; the rest are synthesised on a grid). tools/scen.sh, tools/soak.sh.
- Results (early game, 100-150 s runs, uncapped, cap 128): 8 AI ~30 fps cap, 16 AI ~97 logic fps / 138 MB, 32 AI 47 fps / 143 MB, 64 AI 23 fps / 156 MB, 100 AI 14 fps / 174 MB. No crashes.
  64 AI run twice: identical CRC for 52 samples (1560 frames).
- Observations: the sim cost scales superlinearly with AI count even before bases grow (AI think + pathfinding + shroud); already below 30 fps at 64 AI in the first 1-2 minutes.
  That makes the AI governor/LOD and partition-manager costs the first scale targets (profile with -pg or sampling next).
- Headless fixes needed for the skirmish path: load screens disabled (GameLogic::getLoadScreen), ControlBar::populateObserverList skipped, fonts/windows/strings are dummy objects.

## Phase 3: server, late join, canonical epochs (2026-10-14)
- `src/net/um_net.*`: server-paced lockstep over TCP. The server runs the authoritative headless sim, owns command order (per-frame batches), broadcasts
  FRAME/ADVANCE, compares replica CRCs against its own (`crc-checked` / `MISMATCH`). Clients never run ahead of the server's ADVANCE limit. Own message
  serialisation (stock NetPacket uses 8-bit player masks). Locally generated network-range messages are intercepted like Network::GetCommandsFromCommandList
  (they otherwise execute on one side only and create AI groups -> desync).
- World-affecting settings travel with the WORLD message (UmWorldDesc: map, seed, synthStarts, slots); env-only settings on one side caused the first desyncs.
- Late join by replay: a replica joining at frame 5484 replayed from frame 0 and stayed in CRC sync for 5800+ frames.
- Snapshots: Zero Hour's own save format (7-25 MB raw, 0.8-2.4 MB zlib at 16 AI over 9000 frames). Restore is deterministic across processes but NOT identical to the
  uninterrupted run, so the plan's canonical epoch is used: server saves, broadcasts, and reloads its own snapshot; live clients load it at the same frame; joiners get
  the latest snapshot plus the command log since. Cost today: save+reload ~5-7 s per epoch at 16 AI (to be optimised; epochs are rare).
- Things that made restore non-deterministic/unfaithful (all fixed): logic RNG state was not saved; object list order reversed on load (registerObject prepends);
  XferSTL*List "must be empty" throws; recycled heap blocks leaking stale bytes between A-process-reload and fresh-process load (allocations now poisoned with UM_FILL, default 0).
- EA bug found because libc++ caches list::size(): ResourceGatheringManager::findBestSupplyCenter erased from m_supplyWarehouses instead of m_supplyCenters. STLport's counting
  size() hid it; with libc++ the save wrote count != nodes and the load threw XferVersion errors. Any libc++/MSVC-STL port of this codebase will hit it.
- Debug tooling added: UM_DEEPCRC_FRAME (byte-level world dump), UM_XFERTRACE/UM_XFERTRACE2 (snapshot-entry and primitive-level save/load traces), UM_WATCHDOG (hang location),
  UM_CRC_EVERY/FROM, tools/net_test.sh, snap_test.sh, fill_test.sh.

## Persistence, human slots and world rules
- `UM_WORLD_DIR`: the server writes `epoch.sav`, `world.bin`, `cmds.log`, `frame.txt`, `players.txt`, `status.json` atomically (tmp + rename). `UM_RESUME=1` restores the
  snapshot, re-simulates uncapped to the frame mark and replays the logged batches. `tools/persist_test.sh 16`: kill/restart, identical CRCs over the overlap.
- Seats: reserved SLOT_PLAYER slots named `~free` (humans) and SLOT_MED_AI slots named `~ai` (AI pool) are inert at start (no base, scripts not run; `Player::m_umPending`,
  saved in Player::xfer v9). Server-only network messages BIND/UNBIND/GRANT_CASH/CHARGE/RENT_DAMAGE; the server re-stamps every client command with the sender's player
  index and filters the rest (admitCommand).
- Late-joining AI only builds if its skirmish scripts start fresh: the skirmish AI is driven entirely by per-player scripts (`AutomaticallyBuild` is false for every entry),
  and scripts that ran while the slot was inert were one-shot and spent. Bind calls `Player::umResetScripts` (copy active flags from a fresh duplicateAndQualify) then newMap.
  Random factions must be America/China/GLA: the Supreme/Boss factions have no skirmish side, so Player::init turns them into humans.
- Server records `{key, slot, bound, left, lastCombat, nextRent, missed, expiring}` per human, persisted. Absent players keep their base until
  max(left + 30 min, lastCombat + 30 s), then the server self-destructs it (no last-hit bonus). Rent is server-computed and executed as explicit commands.
  Verified by tools: net_test (elimination + bonus), queue_test (second joiner promoted when a seat frees), return_test (same key gets slot back), rent test (unaffordable
  rent stacks and damages the base), absence expiry test.
- Host: `host/um_host.py` + `um_server.ini` start the server, restart it from the world on exit, and serve a status page from `status.json` (no identity keys in it).
- EA/porting pitfalls added: `AISkirmishPlayer::adjustBuildList` reads `list->` not `cur->` in its transform loop (harmless as the command center heads the list).

## Graphical client (October 14)
- The retail W3D/Win32 stack builds with clang (client.cmake, -DUM_CLIENT_BUILD=ON): WW3D2, W3DDevice, Win32Device, GameClient menus; stubs for Miles audio, Bink video, the
  embedded browser and the bump-shader assembler; a small D3DX8 replacement (shim/d3dx8.h, src/d3dx8_impl.cpp); the retail WinMain copy (src/client/winmain_um.cpp) keeps the
  launcher's working directory instead of forcing the exe's folder.
- Server and client are the same program. Server mode (UM_SERVER=port) hides the window, uses a null Direct3D 8 (src/client/d3dnull, generated base classes via tools/gend3d.py)
  and skips drawing, but still runs W3D model/animation code, because the game logic reads bone positions from render objects. Standalone comparison runs (null D3D vs real D3D)
  give identical deep CRCs; networked client and server stay in sync for thousands of frames, including a client joining from an epoch snapshot.
- Desync causes found: shell-map leftovers (the client must start with -noshellmap), and the retail override-chain copy bug in weather/water INI loading (a copy inherited the
  base's chain and made a cycle -> infinite recursion). Other porting fixes: inline-asm CPU detection replaced by intrinsics, dx8wrapper colour conversion asm replaced by C,
  VC6 for-scope leaks, StringClass passed through varargs, DEBUG_CRASH helper variables outside debug builds.
- Player choices: HELLO carries display name, faction and RGB colour; the server validates the faction (America/China/GLA sides with a starting building), writes it into the
  world description (as of the last epoch) and sends it in the bind command; Player::umApplyProfile applies it everywhere. The launcher (src/launcher) is a plain Win32 dialog
  with a ChooseColor colour picker. UI: the seat holder becomes the local player (control bar, shroud, camera on the base); everyone else is the observer.
- Environment: Windows Defender's cloud heuristics began terminating zh_client.exe (Trojan:Win32/Zenpak.BAB!MTB, false positive) at about 14:00; testing on this machine stopped
  there, because allowing or excluding it is the owner's decision.

## Release round (October 14)
- Cursor: the retail code only made the mouse visible when a load screen existed; the client has none, so the cursor was never shown. The client now shows it once in game.
- Fullscreen: exclusive mode minimises a window that is not foreground, and the retail loop then waits forever ("splash screen, nothing renders"). Fixed (the minimised client keeps
  following the server and restores itself; the launcher allows foreground), plus borderless fullscreen as the default mode. The exe is linked at the fixed base 0x400000 (a d3d8 wrapper in
  the game folder reads the exe image at its preferred base and crashed under ASLR).
- Rent: payable only with a command center; unpaid stacks damage (1% of max health per stack by default) in tiers: attack-capable units, buildings except command centers,
  dozers/workers and other non-attackers, command centers last. Knock-out bonus: percent of the victim's cash (seen before the engine clears it) plus a flat amount, within a configurable
  last-hit window (default 5 minutes). Supply docks can restock slowly (`supply_cash`, `supply_interval_secs`).
- `zh_host.exe`: one program for hosting: window (settings, Start/Stop, log, players, uptime) or `--cli`; restarts the server from its saved world; status page.
- Scale: 56 AI + 4 seats on Tournament Continent ran at roughly 15-20 logic frames per second on the development machine (WSL2 host, early game); 32 AI held 30.
- Build from scratch: `scripts/setup.sh && scripts/build.sh && scripts/package-windows.sh` (EA source pinned in EA_SOURCE_SHA + `patches/zh-living-world.patch`).
- Start spots: the invented start waypoints (large worlds on maps that define few starts) now avoid water, cliffs and steep ground with a buffer ring, stay apart from each other and
  away from the map edge; the buffer and spacing relax in steps when a map has little good ground. Server and clients compute them identically.

## 0.0.1-alpha.3: desync after the 7th AI joined, bare zh_client.exe
- Windows release reports: client sat in a game that diverged (CRC DIVERGED from frame 3630) and the retail main menu appeared when zh_client.exe was started by hand (its buttons, e.g. Options, crash).
- Desync cause (reproduced in WSL with 45 slots, AI every 600 frames, epoch 9000): invented start spots were chosen inside `TerrainLogic::loadMap`, before `TerrainVisual::load` created the render
  heightmap that `W3DTerrainLogic::getGroundHeight` reads, so every height was 0 and the "dry, flat ground" search saw a flat map. The server then re-ran it after its epoch save+reload (heights now real) and
  disagreed with the client for spots that depended on terrain (first one: slot 27). Fix: choose the spots after the visual load (and drop `isCliffCell`, also render-side). 0 mismatches past frame 6600.
- A bare zh_client.exe (no UM_CLIENT / UM_SERVER / UM_STANDALONE) now opens zh_launcher.exe next to it and exits.
- Debug aids: UM_DEEPCRC_FRAME also writes deepobjs_<role>_<frame>.txt (id, template, owner, position); bind lines print the start waypoint used.

## Windows report after alpha.3 (crash to desktop, main menu on 2nd launch, exclusive crash)
- Cause found in the Defender operational log (events 1116/1117): Trojan:Win32/Zenpak.BAB!MTB ("MTB" = machine-learning), raised ~1 s after each zh_client.exe start and remediated (process terminated, exit code 0) ~12 s later;
  they line up exactly with the host's "server exited (code 0x0); restarting" loop and with the client vanishing. The lab build folder is excluded, the downloaded folder is not. The exe is not malicious (built from source in CI);
  fix is an allow/exclusion for the folder by the user, a false-positive submission to Microsoft, later code signing. No evasion tricks.
- The "second launch shows the main menu" case: the client could not connect (server being killed and restarting), and the engine fell back to its retail main menu. The client now shows a "Could not connect" box and exits.
- Exclusive fullscreen on the lab machine: one transient DEVICELOST on Present, recovers and plays.

## 0.0.1-alpha.4: the main menu stayed on top of the game
- With Defender out of the way the user still saw the retail main menu with dead buttons. Reproduced by starting the host and the client within seconds of each other at 1920x1080: the client joined
  ("now playing as a seat holder", frames advancing, 0 mismatches) while the engine's own main menu was still on screen. The retail path that hides the shell (`MSG_NEW_GAME` -> `prepareNewGame`) never runs for a joined world.
  Fast joins in earlier tests happened before the menu was pushed, so it never showed. Fix: `umClientTick` hides the shell as soon as the world is running. Verified by screenshot (base, control bar, $20000).

## Game speed, catch-up message, diagnostics (post alpha.4)
- `game_speed` (15-60, default 30; `--game-speed`, `UM_GAME_SPEED`) is a world setting: the server paces its logic at that rate and sends it to clients in the world description (UM_VERSION 3), the clients set their frame limiter from it. Measured 15/30/60 fps on the server; a client follows a speed-60 server in sync.
- Persistence check with the real client: build a dozer (money 19000, 3 objects), kill the client, server logs "absent (slot 5 keeps its base)", rejoin with the same key: "returning player", slot 5, 19000 / 3 objects after the catch-up. Headless return_test and queue_test (knock-out -> queue -> fresh base) pass.
- Real UI control verified with posted mouse messages (tools/winclick.ps1): select the command center, build button queues a Construction Dozer ($1000). Could not reproduce "AI controls my base"; the client now logs the local player (index, name, type, money, object count) every 3000 frames, and shows "Joining the world: catching up (n%)" while replaying.
- lw_run.sh used to kill every zh_client.exe by name, which also killed the user's own playtest; it now kills only build\cl processes.

## Linux / Docker round (2026-10-15, unattended)
- Added: `LW_<SETTING>` environment overrides in `src/host/host.cpp` (between `um_server.ini` and command-line flags), `public/docker/{Dockerfile,entrypoint.sh,healthcheck.sh,README.md}`,
  `public/docker-compose.yml`, a `docker-image` job in `public/.github/workflows/release.yml`, `docs/linux-native-findings.md`. Wine runs the unchanged Windows server programs; no game data in the image.
- **Nothing was committed, tagged or pushed.** The lab tree has other uncommitted engine work being edited and rebuilt at the same time (see findings), and the large-world cross-platform check is unresolved.

### Assumptions for review
1. Linux = Wine-in-Docker now; native Linux server deferred (no 32-bit native fork exists; findings doc says why). You chose "native" earlier, I assumed the spike's own fallback rule applies.
2. Game files: admin mounts their install at `/data/zerohour` (read-only); the entrypoint links it into a private run folder. Required files checked: INIZH, MapsZH, W3DZH, TerrainZH, EnglishZH `.big`.
3. Image published to `ghcr.io/dwatson251/zh-living-world` (tags: version, and `latest` for non-pre-release tags only, so Portainer's default `latest` will not exist until a non-alpha tag; set `LW_TAG=0.0.1-alpha.N`). Package must be made public once in GitHub settings.
4. Wine 11.19 wow64 build pinned by SHA-256; Debian bookworm-slim base; container runs as uid 1000.
5. I did not tag `0.0.1-alpha.5`: unverified Docker image, unresolved large-world CRC divergence, and someone else was modifying the same tree/game folder during the run.
6. Test side effects: my client wrote `um_winecheck_log.txt`, `um_ctl_*.txt` and `deep*_30.*` files into the Zero Hour folder (safe to delete); the Wine prefixes are `~/.wine-lw2`, Wine is at `~/tools/dl/wine`.

## Seat cap 129, very large map, large-world desync fix (2026-10-15, unattended)
- **Player cap:** `UM_MAX_PLAYERS=137` gives MAX_SLOTS 129 = the observer plus 128 players (humans + AI). Host validation allows 1 + start AI + seats + AI pool <= 129. Snapshots hold per-player arrays, so `world.bin` now carries the build's cap
  and a saved world from a build with another cap is discarded with a log line (new world). `world.bin` also gained the game speed (tagged "UMW1"), UM_VERSION 3.
- **"Every seat is taken" with no players:** the user's saved world was read with a changed `world.bin` layout (speed field added) and came out with 0 human seats. Fixed by the tag; now moot because of the cap check above.
- **Load time of 100+ player worlds:** `NameKeyGenerator::keyToName` scanned all 45007 sockets per call and `TeamsInfoRec::findTeamInfo` was linear: save+reload of the epoch took 26 s at 119 slots, now 3.6 s (6.9 s at 129 slots). Also: the debug log
  is flushed once a second instead of per line, and repeated identical asserts are logged 20 times (scripted AI repeats "Failed to find thing template Tech Center" every frame).
- **MapCache.ini** only knew Player_1..8_Start: any map with more starts made the cache unparseable (a release crash on the next start). Table extended to 128. `MapCache::findMap` also accepts the short `Maps\Name\Name.map` form for maps in the user data folder.
- **Pathfinder pool** raised from 30000 to 400000 cell infos ("Ran out of pathfind cells" on open maps with dozens of AI).
- **"Living World Grand" map** (`src/client/mapgen.cpp`): 800 x 800 cells (8000 units, about 7300 playable), 128 start waypoints on a jittered grid with flattened bases, gentle hills, three texture classes, a supply dock and five piles by every start.
  Built from the player's own Tournament Desert (texture list, lighting, world info, SidesList with the skirmish AI scripts are reused; heights, tiles, objects are new) the first time a server is asked for it, written to
  `<Documents>\Command and Conquer Generals Zero Hour Data\Maps\Living World Grand\`. Nothing is shipped. Clients do not need the file: the server's snapshot embeds the map (checked with the map file hidden).
  Python prototype in the session scratchpad (`gengrand.py`, RefPack decoder, chunk parser) validated the file format first; the format notes: DataChunk TOC + chunks (id u32, version u16, size i32); HeightMapData v4; BlendTileData v8
  (tile index = ((first + (y/2 % w)*w + (x/2 % w)) << 2) | ((y&1)<<1 | (x&1)) for a texture class at `first` with `w` tiles per row; the tail after the per-cell arrays can be copied from any map); objects are chunks with a TOC-keyed dict.
- **Desync with dozens of AI on that map (and in principle any big world): ScriptEngine counters/flags/attack priorities (256 each).** Every AI player has its own qualified counters; they ran out and AI decisions diverged between server and client
  (a team production condition `Counter >= n` was true in one process and false in the other, then one RNG draw later everything differed). Found by: per-frame object CRCs -> first differing frame -> logic RNG call trace (UM_RNGTRACE) -> AI team selection ->
  condition results. Raised to 4096/4096/1024 (`UM_LW`). Result: 129 slots, 40 AI, client in sync to frame 9000+ (epoch at 9000 included).
- Debug aids added (all env-gated): UM_OBJDUMP (object list + per-object CRC per frame), UM_OBJDEEP (deep xfer of matching objects), UM_RNGTRACE, UM_SAMPLE (stack samples), UM_TRACE_DRAW, UM_TRACE_LOCAL. Tools: tools/lw_killall.sh, lw_pids.sh, winclick.ps1 (posted mouse/key messages), screengrab.ps1.
- Screenshots were not possible while the desktop was locked (the user was away); everything above was verified from logs, CRC checks and file contents.
- Harness lesson: a leftover server from an earlier run on the same port made later "tests" talk to old binaries. `tools/lw_killall.sh` before every run.
- The Docker/Linux work (public/docker, docker-image CI job, LW_ env overrides, docs/linux-native-findings.md) came from another session working in this tree at the same time; its open question (large-world divergence) is the counter overflow above.

## Sound, rent panel and spawn protection (2026-10-15)
- **Sound**: the client's audio manager was a no-op (NullAudioManager; the retail Miles Sound System is proprietary and not in EA's source). `src/client/um_audio.cpp` is a new AudioManager that reads sounds, speech and music out of the
  game's archives through TheFileSystem and plays them with miniaudio (`src/audio/`, vendored header fetched into `deps/miniaudio-0.11.22`, built as plain C by its own CMake target). It follows MilesAudioManager's request, limit, loop,
  attack/sound/decay and volume rules; 3D sounds get distance volume and pan from the listener. The server, `UM_NOSOUND=1` and `-noaudio` stay silent. Verified: device opens, music (Game_USA_01) advances, hundreds of effects start.
  `UM_AUDIO_TRACE=1` logs the busiest events. Ambient loops that are culled by volume are restarted by their drawables (same as retail), so the "sounds started" counter is large.
- **Rent panel** (`src/client/um_hud.cpp`, drawn from InGameUI::postDraw): bottom left above the control bar (follows the bar when lowered/hidden; `UM_TEST_BAR=1` cycles the stages for testing), 18 px text, border sized from the text.
  Rows: PROTECTED (bar + time), RENT (amount, countdown bar, "(grace)" while free), UNPAID RENT xN. The server sends the numbers in a new message T_RENT every 15 frames; paying plays the money-withdraw sound for the local player
  (UmCharge now withdraws with sound unless the client is still catching up). UM_VERSION is 4 (old clients are refused instead of desyncing).
- **Rent grace**: `rent_grace_cycles` (UM_RENT_GRACE, default 1) free full cycles; the first payment is due at (grace+1) cycles after binding.
- **Spawn protection**: `immunity_secs` (default 180, 0 = off; UM_IMMUNE_FRAMES) granted when a player binds or returns, not renewed until `immunity_cooldown_secs` (default 900) after it ended. Server command MSG_UM_IMMUNE sets
  `Player::m_umImmuneUntil` (xfer version 10, so it survives epochs). While protected: ActiveBody::attemptDamage ignores damage to the player's objects; `Player::getRelationship` reports NEUTRAL instead of ENEMIES for computer players
  looking at the protected player's teams; Drawable tints everything the player owns red-gold (TINT_STATUS_UM_PROTECTED). Checked: 0 CRC mismatches across 7 epochs while protected.
- Not checked: that AI units already in combat disengage (relationship change only; damage is blocked regardless); capturing/hijacking a protected unit is not blocked.
### Assumptions for review
- Protection defaults: 3 minutes, 15 minute cooldown against reconnect abuse. Grace default stays at 1 cycle = the old "first interval free" behaviour.
- The panel uses the Arial font at 18 px (scaled with the resolution like other in-game text).
- Rent-paid sound is the stock "MoneyWithdrawSound".

## Late-join desync in worlds with an AI pool, panel font (2026-10-15)
- Symptom: a client joining a fresh server (45 slots: 16 seats + 24 AI pool + 4 AI) reported CRC DIVERGED from its first check (frame 30), then the retail main menu showed and the client crashed (user report). Reproduced on Windows only,
  with the published 0.0.1-alpha.6 build too, so it is not Wine and not the sound/HUD/protection work.
- Cause: the server's first AI joins at logic frame 0 (a command batch for frame 0). The retail menu's *shell map* is a running game: the client's network layer ran against it, and when MSG_NEW_GAME arrived the real world started and
  ran its frame 0 in the same GameLogic::update, before the network layer got a turn, so the frame-0 batch was never queued (the client's first look at the world was frame 1 or 2). Small worlds without an AI pool have no frame-0 batch.
- Fix: `UmNetPreFrame(frame)` is called from GameLogic::update right before processCommandList (not in the shell game) and queues the recorded batch for exactly that frame; the replica's per-update logic ignores the shell game.
  Result: 45-slot fresh join 0 mismatches (crc-ok 99); epoch-snapshot late join 0 mismatches. Also a null guard in startNewGame (multiplayer scripts) for a first side without a script list (the crash site in the user's dump).
- The rent/protection panel now uses the font of the game's on-screen messages (InGameUI::umMessageFontName/PointSize/Bold) instead of a fixed 18 px.

## Real-city maps, spawn lots + deadzone, one world per map (2026-10-15/16, unattended)
User asked: one persistent world per map, one big map for 128 players, built from real cities at real size (London, New York, Paris, Dubai) with
surrounding areas, random spawns with a deadzone (no space = no join), push the engine, partial rendering if needed. Mid-run additions: a map drop-down in
the host, map codenames in docker-compose comments, each map with its own persistent world.

### Scale and engine limits (tools/bigmap_bench.sh, generated maps of N cells)
- Scale: a civilian car is 22 units long and a town house ~50 x 32 units, so 1 unit = 0.2 m; a terrain cell (10 units) = 2 m. City maps are 5000 x 5000
  cells = 10 x 10 km (the engine carries 6000 cells; 5000 leaves the client room for 150k scenery props).
- Crashes and walls found on the way, all fixed: pathfinder zones were 14-bit cells / 16-bit tables and a 24000-entry stack array (crash above ~1250 cells);
  zone merging rewrote the whole table per merge (quadratic: minutes of load at 2400 cells; now union-find, same result); zone updates for every structure
  walked every zone block of the map; A* costs were 16-bit; partition cells hold shroud/threat/cash for 137 players (~1.7 KB each: 600 MB at 2400 cells) ->
  partition cell size doubles until a side has <= 512 cells (derived from the map, same on every process); the sync CRC xfer'd every partition cell's
  shroud every 30 frames -> a rotating 1/16 slice (objects are still checked in full); the exe was not large-address-aware (2 GB) -> /LARGEADDRESSAWARE.
- Results, server with 8 AI (uncapped): 800 cells 300 MB; 2400 cells 940 MB / 45 fps before the partition fix -> 540 MB / ~140 fps after; 4000 cells 1.04 GB
  ~130 fps; 6000 cells 1.38 GB steady (2.2 GB peak while generating) ~150 fps. London city map: server 1.22 GB ~135 fps, client 1.75 GB.
- Harness lesson: another session ran lw_killall.sh in the same tree at the same time; my benchmark servers "vanished" with no crash record. My runs now use
  renamed copies (build/citybench/zh_city*.exe) and kill only their own PIDs; never run lw_killall.sh while another session works here.

### City maps
- `tools/citygen/citygen.py <city>` (uv: osmium, shapely, numpy, pillow, rasterio, scipy): Geofabrik OSM extract + Copernicus GLO-30 tiles -> `cities/<city>.lwcity`
  (zlib: height, surface class, flags per cell; buildings as min-area rectangles with height/use; trees, bushes, parked cars along streets, POIs, spawn lots).
  The DEM is a surface model: buildings removed with a 90 m grey opening + Gaussian; relief compressed into the 0-255 height range (London 3..70 m -> 3 steps/m).
  Sea = low ground connected to the map edge. Bridges are drawn over water as land, so units cross where the real bridges are. Dense unlabelled ground -> paving.
  Spawn lots: open ground (parks) first, then cleared "building sites" on a jittered grid, 520 m apart, 120 m radius, flattened; buildings on them demolished.
- `src/client/citymap.cpp` builds the map on each machine from the pack + the player's own files (Tournament Desert for sides/lighting/world info; texture
  classes by Terrain.ini name; tile counts from the TGA headers). Buildings: best-fitting civilian model per footprint (scaled 0.8-1.6, repeated along long
  terraces); up to 6 per lot are real, garrisonable objects, the rest are drawn-only props ("lwProp"/"lwScale" map-object keys) over cliff-flagged (impassable)
  cells. One water plane over the whole map; river beds and sea lie below it. map.ini adds optimised-tree types for the game's own oak/birch/palm/bush models
  (map.ini is an override file: modules go inside AddModule). POIs -> tech buildings (fuel -> oil derrick, hospital, power -> refinery, airfield -> reinforcement pad).
  London: 207 lots, 1232 real buildings, ~152k drawn buildings, 100k trees/bushes, 41k cars; map file 247 MB, built in 6 s.
- Snapshots no longer embed generated city maps: GameStateMap writes "LWMAPREF1 <pack> <crc>"; a loading process builds the map from its own pack, checks the
  CRC and copies it into place (an epoch with a 247 MB map embedded would not work).
- Props: sector culling (800-unit sectors), drawing only visible props, shroud status refreshed lazily via a generation counter (notifyShroudChanged walked every
  prop on every shroud cell change: the client stalled), cheap distance reject in removePropsForConstruction. Caps: props 200k, trees 200k (vertex buffer 65k).
  The server skips props and optimised trees (client-only). While catching up, the client draws one frame in 30.

### Spawn lots + deadzone
- The server reads LW_Lot_<n> waypoints; a new base goes to a random lot where no object of any slot player is within UM_SPAWN_DEADZONE (host: spawn_deadzone in
  metres, default 400 m = 2000 units) and no bind was sent in the last 900 frames nearby. The spot travels in MSG_UM_BIND_PLAYER (args 8, 9) and UmBindPlayer
  moves the slot's start waypoint there before placing the base (the AI plans around its command center, not the waypoint, so nothing else needs saving).
- No free lot: the join is refused (T_KICK with the reason; the client shows it in a message box). AI only joins while more than ai_reserve_lots (default 4) lots
  are free. status.json gains spawnLots / spawnLotsFree. tools/spawn_test.sh: 10 spawns with a 2.4 km deadzone all kept apart, a late joiner refused: PASS.
- UM_VERSION 5.

### Host / Docker
- Map drop-down (editable combo) with codenames london, newyork, paris, dubai, grand, desert (or a .map path); the ini stores the codename. Default map: london.
- Worlds: <world_dir>\<codename>. A world saved by an older build directly in <world_dir> moves into the folder of the map configured when first seen.
  host_log.txt and server_log.txt stay in <world_dir> (the Docker entrypoint tails them there). No codename starts with a dot (the Wine prefix is .wineprefix).
- docker-compose.yml lists the codenames in comments; LW_SPAWN_DEADZONE / LW_AI_RESERVE_LOTS. Packages and the image ship cities/*.lwcity.

### Performance and sync round (2026-10-16)
- 127 players on London (4 start AI + 123 joined, server only, uncapped): before ~7.5 logic fps at frame 7500; profile: StealthUpdate::markAsDetected walked every
  object of every enemy player for each reveal (O(n^2)). Now a partition range query bounded by the largest vision range in the world (cached per frame):
  frame 7500 reached in 469 s instead of ~967 s, ~9.5 fps late. Missing-template asserts ("Tech Center", asked by the AI scripts every frame) report once.
  Conclusion recorded in assumptions.md: the CPU, not the map, sets the player count; 128 AI run the world at about a third of normal speed on this PC.
- Epochs on city maps: snapshot 347 MB raw because the map-reference check was case-sensitive (the engine passes lower-case paths) -> 88 MB / 4.5 MB
  compressed. Save+reload 62 s -> ~20 s: classification tests each grid corner's "underwater" once (was 4x per cell), the terrain classification is
  cached per map, zone passes walk cells column by column (cells are stored per column), the server skips shoreline tiles (null renderer), the
  epoch file is written compressed ("UMZ1"). clearPassableFlags clears only the blocks marked since the last clear (was all 250 000 per path search);
  full re-zones happen at most every 300 frames x (map area / 800^2 map), capped at 9000 frames.
- Desync on big maps after the first epoch (found with new UM_PFDIGEST pathfinder digests + UM_OBJDUMP/UM_OBJDEEP): a live client that was busy
  (loading a big map) ran past the epoch frame before reading the epoch message, skipped it as "stale", while the server had re-based on its reload.
  The client now keeps the batches it ran and, on a late epoch, loads it and replays its batches from the epoch frame. Also Pathfinder::loadPostProcess
  re-zones after every load so the zone schedule cannot depend on the frame counter a process had during the load. 4000-cell test: 0 mismatches.
- Host: epoch_secs "auto" (300 s, 900 s on city maps); docker-compose.yml lists every setting with its default (tools/gen_compose_env.py rebuilds it from
  the host's settings table); the ini parser no longer reads a following comment as an empty value (Zero Hour folder showed "; Zero Hour folder ...").
- Client epoch-load failures (rare, city maps with many AI: a dangling terrain render pointer while reloading in-process) now restart the client, which
  rejoins with its key as a returning player (tested with the UM_TEST_FAIL_EPOCH hook). A failed copy of the map into the save folder is accepted when an
  identical-size copy is already there (it was still open from the previous load).
- Checks on the final build: London 40-AI + seated client with epochs every 2400 frames: 0 mismatches until the renderer-side reload failure above; 4000-cell
  generated map with epochs: 0 mismatches; 45-slot fresh join on Tournament Desert: 5 of 6 runs 0 mismatches (one run admitted at frame 14 diverged from
  frame 30, not reproduced); queue_test, return_test (now with protection so the absent base survives the faster sim) and spawn_test pass; persist_test
  resumes from the compressed epoch (its overlap comparison found no common frames).

## Map sizes: -small, -medium, -large (2026-10-16)
- Playtest on an older PC: the 10 km maps were unplayable. Every city now comes in three sizes at the same real scale, cut around the same centre:
  small 710 cells (1.4 km) = the largest map retail Zero Hour ships (MD_GLA05, 710 x 710; the largest retail skirmish map, Twilight Flame, is 630),
  medium 2400 cells (4.8 km), large 5000 cells (10 km). Spawn lots scale (small: 64 m clearings 190 m apart; medium 100 m / 380 m; large 120 m / 520 m).
  Spawn lots: small 15-24, medium 64-86, large 164-212. Memory (game client, early game): small ~400 MB, medium ~750 MB, large ~1.8 GB.
- Codenames <city>-<size> (london-medium ...); the 0.0.1-alpha.13 names london/newyork/paris/dubai are aliases for -large (map references and world
  folders included). Defaults: map london-medium; spawn_deadzone auto = 120/250/400 m; epoch_secs auto = 300/600/900 s by size.
- citygen: OSM areas and ways are kept when their bounding box touches the map (two sampled nodes missed the Thames polygon on the 1.4 km map).
- Join race fixed (found on Paris medium, where the client loads fast and joins at frame 1; also explains the one 45-slot desync): before the world
  really starts, the client's logic loop ticks with a stale frame counter, and both the network update and the pre-frame hook ran the batch for that
  stale frame into the old state (the joiner's bind was lost or moved to frame 0). Now nothing runs between UmStartWorld and GameLogic's real start
  (UmNetWorldStarting, called from startNewGame after the deferral, for fresh starts and snapshot loads). Checks: 45-slot desert x2, Paris medium x2
  (join at frame 1), Paris medium snapshot join with epochs, London small, London large: 0 mismatches.

## Playtest bugs: factions, settings (2026-10-16)
- Faction choice ignored for every general: the launcher sends e.g. FactionChinaTankGeneral, whose Side is "ChinaTankGeneral" (BaseSide "China"); the
  server only accepted Side America/China/GLA and fell back to the slot's default. Now the base side counts too. Verified: Tank_ChinaCommandCenter +
  Tank_ChinaVehicleDozer, green house colour (colour was already honoured).
- Settings "not taking effect": the host passes every setting (verified end to end with LW_IMMUNITY_SECS=20, LW_RENT_GRACE_CYCLES=0, LW_RENT_MINUTES=1:
  protection 600 frames, rent each minute from the first). Three ways a change could still look ignored, all addressed:
  the host window kept the values it read at start-up and overwrote hand edits of um_server.ini (it now reloads the file on Start when it changed
  on disk and says so in the log); a resumed world kept each player's rent schedule from their first join (players who have not paid yet now follow
  the current interval and grace); and nothing showed the values in use (the server logs "server settings: ..." at start).
  Note: UM_* variables set by hand in the environment are not passed through by the host on purpose; the supported variables are LW_<SETTING>.

## Playtest bugs: epoch reload control, music, Exit (2026-10-16)
- Control after a scheduled epoch: the reload rebuilds the player list with the map's default local player, and the client only applied its seat
  when the seat changed, so the player was left watching. The seat is re-applied after every reload (selection, shroud, control bar), and the
  camera keeps its position, angle and zoom.
- Music: the reload restarts the map's music from the top. The client notes the track and position and, when the game starts the music again,
  seeks to where it was (or switches back to the track that was playing). Needed uma_seek_ms in the miniaudio wrapper.
- Exit in the game menu crashed (Shell pushes the score screen, ScoreScreen::hideWindows on missing windows). A client now closes on Exit; the
  server keeps the base as an absent player's.
- Found while testing: a server and a client on the same PC share Documents\...\Save. Each snapshot load deleted every .map there
  (clearScratchPadMaps) and extracted the map to the same name, so one process deleted or overwrote the other's map mid-load: "epoch reload
  FAILED" (TeamFactory prototype count mismatch) or a crash in W3DTerrainVisual::xfer. Each process now extracts to <map>_<pid>_<n>.map and
  leaves other processes' copies alone. Test: London small, epochs every 1500 frames, 4 reloads on both sides, 0 mismatches, music resumed each time,
  test Exit (UM_TEST_EXIT_FRAME) closed the client cleanly.

## Spectate mode, queue and defeat splash (2026-10-16)
- Launcher: "Spectate" checkbox (`UM_SPECTATE=1`, saved in client.ini). Protocol v6: hello carries the flag, new `T_JOIN` (client asks for a seat) and `T_QUEUE` (position, total).
- Server: a spectator, or anyone arriving while the world is full, is no longer auto-queued; they watch (slot -1, full-map `ReplayObserver` view) until they send `T_JOIN`. A returning player with a base takes it back through `T_JOIN`. Eliminated or expired players keep their connection and become spectators instead of being closed.
- Client: permanent top-left "Spectator view, Open Menu (Esc) to Join" (queued: "in the queue (X of Y)"), a "DEFEATED" splash for ~4.5 s after a knock-out, no more 8 s exit (only a "replaced by a new connection" kick still closes). Game menu (`QuitMenu.cpp`, `UmQuitMenuRefresh`): Continue / Join Queue (X) greyed once asked / Exit, relabelling the retail buttons because the `.wnd` layout is in the .big files; Surrender/Restart is hidden for seat holders.
- Tested: `tools/spectate_test.sh` (full world: carol watches, asks, is queued at 1, promoted when alice's base falls; dave stays unqueued; 0 mismatches). Not yet checked on screen (desktop/graphics step pending).

## Faction currencies, money pools and inflation (2026-10-16)
- Sim state `gEcon` (GameLogic.cpp, API in `src/net/um_economy.h`), in `GameLogic::xfer` (version 12) and `GameLogic::crc`; it changes only through `MSG_UM_ECON_TICK` (server, once per rent cycle; carries the settings), `UmCharge` (rent goes into the pool of the payer's currency), `UmBindPlayer` (join cash counts as GDP, optional pool top-up) and `UmGrantCash` (bonuses count as GDP).
- Currency = faction group of the player (side "America"/base side "USA" -> USD $, China -> CNY yen, GLA -> ILS shekel); every group has its own pool.
- Each cycle: unspent budgets fold back into the pool, the pool is split 60/10/30 (settings) into depot / derrick / other budgets; the depot budget is divided equally per supply center (`depotCap`). Income calls draw through `UmEconDraw`: supply center (`SupplyCenterDockUpdate`), oil derrick and Black Market / Supply Drop Zone (`AutoDepositUpdate`, by KindOf), hackers (`HackInternetAIUpdate`), supply crates (`MoneyCrateCollide`). An empty budget pays 0; supply drop planes throw their crates away when the 'other' budget is empty (`DeliverPayloadAIUpdate`). Black Lotus cash hack, bounties, salvage and the initial capture bonus are not limited.
- Inflation (`UM_INFLATION`, off by default): price level = GDP (cash minted by joins and bonuses, minus pool tax and the cash of players who leave) / baseline, clamped to [1, max]; applied in `ThingTemplate::calcCostToBuild` (so rent by value, bounties and sell value follow) and to a flat rent. Deflation: `UM_DEFLATION_PERMILLE` of the pool is burned per cycle, players leaving destroy their cash.
- Currency symbols: money display (`InGameUI`), floating +/- cash text (`GameTextManager::fetch` for GUI:AddCash / GUI:LoseCash, local player's currency) and the rent panel. Yen and shekel are U+00A5 / U+20AA; the GDI font renderer takes any glyph the face has. Not yet checked on screen.
- Host: new `[economy]` settings (`kSettings`), `economy` block in status.json, a pool panel in the admin window and cards on the web status page.
- Tested: `tools/economy_test.sh` (server + late replica, short cycles, pools and inflation on, 4 AI): 0 mismatches in 6 runs; persist/snap tests unchanged. With 6 AI the same scenario desyncs in 3 of 4 runs even with pools off (`POOLS=0`): an existing early-join desync around frame 2370 (Chinook supply behaviour), not caused by the economy. `snap_test.sh` without `RELOAD=1` also mismatches without the economy xfer; `RELOAD=1` (the in-process path the server uses) is clean.
