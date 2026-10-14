Living World for Command & Conquer Generals: Zero Hour
=======================================================

A persistent, join-any-time Zero Hour world on a dedicated server. You need your own copy of Zero Hour (Steam or EA App); this package contains no game data.

Playing
-------
1. Run zh_launcher.exe.
2. Pick a name, a faction and a colour (click the colour bar for a full colour picker), enter the server address as address:port (for example 203.0.113.5:7777),
   check the Zero Hour folder, and choose Windowed, Borderless fullscreen (recommended) or Exclusive fullscreen.
3. Press Start. You get a base in the server's world. If every seat is taken you watch until one frees up.
If you are knocked out the game shows why and closes; start it again to rejoin. If you leave, your base stays (30 minutes by default, longer while it is in a fight).
The launcher keeps a private identity key in %APPDATA%\LivingWorld\client.ini; keep that file to get your base back.

World rules (all adjustable by the server)
------------------------------------------
- Rent is due every 15 minutes (the first period is free): the larger of a minimum and a share of what you own, or a flat amount. It can only be paid with a command center.
- Unpaid rent stacks. Each stack hurts more, and the damage lands in order: your fighting units first; when none are left your buildings (not the command center);
  then dozers and workers; the command center last. Paying a stack off needs a command center and cash.
- Knocking out a player's last unit pays you a share of their cash plus a flat amount, if you were the last to hit them within the last 5 minutes.
- Supply docks can slowly restock (off by default).

Hosting
-------
Run zh_host.exe: set the Zero Hour folder, seats, AI, cash, rent and the rest, press Start. The window shows the log, who is connected and how long it has been up.
No window: zh_host.exe --cli --port 7777 --max-players 16 --max-ai 16 --start-cash 10000 --rent-minutes 15 ...   (zh_host.exe --help lists every option; um_server.ini holds the defaults).
It restarts the server from the saved world if it stops, and serves a status page (default http://localhost:8080/). Open the TCP ports on your firewall
(the "Allow through Windows Firewall" button does it).

Windows Defender
----------------
These programs are unsigned. Defender's cloud heuristics have flagged zh_client.exe as Trojan:Win32/Zenpak.BAB!MTB, a false positive on a custom build of the open-sourced
game engine. Everything can be rebuilt from source (see the repository). If Defender removes or blocks the file you have to allow it yourself.

Source and license: GPL-3.0, built on the engine source EA released for Zero Hour. Not affiliated with or endorsed by EA.
