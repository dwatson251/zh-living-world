#!/usr/bin/env python3
"""citygen.py <city> [--size small|medium|large] [--out PATH] [--preview PNG]: build a Living World city pack from open data.

A city pack (.lwcity) is the real-world part of a city map: a height field, a surface class and a flag per terrain cell, plus a list of
buildings, trees, bushes, parked cars, points of interest and spawn lots. It holds no game data: the map builder in the game program
(src/client/citymap.cpp) turns it into a Zero Hour map on the host, using the player's own game files for textures and objects.

Sources (both may be redistributed with attribution, which the pack carries):
  - OpenStreetMap (c) OpenStreetMap contributors, ODbL 1.0: Geofabrik extracts.
  - Copernicus GLO-30 elevation (c) DLR e.V. 2010-2014 and (c) Airbus Defence and Space GmbH 2014-2018, provided under COPERNICUS by the
    European Union and ESA. It is a surface model (it includes buildings), so the generator removes the buildings with a minimum filter.

Scale: Zero Hour's civilian cars are 22 units long and its town houses about 50 x 32 units, which puts one world unit at 0.2 m. A terrain cell is
10 units, so 2 m. Sizes (see SIZES): small 710 cells (1.4 km, the largest retail Zero Hour map), medium 2400 (4.8 km), large 5000 (10 km): what `tools/bigmap_bench.sh` showed the engine can carry with 128 players.

Run it with uv (dependencies are fetched on the fly):
  uv run --no-project --with osmium --with shapely --with numpy --with pillow --with rasterio --with scipy tools/citygen/citygen.py london --size medium
"""
import argparse, json, math, os, struct, sys, time, urllib.request, zlib
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.dirname(os.path.dirname(HERE))
CACHE = os.environ.get('LWCITY_CACHE', os.path.join(LAB, 'build', 'citydata'))
GEOFABRIK = 'https://download.geofabrik.de/'
COPDEM = 'https://copernicus-dem-30m.s3.amazonaws.com/Copernicus_DSM_COG_10_{t}_DEM/Copernicus_DSM_COG_10_{t}_DEM.tif'

# name: centre (lat, lon), Geofabrik extracts, ground left blank by OSM, what the open sea / land outside the data looks like
CITIES = {
    'london': dict(title='London', centre=(51.5072, -0.1276), pbf=['europe/united-kingdom/england/greater-london'], ground='grass'),
    'newyork': dict(title='New York', centre=(40.7484, -73.9857), pbf=['north-america/us/new-york', 'north-america/us/new-jersey'], ground='grass'),
    'paris': dict(title='Paris', centre=(48.8566, 2.3522), pbf=['europe/france/ile-de-france'], ground='grass'),
    'dubai': dict(title='Dubai', centre=(25.1972, 55.2744), pbf=['asia/gcc-states'], ground='desert'),
}

MPC = 2.0            # metres per terrain cell
# map sizes (all at the same real scale, cut around the same centre). small = the largest map that ships with Zero Hour (MD_GLA05, 710 x 710 cells);
# large = what the engine carries for 128 players after the Living World changes; medium in between. Spawn lots scale with the map.
SIZES = {
    'small':  dict(cells=710,  lot_r=32, spacing=95,  label='Small'),    # 1.4 km
    'medium': dict(cells=2400, lot_r=50, spacing=190, label='Medium'),   # 4.8 km
    'large':  dict(cells=5000, lot_r=60, spacing=260, label='Large'),    # 10 km
}
UPM = 5.0            # world units per metre (10 units per cell)
BORDER = 40          # cells of non-playable border on each side (part of the grid)

# surface classes (the map builder picks a terrain texture for each)
S_GRASS, S_FOREST, S_FARM, S_URBAN, S_ROAD_MAJOR, S_ROAD_MINOR, S_PLAZA, S_SAND, S_WATERBED, S_RAIL, S_PARKING, S_BROWN, S_DESERT, S_DIRT = range(14)
SURFACE_NAMES = ['grass', 'forest', 'farm', 'urban', 'road_major', 'road_minor', 'plaza', 'sand', 'waterbed', 'rail', 'parking', 'brownfield', 'desert', 'dirt']
# cell flags
F_BLOCKED = 1        # a drawn-only building stands here: impassable, nothing can be built
F_WATER = 2
F_LOT = 4            # inside a spawn lot (cleared, flattened)
# object kinds
K_BUILDING, K_TREE, K_BUSH, K_CAR, K_POI, K_LOT = 1, 2, 3, 4, 5, 6
# building uses (sub of K_BUILDING) and points of interest (sub of K_POI)
B_HOUSE, B_APARTMENTS, B_COMMERCIAL, B_INDUSTRIAL, B_CIVIC, B_RELIGIOUS, B_TOWER, B_SHED = range(8)
P_FUEL, P_HOSPITAL, P_POWER, P_AIRFIELD, P_PORT, P_POLICE, P_STATION = range(7)

ROAD_W = {'motorway': 14, 'trunk': 12, 'primary': 11, 'secondary': 9, 'tertiary': 8, 'motorway_link': 7, 'trunk_link': 7, 'primary_link': 7,
          'secondary_link': 7, 'tertiary_link': 6, 'residential': 7, 'unclassified': 6, 'living_street': 5, 'service': 4, 'pedestrian': 5, 'track': 3,
          'busway': 7, 'road': 6}
MAJOR = {'motorway', 'trunk', 'primary', 'secondary', 'motorway_link', 'trunk_link', 'primary_link', 'secondary_link', 'busway'}
CAR_ROADS = {'residential': 22.0, 'tertiary': 26.0, 'unclassified': 30.0, 'living_street': 22.0, 'secondary': 40.0}   # metres between parked cars, per side

LANDUSE = {
    ('landuse', 'residential'): S_URBAN, ('landuse', 'commercial'): S_URBAN, ('landuse', 'retail'): S_URBAN, ('landuse', 'industrial'): S_URBAN,
    ('landuse', 'railway'): S_RAIL, ('landuse', 'construction'): S_BROWN, ('landuse', 'brownfield'): S_BROWN, ('landuse', 'garages'): S_PARKING,
    ('landuse', 'grass'): S_GRASS, ('landuse', 'meadow'): S_GRASS, ('landuse', 'recreation_ground'): S_GRASS, ('landuse', 'village_green'): S_GRASS,
    ('landuse', 'cemetery'): S_GRASS, ('landuse', 'allotments'): S_FARM, ('landuse', 'farmland'): S_FARM, ('landuse', 'farmyard'): S_DIRT,
    ('landuse', 'orchard'): S_FARM, ('landuse', 'vineyard'): S_FARM, ('landuse', 'forest'): S_FOREST, ('landuse', 'quarry'): S_DIRT,
    ('landuse', 'greenfield'): S_GRASS, ('landuse', 'plant_nursery'): S_FARM, ('landuse', 'religious'): S_URBAN, ('landuse', 'education'): S_URBAN,
    ('leisure', 'park'): S_GRASS, ('leisure', 'garden'): S_GRASS, ('leisure', 'pitch'): S_GRASS, ('leisure', 'golf_course'): S_GRASS,
    ('leisure', 'common'): S_GRASS, ('leisure', 'nature_reserve'): S_GRASS, ('leisure', 'playground'): S_PLAZA, ('leisure', 'track'): S_DIRT,
    ('natural', 'wood'): S_FOREST, ('natural', 'scrub'): S_GRASS, ('natural', 'heath'): S_GRASS, ('natural', 'grassland'): S_GRASS,
    ('natural', 'sand'): S_SAND, ('natural', 'beach'): S_SAND, ('natural', 'bare_rock'): S_DIRT, ('natural', 'mud'): S_DIRT, ('natural', 'wetland'): S_GRASS,
    ('amenity', 'parking'): S_PARKING, ('amenity', 'school'): S_URBAN, ('amenity', 'university'): S_URBAN, ('amenity', 'hospital'): S_URBAN,
    ('place', 'square'): S_PLAZA, ('highway', 'pedestrian'): S_PLAZA, ('aeroway', 'aerodrome'): S_GRASS, ('aeroway', 'apron'): S_PARKING,
    ('aeroway', 'runway'): S_ROAD_MAJOR, ('aeroway', 'taxiway'): S_ROAD_MINOR, ('man_made', 'pier'): S_PLAZA,
}
LANDUSE_ORDER = [S_FARM, S_GRASS, S_URBAN, S_FOREST, S_DIRT, S_BROWN, S_SAND, S_RAIL, S_PARKING, S_PLAZA, S_ROAD_MINOR, S_ROAD_MAJOR]   # later wins
PARKISH = {S_GRASS, S_FOREST, S_FARM}


def log(*a):
    print('[citygen]', *a, flush=True)


def fetch(url, path):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    log('downloading', url)
    tmp = path + '.part'
    with urllib.request.urlopen(url, timeout=120) as r, open(tmp, 'wb') as f:
        while True:
            b = r.read(1 << 20)
            if not b:
                break
            f.write(b)
    os.replace(tmp, path)
    return path


class Grid:
    """Projection from lon/lat to metres around the centre (equirectangular: under 0.1% error over 12 km) and to grid cells."""
    def __init__(self, lat0, lon0, cells):
        self.lat0, self.lon0, self.n = lat0, lon0, cells
        self.kx = 111320.0 * math.cos(math.radians(lat0))
        self.ky = 110574.0
        half = cells * MPC / 2.0
        self.lon_min = lon0 - half / self.kx; self.lon_max = lon0 + half / self.kx
        self.lat_min = lat0 - half / self.ky; self.lat_max = lat0 + half / self.ky

    def m(self, lon, lat):            # metres from the grid's south-west corner
        return ((lon - self.lon0) * self.kx + self.n * MPC / 2.0, (lat - self.lat0) * self.ky + self.n * MPC / 2.0)

    def px(self, lon, lat):           # image pixel (x right, y down = north up), cell units
        x, y = self.m(lon, lat)
        return (x / MPC, self.n - y / MPC)

    def overlaps(self, pts, pad=0.002):
        """True when the bounding box of the points touches the map (a big river or park can cover the map without a node inside it)."""
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        return min(xs) <= self.lon_max + pad and max(xs) >= self.lon_min - pad and min(ys) <= self.lat_max + pad and max(ys) >= self.lat_min - pad

    def inside(self, lon, lat, pad=0.002):
        return self.lon_min - pad <= lon <= self.lon_max + pad and self.lat_min - pad <= lat <= self.lat_max + pad


def parse_osm(cfg, grid):
    import osmium
    from shapely.geometry import Polygon
    surf_polys = []      # (class, [ring px], [holes])
    water_polys = []
    buildings = []       # (Polygon in metres, use, height_m)
    roads = []           # (class name, [px], bridge, name)
    rails = []
    waterways = []       # ([px], width_m)
    trees, pois = [], []
    keys = ['building', 'highway', 'landuse', 'natural', 'leisure', 'waterway', 'water', 'amenity', 'railway', 'aeroway', 'man_made', 'power', 'place']
    t0 = time.time(); seen = 0
    for name in cfg['pbf']:
        path = fetch(GEOFABRIK + name + '-latest.osm.pbf', os.path.join(CACHE, name.replace('/', '_') + '.osm.pbf'))
        fp = osmium.FileProcessor(path).with_locations().with_areas().with_filter(osmium.filter.KeyFilter(*keys))
        for o in fp:
            seen += 1
            if seen % 2000000 == 0:
                log(f'  {seen} objects read, {len(buildings)} buildings so far ({time.time() - t0:.0f} s)')
            tags = o.tags
            if o.is_node():
                loc = o.location
                if not loc.valid() or not grid.inside(loc.lon, loc.lat, 0):
                    continue
                p = grid.m(loc.lon, loc.lat)
                if tags.get('natural') == 'tree':
                    trees.append(p)
                elif tags.get('amenity') == 'fuel':
                    pois.append((P_FUEL, p))
                elif tags.get('amenity') == 'hospital':
                    pois.append((P_HOSPITAL, p))
                elif tags.get('amenity') == 'police':
                    pois.append((P_POLICE, p))
                continue
            if o.is_way():
                hw, rw, ww = tags.get('highway'), tags.get('railway'), tags.get('waterway')
                if not (hw or rw or ww):
                    continue
                if tags.get('tunnel') in ('yes', 'building_passage', 'culvert') or tags.get('layer', '0').startswith('-') or tags.get('area') == 'yes':
                    continue
                try:
                    pts = [(n.lon, n.lat) for n in o.nodes if n.location.valid()]
                except osmium.InvalidLocationError:
                    continue
                if len(pts) < 2 or not grid.overlaps(pts):
                    continue
                pp = [grid.px(x, y) for x, y in pts]
                if hw in ROAD_W:
                    roads.append((hw, pp, tags.get('bridge') not in (None, 'no'), [grid.m(x, y) for x, y in pts]))
                elif rw in ('rail', 'light_rail', 'tram') and rw != 'tram':
                    rails.append(pp)
                elif ww in ('river', 'canal', 'stream', 'dock'):
                    width = {'river': 30.0, 'canal': 12.0, 'stream': 3.0, 'dock': 20.0}[ww]
                    width = _num(tags.get('width'), width) or width
                    waterways.append((pp, width))
                continue
            if o.is_area():
                rings = []
                try:
                    for outer in o.outer_rings():
                        ring = [(n.lon, n.lat) for n in outer]
                        holes = [[(n.lon, n.lat) for n in inner] for inner in o.inner_rings(outer)]
                        rings.append((ring, holes))
                except osmium.InvalidLocationError:
                    continue
                if not rings or not any(grid.overlaps(r[0]) for r in rings):
                    continue
                b = tags.get('building')
                if b and b != 'no' and tags.get('location') != 'underground':
                    use = building_use(b, tags)
                    h = building_height(tags, use)
                    for ring, holes in rings:
                        if len(ring) < 4:
                            continue
                        poly = Polygon([grid.m(x, y) for x, y in ring])
                        if poly.is_valid and poly.area >= 12.0:
                            buildings.append((poly, use, h))
                    continue
                cls = None
                for k in ('natural', 'water', 'waterway', 'landuse', 'leisure', 'amenity', 'place', 'highway', 'aeroway', 'man_made'):
                    v = tags.get(k)
                    if v is None:
                        continue
                    if (k == 'natural' and v == 'water') or k == 'water' or (k == 'waterway' and v in ('riverbank', 'dock', 'canal', 'river')):
                        cls = 'water'; break
                    if (k, v) in LANDUSE:
                        cls = LANDUSE[(k, v)]; break
                if cls is None and (tags.get('power') in ('plant', 'substation') or tags.get('amenity') in ('fuel', 'hospital')):
                    r0 = rings[0][0]
                    c = grid.m(sum(x for x, _ in r0) / len(r0), sum(y for _, y in r0) / len(r0))
                    kind = P_POWER if tags.get('power') else (P_FUEL if tags.get('amenity') == 'fuel' else P_HOSPITAL)
                    pois.append((kind, c))
                    continue
                if cls is None and tags.get('aeroway') == 'aerodrome':
                    r0 = rings[0][0]
                    pois.append((P_AIRFIELD, grid.m(sum(x for x, _ in r0) / len(r0), sum(y for _, y in r0) / len(r0))))
                if cls is None:
                    continue
                for ring, holes in rings:
                    item = ([grid.px(x, y) for x, y in ring], [[grid.px(x, y) for x, y in h] for h in holes])
                    (water_polys if cls == 'water' else surf_polys).append((cls, item) if cls != 'water' else item)
    log(f'OSM: {seen} objects in {time.time() - t0:.0f} s: {len(buildings)} buildings, {len(roads)} roads, {len(surf_polys)} land areas, '
        f'{len(water_polys)} water areas, {len(waterways)} waterways, {len(trees)} trees, {len(pois)} points of interest')
    return dict(surf=surf_polys, water=water_polys, buildings=buildings, roads=roads, rails=rails, waterways=waterways, trees=trees, pois=pois)


def building_use(b, tags):
    if b in ('house', 'detached', 'semidetached_house', 'terrace', 'bungalow', 'residential', 'farm', 'cabin', 'hut'):
        return B_HOUSE
    if b in ('apartments', 'dormitory', 'hotel'):
        return B_APARTMENTS
    if b in ('commercial', 'retail', 'office', 'supermarket', 'kiosk', 'shop'):
        return B_COMMERCIAL
    if b in ('industrial', 'warehouse', 'factory', 'manufacture', 'hangar', 'storage_tank', 'service', 'transformer_tower'):
        return B_INDUSTRIAL
    if b in ('church', 'cathedral', 'chapel', 'mosque', 'synagogue', 'temple', 'religious', 'shrine'):
        return B_RELIGIOUS
    if b in ('civic', 'public', 'government', 'school', 'university', 'college', 'hospital', 'museum', 'train_station', 'transportation', 'stadium'):
        return B_CIVIC
    if b in ('shed', 'garage', 'garages', 'roof', 'carport', 'greenhouse', 'allotment_house', 'container'):
        return B_SHED
    return B_APARTMENTS if tags.get('building:levels') and _num(tags.get('building:levels'), 0) >= 4 else B_COMMERCIAL


def _num(s, default):
    try:
        return float(str(s).split(';')[0].replace('m', '').strip())
    except (ValueError, TypeError):
        return default


def building_height(tags, use):
    h = _num(tags.get('height'), 0)
    if h <= 0:
        lv = _num(tags.get('building:levels'), 0)
        h = lv * 3.2 if lv > 0 else {B_HOUSE: 8, B_APARTMENTS: 16, B_COMMERCIAL: 14, B_INDUSTRIAL: 9, B_CIVIC: 15, B_RELIGIOUS: 18, B_TOWER: 60, B_SHED: 3}[use]
    return min(h, 400.0)


def load_dem(grid):
    """Ground height in metres on a 10 m grid covering the map (Copernicus DSM, buildings removed)."""
    import rasterio
    from scipy import ndimage
    step = 5                                   # dem cells every 5 terrain cells (10 m)
    m = grid.n // step + 2
    xs = (np.arange(m) * step * MPC); ys = (np.arange(m) * step * MPC)
    lon = grid.lon0 + (xs - grid.n * MPC / 2.0) / grid.kx
    lat = grid.lat0 + (ys - grid.n * MPC / 2.0) / grid.ky
    out = np.full((m, m), np.nan, np.float32)
    tiles = set()
    for la in range(int(math.floor(lat.min())), int(math.floor(lat.max())) + 1):
        for lo in range(int(math.floor(lon.min())), int(math.floor(lon.max())) + 1):
            tiles.add(('N' if la >= 0 else 'S') + '%02d_00_' % abs(la) + ('E' if lo >= 0 else 'W') + '%03d_00' % abs(lo))
    for t in sorted(tiles):
        path = os.path.join(CACHE, 'cop_' + t + '.tif')
        try:
            fetch(COPDEM.format(t=t), path)
        except Exception as e:                # tiles over open sea do not exist
            log('no elevation tile', t, '(sea?)', e); continue
        with rasterio.open(path) as ds:
            a = ds.read(1).astype(np.float32)
            inv = ~ds.transform
            LON, LAT = np.meshgrid(lon, lat)
            col, row = inv * (LON, LAT)
            ok = (col >= 0) & (row >= 0) & (col < ds.width - 1) & (row < ds.height - 1)
            c0 = np.clip(np.floor(col).astype(int), 0, ds.width - 2); r0 = np.clip(np.floor(row).astype(int), 0, ds.height - 2)
            fc = col - c0; fr = row - r0
            v = (a[r0, c0] * (1 - fc) * (1 - fr) + a[r0, c0 + 1] * fc * (1 - fr) + a[r0 + 1, c0] * (1 - fc) * fr + a[r0 + 1, c0 + 1] * fc * fr)
            out[ok] = v[ok]
    out = np.where(np.isnan(out), 0.0, out)
    out = np.where(out < -50, 0.0, out)
    # surface model -> ground: a minimum over ~80 m drops buildings and trees, a Gaussian over ~60 m smooths what is left
    ground = ndimage.grey_opening(out, size=(9, 9))
    ground = ndimage.gaussian_filter(ground, 6)
    return ground, step


def raster_lines(draw, pts, width_px, fill):
    if len(pts) < 2:
        return
    draw.line(pts, fill=fill, width=max(1, int(round(width_px))), joint='curve')
    if width_px >= 3:
        r = width_px / 2.0
        for x, y in (pts[0], pts[-1]):
            draw.ellipse((x - r, y - r, x + r, y + r), fill=fill)


def build(city, size, out, preview):
    sz = SIZES[size]; cells = sz['cells']
    from PIL import Image, ImageDraw
    from scipy import ndimage
    import shapely
    cfg = CITIES[city]
    grid = Grid(cfg['centre'][0], cfg['centre'][1], cells)
    n = cells
    log(f'{cfg["title"]}: {n} x {n} cells = {n * MPC / 1000:.1f} km across, lon {grid.lon_min:.4f}..{grid.lon_max:.4f}, lat {grid.lat_min:.4f}..{grid.lat_max:.4f}')
    osm = parse_osm(cfg, grid)
    rng = np.random.default_rng(20261015)

    # ---- surface classes
    ground = S_DESERT if cfg['ground'] == 'desert' else S_GRASS
    surf = Image.new('L', (n, n), ground); d = ImageDraw.Draw(surf)
    for cls in LANDUSE_ORDER:
        for c, (ring, holes) in osm['surf']:
            if c == cls and len(ring) >= 3:
                d.polygon(ring, fill=cls)
                for h in holes:
                    if len(h) >= 3:
                        d.polygon(h, fill=ground)
    for pp in osm['rails']:
        raster_lines(d, pp, 6 / MPC, S_RAIL)
    # water: polygons, then linear waterways (rivers mapped only as lines)
    water = Image.new('L', (n, n), 0); dw = ImageDraw.Draw(water)
    for ring, holes in osm['water']:
        if len(ring) >= 3:
            dw.polygon(ring, fill=255)
            for h in holes:
                if len(h) >= 3:
                    dw.polygon(h, fill=0)
    for pp, w in osm['waterways']:
        raster_lines(dw, pp, w / MPC, 255)
    # roads last: bridges become causeways over water, so units cross where the real bridges are
    roadmask = Image.new('L', (n, n), 0); dr = ImageDraw.Draw(roadmask)
    for major in (False, True):
        for hw, pp, bridge, _ in osm['roads']:
            if (hw in MAJOR) != major:
                continue
            cls = S_DIRT if hw == 'track' else (S_ROAD_MAJOR if major else (S_PLAZA if hw == 'pedestrian' else S_ROAD_MINOR))
            raster_lines(d, pp, ROAD_W[hw] / MPC, cls)
            raster_lines(dr, pp, ROAD_W[hw] / MPC, 255 if bridge else 128)
    S = np.array(surf, np.uint8)
    W = np.array(water, np.uint8) > 0
    R = np.array(roadmask, np.uint8)
    W &= ~(R == 255)                                   # bridges
    # sea: low ground (at or below 0.5 m in the elevation model) connected to the map edge, where OSM has no land use
    dem, step = load_dem(grid)
    demc = ndimage.zoom(dem, step, order=1)[:n, :n][::-1]     # to cells, rows north-first like the images
    low = (demc <= 0.5) & (S == ground) & (R == 0)
    lab, nl = ndimage.label(low)
    edge = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
    sea = np.isin(lab, list(edge)) if edge else np.zeros_like(low)
    if sea.any():
        log(f'sea: {sea.mean() * 100:.1f}% of the map')
    W |= sea
    S[W] = S_WATERBED

    # ---- buildings: footprint rectangles, blocked cells
    blocked = Image.new('L', (n, n), 0); db = ImageDraw.Draw(blocked)
    polys = [b[0] for b in osm['buildings']]
    rects = shapely.minimum_rotated_rectangle(np.array(polys, dtype=object)) if polys else []
    blds = []
    for (poly, use, h), rect in zip(osm['buildings'], rects):
        c = np.array(rect.exterior.coords)[:4]
        e1, e2 = c[1] - c[0], c[2] - c[1]
        l1, l2 = float(np.hypot(*e1)), float(np.hypot(*e2))
        if l1 < l2:
            e1, l1, l2 = e2, l2, l1
        ang = math.atan2(e1[1], e1[0])
        cx, cy = poly.centroid.x, poly.centroid.y
        if not (0 <= cx < n * MPC and 0 <= cy < n * MPC):
            continue
        use2 = B_TOWER if h >= 60 else use
        blds.append([cx, cy, ang, l1, l2, h, use2])
        db.polygon([(x / MPC, n - y / MPC) for x, y in poly.exterior.coords], fill=255)
    B = np.array(blocked, np.uint8) > 0
    B &= ~W
    log(f'{len(blds)} buildings in the map, {B.mean() * 100:.1f}% of cells under buildings')
    # ground OSM leaves unlabelled between dense buildings (Manhattan, central Paris) is paved, not grass or desert
    dense = ndimage.uniform_filter(B.astype(np.float32), size=40) > 0.18
    S[(S == ground) & dense & ~W] = S_URBAN

    # ---- heights: compress real relief into the engine's 0..255 range (0.625 units per step), water below one flat sea level
    land = ~W
    lo = float(np.percentile(demc[land], 1)) if land.any() else 0.0
    hi = float(np.percentile(demc[land], 99.5)) if land.any() else 1.0
    rng_m = max(hi - lo, 1.0)
    per_m = min(8.0, 200.0 / rng_m)                     # 8 steps per metre would be true scale (1 m = 5 units)
    H = 40.0 + np.clip(demc - lo, 0, None) * per_m
    H = np.minimum(H, 250.0)
    dist_w = ndimage.distance_transform_edt(~W) if W.any() else np.full((n, n), 1e9)
    H = np.where(W, 18.0 + np.clip(ndimage.distance_transform_edt(W), 0, 6) * -1.5, H)   # beds slope down from 18 to 9
    bank = (~W) & (dist_w < 6)
    H = np.where(bank, np.maximum(H, 30.0 + dist_w * 1.5), H)   # banks rise from just above the water (level 24)
    log(f'relief: {lo:.0f}..{hi:.0f} m -> {per_m:.2f} height steps per metre (true scale would be 8)')

    # ---- spawn lots: open, flat, dry ground far from buildings; cleared building sites where the city is too dense
    free = ~(B | W)
    edt = ndimage.distance_transform_edt(free)
    lot_r = sz['lot_r']                                 # cells: cleared radius of a base (large: 60 = 120 m)
    spacing = sz['spacing']                             # cells between lot centres (large: 260 = 520 m)
    margin = BORDER + lot_r + 20
    lots = []
    def far_enough(x, y, sp):
        return all((x - a) ** 2 + (y - b) ** 2 >= sp * sp for a, b, _ in lots)
    parkish = np.isin(S, list(PARKISH | {S_DESERT, S_BROWN, S_PARKING}))
    cand = np.argwhere((edt >= lot_r * 0.8) & parkish)
    cand = cand[(cand[:, 0] > margin) & (cand[:, 0] < n - margin) & (cand[:, 1] > margin) & (cand[:, 1] < n - margin)]
    rng.shuffle(cand)
    for r, c in cand[:30000]:
        if far_enough(c, r, spacing):
            lots.append((c, r, 'open'))
    natural = len(lots)
    # fill the gaps on a jittered grid: those lots are cleared building sites
    g = spacing
    for r in range(margin, n - margin, g):
        for c in range(margin, n - margin, g):
            x = int(c + rng.uniform(-0.25, 0.25) * g); y = int(r + rng.uniform(-0.25, 0.25) * g)
            if not (margin <= x < n - margin and margin <= y < n - margin):
                continue
            if W[max(0, y - lot_r):y + lot_r, max(0, x - lot_r):x + lot_r].mean() > 0.05:
                continue
            if far_enough(x, y, spacing * 0.85):
                lots.append((x, y, 'cleared'))
    log(f'{len(lots)} spawn lots ({natural} on open ground, {len(lots) - natural} cleared building sites)')
    yy, xx = np.ogrid[:n, :n]
    L = np.zeros((n, n), bool)
    removed = 0
    for x, y, how in lots:
        y0, y1, x0, x1 = max(0, y - lot_r - 30), min(n, y + lot_r + 30), max(0, x - lot_r - 30), min(n, x + lot_r + 30)
        sub_d = np.hypot(xx[:, x0:x1] - x, yy[y0:y1, :] - y)
        inner = sub_d <= lot_r
        L[y0:y1, x0:x1] |= inner
        # flatten: the lot takes its mean height, blending back over 30 cells
        level = float(np.mean(H[y0:y1, x0:x1][inner]))
        wgt = np.clip((sub_d - lot_r) / 30.0, 0, 1)
        H[y0:y1, x0:x1] = np.where(W[y0:y1, x0:x1], H[y0:y1, x0:x1], level * (1 - wgt) + H[y0:y1, x0:x1] * wgt)
        if how == 'cleared':
            S[y0:y1, x0:x1] = np.where(inner & ~W[y0:y1, x0:x1], S_BROWN, S[y0:y1, x0:x1])
    # buildings touching a lot are demolished (cells freed, records dropped)
    bxy = np.array([(b[0] / MPC, n - b[1] / MPC, max(b[3], b[4]) / MPC / 2) for b in blds]) if blds else np.zeros((0, 3))
    gone = np.zeros(len(blds), bool)
    for x, y, _ in lots:
        gone |= (bxy[:, 0] - x) ** 2 + (bxy[:, 1] - y) ** 2 < (lot_r + bxy[:, 2] + 4) ** 2
    removed = int(gone.sum())
    keep = [b for b, g in zip(blds, gone) if not g]
    blds = keep
    B &= ~ndimage.binary_dilation(L, iterations=4)
    log(f'{removed} buildings demolished for spawn lots')

    # ---- scenery points: trees (OSM + parks/woods), bushes, parked cars, points of interest
    objs = []
    def ok_cell(xm, ym):
        c, r = int(xm / MPC), int(n - ym / MPC)
        return 0 <= c < n and 0 <= r < n and not (B[r, c] or W[r, c] or L[r, c])
    for xm, ym in osm['trees']:
        if ok_cell(xm, ym):
            objs.append((K_TREE, 0, xm, ym, rng.uniform(0, 6.283), 0, 0, 0))
    for cls, density, kind in ((S_FOREST, 1 / 90.0, K_TREE), (S_GRASS, 1 / 900.0, K_TREE), (S_GRASS, 1 / 1400.0, K_BUSH), (S_FARM, 1 / 4000.0, K_TREE)):
        if ground == S_DESERT and cls == S_GRASS:
            density /= 3
        mask = (S == cls) & ~B & ~W & ~L
        cnt = int(mask.sum() * MPC * MPC * density)
        idx = np.flatnonzero(mask)
        if cnt and len(idx):
            pick = rng.choice(idx, size=min(cnt, len(idx)), replace=False)
            for k in pick:
                r, c = divmod(int(k), n)
                objs.append((kind, 0, (c + rng.uniform(0, 1)) * MPC, (n - r - rng.uniform(0, 1)) * MPC, rng.uniform(0, 6.283), 0, 0, 0))
    for hw, _, bridge, pm in osm['roads']:
        gap = CAR_ROADS.get(hw)
        if not gap or bridge:
            continue
        off = ROAD_W[hw] / 2.0 - 1.2
        for (x0, y0), (x1, y1) in zip(pm, pm[1:]):
            seg = math.hypot(x1 - x0, y1 - y0)
            if seg < 8:
                continue
            ux, uy = (x1 - x0) / seg, (y1 - y0) / seg
            t = rng.uniform(3, gap)
            while t < seg - 3:
                for side in (1, -1):
                    if rng.random() < 0.55:
                        x, y = x0 + ux * t - uy * off * side, y0 + uy * t + ux * off * side
                        if ok_cell(x, y):
                            objs.append((K_CAR, 0, x, y, math.atan2(uy, ux) + (0 if side > 0 else math.pi), 0, 0, 0))
                t += gap * rng.uniform(0.7, 1.3)
    for kind, (xm, ym) in osm['pois']:
        if ok_cell(xm, ym):
            objs.append((K_POI, kind, xm, ym, 0, 0, 0, 0))
    for x, y, how in lots:
        objs.append((K_LOT, 1 if how == 'open' else 0, x * MPC + MPC / 2, (n - y) * MPC - MPC / 2, 0, lot_r * MPC, 0, 0))
    for cx, cy, ang, l1, l2, h, use in blds:
        objs.append((K_BUILDING, use, cx, cy, ang, l1, l2, h))
    counts = {k: sum(1 for o in objs if o[0] == v) for k, v in (('buildings', K_BUILDING), ('trees', K_TREE), ('bushes', K_BUSH), ('cars', K_CAR), ('poi', K_POI), ('lots', K_LOT))}
    log('objects:', counts)

    # ---- flags, flip rows to south-first (the engine's row 0 is the south edge), write
    F = (B.astype(np.uint8) * F_BLOCKED) | (W.astype(np.uint8) * F_WATER) | (L.astype(np.uint8) * F_LOT)
    Hb = np.clip(np.round(H), 1, 255).astype(np.uint8)
    header = dict(format=1, city=f'{city}-{size}', size=size, title=f"{cfg['title']} {sz['label']}", cells=n, border=BORDER, metres_per_cell=MPC, units_per_metre=UPM,
                  centre=cfg['centre'], bbox=[grid.lon_min, grid.lat_min, grid.lon_max, grid.lat_max], water_level=24,
                  surfaces=SURFACE_NAMES, ground=SURFACE_NAMES[ground], lot_radius_cells=lot_r, objects=counts,
                  relief=dict(low_m=lo, high_m=hi, steps_per_m=per_m),
                  attribution=['Map data (c) OpenStreetMap contributors, ODbL 1.0 (openstreetmap.org/copyright)',
                               'Elevation: Copernicus GLO-30 DEM, (c) DLR e.V. 2010-2014 and (c) Airbus Defence and Space GmbH 2014-2018, provided under COPERNICUS by the European Union and ESA'],
                  generated=time.strftime('%Y-%m-%d'))
    body = bytearray()
    body += Hb[::-1].tobytes(); body += S[::-1].tobytes(); body += F[::-1].tobytes()
    body += struct.pack('<I', len(objs))
    for o in objs:
        body += struct.pack('<BBHffffff', o[0], o[1], 0, o[2], o[3], o[4], o[5], o[6], o[7])
    hj = json.dumps(header).encode()
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, 'wb') as f:
        f.write(b'LWC1' + struct.pack('<I', len(hj)) + hj + zlib.compress(bytes(body), 9))
    log(f'wrote {out} ({os.path.getsize(out) / 1e6:.1f} MB)')
    if preview:
        write_preview(preview, S, B, W, L, objs, n)


def write_preview(path, S, B, W, L, objs, n, size=2000):
    from PIL import Image, ImageDraw
    pal = np.array([(110, 150, 80), (50, 95, 45), (170, 160, 90), (150, 145, 135), (70, 70, 75), (95, 95, 100), (180, 170, 150), (225, 210, 160),
                    (60, 90, 150), (120, 100, 90), (110, 110, 115), (150, 120, 90), (220, 195, 140), (140, 110, 80)], np.uint8)
    img = pal[S]
    img[B] = (60, 50, 50)
    img[W] = (60, 100, 170)
    im = Image.fromarray(img).resize((size, size), Image.BILINEAR)
    d = ImageDraw.Draw(im)
    k = size / n
    for o in objs:
        if o[0] == K_LOT:
            x, y, r = o[2] / MPC * k, (n - o[3] / MPC) * k, o[5] / MPC * k
            d.ellipse((x - r, y - r, x + r, y + r), outline=(255, 40, 40) if o[1] else (255, 160, 0), width=2)
        elif o[0] == K_POI:
            x, y = o[2] / MPC * k, (n - o[3] / MPC) * k
            d.rectangle((x - 3, y - 3, x + 3, y + 3), fill=(255, 255, 0))
    im.save(path)
    log('preview', path)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('city', choices=sorted(CITIES))
    ap.add_argument('--size', choices=sorted(SIZES), default='large')
    ap.add_argument('--out')
    ap.add_argument('--preview')
    a = ap.parse_args()
    build(a.city, a.size, a.out or os.path.join(LAB, 'cities', f'{a.city}-{a.size}.lwcity'), a.preview)
