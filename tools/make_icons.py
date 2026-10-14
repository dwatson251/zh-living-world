#!/usr/bin/env python3
"""make_icons.py: draw the program icons (src/res/*.ico). Run with: uv run --with pillow python tools/make_icons.py"""
from PIL import Image, ImageDraw
import math, os
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'res'); os.makedirs(OUT, exist_ok=True)
S = 1024   # draw large, shrink for quality

def rounded(d, box, r, fill):
    d.rounded_rectangle(box, radius=r, fill=fill)

def globe(d, cx, cy, r, base, line, glow):
    d.ellipse([cx - r - 18, cy - r - 18, cx + r + 18, cy + r + 18], fill=glow)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=base)
    for k in (-0.55, 0, 0.55):   # parallels
        y = cy + k * r; w = math.sqrt(max(r * r - (k * r) ** 2, 0)); d.line([cx - w, y, cx + w, y], fill=line, width=14)
    for k in (0.35, 0.75):       # meridians
        d.ellipse([cx - r * k, cy - r, cx + r * k, cy + r], outline=line, width=14)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=line, width=22)

def client():
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    rounded(d, [40, 40, S - 40, S - 40], 190, (18, 24, 38, 255))
    globe(d, S // 2, S // 2, 330, (28, 64, 120, 255), (110, 170, 235, 255), (230, 120, 30, 255))
    # a base marker
    d.polygon([(S // 2, S // 2 - 150), (S // 2 + 110, S // 2 + 60), (S // 2 - 110, S // 2 + 60)], fill=(240, 150, 40, 255))
    d.ellipse([S // 2 - 36, S // 2 - 10, S // 2 + 36, S // 2 + 62], fill=(18, 24, 38, 255))
    return im

def launcher():
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    rounded(d, [40, 40, S - 40, S - 40], 190, (230, 120, 30, 255))
    globe(d, S // 2, S // 2, 300, (255, 255, 255, 40), (255, 255, 255, 120), (0, 0, 0, 0))
    d.polygon([(S // 2 - 130, S // 2 - 190), (S // 2 + 230, S // 2), (S // 2 - 130, S // 2 + 190)], fill=(255, 255, 255, 255))
    return im

def host():
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    rounded(d, [40, 40, S - 40, S - 40], 190, (24, 32, 42, 255))
    for i in range(3):
        y = 180 + i * 240
        rounded(d, [150, y, S - 150, y + 190], 40, (52, 66, 84, 255))
        d.ellipse([200, y + 55, 280, y + 135], fill=(63, 185, 80, 255))
        d.rectangle([340, y + 80, S - 230, y + 110], fill=(30, 40, 52, 255))
    return im

for name, fn in (('zh_client', client), ('zh_launcher', launcher), ('zh_host', host)):
    img = fn(); img.save(os.path.join(OUT, name + '.ico'), format='ICO', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print('wrote', name + '.ico')
