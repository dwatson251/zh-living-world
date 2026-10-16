# Living World for Command & Conquer Generals: Zero Hour

One persistent, join-any-time Zero Hour world on a dedicated server. Gameplay stays the real game; getting knocked out closes your client until you rejoin; AI players fill empty seats;
bases left behind are cleaned up after a while; rent keeps the world turning over. Everything about the rules is a server setting.

- **Players**: install = download the release zip from the Releases page, unzip it anywhere (nothing is installed into Windows or your game), run `zh_launcher.exe` (name, faction, colour,
  server address, Start). You need your own copy of Zero Hour (Steam, EA app or the Ultimate Collection); the launcher finds it. If Windows Defender flags `zh_client.exe`, it is a false positive
  on an unsigned build: allow it or exclude the folder (details in the README inside the zip).
- **Hosts**: `zh_host.exe` (window with Start/Stop, log, players, uptime) or `zh_host.exe --cli`. Settings: `um_server.ini`; every option is also a command-line flag.
- **Maps** (host drop-down, `map =` in `um_server.ini`, `LW_MAP` in Docker): London, New York, Paris and Dubai at real scale (2 m per terrain cell),
  each in three sizes: `-small` (1.4 km, the largest map size retail Zero Hour ships; for older PCs), `-medium` (4.8 km, the default `london-medium`)
  and `-large` (10 km, up to 128 players), e.g. `paris-small`. Built from OpenStreetMap and Copernicus elevation data: streets, parks, rivers,
  bridges, buildings drawn with the game's own models, trees and parked cars. `grand` is a generated 1.6 km map, `desert` the retail Tournament
  Desert. Each map keeps its own persistent world (`world/<codename>`). The packs in `cities/` hold only open data; every machine builds the map
  from its pack and its own game files on first use. See `cities/README.txt` and `tools/citygen/`.
- **Spawning**: new bases go to a random free spawn spot with no other player within the spawn deadzone (auto: 120/250/400 m on small/medium/large maps); no free spot = the join is
  refused with a message. AI players leave spots free for people.
- **Size and speed**: up to 128 players (human seats plus AI) and an observer. Game speed is a server setting (15-60, default 30). The CPU (every
  machine simulates the whole world) is the limit: about 40-60 AI plus people run at full speed on a fast desktop; see `assumptions.md`.
- **How it works**: the server is the same program as the client (`zh_client.exe` with `UM_SERVER` set, window hidden, a Direct3D that draws nothing). It runs the real game logic and
  render-object code, orders every command, and saves the world as periodic snapshots plus a command log; clients are lock-step replicas that join from a snapshot and are checked
  against the server every second. See `MODLOG.md` for the porting notes (clang/mingw build of EA's source, determinism fixes, the 128-player widening, the city maps) and `assumptions.md` for open design decisions.

## Building
Linux or WSL: `scripts/setup.sh && scripts/build.sh && scripts/package-windows.sh`. Setup downloads EA's released source (pinned in `EA_SOURCE_SHA`), applies `patches/zh-living-world.patch`,
fetches llvm-mingw and zlib, and generates the build inputs. Nothing from the game itself is needed to build or is shipped.

## License
GPL-3.0 with the additional terms of EA's source release (see `LICENSE`). Not affiliated with or endorsed by EA.
