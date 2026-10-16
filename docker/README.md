# Running the Living World server in Docker / Portainer (Linux, CentOS)

The image runs the same server programs as the Windows release (`zh_host.exe --cli` and `zh_client.exe` in server mode) under Wine, headless
(no X server). Wine keeps the simulation bit-compatible with Windows clients. **The image contains no game files**: the server's simulation needs
Zero Hour's INI data, maps and models, so you provide a copy of a Zero Hour install you own.

1. **Game files.** Copy the Zero Hour install folder to the server, e.g. `/srv/zerohour` (it must contain `INIZH.big`, `MapsZH.big`, `W3DZH.big` ...).
   About 4 GB is enough; the server never writes into it (it works through a folder of links).
2. **Portainer:** Stacks > Add stack > Web editor, paste `docker-compose.yml`, add environment variable `ZH_FILES=/srv/zerohour`, Deploy.
   Or "Repository": URL of this repo, compose path `docker-compose.yml`. **CLI:** `ZH_FILES=/srv/zerohour docker compose up -d`.
3. First start takes about two minutes (the engine loads the game data). The container is "healthy" when the status page answers:
   `http://<server>:8080/`. Players connect with `zh_client.exe`/`zh_launcher.exe` to `<server>:7777`.
4. **CentOS:** open the ports (`firewall-cmd --permanent --add-port=7777/tcp --add-port=8080/tcp && firewall-cmd --reload`). With SELinux enforcing,
   keep the `:z` on the game-files volume. If you bind-mount the world folder instead of using the named volume, `chown 1000:1000` it.
5. **Settings:** `LW_<SETTING>` environment variables (see `um_server.ini` for the list), or mount your own file at `/data/config/um_server.ini`.
6. **Maps:** set `LW_MAP` to a codename `<city>-<size>`: city `london`, `newyork`, `paris` or `dubai`; size `small` (1.4 km, the largest retail
   map size, light), `medium` (4.8 km, default `london-medium`) or `large` (10 km, up to 128 players), e.g. `paris-small`; or `grand`, `desert`.
   Each map keeps its own world in the volume (`/data/world/<codename>`), so switching maps and back resumes each world. City maps are built from the
   packs in the image (`/opt/livingworld/cities`) and your game files on first start; a large map needs roughly 1.5 GB of memory for the server.
7. **Saves/backups:** the named volume `lw-world` holds one folder per map (`epoch.sav`, `cmds.log` and the rest); stop the stack and copy the volume to back up.
7. Wrong or missing files: the container exits with a message naming the missing `.big` files.

The image is published to `ghcr.io/dwatson251/zh-living-world` by the release workflow. The first time, set that package to public in the GitHub
package settings, otherwise Portainer needs a registry login.
