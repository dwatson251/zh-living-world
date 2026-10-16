// Living World: builds the very large skirmish map "Living World Grand" on the machine that hosts the world.
//
// The map is made out of the player's own copy of Tournament Desert (its texture list, lighting, world info and the skirmish AI's side/script data are reused
// as they are), with a new height field, a new tile layout, 128 start waypoints and supply docks and piles by every start. Nothing is shipped: the file is
// written into the Maps folder of the user's Zero Hour data folder the first time a server asks for it. Clients do not need it: the server's world snapshot
// carries the map, as it does for any custom map.
#include "PreRTS.h"
#include "Common/FileSystem.h"
#include "Common/GlobalData.h"
#include "Common/AsciiString.h"
#include "Common/File.h"
#include "GameClient/MapUtil.h"
#include "Compression.h"
#include <string>
#include <vector>
#include <map>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "um_mapfile.h"
using namespace um_map;

namespace
{
const int GRAND_CELLS_DEFAULT = 800;   // cells per side, including the border (10 world units per cell); "Living World Grand <n>" asks for n cells
const int GRAND_BORDER = 35;
const int GRAND_STARTS = 128;      // the engine holds the observer plus 128 players

struct Hills
{
	double ph[6][3];
	Hills(Rng &r) { for (int i = 0; i < 6; ++i) for (int j = 0; j < 3; ++j) ph[i][j] = r.uni(0, 6.28); }
	double at(double x, double y) const
	{
		double v = 0;
		for (int k = 0; k < 6; ++k) { double f = 0.012 + 0.009 * k, a = 9.0 / (1 + k * 0.7); v += a * sin(x * f + ph[k][0] + sin(y * f * 0.7 + ph[k][1])) * cos(y * f * 0.9 + ph[k][2]); }
		return v;
	}
};
}

// Writes the map if it is not there yet. Returns true when the map file exists afterwards. mapPath is the requested map ("...Living World Grand.map", or
// "...Living World Grand <cells>..." for another size, used to measure how large a map the engine can carry).
bool UmEnsureGrandMap(const char *mapPath)
{
	int cells = GRAND_CELLS_DEFAULT;
	if (const char *g = strstr(mapPath, "Living World Grand ")) { int n = atoi(g + 19); if (n >= 200 && n <= 20000) cells = n; }
	AsciiString name("Living World Grand"); if (cells != GRAND_CELLS_DEFAULT) { AsciiString t; t.format(" %d", cells); name.concat(t); }
	AsciiString dir = TheGlobalData->getPath_UserData(); dir.concat("Maps\\"); dir.concat(name); dir.concat("\\");
	AsciiString path = dir; path.concat(name); path.concat(".map");
	{ File *f = TheFileSystem->openFile(path.str(), File::READ | File::BINARY); if (f) { f->close(); return true; } }

	Source src;
	if (!src.load("Maps\\Tournament Desert\\Tournament Desert.map")) { printf("[um] map generator: cannot read Tournament Desert from the game files\n"); return false; }
	const Chunk *hc = src.find("HeightMapData"), *bc = src.find("BlendTileData");
	if (!hc || !bc || !src.find("SidesList") || !src.find("GlobalLighting") || !src.find("WorldInfo")) { printf("[um] map generator: unexpected map layout\n"); return false; }

	const int W = cells, H = cells, B = GRAND_BORDER, inner = W - 2 * B;
	Rng rng; Hills hills(rng);
	// start spots on a grid of cells, each moved a little off the centre
	int cols = 1; while (cols * cols < GRAND_STARTS) ++cols;
	double step = (double)inner / cols;
	std::vector<double> sx, sy;
	for (int k = 0; k < GRAND_STARTS; ++k) { int gx = k % cols, gy = k / cols; double jx = rng.uni(-0.12, 0.12) * step, jy = rng.uni(-0.12, 0.12) * step; sx.push_back(B + step * (gx + 0.5) + jx); sy.push_back(B + step * (gy + 0.5) + jy); }
	const double BASEH = 70.0, R1 = 24.0, R2 = 40.0; const int blk = 40;
	std::map<int, std::vector<int> > grid;
	for (int i = 0; i < GRAND_STARTS; ++i) grid[((int)(sy[i] / blk)) * 1000 + (int)(sx[i] / blk)].push_back(i);

	std::string hm((size_t)W * H, '\0');
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x)
		{
			double h = BASEH + hills.at(x, y), w = 0; int gx0 = x / blk, gy0 = y / blk;
			for (int gy = gy0 - 2; gy <= gy0 + 2; ++gy)
				for (int gx = gx0 - 2; gx <= gx0 + 2; ++gx)
				{
					std::map<int, std::vector<int> >::const_iterator it = grid.find(gy * 1000 + gx); if (it == grid.end()) continue;
					for (size_t q = 0; q < it->second.size(); ++q) { int i = it->second[q]; double dd = sqrt((x - sx[i]) * (x - sx[i]) + (y - sy[i]) * (y - sy[i])); double ww = 1.0 - smooth((dd - R1) / (R2 - R1)); if (ww > w) w = ww; }
				}
			h = h * (1 - w) + BASEH * w; int v = (int)floor(h + 0.5); if (v < 5) v = 5; if (v > 250) v = 250; hm[(size_t)y * W + x] = (char)v;
		}
	std::string hb; put32(hb, W); put32(hb, H); put32(hb, B); put32(hb, 1); put32(hb, inner); put32(hb, inner); put32(hb, W * H); hb += hm;
	std::string heightChunk = src.chunk("HeightMapData", 4, hb);

	// tile layout: three large-scale texture classes of the source map, hard-edged
	struct TC { int first, nt, w; };
	static const TC SAND = { 0, 36, 6 }, DIRT = { 152, 16, 4 }, GRASS = { 252, 16, 4 };
	double tp[3][2]; for (int i = 0; i < 3; ++i) { tp[i][0] = rng.uni(0, 6.28); tp[i][1] = rng.uni(0, 6.28); }
	std::string tiles; tiles.reserve((size_t)W * H * 2);
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x)
		{
			double a = sin(x * 0.021 + tp[0][0]) + sin(y * 0.017 + tp[0][1]) + sin((x + y) * 0.011 + tp[1][0]);
			const TC &c = a > 1.6 ? GRASS : (a < -1.5 ? DIRT : SAND);
			unsigned t = ((unsigned)(c.first + ((y >> 1) % c.w) * c.w + ((x >> 1) % c.w)) << 2) | (unsigned)(((y & 1) << 1) | (x & 1));
			put16(tiles, t);
		}
	// the rest of the blend tile chunk (texture classes, blended tiles, cliff info) is the source map's, unchanged
	int sw = (int)get32(src.data, hc->a), sh = (int)get32(src.data, hc->a + 4);
	size_t tail = bc->a + 4 + (size_t)sw * sh * 2 * 4 + (size_t)sh * ((sw + 7) / 8);
	std::string zeros((size_t)W * H * 2, '\0'), btd; put32(btd, W * H); btd += tiles; btd += zeros; btd += zeros; btd += zeros; btd += std::string((size_t)H * ((W + 7) / 8), '\0'); btd += src.data.substr(tail, bc->b - tail);
	std::string blendChunk = src.chunk("BlendTileData", 8, btd);

	std::string objs; int wid = 1, uid = 1;
	for (int i = 0; i < GRAND_STARTS; ++i)
	{
		float wx = (float)((sx[i] - B) * 10.0), wy = (float)((sy[i] - B) * 10.0); char nm[64];
		sprintf(nm, "Player_%d_Start", i + 1);
		objs += object(src, wx, wy, 0.0f, "*Waypoints/Waypoint", true, wid++, nm, "Start Points", "team", nm);
		sprintf(nm, "SupplyDock %d", uid++);
		objs += object(src, wx - 230, wy + 70, 0.78f, "SupplyDock", false, 0, nm, "Supply Piles", "teamPlyrCivilian", "");
		static const int off[5][2] = { { 190, -50 }, { 225, -25 }, { 190, 0 }, { 225, 25 }, { 190, 50 } };
		for (int j = 0; j < 5; ++j) { sprintf(nm, "SupplyPileSmall %d", uid++); objs += object(src, wx + off[j][0], wy + off[j][1], 0.0f, "SupplyPileSmall", false, 0, nm, "Small Supply Piles", "teamPlyrCivilian", ""); }
	}
	std::string zero4; put32(zero4, 0);
	std::string out("CkMp", 4); put32(out, (unsigned)src.names.size());
	for (std::map<unsigned, std::string>::const_iterator it = src.names.begin(); it != src.names.end(); ++it) { out += (char)it->second.size(); out += it->second; put32(out, it->first); }
	out += heightChunk; out += blendChunk; out += src.copy("WorldInfo"); out += src.copy("SidesList"); out += src.chunk("ObjectsList", 3, objs); out += src.chunk("PolygonTriggers", 4, zero4);
	out += src.copy("GlobalLighting"); out += src.chunk("WaypointsList", 1, zero4);

	{ AsciiString mapsDir = TheGlobalData->getPath_UserData(); mapsDir.concat("Maps"); TheFileSystem->createDirectory(mapsDir); }
	{ AsciiString d2 = dir; d2.removeLastChar(); TheFileSystem->createDirectory(d2); }
	FILE *fp = fopen(path.str(), "wb"); if (!fp) { printf("[um] map generator: cannot write %s\n", path.str()); return false; }
	fwrite(out.data(), 1, out.size(), fp); fclose(fp);
	printf("[um] map generator: wrote %s (%u bytes, %d x %d cells, %d starts)\n", path.str(), (unsigned)out.size(), W, H, GRAND_STARTS);
	if (TheMapCache) TheMapCache->updateCache();
	return true;
}
