// Living World: real-city maps. Builds "Living World <City>" (for example Living World London) from a city pack (cities/<city>.lwcity, made by
// tools/citygen/citygen.py from OpenStreetMap and Copernicus elevation data) and the player's own Zero Hour files.
//
// The pack holds the real-world part: per terrain cell a height, a surface class and flags (blocked by a building, water, spawn lot), and a list of
// buildings, trees, bushes, parked cars, points of interest and spawn lots. This file turns that into a map:
//   - surface classes become terrain textures, picked by name from the game's Terrain.ini (no texture is shipped);
//   - one flat water plane covers the map; river beds and the sea lie below it, the land above it;
//   - most buildings are drawn-only props ("lwProp", scaled to the real footprint) over cells flagged impassable (cliff bits), so a city of
//     100 000+ buildings costs no logic; a few near every spawn lot are real, garrisonable civilian buildings;
//   - trees and bushes are optimised trees (client-side only), parked cars are props;
//   - fuel stations, hospitals, power stations and airfields become tech buildings; every spawn lot gets a supply dock and piles;
//   - every spawn lot is a waypoint LW_Lot_<n>; the first 128 also carry the Player_<n>_Start names the skirmish set-up needs.
// The map, its map.ini (tree types) and a stamp of the pack it came from are written to the user's Maps folder. Every process (server and
// clients) builds the same file from the same pack, so the server's snapshots only carry a reference to the map (see GameStateMap.cpp).
#include "PreRTS.h"
#include "Common/FileSystem.h"
#include "Common/GlobalData.h"
#include "Common/AsciiString.h"
#include "Common/File.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/MapObject.h"
#include "GameClient/MapUtil.h"
#include "ZLib/zlib.h"
#include "um_mapfile.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

using namespace um_map;

namespace
{
const int CITYMAP_VERSION = 2;          // bump when the builder's output changes (forces a rebuild of maps made by older builds)

enum { K_BUILDING = 1, K_TREE, K_BUSH, K_CAR, K_POI, K_LOT };
enum { B_HOUSE, B_APARTMENTS, B_COMMERCIAL, B_INDUSTRIAL, B_CIVIC, B_RELIGIOUS, B_TOWER, B_SHED, B_USES };
enum { P_FUEL, P_HOSPITAL, P_POWER, P_AIRFIELD, P_PORT, P_POLICE, P_STATION };
enum { F_BLOCKED = 1, F_WATER = 2, F_LOT = 4 };

#pragma pack(push, 1)
struct PackObj { unsigned char kind, sub; unsigned short pad; float x, y, ang, a, b, c; };   // x, y in metres from the grid's south-west corner
#pragma pack(pop)

struct Pack
{
	std::string header, title, city;
	int n, border, waterLevel, lotRadiusCells; double mpc;
	std::string H, S, F; std::vector<PackObj> objs; std::vector<std::string> surfaces; std::string ground;
	unsigned long crc; size_t fileSize;
};

// --- tiny JSON readers for the pack header (flat keys only)
std::string jsonRaw(const std::string &j, const char *key)
{
	std::string k = std::string("\"") + key + "\":"; size_t p = j.find(k); if (p == std::string::npos) return std::string();
	p += k.size(); while (p < j.size() && j[p] == ' ') ++p;
	size_t e = p; int depth = 0; bool str = false;
	for (; e < j.size(); ++e) { char ch = j[e]; if (str) { if (ch == '\\') ++e; else if (ch == '"') str = false; continue; } if (ch == '"') str = true; else if (ch == '[' || ch == '{') ++depth; else if (ch == ']' || ch == '}') { if (depth == 0) break; --depth; } else if (ch == ',' && depth == 0) break; }
	return j.substr(p, e - p);
}
std::string jsonStr(const std::string &j, const char *key) { std::string r = jsonRaw(j, key); if (r.size() >= 2 && r[0] == '"') return r.substr(1, r.size() - 2); return r; }
double jsonNum(const std::string &j, const char *key, double def) { std::string r = jsonRaw(j, key); return r.empty() ? def : atof(r.c_str()); }
std::vector<std::string> jsonStrList(const std::string &j, const char *key)
{
	std::vector<std::string> out; std::string r = jsonRaw(j, key); size_t p = 0;
	while ((p = r.find('"', p)) != std::string::npos) { size_t e = r.find('"', p + 1); if (e == std::string::npos) break; out.push_back(r.substr(p + 1, e - p - 1)); p = e + 1; }
	return out;
}

std::string exeDir()
{
	char b[MAX_PATH]; DWORD n = GetModuleFileNameA(NULL, b, MAX_PATH); std::string s(b, n); size_t k = s.find_last_of("\\/"); return k == std::string::npos ? std::string() : s.substr(0, k + 1);
}

bool readWhole(const std::string &path, std::string &out)
{
	FILE *f = fopen(path.c_str(), "rb"); if (!f) return false;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); out.resize(n > 0 ? n : 0);
	size_t r = n > 0 ? fread(&out[0], 1, n, f) : 0; fclose(f); return (long)r == n;
}

std::string findPack(const std::string &city)
{
	std::vector<std::string> dirs; if (const char *d = getenv("UM_CITY_DIR")) dirs.push_back(std::string(d) + "\\");
	dirs.push_back(exeDir() + "cities\\"); dirs.push_back("cities\\");
	for (size_t i = 0; i < dirs.size(); ++i) { std::string p = dirs[i] + city + ".lwcity"; if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p; }
	return std::string();
}

bool loadPack(const std::string &path, Pack &pk)
{
	std::string d; if (!readWhole(path, d) || d.size() < 8 || memcmp(d.data(), "LWC1", 4) != 0) return false;
	pk.fileSize = d.size(); pk.crc = crc32(0L, (const Bytef *)d.data(), (uInt)d.size());
	unsigned hl = get32(d, 4); if (8 + (size_t)hl > d.size()) return false;
	pk.header = d.substr(8, hl);
	pk.n = (int)jsonNum(pk.header, "cells", 0); pk.border = (int)jsonNum(pk.header, "border", 40); pk.mpc = jsonNum(pk.header, "metres_per_cell", 2.0);
	pk.waterLevel = (int)jsonNum(pk.header, "water_level", 24); pk.lotRadiusCells = (int)jsonNum(pk.header, "lot_radius_cells", 60);
	pk.title = jsonStr(pk.header, "title"); pk.city = jsonStr(pk.header, "city"); pk.surfaces = jsonStrList(pk.header, "surfaces"); pk.ground = jsonStr(pk.header, "ground");
	if (pk.n < 100 || pk.n > 8000) return false;
	// inflate the body: three n*n layers, then the object list
	z_stream zs; memset(&zs, 0, sizeof zs); if (inflateInit(&zs) != Z_OK) return false;
	zs.next_in = (Bytef *)&d[8 + hl]; zs.avail_in = (uInt)(d.size() - 8 - hl);
	std::string body; body.resize((size_t)pk.n * pk.n * 3 + (64u << 20)); size_t have = 0; int rc = Z_OK;
	while (rc == Z_OK) { if (have == body.size()) body.resize(body.size() * 2); zs.next_out = (Bytef *)&body[have]; zs.avail_out = (uInt)(body.size() - have); rc = inflate(&zs, Z_NO_FLUSH); have = body.size() - zs.avail_out; }
	inflateEnd(&zs); if (rc != Z_STREAM_END) return false; body.resize(have);
	size_t nn = (size_t)pk.n * pk.n; if (body.size() < nn * 3 + 4) return false;
	pk.H = body.substr(0, nn); pk.S = body.substr(nn, nn); pk.F = body.substr(2 * nn, nn);
	unsigned cnt = get32(body, 3 * nn); if (body.size() < 3 * nn + 4 + (size_t)cnt * sizeof(PackObj)) return false;
	pk.objs.resize(cnt); if (cnt) memcpy(&pk.objs[0], &body[3 * nn + 4], (size_t)cnt * sizeof(PackObj));
	return true;
}

// --- terrain textures for the pack's surface classes (Terrain.ini names; the first that exists is used)
const char *textureFor(const std::string &surface, bool desertCity)
{
	struct T { const char *s, *t, *desert; };
	static const T tab[] = {
		{ "grass", "GrassLargeType1", "SandMediumType13grassy" }, { "forest", "GrassMediumType12Leaves", "GrassMediumType27olive" },
		{ "farm", "GrassMediumType22", "SandLargeType4Grassy" }, { "urban", "Cobble01", "SandMediumType6Light" }, { "road_major", "AsphaltType1", "AsphaltType1" },
		{ "road_minor", "AsphaltType2", "AsphaltType2" }, { "plaza", "Cobble02", "Cobble04" }, { "sand", "SandType1", "SandType1" },
		{ "waterbed", "SandType3wet", "SandMediumType9blue" }, { "rail", "RocksType4", "RocksType4" }, { "parking", "ParkingLotType1a", "ParkingLotType1a" },
		{ "brownfield", "DirtType1", "SandMediumType5" }, { "desert", "SandMediumType9", "SandMediumType9" }, { "dirt", "DirtType2", "SandMediumType4" } };
	for (size_t i = 0; i < sizeof tab / sizeof tab[0]; ++i) if (surface == tab[i].s) return desertCity ? tab[i].desert : tab[i].t;
	return "GrassLargeType1";
}

// --- building templates per use: the builder takes the one whose footprint fills the real footprint best
struct Style { const char *uses[B_USES]; const char *trees, *bushes, *cars; };
const Style &styleFor(const std::string &city)
{
	static const Style euro = { {
		"GermanTownHouse1 GermanTownHouse2 GermanCotage01 FarmHouse RiverHouse02 Chalet",
		"StanApartment01 StanApartment02 EuroHighrise2 AsianApartmentUnit MogadishuHouse13 SovietBuilding",
		"GermanPub01 GermanPub02 StanSmallRetail01 StanSmallRetail02 StanSmallRetail03 AsianRetailStore01 ConvienceStore StanRestaurant01 StanHotel01 AsianElectricRetail",
		"IndustrialBuilding01 IndustrialBuilding02 VehicleHanger01 StanHanger01 CivilianHangar WaterPlant",
		"GermanTownHall EuropeanMuseum StanTownHall02 SovietGovernmentBuildingSmall StanSovietOffice01 StanSovietOffice02 EuropeanParkingGarage",
		"EuropeanMuseum GermanTownHall Crypt01",
		"EuroHighrise CivilianHighrise01 AsianOffice01 AsianBank AsianHotel01 MogadishuHighrise01",
		"Shack BarnShed QuonsetHut01 MogadishuGarage" },
		"LWTreeOak05 LWTreeOak06 LWTreeBirch05 LWTreeBirch06 LWTreeBirch07 TreeDogwood1",
		"Bush01 LWBush17 LWBush18 LWBush20 LWBush21",
		"CarSedan01 CarCompact CarSUV01 CarTaxiCab01 CarEuroVan1 CarLuxury01 CarBug1 CarSwatchEuro CarEuroVan2" };
	static const Style ny = { {
		"GermanTownHouse1 GermanTownHouse2 RiverHouse02 StanApartment02",
		"StanApartment01 EuroHighrise2 AsianApartmentUnit SovietBuilding MogadishuHighrise02 CivilianHighrise01",
		"StanSmallRetail01 StanSmallRetail02 AsianRetailStore01 AsianRetailStore02 ConvienceStore StanRestaurant01 AsianElectricRetail AsianDealership AsianNightClub",
		"IndustrialBuilding01 IndustrialBuilding02 VehicleHanger01 StanHanger01 CivilianHangar",
		"StanTownHall02 SovietGovernmentBuildingSmall StanSovietOffice01 StanSovietOffice02 EuropeanParkingGarage EuropeanMuseum",
		"EuropeanMuseum Crypt01",
		"EuroHighrise CivilianHighrise01 AsianOffice01 AsianBank AsianHotel01 MogadishuHighrise01 AsianTVStation",
		"Shack QuonsetHut01 MogadishuGarage" },
		"LWTreeOak05 LWTreeOak06 LWTreeBirch05 TreeDogwood1",
		"Bush01 LWBush17 LWBush20",
		"CarSedan01 CarCompact CarSUV01 CarTaxiCab02 CarTaxiCab03 CarLuxury01 CarLuxury02 CarSport01 CarCamaro" };
	static const Style desert = { {
		"MogadishuHouse02 MogadishuHouse04 MogadishuHouse05 MogadishuHouse06 MogadishuHouse15 MogadishuArchedHouse",
		"MogadishuHouse13 MogadishuHouse07 StanApartment01 AsianApartmentUnit MogadishuSmallApartment",
		"StanSmallRetail01 StanConvenienceStore02 AsianRetailStore01 StanRestaurant02 MogadishuShop01 AsianDealership StanHotel02",
		"IndustrialBuilding01 IndustrialBuilding02 StanHanger01 CivilianHangar StanOilTank01",
		"StanTownHall01 StanTownHall02 SovietGovernmentBuildingSmall StanSovietOffice02",
		"MidEastCathedral02 MogadishuDome",
		"AsianHotel01 AsianBank AsianOffice01 EuroHighrise CivilianHighrise01 MogadishuHighrise01 MogadishuTower05",
		"Shack MogadishuGarage Tent04" },
		"LWTreePalm01 LWTreePalm02 LWTreePalmX01",
		"LWBush17 DesertShrub09 LWBush20",
		"CarSedan01 CarSUV01 CarLuxury01 CarLuxury03 CarLimo2 CarSport01 CarTaxiCab01 TruckWork" };
	if (city == "dubai") return desert;
	if (city == "newyork") return ny;
	return euro;
}

struct Tpl { const ThingTemplate *tt; std::string name; float len, wid; };   // footprint in world units (len >= wid; rotated 90 degrees when the model is wider than long)

std::vector<Tpl> templates(const char *names, bool rotateAware)
{
	std::vector<Tpl> out; std::string s(names); size_t p = 0;
	while (p < s.size())
	{
		size_t e = s.find(' ', p); if (e == std::string::npos) e = s.size();
		std::string nm = s.substr(p, e - p); p = e + 1; if (nm.empty()) continue;
		const ThingTemplate *tt = TheThingFactory->findTemplate(AsciiString(nm.c_str())); if (!tt) continue;
		const GeometryInfo &g = tt->getTemplateGeometryInfo();
		Tpl t; t.tt = tt; t.name = nm; t.len = 2 * g.getMajorRadius(); t.wid = 2 * (g.getGeomType() == GEOMETRY_BOX ? g.getMinorRadius() : g.getMajorRadius());
		if (t.len < 1 || t.wid < 1) { t.len = t.wid = 30; }
		(void)rotateAware; out.push_back(t);
	}
	return out;
}

// world position (units) of a pack position (metres from the grid's south-west corner)
inline float wx(const Pack &pk, float m) { return (float)(m / pk.mpc - pk.border) * 10.0f; }

struct Rand { unsigned s; explicit Rand(unsigned seed) : s(seed) {} unsigned next() { s = s * 1664525u + 1013904223u; return s >> 8; } float uni(float lo, float hi) { return lo + (hi - lo) * (next() / 16777216.0f); } };

std::string mapIni(const Style &st)
{
	// tree and bush types for the game's own tree models (optimised trees: drawn by the client's tree buffer, no logic object).
	// map.ini is read as an override file, where new modules must come inside AddModule.
	struct T { const char *name, *model; float r, h; bool topple; };
	static const T trees[] = {
		{ "LWTreeOak05", "PTXOAK05", 6, 30, true }, { "LWTreeOak06", "PTXOAK06", 6, 30, true }, { "LWTreeBirch05", "PTXBIRCH05", 4, 28, true },
		{ "LWTreeBirch06", "PTXBIRCH06", 4, 28, true }, { "LWTreeBirch07", "PTXBIRCH07", 4, 28, true }, { "LWTreePalm01", "PTXPALM01", 4, 30, true },
		{ "LWTreePalm02", "PTXPALM02", 4, 30, true }, { "LWTreePalmX01", "PTPalm01", 4, 30, true }, { "LWBush17", "PTBush17", 3, 5, false },
		{ "LWBush18", "PTBush18", 3, 5, false }, { "LWBush20", "PTBush20", 3, 5, false }, { "LWBush21", "PTBush21", 3, 5, false } };
	std::string o = "; Living World city map: tree and bush types made from the game's own tree models (written by the map builder)\n\n";
	for (size_t i = 0; i < sizeof trees / sizeof trees[0]; ++i)
	{
		const T &t = trees[i]; char b[1600];
		sprintf(b, "Object %s\n  AddModule\n  Draw = W3DTreeDraw ModuleTag_01\n    ModelName = %s\n    TextureName = %s.tga\n    DoTopple = %s\n    DoShadow = Yes\n%s  End\n  End\n"
			"  DisplayName = OBJECT:DogwoodTree\n  EditorSorting = SHRUBBERY\n  CrushableLevel = 1\n  KindOf = SHRUBBERY IMMOBILE IGNORED_IN_GUI OPTIMIZED_TREE\n"
			"  Geometry = CYLINDER\n  GeometryIsSmall = Yes\n  GeometryMajorRadius = %.1f\n  GeometryHeight = %.1f\n  Shadow = SHADOW_DECAL\n  InstanceScaleFuzziness = 0.2\nEnd\n\n",
			t.name, t.model, t.model, t.topple ? "Yes" : "No",
			t.topple ? "    ToppleFX = FX_ToppleTree\n    BounceFX = FX_OptTreeBounce\n    KillWhenFinishedToppling = Yes\n    SinkDistance = 10\n    SinkTime = 5000\n" : "",
			t.r, t.h);
		o += b;
	}
	(void)st; return o;
}

int tgaTilesAcross(const char *terrainTexFile)
{
	AsciiString path(TERRAIN_TGA_DIR_PATH); path.concat(terrainTexFile);
	File *f = TheFileSystem->openFile(path.str(), File::READ | File::BINARY); if (!f) return 0;
	unsigned char h[18]; Int r = f->read(h, 18); f->close(); if (r != 18) return 0;
	int w = h[12] | (h[13] << 8); return w / 64;
}
}

// Terrain.ini lookup (texture file of a terrain type), from the engine
#include "Common/TerrainTypes.h"

std::string UmCityMapName(const std::string &title) { return std::string("Living World ") + title; }

// Builds the city map if it is missing or was made from another pack/builder version. Returns the map path relative to the user data folder
// (Maps\Living World <Title>\Living World <Title>.map) in mapPathOut. city is the pack name (<city>-<size>: london-medium, dubai-small, ...).
bool UmEnsureCityMap(const char *city, std::string *mapPathOut)
{
	std::string packPath = findPack(city);
	if (packPath.empty()) { printf("[um] city map: no pack for '%s' (looked for cities\\%s.lwcity next to the program)\n", city, city); return false; }
	Pack pk; if (!loadPack(packPath, pk)) { printf("[um] city map: cannot read %s\n", packPath.c_str()); return false; }
	std::string name = UmCityMapName(pk.title);
	AsciiString userData = TheGlobalData->getPath_UserData();
	std::string dir = std::string(userData.str()) + "Maps\\" + name + "\\", path = dir + name + ".map", stampPath = dir + "lwcity.stamp";
	if (mapPathOut) *mapPathOut = std::string("Maps\\") + name + "\\" + name + ".map";
	// UM_CITY_SKIP (measuring aid): letters leave content out: t trees/bushes, c cars, p drawn buildings, b real buildings, w water plane, k impassable cells, x tech, s supply
	const std::string skip = getenv("UM_CITY_SKIP") ? getenv("UM_CITY_SKIP") : "";
	char stamp[200]; sprintf(stamp, "pack %s crc %08lx size %lu builder %d skip '%s'\n", pk.city.c_str(), pk.crc, (unsigned long)pk.fileSize, CITYMAP_VERSION, skip.c_str());
	{ std::string old; if (readWhole(stampPath, old) && old == stamp && GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) return true; }
	DWORD t0 = GetTickCount();
	printf("[um] city map: building '%s' from %s (%d x %d cells, %u objects)\n", name.c_str(), packPath.c_str(), pk.n, pk.n, (unsigned)pk.objs.size());

	Source src;
	if (!src.load("Maps\\Tournament Desert\\Tournament Desert.map")) { printf("[um] city map: cannot read Tournament Desert from the game files\n"); return false; }
	if (!src.find("HeightMapData") || !src.find("BlendTileData") || !src.find("SidesList") || !src.find("GlobalLighting") || !src.find("WorldInfo")) { printf("[um] city map: unexpected map layout\n"); return false; }
	src.addName("lwProp"); src.addName("lwScale");
	const bool desertCity = pk.ground == "desert";
	const Style &st = styleFor(pk.city);
	const int N = pk.n, B = pk.border, inner = N - 2 * B;
	const size_t NN = (size_t)N * N;

	// ---- texture classes
	struct TC { std::string name; int first, nt, w; };
	std::vector<TC> classes; std::vector<int> classOf(pk.surfaces.size(), 0); int nextTile = 0;
	for (size_t s = 0; s < pk.surfaces.size(); ++s)
	{
		const char *want = textureFor(pk.surfaces[s], desertCity);
		int found = -1; for (size_t c = 0; c < classes.size(); ++c) if (classes[c].name == want) found = (int)c;
		if (found < 0)
		{
			TerrainType *tt = TheTerrainTypes ? TheTerrainTypes->findTerrain(AsciiString(want)) : NULL;
			int across = tt ? tgaTilesAcross(tt->getTexture().str()) : 0;
			if (across > 10) across = 10;
			if (across <= 0) { printf("[um] city map: terrain '%s' not found, using grass\n", want); found = classes.empty() ? -1 : 0; }
			if (across > 0 && nextTile + across * across <= 1000) { TC c; c.name = want; c.first = nextTile; c.nt = across * across; c.w = across; nextTile += c.nt; classes.push_back(c); found = (int)classes.size() - 1; }
			if (found < 0) found = 0;
		}
		classOf[s] = found;
	}
	if (classes.empty()) { printf("[um] city map: no terrain textures found\n"); return false; }

	// ---- height map
	std::string hb; put32(hb, N); put32(hb, N); put32(hb, B); put32(hb, 1); put32(hb, inner); put32(hb, inner); put32(hb, (unsigned)NN); hb += pk.H;
	std::string heightChunk = src.chunk("HeightMapData", 4, hb);

	// ---- tiles + cliff bits (impassable: blocked by a drawn-only building)
	std::string tiles; tiles.resize(NN * 2);
	for (int y = 0; y < N; ++y)
		for (int x = 0; x < N; ++x)
		{
			unsigned char s = (unsigned char)pk.S[(size_t)y * N + x]; const TC &c = classes[s < classOf.size() ? classOf[s] : 0];
			unsigned t = ((unsigned)(c.first + ((y >> 1) % c.w) * c.w + ((x >> 1) % c.w)) << 2) | (unsigned)(((y & 1) << 1) | (x & 1));
			unsigned short v = (unsigned short)t; memcpy(&tiles[((size_t)y * N + x) * 2], &v, 2);
		}
	const int fsw = (N + 7) / 8;
	std::string cliff((size_t)N * fsw, '\0');
	for (int y = 0; y < N; ++y)
		for (int x = 0; x < N; ++x)
			if ((pk.F[(size_t)y * N + x] & F_BLOCKED) && skip.find('k') == std::string::npos) cliff[(size_t)y * fsw + (x >> 3)] |= (char)(1 << (x & 7));

	// ---- objects
	Rand rnd(0x10c17u ^ pk.crc);
	std::vector<Tpl> useT[B_USES]; for (int u = 0; u < B_USES; ++u) useT[u] = templates(st.uses[u], true);
	std::vector<Tpl> treeT = templates(st.trees, false), bushT = templates(st.bushes, false), carT = templates(st.cars, false);
	std::string objs; int wid = 1, uid = 1, nLogic = 0, nProps = 0, nTrees = 0, nCars = 0, nTech = 0;
	char nm[96];
	// spawn lots: waypoints, supply, and the lot list
	std::vector<std::pair<float, float> > lots;
	for (size_t i = 0; i < pk.objs.size(); ++i) if (pk.objs[i].kind == K_LOT) lots.push_back(std::make_pair(wx(pk, pk.objs[i].x), wx(pk, pk.objs[i].y)));
	const float lotR = (float)pk.lotRadiusCells * 10.0f;
	for (size_t k = 0; k < lots.size(); ++k)
	{
		float x = lots[k].first, y = lots[k].second;
		sprintf(nm, "LW_Lot_%d", (int)k + 1);
		objs += object(src, x, y, 0.0f, "*Waypoints/Waypoint", true, wid++, nm, "Spawn Lots", "team", nm);
		if (k < 128) { sprintf(nm, "Player_%d_Start", (int)k + 1); objs += object(src, x, y, 0.0f, "*Waypoints/Waypoint", true, wid++, nm, "Start Points", "team", nm); }
		if (skip.find('s') != std::string::npos) continue;
		sprintf(nm, "SupplyDock %d", uid++); objs += object(src, x - 230, y + 70, 0.78f, "SupplyDock", false, 0, nm, "Supply Piles", "teamPlyrCivilian", "");
		static const int off[5][2] = { { 190, -50 }, { 225, -25 }, { 190, 0 }, { 225, 25 }, { 190, 50 } };
		for (int j = 0; j < 5; ++j) { sprintf(nm, "SupplyPileSmall %d", uid++); objs += object(src, x + off[j][0], y + off[j][1], 0.0f, "SupplyPileSmall", false, 0, nm, "Small Supply Piles", "teamPlyrCivilian", ""); }
	}
	// buildings near a lot are real, garrisonable buildings (up to 6 per lot); everything else is drawn-only
	std::vector<int> logicPerLot(lots.size(), 0);
	for (size_t i = 0; i < pk.objs.size(); ++i)
	{
		const PackObj &o = pk.objs[i]; if (o.kind != K_BUILDING) continue;
		float x = wx(pk, o.x), y = wx(pk, o.y), L = o.a * 5.0f, Wd = o.b * 5.0f;
		const std::vector<Tpl> &cand = useT[o.sub < B_USES ? o.sub : B_COMMERCIAL].empty() ? useT[B_COMMERCIAL] : useT[o.sub < B_USES ? o.sub : B_COMMERCIAL];
		if (cand.empty()) continue;
		// best fit: largest model that fits the footprint (rotated if that fits better), else the smallest
		int best = -1; float bestFill = -1; bool bestRot = false;
		for (size_t c = 0; c < cand.size(); ++c)
			for (int rot = 0; rot < 2; ++rot)
			{
				float tl = rot ? cand[c].wid : cand[c].len, tw = rot ? cand[c].len : cand[c].wid;
				if (tl > L * 1.15f || tw > Wd * 1.15f) continue;
				float fill = (tl * tw) / (L * Wd) + rnd.uni(0, 0.08f);
				if (fill > bestFill) { bestFill = fill; best = (int)c; bestRot = rot != 0; }
			}
		float scale = 1.0f;
		if (best < 0) { best = (int)(rnd.next() % cand.size()); bestRot = cand[best].wid > cand[best].len && Wd > 0; float tl = cand[best].len, tw = cand[best].wid; scale = std::max(0.45f, std::min(L / tl, Wd / tw)); }
		else { float tl = bestRot ? cand[best].wid : cand[best].len, tw = bestRot ? cand[best].len : cand[best].wid; scale = std::min(1.6f, std::max(0.8f, std::min(L / tl, Wd / tw))); }
		float ang = o.ang + (bestRot ? 1.5708f : 0.0f);
		// how many copies along the long side when one model is far shorter than a long building (terraces, blocks)
		float tl = (bestRot ? cand[best].wid : cand[best].len) * scale; int copies = 1;
		if (tl > 1 && L > tl * 1.8f) copies = std::min(6, (int)(L / tl));
		// real building near a lot?
		int nearLot = -1; for (size_t k = 0; k < lots.size(); ++k) { float dx = x - lots[k].first, dy = y - lots[k].second, d2 = dx * dx + dy * dy; if (d2 > (lotR + 60) * (lotR + 60) && d2 < (lotR + 900) * (lotR + 900)) { nearLot = (int)k; break; } }
		bool logic = nearLot >= 0 && copies == 1 && scale <= 1.15f && logicPerLot[nearLot] < 6;
		if (skip.find(logic ? 'b' : 'p') != std::string::npos) continue;
		for (int k = 0; k < copies; ++k)
		{
			float t = copies == 1 ? 0.0f : ((k + 0.5f) / copies - 0.5f) * L, cx = x + cosf(o.ang) * t, cy = y + sinf(o.ang) * t;
			sprintf(nm, "B%d", uid++);
			std::string body; putF(body, cx); putF(body, cy); putF(body, 0.0f); putF(body, ang); put32(body, 0); putAscii(body, cand[best].name);
			std::string dd; int nd = 0;
			dInt(dd, src, "objectInitialHealth", 100); ++nd; dBool(dd, src, "objectEnabled", true); ++nd; dBool(dd, src, "objectIndestructible", false); ++nd;
			dStr(dd, src, "originalOwner", "teamPlyrCivilian"); ++nd; dStr(dd, src, "uniqueID", nm); ++nd; dStr(dd, src, "objectLayer", logic ? "Buildings" : "City"); ++nd;
			if (!logic) { dBool(dd, src, "lwProp", true); ++nd; dReal(dd, src, "lwScale", scale); ++nd; }
			put16(body, (unsigned)nd); body += dd; objs += src.chunk("Object", 3, body);
		}
		if (logic)
		{
			++logicPerLot[nearLot]; ++nLogic;
			// a real building blocks movement by itself: clear the cliff bits under it
			int cx0 = (int)(x / 10) + B, cy0 = (int)(y / 10) + B, r = (int)(std::max(L, Wd) / 20) + 1;
			for (int yy = cy0 - r; yy <= cy0 + r; ++yy) for (int xx = cx0 - r; xx <= cx0 + r; ++xx) if (xx >= 0 && yy >= 0 && xx < N && yy < N) cliff[(size_t)yy * fsw + (xx >> 3)] &= (char)~(1 << (xx & 7));
		}
		else nProps += copies;
	}
	// trees, bushes, cars
	for (size_t i = 0; i < pk.objs.size(); ++i)
	{
		const PackObj &o = pk.objs[i];
		const std::vector<Tpl> *set = o.kind == K_TREE ? &treeT : o.kind == K_BUSH ? &bushT : o.kind == K_CAR ? &carT : NULL;
		if (!set || set->empty() || skip.find(o.kind == K_CAR ? 'c' : 't') != std::string::npos) continue;
		const Tpl &t = (*set)[rnd.next() % set->size()];
		sprintf(nm, "S%d", uid++);
		std::string body; putF(body, wx(pk, o.x)); putF(body, wx(pk, o.y)); putF(body, 0.0f); putF(body, o.ang); put32(body, 0); putAscii(body, t.name);
		std::string dd; int nd = 0;
		dInt(dd, src, "objectInitialHealth", 100); ++nd; dStr(dd, src, "originalOwner", "teamPlyrCivilian"); ++nd; dStr(dd, src, "uniqueID", nm); ++nd;
		if (o.kind == K_CAR) { dBool(dd, src, "lwProp", true); ++nd; ++nCars; } else ++nTrees;
		put16(body, (unsigned)nd); body += dd; objs += src.chunk("Object", 3, body);
	}
	// points of interest -> tech buildings (capped)
	int techCount[8] = { 0 }; static const int techCap[8] = { 60, 25, 20, 10, 10, 15, 10, 0 };
	static const char *techName[8] = { "TechOilDerrick", "TechHospital", "TechOilRefinery", "TechReinforcementPad", "TechRepairbay", "TechArtilleryPlatform", "TechRepairPad", "" };
	for (size_t i = 0; i < pk.objs.size(); ++i)
	{
		const PackObj &o = pk.objs[i]; if (skip.find('x') != std::string::npos || o.kind != K_POI || o.sub >= 7 || techCount[o.sub] >= techCap[o.sub]) continue;
		if (!TheThingFactory->findTemplate(AsciiString(techName[o.sub]))) continue;
		++techCount[o.sub]; ++nTech;
		sprintf(nm, "T%d", uid++); objs += object(src, wx(pk, o.x), wx(pk, o.y), 0.0f, techName[o.sub], false, 0, nm, "Tech", "teamPlyrCivilian", "");
	}

	// ---- blend tile data: tiles, empty blend layers, cliff bits, then our texture class list
	std::string btd; btd.reserve(NN * 8 + cliff.size() + 4096);
	put32(btd, (unsigned)NN); btd += tiles; std::string zeros(NN * 2, '\0'); btd += zeros; btd += zeros; btd += zeros; btd += cliff;
	put32(btd, (unsigned)nextTile); put32(btd, 1); put32(btd, 1);                  // bitmap tiles, blended tiles (index 0 unused), cliff infos (index 0 = default)
	put32(btd, (unsigned)classes.size());
	for (size_t c = 0; c < classes.size(); ++c) { put32(btd, classes[c].first); put32(btd, classes[c].nt); put32(btd, classes[c].w); put32(btd, 0); putAscii(btd, classes[c].name); }
	put32(btd, 0); put32(btd, 0);                                                  // edge tiles, edge texture classes
	std::string blendChunk = src.chunk("BlendTileData", 8, btd);

	// ---- one water plane over the whole map: everything below it (river beds, the sea) is water
	std::string pt; put32(pt, 1); putAscii(pt, "LW Water"); putAscii(pt, ""); put32(pt, 1); pt += (char)1; pt += (char)0; put32(pt, 0); put32(pt, 4);
	{
		int lo = -B * 10, hi = (N - B) * 10, z = (int)floor(pk.waterLevel * MAP_HEIGHT_SCALE + 0.5);
		int P[4][2] = { { lo, lo }, { hi, lo }, { hi, hi }, { lo, hi } };
		for (int i = 0; i < 4; ++i) { put32(pt, (unsigned)P[i][0]); put32(pt, (unsigned)P[i][1]); put32(pt, (unsigned)z); }
	}
	std::string zero4; put32(zero4, 0);
	if (skip.find('w') != std::string::npos) pt = zero4;
	std::string out("CkMp", 4); put32(out, (unsigned)src.names.size());
	for (std::map<unsigned, std::string>::const_iterator it = src.names.begin(); it != src.names.end(); ++it) { out += (char)it->second.size(); out += it->second; put32(out, it->first); }
	out += heightChunk; out += blendChunk; out += src.copy("WorldInfo"); out += src.copy("SidesList"); out += src.chunk("ObjectsList", 3, objs); out += src.chunk("PolygonTriggers", 4, pt);
	out += src.copy("GlobalLighting"); out += src.chunk("WaypointsList", 1, zero4);

	{ AsciiString mapsDir = userData; mapsDir.concat("Maps"); TheFileSystem->createDirectory(mapsDir); }
	{ std::string d2 = dir.substr(0, dir.size() - 1); TheFileSystem->createDirectory(AsciiString(d2.c_str())); }
	{ FILE *fp = fopen((dir + "map.ini").c_str(), "wb"); if (fp) { std::string ini = mapIni(st); fwrite(ini.data(), 1, ini.size(), fp); fclose(fp); } }
	FILE *fp = fopen(path.c_str(), "wb"); if (!fp) { printf("[um] city map: cannot write %s\n", path.c_str()); return false; }
	fwrite(out.data(), 1, out.size(), fp); fclose(fp);
	{ FILE *sf = fopen(stampPath.c_str(), "wb"); if (sf) { fputs(stamp, sf); fclose(sf); } }
	printf("[um] city map: wrote %s (%u MB) in %lu s: %d spawn lots, %d real buildings, %d drawn buildings, %d trees/bushes, %d cars, %d tech buildings, %d textures\n",
		path.c_str(), (unsigned)(out.size() >> 20), (unsigned long)((GetTickCount() - t0) / 1000), (int)lots.size(), nLogic, nProps, nTrees, nCars, nTech, (int)classes.size());
	if (TheMapCache) TheMapCache->updateCache();
	return true;
}

// ---- snapshot map references (see GameStateMap.cpp): a city map travels as "<pack> <crc of the map file>", every process builds its own copy
extern bool (*UmMapRefMake)(const char *, std::string *);
extern bool (*UmMapRefResolve)(const std::string &, std::string *);
namespace
{
unsigned long fileCrc(const std::string &path)
{
	static std::map<std::string, std::pair<unsigned long long, unsigned long> > cache;   // path -> (size + write time, crc)
	WIN32_FILE_ATTRIBUTE_DATA a; if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)) return 0;
	unsigned long long key = ((unsigned long long)a.nFileSizeLow) ^ ((unsigned long long)a.ftLastWriteTime.dwLowDateTime << 32) ^ a.ftLastWriteTime.dwHighDateTime;
	std::map<std::string, std::pair<unsigned long long, unsigned long> >::iterator it = cache.find(path);
	if (it != cache.end() && it->second.first == key) return it->second.second;
	FILE *f = fopen(path.c_str(), "rb"); if (!f) return 0;
	unsigned long crc = crc32(0L, Z_NULL, 0); std::vector<char> buf(1 << 20); size_t n;
	while ((n = fread(&buf[0], 1, buf.size(), f)) > 0) crc = crc32(crc, (const Bytef *)&buf[0], (uInt)n);
	fclose(f); cache[path] = std::make_pair(key, crc); return crc;
}
std::string leafNoExt(const std::string &p) { size_t s = p.find_last_of("\\/"); std::string l = s == std::string::npos ? p : p.substr(s + 1); if (l.size() > 4 && _stricmp(l.c_str() + l.size() - 4, ".map") == 0) l.resize(l.size() - 4); return l; }

bool mapRefMake(const char *mapPath, std::string *ref)
{
	std::string leaf = leafNoExt(mapPath); if (leaf.size() < 14 || _strnicmp(leaf.c_str(), "Living World ", 13) != 0) return false;   // the engine passes lower-case paths
	for (int cut = 0; cut < 2; ++cut) { size_t u = leaf.find_last_of('_'); if (u == std::string::npos || u + 1 >= leaf.size() || leaf.find_first_not_of("0123456789", u + 1) != std::string::npos) break; leaf.resize(u); }   // a loaded copy: <map>_<pid>_<n>
	std::string dir = std::string(TheGlobalData->getPath_UserData().str()) + "Maps\\" + leaf + "\\", stamp;
	if (!readWhole(dir + "lwcity.stamp", stamp) || stamp.compare(0, 5, "pack ") != 0) return false;   // only city maps (they carry a stamp)
	std::string city = stamp.substr(5, stamp.find(' ', 5) - 5);
	unsigned long crc = fileCrc(dir + leaf + ".map"); if (!crc) return false;
	char b[96]; sprintf(b, "%s %08lx", city.c_str(), crc); *ref = b; return true;
}
bool mapRefResolve(const std::string &ref, std::string *local)
{
	char city[64]; unsigned long crc = 0; if (sscanf(ref.c_str(), "%63s %lx", city, &crc) != 2) return false;
	if (!strchr(city, '-') && strlen(city) < 56) strcat(city, "-large");   // 0.0.1-alpha.13 had one size per city (now the large maps)
	std::string rel; if (!UmEnsureCityMap(city, &rel)) return false;
	*local = std::string(TheGlobalData->getPath_UserData().str()) + rel;
	unsigned long have = fileCrc(*local);
	if (have != crc) { printf("[um] city map: '%s' here (crc %08lx) differs from the server's (crc %08lx): different city pack or game files\n", local->c_str(), have, crc); return false; }
	return true;
}
struct RegisterMapRefs { RegisterMapRefs() { UmMapRefMake = mapRefMake; UmMapRefResolve = mapRefResolve; } } s_registerMapRefs;
}
