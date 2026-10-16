#!/usr/bin/env python3
"""gen_compose_env.py: rewrite the environment block of public/docker-compose.yml from the host's settings table (src/host/host.cpp kSettings),
so every server setting is listed with its default and description. Run it after adding or changing a setting."""
import os, re
LAB = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(LAB, 'src/host/host.cpp')).read()
rows = re.findall(r'\{\s*"(\w+)",\s*"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)",\s*"(--[\w-]+)",\s*"(\w+)"\s*\}', src)
SKIP = {'game_dir', 'world_dir', 'host'}          # set by the image's entrypoint
FIXED = {'port': '7777', 'status_port': '8080'}   # ports inside the container: change the published ports instead
MAPS = '''      # Map codenames (each map keeps its own persistent world in the volume, /data/world/<codename>; switch freely and come back):
      #   london   London, real city from OpenStreetMap, 10 x 10 km, up to 128 players
      #   newyork  New York (Midtown, Lower Manhattan, Brooklyn, Queens, Jersey City), 10 x 10 km, up to 128 players
      #   paris    Paris, 10 x 10 km, up to 128 players
      #   dubai    Dubai (Downtown, Business Bay, Jumeirah), 10 x 10 km, up to 128 players
      #   grand    Living World Grand: generated hills, 1.6 km, up to 128 players
      #   desert   Tournament Desert: the small retail map
      # (a path such as "Maps\\\\Some Map\\\\Some Map.map" also works)
'''
out = ['    environment:']
section = None
for key, label, default, flag, sec in rows:
    if key in SKIP: continue
    if sec != section:
        section = sec; out.append(f'      # ---- [{sec}]')
    env = 'LW_' + key.upper()
    label = label.replace('\\"', '"')
    if key in FIXED:
        out.append(f'      {env}: "{FIXED[key]}"'.ljust(60) + f' # {label} (inside the container; change the published port above)')
        continue
    if key == 'map': out.append(MAPS.rstrip('\n'))
    value = '${' + env + ':-' + default.replace('\\\\', '\\\\') + '}'
    if key == 'name': value = '${LW_NAME:-Living World}'
    out.append(f'      {env}: "{value}"'.ljust(60) + f' # {label}')
block = '\n'.join(out) + '\n'
p = os.path.join(LAB, 'public/docker-compose.yml'); y = open(p).read()
a = y.index('    environment:'); b = y.index('\nvolumes:', a)
y = y[:a] + block + '      # every setting above can also be set in um_server.ini; LW_ variables win over the file' + y[b:]
open(p, 'w').write(y)
print(f'{len(out)} lines written')
