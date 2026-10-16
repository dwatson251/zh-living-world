# Assumptions to discuss (real-city maps run, 2026-10-15/16)

Everything below was decided without you while you were away. Each item says what I chose and why; all of them are easy to change.

## Maps and scale
1. **Scale: 1 game unit = 0.2 m, so a terrain cell = 2 m.** Taken from the game's own models (a sedan is 22 units long, a town house about
   50 x 32 units). Real roads, parks and rivers therefore look right next to the game's buildings and vehicles.
2. **Map size: 10 x 10 km (5000 x 5000 cells) for every city.** The engine now carries 12 km (6000 cells), but 10 km leaves memory for the client's
   ~150 000 scenery props. "Surrounding towns and countryside" only fit at the edges: 10 km around central London reaches Hammersmith, Hampstead,
   Stratford and Brixton, not the green belt. A bigger area would need a coarser scale (for example 4 m per cell, double the area), which makes
   buildings and units look small against the streets.
3. **City centres and areas:** London = Trafalgar Square; New York = Empire State Building (Midtown/Lower Manhattan, Brooklyn, Queens, Jersey City);
   Paris = Notre-Dame/Hôtel de Ville; Dubai = Burj Khalifa (Downtown, Business Bay, Jumeirah coast). Change `CITIES` in `tools/citygen/citygen.py`.
4. **Relief is compressed.** The engine's whole height range is about 32 m of real height; London's 3-70 m becomes ~3 height steps per metre.
5. **One water level per map.** Rivers, docks and sea are carved below a single water plane. Real bridges are land causeways, so units cross where
   the real bridges are; the Hudson has no bridges in this area (only tunnels), so Manhattan is reached from New Jersey by air/water only.
6. **Buildings use the game's own civilian models**, picked per real footprint (best fit, scaled 0.8-1.6x, repeated along long terraces). Only about
   6 per spawn lot (~1200 per city) are real, garrisonable, destructible buildings; the other ~100 000+ are drawn-only scenery over impassable cells.
   Making more of them real costs CPU for everyone (every client simulates the whole world).
7. **Trees:** the game's oak, birch, dogwood, palm and bush models, as optimised (client-only) trees: OSM's mapped trees plus random trees in parks,
   woods and farmland. Parked cars along residential streets. Points of interest become tech buildings: fuel station -> oil derrick, hospital ->
   hospital, power station -> oil refinery, airfield -> reinforcement pad (capped per type).
8. **Default map is now `london`** (was Tournament Desert). Existing servers with an explicit `map =` keep their map.
9. **Data:** OpenStreetMap (ODbL) and Copernicus GLO-30 elevation; the packs (`cities/*.lwcity`, 4-8 MB each) are committed and ship in the zip
   and the Docker image, with attribution in `cities/README.txt`. They contain no game data; each machine builds the actual map from the pack and
   its own game files on first use (a few seconds, 250 MB written to Documents\...\Maps).

## Spawning
10. **Spawn lots:** parks/open ground first, then cleared "building sites" (buildings demolished) on a 520 m grid; each lot is a 120 m flattened
    circle with a supply dock and piles. London has 207, Paris 212, Dubai 197, New York 167 lots (enough for 128 players).
11. **Deadzone default 400 m**, measured from any unit or building of any other player (including AI and absent players' bases) to the lot centre.
    A refused player sees "No space in the world right now ... Try again later." Queued players are refused the same way when their turn comes and
    there is no room.
12. **AI keeps 4 lots free** for people (`ai_reserve_lots`), matching the old "leave 4 slots free" rule.
13. Returning players get their own base back (no new lot); a player whose base was destroyed gets a new random lot.

## Worlds, epochs, host
14. **One world per map:** `<world_dir>\<codename>`. A world saved by an older build directly in `<world_dir>` is moved into the folder of the map
    configured when the new build first starts (older builds had one world per server).
15. **Epoch interval `auto`:** 5 minutes on small maps, 15 minutes on city maps. An epoch pauses the world for everyone while the server saves and
    reloads (London: ~20 s, down from 62 s); joiners replay the commands since the last epoch, so longer intervals mean longer joins.
16. **The epoch file is now compressed on disk** (`UMZ1` header; 90 MB -> 4.5 MB on London). Older raw files still load.
17. **Snapshots carry a reference to a city map** (pack name + CRC) instead of the 250 MB map. A client whose pack or game files produce a different
    map is refused with a message rather than desyncing.
18. Map drop-down labels are short ("london - London, 10 km"); a typed .map path still works.

## Performance (what to expect)
19. **CPU, not the map, limits player count.** London server, uncapped, on this PC: ~135 logic fps with 8 AI; with 127 players (4 start AI + 123
    joined) ~9.5 fps late in the game after the fixes (~7.5 before) against the 30 fps the game runs at. So 128 AI players run the world at about
    a third of normal speed on this machine; ~40-60 AI plus humans keep real time. Every client simulates the whole world too (lockstep), so
    clients need similar CPU. True "partial simulation" would need a different network model (server state sync), which this plan left out.
20. Partial rendering done on the client: scenery props are culled by 800-unit sectors and only visible ones are drawn; shroud for props is
    refreshed lazily; while a joining client catches up it draws one frame in 30. The server never loads props or trees.
21. Big-map engine changes are in the shared engine (pathfinder zones, zone merging, partition cell size, sync CRC slice, stealth reveal wake-ups),
    so this release is not sync-compatible with earlier ones (UM_VERSION 5).

## Not done / open
22. Paris/New York/Dubai were checked from their previews and loads, not played; London was played (server + client in sync, screenshots).
23. Building looks: the game has few European/American civilian models, so streets repeat a handful of models; real heights are only matched
    roughly (towers use the tallest models).
24. **Epoch reloads on clients:** a live client reloads every epoch in-process. On city maps with many AI a reload occasionally failed inside the
    renderer (a dangling terrain pointer during the reload; seen twice in ~10 epochs with 40 AI). Instead of hanging, the client now restarts itself
    and (was: restarted itself and rejoined; removed in the post-alpha.13 build because it could loop: it now shows an error and closes). The root cause in the W3D reload path is still open.
25. **Rare early-join desync:** one of six 45-slot fresh-join runs (16 seats, 24 AI pool, 4 AI on Tournament Desert) diverged from frame 30 when the
    client was admitted at frame 14; five runs (admitted at frames 6, 9, 40, 70, 135) had 0 mismatches. Not reproduced since; worth a look.
26. City maps need ~1.8 GB in the game client and ~1.5 GB in the server (both now large-address-aware, so 64-bit Windows is required).

## Map sizes (2026-10-16)
27. **Three sizes per city at the same real scale**, cut around the same centre (not the same area at a coarser scale): small = 710 cells
    (1.4 km), the largest map retail Zero Hour ships (MD_GLA05); medium = 2400 cells (4.8 km); large = 5000 (10 km). Keeping 2 m per cell keeps
    buildings, streets and units in proportion; the price is that a small map shows only the centre (e.g. Trafalgar Square to Westminster).
28. **Default map is london-medium.** Small maps have 15-24 spawn spots, so a small world holds roughly 10-20 players; medium 60-90.
29. **Deadzone and save interval follow the size** when left on "auto" (120/250/400 m; 5/10/15 min).
30. The 0.0.1-alpha.13 codenames (london, ...) keep working and mean the large maps; their world folders move to <city>-large.

## Playtest fixes (2026-10-16, evening)
31. **"Exit" in the game menu closes the game.** A Living World client has no main menu or score screen to go back to (pushing the score screen
    is what crashed). Your base stays on the server as an absent player's for `absent_secs` and you get it back by joining again. Alternative:
    reopen the launcher instead of closing.
32. **After a scheduled save/reload, the music continues from where it was** (same track, same position) and the camera keeps its place. If the
    reload starts a different track, the client switches back to the one that was playing.

## Economy and spectating (2026-10-16)
- "Arms Dealer" (a GLA War Factory in Zero Hour, no income) is read as the Black Market. The 30% group is Black Market, Supply Drop Zone, hackers and supply crates; combat income (bounty, salvage, POW) stays unlimited.
- Pool defaults: 50,000 per currency at world start and 2,000 added per join (settings), because with an empty pool every generator would pay 0 until the first rent. Pools can be switched off (`pools = 0`).
- Inflation baseline 200,000 GDP and a 300% cap are guesses to tune. A cancelled build refunds the price at cancel time, so a price change in between shifts the refund a little (prices change once per cycle).
- Spectators are not queued automatically any more; the player asks from the Esc menu. The Restart/Surrender button is hidden for seat holders (it has no meaning in a persistent world).
- Floating +$ text uses the local player's currency symbol even for other players' generators.
