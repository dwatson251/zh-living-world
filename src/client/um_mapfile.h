// Living World: helpers for writing Zero Hour .map files (chunk TOC + chunks: id u32, version u16, size i32), shared by the generated maps
// (mapgen.cpp: Living World Grand, citymap.cpp: real-city maps). A Source is a map read from the game's files whose name table and chunks are reused.
#pragma once
#include "Common/FileSystem.h"
#include "Common/File.h"
#include "Compression.h"
#include <string>
#include <vector>
#include <map>
#include <string.h>

namespace um_map
{
typedef unsigned char u8;
struct Chunk { std::string name; unsigned short ver; size_t a, b; };

struct Rng { unsigned s; Rng() : s(12345u) {} unsigned next() { s = s * 1664525u + 1013904223u; return s >> 8; } double uni(double lo, double hi) { return lo + (hi - lo) * (next() / 16777216.0); } };

inline void put32(std::string &o, unsigned v) { o.append((const char *)&v, 4); }
inline void put16(std::string &o, unsigned v) { unsigned short s = (unsigned short)v; o.append((const char *)&s, 2); }
inline void putF(std::string &o, float f) { o.append((const char *)&f, 4); }
inline void putAscii(std::string &o, const std::string &s) { put16(o, (unsigned)s.size()); o += s; }
inline unsigned get32(const std::string &d, size_t p) { unsigned v; memcpy(&v, d.data() + p, 4); return v; }
inline unsigned get16(const std::string &d, size_t p) { unsigned short v; memcpy(&v, d.data() + p, 2); return v; }

inline double smooth(double t) { if (t < 0) t = 0; if (t > 1) t = 1; return t * t * (3 - 2 * t); }

struct Source
{
	std::string data; std::map<unsigned, std::string> names; std::map<std::string, unsigned> ids; std::vector<Chunk> chunks;
	const Chunk *find(const char *n) const { for (size_t i = 0; i < chunks.size(); ++i) if (chunks[i].name == n) return &chunks[i]; return NULL; }
	bool load(const char *path)
	{
		File *f = TheFileSystem->openFile(path, File::READ | File::BINARY); if (!f) return false;
		Int sz = f->size(); if (sz <= 0) { f->close(); return false; }
		char *buf = f->readEntireAndClose(); data.assign(buf, sz); delete[] buf;
		if (CompressionManager::isDataCompressed(data.data(), (Int)data.size()))
		{
			Int n = CompressionManager::getUncompressedSize(data.data(), (Int)data.size()); std::string out; out.resize(n);
			if (CompressionManager::decompressData((void *)data.data(), (Int)data.size(), &out[0], n) != n) return false;
			data.swap(out);
		}
		if (data.size() < 8 || memcmp(data.data(), "CkMp", 4) != 0) return false;
		int count = (int)get32(data, 4); size_t p = 8;
		for (int i = 0; i < count; ++i) { unsigned len = (u8)data[p++]; std::string nm = data.substr(p, len); p += len; unsigned id = get32(data, p); p += 4; names[id] = nm; ids[nm] = id; }
		while (p + 10 <= data.size())
		{
			Chunk c; unsigned id = get32(data, p); c.ver = (unsigned short)get16(data, p + 4); int sz2 = (int)get32(data, p + 6);
			c.name = names.count(id) ? names[id] : std::string(); c.a = p + 10; c.b = p + 10 + sz2; if (c.b > data.size()) return false; chunks.push_back(c); p = c.b;
		}
		return true;
	}
	unsigned addName(const std::string &nm) { if (ids.count(nm)) return ids[nm]; unsigned id = 1; for (std::map<unsigned, std::string>::const_iterator it = names.begin(); it != names.end(); ++it) if (it->first >= id) id = it->first + 1; names[id] = nm; ids[nm] = id; return id; }
	std::string chunk(const char *name, unsigned ver, const std::string &body) const { std::string o; put32(o, ids.find(name)->second); put16(o, ver); put32(o, (unsigned)body.size()); return o + body; }
	std::string copy(const char *name) const { const Chunk *c = find(name); return chunk(name, c->ver, data.substr(c->a, c->b - c->a)); }
};

inline void putDictEntry(std::string &o, const Source &src, const char *key, int type, const std::string &val) { put32(o, (src.ids.find(key)->second << 8) | (unsigned)type); o += val; }
inline void dBool(std::string &o, const Source &s, const char *k, bool v) { putDictEntry(o, s, k, 0, std::string(1, v ? 1 : 0)); }
inline void dInt(std::string &o, const Source &s, const char *k, int v) { std::string x; put32(x, (unsigned)v); putDictEntry(o, s, k, 1, x); }
inline void dReal(std::string &o, const Source &s, const char *k, float v) { std::string x; putF(x, v); putDictEntry(o, s, k, 2, x); }
inline void dStr(std::string &o, const Source &s, const char *k, const std::string &v) { std::string x; putAscii(x, v); putDictEntry(o, s, k, 3, x); }

inline std::string object(const Source &s, float x, float y, float ang, const std::string &name, bool waypoint, int wid, const std::string &uid, const char *layer, const char *owner, const std::string &wname)
{
	std::string body; putF(body, x); putF(body, y); putF(body, 0.0f); putF(body, ang); put32(body, 0); putAscii(body, name);
	std::string d; int n = 0;
	dInt(d, s, "objectInitialHealth", 100); ++n; dBool(d, s, "objectEnabled", true); ++n; dBool(d, s, "objectIndestructible", false); ++n; dBool(d, s, "objectUnsellable", false); ++n;
	dBool(d, s, "objectPowered", true); ++n; dBool(d, s, "objectRecruitableAI", true); ++n; dStr(d, s, "originalOwner", owner); ++n; dStr(d, s, "uniqueID", uid); ++n; dStr(d, s, "objectLayer", layer); ++n;
	dBool(d, s, "objectSelectable", !waypoint); ++n;
	if (waypoint) { dInt(d, s, "waypointID", wid); ++n; dStr(d, s, "waypointName", wname); ++n; }
	put16(body, (unsigned)n); body += d;
	return s.chunk("Object", 3, body);
}

}
