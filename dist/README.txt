Living World for Command & Conquer Generals: Zero Hour
=======================================================

A persistent, join-any-time Zero Hour world on a dedicated server. This package contains no game data: you need your own copy of Zero Hour.

What you need
-------------
- Windows 10 or 11 (64-bit; the programs are 32-bit like the original game).
- Command & Conquer Generals: Zero Hour, installed and run at least once. Steam, the EA app, or "C&C The Ultimate Collection" all work.
  Have it patched to the latest version (the Steam and EA versions already are).
- The address and port of a Living World server (ask whoever runs one), or run your own (see "Hosting").

Installing
----------
1. Download LivingWorld-<version>-win-x86.zip from the Releases page of the project and unzip it anywhere you like, for example C:\Games\LivingWorld.
   Keep the programs together in that folder. Nothing is installed into Windows, and nothing is changed in your Zero Hour folder except log files the game writes there.
2. Windows Defender / SmartScreen: these programs are unsigned, so Windows may warn you or Defender may flag zh_client.exe (it has been reported as
   Trojan:Win32/Zenpak.BAB!MTB, a false positive on a custom build of the open-sourced game engine). If it is blocked: Windows Security > Virus & threat protection >
   Protection history > Allow, or add the LivingWorld folder as an exclusion. You can rebuild everything from source to check it yourself.
3. Run zh_launcher.exe. It finds your Zero Hour folder by itself; if it cannot, press Browse and pick the folder that contains INIZH.big.

Updating: unzip the new version over the old folder. Uninstalling: delete the folder and %APPDATA%\LivingWorld (your settings and identity key).

Playing
-------
1. In the launcher choose a player name, a faction (America, China or GLA and their generals) and a colour (click the colour bar for the full colour picker).
2. Type the server address as address:port, for example 203.0.113.5:7777.
3. Choose Windowed, Borderless fullscreen (recommended) or Exclusive fullscreen, then press Start.
You get a base in the server's world. If every seat is taken you watch until one frees up. If you are knocked out the game shows why and closes; start it again to rejoin.
If you leave, your base stays (30 minutes by default, longer while it is in a fight). The launcher keeps a private identity key in %APPDATA%\LivingWorld\client.ini: keep that file to get your base back.

World rules (all adjustable by the server)
------------------------------------------
- Rent is due every 15 minutes (the first period is free): the larger of a minimum and a share of what you own, or a flat amount. It can only be paid with a command center.
- Unpaid rent stacks. Each stack hurts more, and the damage lands in order: your fighting units first; when none are left your buildings (not the command center);
  then dozers and workers; the command center last. Paying a stack off needs a command center and cash.
- Knocking out a player's last unit pays you a share of their cash plus a flat amount, if you were the last to hit them within the last 5 minutes.
- Supply docks can slowly restock (off by default).

Hosting a world
---------------
1. Run zh_host.exe. Set the Zero Hour folder, the number of seats and AI players, starting cash, rent and the rest, then press Start.
   The window shows the log, who is connected and how long the server has been up. Press "Save settings" to keep your choices in um_server.ini.
2. To let other people in, open the game port (default TCP 7777) and, if you want the status page visible, 8080: press "Allow through Windows Firewall"
   and forward those TCP ports on your router to this PC. Give players your public address and the port.
3. No window? Run  zh_host.exe --cli --port 7777 --max-players 16 --max-ai 16 --start-cash 10000 --rent-minutes 15 ...  (zh_host.exe --help lists every option).
   Every option is also a line in um_server.ini; command-line options win.
Seats and AI: the world holds up to 128 players in total (human seats + AI), plus the server's observer. Set "Max players (seats)", "Max AI players", "AI seats (pool)" and "AI players at world start"; their sum plus 1 must not
be more than 129. A saved world keeps the seat counts it was started with: to change them, stop the server, delete the "world" folder (this ends the current world) and start again.
Game speed (15-60, default 30) sets how fast the game runs for everybody, like the speed slider in a skirmish.
Very large map: set Map to  Maps\Living World Grand\Living World Grand.map  and start. On first use the server builds this 8000 x 8000 unit map with 128 start spots from your own Tournament Desert (nothing is downloaded
or shipped) and saves it to the Maps folder of your Zero Hour data folder in Documents; players do not need the file, the world snapshot carries it. The default map is Tournament Desert with invented start spots.
Joining a world that has been running for a while replays it from the last save, which can take a few minutes with many AI players; the game shows how far it has got.
The host restarts the server from its saved world if it stops. The world lives in the "world" folder next to the program: back it up to keep a world.
A status page runs at http://localhost:8080/ (players, seats, uptime).

If something goes wrong
-----------------------
- The game writes um_client_log.txt (and um_client_trace.txt after a crash) into your Zero Hour folder; the server writes world\server_log.txt. Send these with a bug report.
- "Cannot connect": check the address and port, the server's firewall and port forwarding.
- Black window in fullscreen: use Borderless fullscreen. A d3d8.dll wrapper in your Zero Hour folder is fine; set UM_GAME_D3D8=1 only if you want the game to use it.

Source and license: GPL-3.0, built on the engine source EA released for Zero Hour. Not affiliated with or endorsed by EA.
