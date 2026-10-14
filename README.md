# Living World for Command & Conquer Generals: Zero Hour

One persistent, join-any-time Zero Hour world on a dedicated server. Gameplay stays the real game; getting knocked out closes your client until you rejoin; AI players fill empty seats;
bases left behind are cleaned up after a while; rent keeps the world turning over. Everything about the rules is a server setting.

- **Players**: install = download the release zip from the Releases page, unzip it anywhere (nothing is installed into Windows or your game), run `zh_launcher.exe` (name, faction, colour,
  server address, Start). You need your own copy of Zero Hour (Steam, EA app or the Ultimate Collection); the launcher finds it. If Windows Defender flags `zh_client.exe`, it is a false positive
  on an unsigned build: allow it or exclude the folder (details in the README inside the zip).
- **Hosts**: `zh_host.exe` (window with Start/Stop, log, players, uptime) or `zh_host.exe --cli`. Settings: `um_server.ini`; every option is also a command-line flag.
- **How it works**: the server is the same program as the client (`zh_client.exe` with `UM_SERVER` set, window hidden, a Direct3D that draws nothing). It runs the real game logic and
  render-object code, orders every command, and saves the world as periodic snapshots plus a command log; clients are lock-step replicas that join from a snapshot and are checked
  against the server every second. See `MODLOG.md` for the porting notes (clang/mingw build of EA's source, determinism fixes, the 128-player widening).

## Building
Linux or WSL: `scripts/setup.sh && scripts/build.sh && scripts/package-windows.sh`. Setup downloads EA's released source (pinned in `EA_SOURCE_SHA`), applies `patches/zh-living-world.patch`,
fetches llvm-mingw and zlib, and generates the build inputs. Nothing from the game itself is needed to build or is shipped.

## License
GPL-3.0 with the additional terms of EA's source release (see `LICENSE`). Not affiliated with or endorsed by EA.
