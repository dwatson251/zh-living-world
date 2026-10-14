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
