// Living World host: starts, watches and restarts the dedicated server (zh_client.exe in server mode), serves a status page, and shows a window with
// settings, Start/Stop, the server log, connected players and uptime. `zh_host.exe --cli` does the same from a terminal without a window.
//
//   zh_host.exe                      window (settings are read from and saved to um_server.ini next to the program)
//   zh_host.exe --cli [options]      run in the terminal until Ctrl+C; options override the ini:  --port 7777 --max-players 16 --max-ai 16 --start-cash 10000 ...
#include <windows.h>
#include <winsock2.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <algorithm>

// ------------------------------------------------------------------------------------------------ settings
struct Setting { const char *key, *label, *def, *flag, *section; };
static const Setting kSettings[] = {
	{ "name",                   "Server name",                      "Living World",     "--name",               "server" },
	{ "game_dir",               "Zero Hour folder",                 "",                 "--game-dir",           "server" },
	{ "map",                    "Map (codename or .map path)",      "london-medium",    "--map",                "server" },
	{ "host",                   "Listen on (address)",              "0.0.0.0",          "--host",               "server" },
	{ "port",                   "Game port",                        "7777",             "--port",               "server" },
	{ "status_port",            "Status page port (0 = off)",       "8080",             "--status-port",        "server" },
	{ "world_dir",              "World folder (saves)",             "world",            "--world-dir",          "server" },
	{ "max_players",            "Max players (seats)",              "16",               "--max-players",        "world" },
	{ "max_ai",                 "Max AI players at once",           "16",               "--max-ai",             "world" },
	{ "start_ai",               "AI players at world start",        "4",                "--start-ai",           "world" },
	{ "ai_pool",                "AI seats (at least max AI)",       "24",               "--ai-pool",            "world" },
	{ "game_speed",             "Game speed (15-60, default 30)",   "30",               "--game-speed",         "world" },
	{ "ai_join_secs",           "Seconds between AI joining",       "20",               "--ai-join-secs",       "world" },
	{ "epoch_secs",             "Save the world every (secs, auto)", "auto",            "--epoch-secs",         "world" },
	{ "spawn_deadzone",         "Spawn deadzone (metres, auto)",    "auto",             "--spawn-deadzone",     "world" },
	{ "ai_reserve_lots",        "Spawn lots kept free for players", "4",                "--ai-reserve-lots",    "world" },
	{ "start_cash",             "Starting cash",                    "10000",            "--start-cash",         "rules" },
	{ "last_hit_percent",       "Knock-out bonus: % of victim cash","10",               "--last-hit-percent",   "rules" },
	{ "last_hit_bonus",         "Knock-out bonus: flat amount",     "10000",            "--last-hit-bonus",     "rules" },
	{ "last_hit_window_secs",   "Last hit counts for (seconds)",    "300",              "--last-hit-window",    "rules" },
	{ "supply_cash",            "Supply dock refill: cash per time","0",                "--supply-cash",        "rules" },
	{ "supply_interval_secs",   "Supply dock refill every (secs)",  "0",                "--supply-interval",    "rules" },
	{ "rent_minutes",           "Rent every (minutes)",             "15",               "--rent-minutes",       "rules" },
	{ "rent_fixed",             "Rent: flat amount (0 = by value)", "0",                "--rent-fixed",         "rules" },
	{ "rent_percent",           "Rent: % of base value",            "5",                "--rent-percent",       "rules" },
	{ "rent_floor",             "Rent: minimum amount",             "500",              "--rent-floor",         "rules" },
	{ "rent_grace_cycles",      "Rent: free cycles before the first payment","1",           "--rent-grace",         "rules" },
	{ "immunity_secs",          "Spawn protection when joining (seconds, 0 = off)","180",    "--immunity-secs",      "rules" },
	{ "immunity_cooldown_secs", "Protection is not renewed for (seconds after it ends)","900","--immunity-cooldown",  "rules" },
	{ "rent_damage_percent",    "Damage per unpaid stack (%)",      "1",                "--rent-damage",        "rules" },
	{ "absent_minutes",         "Keep an absent base (minutes)",    "30",               "--absent-minutes",     "rules" },
	{ "combat_secs",            "Combat grace period (seconds)",    "30",               "--combat-secs",        "rules" },
	{ "pools",                  "Money pools cap generator income (1 = on)","1",        "--pools",              "economy" },
	{ "pool_seed",              "Pool: starting amount per currency","50000",           "--pool-seed",          "economy" },
	{ "pool_join",              "Pool: added when a player joins",  "2000",             "--pool-join",          "economy" },
	{ "pool_depot_percent",     "Pool share: supply depots (%)",    "60",               "--pool-depot",         "economy" },
	{ "pool_derrick_percent",   "Pool share: oil derricks (%)",     "10",               "--pool-derrick",       "economy" },
	{ "pool_other_percent",     "Pool share: Black Market, hackers, supply drops (%)","30","--pool-other",      "economy" },
	{ "inflation",              "Inflation: prices and rent follow GDP (1 = on)","0",   "--inflation",          "economy" },
	{ "gdp_baseline",           "Inflation: GDP at which prices start to rise","200000","--gdp-baseline",       "economy" },
	{ "inflation_max_percent",  "Inflation: highest price level (%)","300",             "--inflation-max",      "economy" },
	{ "deflation_permille",     "Deflation: pool tax per cycle (per mille)","20",       "--deflation",          "economy" },
};
static const int kNumSettings = sizeof(kSettings) / sizeof(kSettings[0]);
static const int kMapSetting = 2;   // the "map" row is a drop-down

// Maps the host offers by codename. City maps are built on the host from cities/<codename>.lwcity (OpenStreetMap + Copernicus elevation) and the
// local game files; every map keeps its own persistent world in <world_dir>\<codename>. A .map path also works (its world folder is named after the map).
struct MapChoice { const char *code, *label, *serverMap; };
static const MapChoice kMaps[] = {
	{ "london-large",  "London, 10 km",      "city:london-large" },
	{ "london-medium", "London, 4.8 km",     "city:london-medium" },
	{ "london-small",  "London, 1.4 km",     "city:london-small" },
	{ "newyork-large", "New York, 10 km",    "city:newyork-large" },
	{ "newyork-medium","New York, 4.8 km",   "city:newyork-medium" },
	{ "newyork-small", "New York, 1.4 km",   "city:newyork-small" },
	{ "paris-large",   "Paris, 10 km",       "city:paris-large" },
	{ "paris-medium",  "Paris, 4.8 km",      "city:paris-medium" },
	{ "paris-small",   "Paris, 1.4 km",      "city:paris-small" },
	{ "dubai-large",   "Dubai, 10 km",       "city:dubai-large" },
	{ "dubai-medium",  "Dubai, 4.8 km",      "city:dubai-medium" },
	{ "dubai-small",   "Dubai, 1.4 km",      "city:dubai-small" },
	{ "grand",         "generated, 1.6 km",  "Maps\\Living World Grand\\Living World Grand.map" },
	{ "desert",        "retail, small",      "Maps\\Tournament Desert\\Tournament Desert.map" },
};
// codenames of 0.0.1-alpha.13 (one size per city): the large maps
static const char *kMapAliases[][2] = { { "london", "london-large" }, { "newyork", "newyork-large" }, { "paris", "paris-large" }, { "dubai", "dubai-large" } };
static const int kNumMaps = sizeof(kMaps) / sizeof(kMaps[0]);
// "london", "london  (London: ...)" or a path -> codename (world folder name) and the server's UM_SKIRMISH_MAP value
static void resolveMap(const std::string &value, std::string &code, std::string &serverMap)
{
	std::string v = value; size_t sp = v.find_first_of(" \t"); std::string first = sp == std::string::npos ? v : v.substr(0, sp);
	for (size_t k = 0; k < sizeof kMapAliases / sizeof kMapAliases[0]; ++k) if (_stricmp(first.c_str(), kMapAliases[k][0]) == 0) first = kMapAliases[k][1];
	for (int i = 0; i < kNumMaps; ++i) if (_stricmp(first.c_str(), kMaps[i].code) == 0) { code = kMaps[i].code; serverMap = kMaps[i].serverMap; return; }
	for (int i = 0; i < kNumMaps; ++i) if (_stricmp(v.c_str(), kMaps[i].serverMap) == 0) { code = kMaps[i].code; serverMap = kMaps[i].serverMap; return; }
	serverMap = v;   // a map path: world folder named after the map file
	size_t a = v.find_last_of("\\/"); std::string leaf = a == std::string::npos ? v : v.substr(a + 1); if (leaf.size() > 4 && _stricmp(leaf.c_str() + leaf.size() - 4, ".map") == 0) leaf.resize(leaf.size() - 4);
	code.clear(); for (size_t k = 0; k < leaf.size(); ++k) { char ch = leaf[k]; code += (isalnum((unsigned char)ch) ? (char)tolower((unsigned char)ch) : '-'); }
	if (code.empty() || code[0] == '.' || code[0] == '-') code = "map" + code;
}
// the persistent world of a map: <world_dir>\<codename>. A world saved by an older build directly in <world_dir> moves into the folder of the map
// configured when it is first seen (older builds had one world per server).
static std::string worldDirFor(const std::map<std::string, std::string> &c, const std::string &hostDir);

typedef std::map<std::string, std::string> Config;

static std::string gHostDir, gIniPath;

static std::string dirOf(const std::string &p) { size_t s = p.find_last_of("\\/"); return s == std::string::npos ? "." : p.substr(0, s); }
static bool fileExists(const std::string &p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
static std::string absPath(const std::string &p, const std::string &base)
{
	if (p.size() > 1 && (p[1] == ':' || (p[0] == '\\' && p[1] == '\\'))) return p;
	return base + "\\" + p;
}
static std::string readFile(const std::string &path)
{
	std::string out; FILE *f = fopen(path.c_str(), "rb"); if (!f) return out;
	char b[8192]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) out.append(b, n); fclose(f); return out;
}
static std::string detectGameDir()
{
	const char *keys[] = { "SOFTWARE\\Wow6432Node\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour", "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour" };
	for (int i = 0; i < 2; ++i)
	{
		HKEY k; char b[512]; DWORD n = sizeof b, t;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keys[i], 0, KEY_READ, &k) == ERROR_SUCCESS)
		{ if (RegQueryValueExA(k, "InstallPath", NULL, &t, (LPBYTE)b, &n) == ERROR_SUCCESS) { RegCloseKey(k); return b; } RegCloseKey(k); }
	}
	const char *guess[] = { "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Generals - Zero Hour", "D:\\SteamLibrary\\steamapps\\common\\Command & Conquer Generals - Zero Hour", "E:\\SteamLibrary\\steamapps\\common\\Command & Conquer Generals - Zero Hour" };
	for (int i = 0; i < 3; ++i) if (fileExists(std::string(guess[i]) + "\\INIZH.big")) return guess[i];
	return "";
}

static std::string worldDirFor(const std::map<std::string, std::string> &c, const std::string &hostDir)
{
	std::string base = absPath(c.find("world_dir")->second, hostDir), code, serverMap; resolveMap(c.find("map")->second, code, serverMap);
	CreateDirectoryA(base.c_str(), NULL);
	std::string dir = base + "\\" + code;
	{	// a world from 0.0.1-alpha.13 lives in <world_dir>\<city> (that release had only the large maps)
		size_t dash = code.rfind("-large");
		if (dash != std::string::npos && dash + 6 == code.size() && !fileExists(dir) && fileExists(base + "\\" + code.substr(0, dash))) MoveFileA((base + "\\" + code.substr(0, dash)).c_str(), dir.c_str());
	}
	if (!fileExists(dir) && fileExists(base + "\\epoch.sav"))
	{
		CreateDirectoryA(dir.c_str(), NULL);
		static const char *files[] = { "epoch.sav", "world.bin", "cmds.log", "frame.txt", "players.txt", "status.json" };
		for (int i = 0; i < 6; ++i) MoveFileA((base + "\\" + files[i]).c_str(), (dir + "\\" + files[i]).c_str());
	}
	CreateDirectoryA(dir.c_str(), NULL);
	return dir;
}

// The Windows INI API does not like network-style paths (\\wsl.localhost\...), so the file is read and written directly.
static std::string trim(std::string v) { size_t a = v.find_first_not_of(" \t\r"); if (a == std::string::npos) return ""; size_t b = v.find_last_not_of(" \t\r"); return v.substr(a, b - a + 1); }
static Config loadConfig(const std::string &ini)
{
	std::map<std::string, std::string> raw; std::string text = readFile(ini), section; size_t p = 0;
	while (p <= text.size())
	{
		size_t e = text.find('\n', p); if (e == std::string::npos) e = text.size(); std::string ln = trim(text.substr(p, e - p)); p = e + 1;
		if (ln.empty() || ln[0] == ';' || ln[0] == '#') continue;
		if (ln[0] == '[') { size_t c = ln.find(']'); if (c != std::string::npos) section = ln.substr(1, c - 1); continue; }
		size_t eq = ln.find('='); if (eq == std::string::npos) continue;
		std::string v = trim(ln.substr(eq + 1)); if (!v.empty() && v[0] == ';') v.clear(); size_t semi = v.find(" ;"); if (semi != std::string::npos) v = trim(v.substr(0, semi));   // an empty value followed by a comment is empty
		raw[section + "." + trim(ln.substr(0, eq))] = v;
	}
	Config c;
	for (int i = 0; i < kNumSettings; ++i) { std::map<std::string, std::string>::iterator it = raw.find(std::string(kSettings[i].section) + "." + kSettings[i].key); c[kSettings[i].key] = it != raw.end() ? it->second : kSettings[i].def; }
	// LW_<SETTING> environment variables (LW_PORT, LW_MAX_AI, LW_GAME_DIR ...) sit between the file and the command line: this is how the Docker image is configured
	for (int i = 0; i < kNumSettings; ++i)
	{
		std::string name = std::string("LW_") + kSettings[i].key; for (size_t k = 0; k < name.size(); ++k) name[k] = (char)toupper((unsigned char)name[k]);
		const char *v = getenv(name.c_str()); if (v && *v) c[kSettings[i].key] = v;
	}
	if (c["game_dir"].empty()) c["game_dir"] = detectGameDir();
	return c;
}
static void saveConfig(const Config &c, const std::string &ini)
{
	std::string out = "; Living World server settings (written by zh_host.exe; every value is optional).\r\n"; const char *last = "";
	for (int i = 0; i < kNumSettings; ++i)
	{
		if (strcmp(last, kSettings[i].section) != 0) { out += std::string("\r\n[") + kSettings[i].section + "]\r\n"; last = kSettings[i].section; }
		Config::const_iterator it = c.find(kSettings[i].key); out += std::string(kSettings[i].key) + " = " + (it != c.end() ? it->second : kSettings[i].def) + "   ; " + kSettings[i].label + "\r\n";
	}
	FILE *f = fopen(ini.c_str(), "wb"); if (f) { fwrite(out.data(), 1, out.size(), f); fclose(f); }
}
static int num(const Config &c, const char *k, int d) { Config::const_iterator it = c.find(k); return it == c.end() || it->second.empty() ? d : atoi(it->second.c_str()); }

// ------------------------------------------------------------------------------------------------ log
static CRITICAL_SECTION gLogLock;
static std::deque<std::string> gLog;
static std::string gLogFile;
static bool gCli = false;
static void logLine(const std::string &s)
{
	EnterCriticalSection(&gLogLock);
	SYSTEMTIME t; GetLocalTime(&t); char ts[32]; sprintf(ts, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
	std::string line = std::string(ts) + s; gLog.push_back(line); while (gLog.size() > 400) gLog.pop_front();
	if (gCli) { printf("%s\n", line.c_str()); fflush(stdout); }
	if (!gLogFile.empty()) { FILE *f = fopen(gLogFile.c_str(), "ab"); if (f) { fprintf(f, "%s\r\n", line.c_str()); fclose(f); } }
	LeaveCriticalSection(&gLogLock);
}

// ------------------------------------------------------------------------------------------------ the server process
struct Runner
{
	HANDLE job, proc; bool wanted; DWORD startedTick; DWORD exitedTick; int restarts; Config cfg; std::string worldDir, serverLog;
	Runner() : job(NULL), proc(NULL), wanted(false), startedTick(0), exitedTick(0), restarts(0) {}

	std::string serverExe() { std::string e = gHostDir + "\\zh_client.exe"; if (!fileExists(e)) e = gHostDir + "\\server\\zh_client.exe"; return e; }

	bool validate(const Config &c, std::string &why)
	{
		std::string gd = c.find("game_dir")->second;
		if (gd.empty() || !fileExists(gd + "\\INIZH.big")) { why = "The Zero Hour folder is not set or does not contain INIZH.big."; return false; }
		if (!fileExists(serverExe())) { why = "zh_client.exe was not found next to the host program."; return false; }
		int total = 1 + num(c, "start_ai", 4) + num(c, "max_players", 16) + std::max(num(c, "ai_pool", 24), num(c, "max_ai", 16));
		if (total > 129) { char b[160]; sprintf(b, "Too many seats: 1 observer + start AI + players + AI pool = %d, the engine allows 129 (the observer plus 128 players).", total); why = b; return false; }
		return true;
	}

	bool start(const Config &c)
	{
		std::string why; if (!validate(c, why)) { logLine("Cannot start: " + why); return false; }
		cfg = c; wanted = true;
		return launch(true);
	}
	bool launch(bool fresh)
	{
		std::string gameDir = cfg["game_dir"]; worldDir = worldDirFor(cfg, gHostDir);
		serverLog = absPath(cfg["world_dir"], gHostDir) + "\\server_log.txt";   // one log next to host_log.txt, whichever map runs
		std::string mapCode, serverMap; resolveMap(cfg["map"], mapCode, serverMap);
		int startAi = num(cfg, "start_ai", 4), players = num(cfg, "max_players", 16), maxAi = num(cfg, "max_ai", 16), pool = std::max(num(cfg, "ai_pool", 24), maxAi);
		std::vector<std::string> env;
		char b[64];
		env.push_back("UM_SERVER=" + cfg["port"]);
		env.push_back("UM_BIND=" + cfg["host"]);
		env.push_back("UM_NULLD3D=1");
		env.push_back("UM_LOGFILE=" + serverLog);
		env.push_back("UM_WORLD_DIR=" + worldDir);
		env.push_back("UM_SERVER_NAME=" + cfg["name"]);
		env.push_back("UM_SKIRMISH_MAP=" + serverMap);
		env.push_back("UM_CITY_DIR=" + gHostDir + "\\cities");
		{ char sd[64]; int dz = atoi(cfg["spawn_deadzone"].c_str());
		  if (dz <= 0) dz = mapCode.find("-small") != std::string::npos ? 120 : mapCode.find("-medium") != std::string::npos ? 250 : 400;   // auto: by map size
		  sprintf(sd, "UM_SPAWN_DEADZONE=%d", dz * 5); env.push_back(sd); sprintf(sd, "UM_AI_RESERVE_LOTS=%d", num(cfg, "ai_reserve_lots", 4)); env.push_back(sd); }   // metres -> world units (0.2 m each)
		sprintf(b, "%d", startAi); env.push_back(std::string("UM_SKIRMISH_AI=") + b);
		sprintf(b, "%d", players); env.push_back(std::string("UM_HUMAN_SLOTS=") + b);
		sprintf(b, "%d", pool); env.push_back(std::string("UM_AI_POOL=") + b);
		sprintf(b, "%d", maxAi); env.push_back(std::string("UM_AI_MAX=") + b);
		sprintf(b, "%d", startAi + players + pool); env.push_back(std::string("UM_SYNTH_STARTS=") + b);
		{ int sp = num(cfg, "game_speed", 30); if (sp < 15) sp = 15; if (sp > 60) sp = 60; sprintf(b, "%d", sp); env.push_back(std::string("UM_GAME_SPEED=") + b); }
		sprintf(b, "%d", num(cfg, "ai_join_secs", 20) * 30); env.push_back(std::string("UM_AI_JOIN_FRAMES=") + b);
		{	// "auto": every 5 minutes; 10 on medium and 15 on large city maps (an epoch pauses the world while it is saved and reloaded: ~20 s on a large map)
			int es = atoi(cfg["epoch_secs"].c_str()); if (es <= 0) es = mapCode.find("-large") != std::string::npos ? 900 : mapCode.find("-medium") != std::string::npos ? 600 : 300;
			sprintf(b, "%d", es * 30 * num(cfg, "game_speed", 30) / 30); env.push_back(std::string("UM_EPOCH=") + b);
		}
		env.push_back("UM_START_CASH=" + cfg["start_cash"]);
		env.push_back("UM_LASTHIT=" + cfg["last_hit_bonus"]);
		env.push_back("UM_LASTHIT_PCT=" + cfg["last_hit_percent"]);
		sprintf(b, "%d", num(cfg, "last_hit_window_secs", 300) * 30); env.push_back(std::string("UM_LASTHIT_WINDOW=") + b);
		env.push_back("UM_SUPPLY_AMOUNT=" + cfg["supply_cash"]);
		sprintf(b, "%d", num(cfg, "supply_interval_secs", 0) * 30); env.push_back(std::string("UM_SUPPLY_FRAMES=") + b);
		sprintf(b, "%d", num(cfg, "absent_minutes", 30) * 60 * 30); env.push_back(std::string("UM_ABSENT_FRAMES=") + b);
		sprintf(b, "%d", num(cfg, "combat_secs", 30) * 30); env.push_back(std::string("UM_COMBAT_FRAMES=") + b);
		sprintf(b, "%d", num(cfg, "rent_minutes", 15) * 60 * 30); env.push_back(std::string("UM_RENT_FRAMES=") + b);
		env.push_back("UM_RENT_FIXED=" + cfg["rent_fixed"]);
		env.push_back("UM_RENT_PCT=" + cfg["rent_percent"]);
		env.push_back("UM_RENT_FLOOR=" + cfg["rent_floor"]);
		env.push_back("UM_RENT_GRACE=" + cfg["rent_grace_cycles"]);
		sprintf(b, "%d", num(cfg, "immunity_secs", 180) * 30); env.push_back(std::string("UM_IMMUNE_FRAMES=") + b);
		sprintf(b, "%d", num(cfg, "immunity_cooldown_secs", 900) * 30); env.push_back(std::string("UM_IMMUNE_COOLDOWN_FRAMES=") + b);
		sprintf(b, "%d", num(cfg, "rent_damage_percent", 1) * 10); env.push_back(std::string("UM_RENT_DMG_PERMILLE=") + b);
		env.push_back("UM_POOLS=" + cfg["pools"]); env.push_back("UM_POOL_SEED=" + cfg["pool_seed"]); env.push_back("UM_POOL_JOIN=" + cfg["pool_join"]);
		env.push_back("UM_POOL_DEPOT=" + cfg["pool_depot_percent"]); env.push_back("UM_POOL_DERRICK=" + cfg["pool_derrick_percent"]); env.push_back("UM_POOL_OTHER=" + cfg["pool_other_percent"]);
		env.push_back("UM_INFLATION=" + cfg["inflation"]); env.push_back("UM_GDP_BASELINE=" + cfg["gdp_baseline"]); env.push_back("UM_INFLATION_MAX=" + cfg["inflation_max_percent"]); env.push_back("UM_DEFLATION_PERMILLE=" + cfg["deflation_permille"]);
		if (fileExists(worldDir + "\\epoch.sav")) env.push_back("UM_RESUME=1");

		// environment block: ours plus the UM_ variables
		std::string block; char *cur = GetEnvironmentStringsA();
		for (char *p = cur; *p; p += strlen(p) + 1) { if (strncmp(p, "UM_", 3) != 0) { block += p; block += '\0'; } }
		FreeEnvironmentStringsA(cur);
		for (size_t i = 0; i < env.size(); ++i) { block += env[i]; block += '\0'; }
		block += '\0';

		if (!job) { job = CreateJobObjectA(NULL, NULL); JOBOBJECT_EXTENDED_LIMIT_INFORMATION li; memset(&li, 0, sizeof li); li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li); }
		std::string cmd = "\"" + serverExe() + "\" -win -noshellmap -xres 800 -yres 600";
		STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si; PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
		if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB, (LPVOID)block.data(), gameDir.c_str(), &si, &pi))
			if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, (LPVOID)block.data(), gameDir.c_str(), &si, &pi))
			{ char m[96]; sprintf(m, "Could not start the server (error %lu).", GetLastError()); logLine(m); return false; }
		AssignProcessToJobObject(job, pi.hProcess); ResumeThread(pi.hThread); CloseHandle(pi.hThread);
		if (proc) CloseHandle(proc);
		proc = pi.hProcess; startedTick = GetTickCount();
		char m[400]; sprintf(m, "Server started (process %lu) on %s:%s, map %s, %s (%s).", (unsigned long)pi.dwProcessId, cfg["host"].c_str(), cfg["port"].c_str(), mapCode.c_str(), fileExists(worldDir + "\\epoch.sav") ? "resuming its saved world" : "new world", worldDir.c_str()); logLine(m);
		DeleteFileA((worldDir + "\\status.json").c_str());
		return true;
	}
	bool alive() { return proc && WaitForSingleObject(proc, 0) == WAIT_TIMEOUT; }
	void stop()
	{
		wanted = false;
		if (proc) { TerminateProcess(proc, 0); WaitForSingleObject(proc, 5000); CloseHandle(proc); proc = NULL; logLine("Server stopped."); }
	}
	// called about once a second: restart an unexpected exit from the saved world
	void supervise()
	{
		if (!wanted) return;
		if (proc && WaitForSingleObject(proc, 0) == WAIT_OBJECT_0)
		{
			DWORD code = 0; GetExitCodeProcess(proc, &code); CloseHandle(proc); proc = NULL; exitedTick = GetTickCount();
			char m[96]; sprintf(m, "The server exited (code 0x%08lx); restarting in 5 s.", (unsigned long)code); logLine(m);
		}
		if (!proc && exitedTick && GetTickCount() - exitedTick > 5000) { exitedTick = 0; ++restarts; launch(false); }
	}
	bool running() { return proc != NULL; }
};

static Runner gRunner;

// ------------------------------------------------------------------------------------------------ status
struct EcoRow { bool valid; int pool, depots, derricks, other, gdp; double mult; EcoRow() : valid(false), pool(0), depots(0), derricks(0), other(0), gdp(0), mult(1.0) {} };
struct PlayerRow { int slot; std::string name, faction; bool online; int missed; unsigned absentSecs; bool expiring; };
struct Status
{
	bool valid; std::string name, map; unsigned uptime, frame, gameSeconds, connections, queue, aiActive, humanSlots, humanFree, mismatches, maxAi, maxPlayers; std::vector<PlayerRow> players; EcoRow eco[3]; bool pools, inflation;
	Status() : valid(false), uptime(0), frame(0), gameSeconds(0), connections(0), queue(0), aiActive(0), humanSlots(0), humanFree(0), mismatches(0), maxAi(0), maxPlayers(0), pools(false), inflation(false) {}
};
static std::string jstr(const std::string &j, const std::string &key, size_t from = 0, size_t to = std::string::npos)
{
	std::string k = "\"" + key + "\":"; size_t p = j.find(k, from); if (p == std::string::npos || (to != std::string::npos && p >= to)) return "";
	p += k.size(); if (p < j.size() && j[p] == '"') { std::string o; for (++p; p < j.size() && j[p] != '"'; ++p) { if (j[p] == '\\' && p + 1 < j.size()) ++p; o += j[p]; } return o; }
	size_t e = j.find_first_of(",}]", p); return j.substr(p, e - p);
}
static Status readStatus(const std::string &worldDir)
{
	Status s; std::string j = readFile(worldDir + "\\status.json"); if (j.empty()) return s;
	s.valid = true; size_t pl = j.find("\"players\":[");
	s.name = jstr(j, "name"); s.map = jstr(j, "map"); s.uptime = (unsigned)atoi(jstr(j, "uptimeSeconds").c_str()); s.frame = (unsigned)atoi(jstr(j, "frame").c_str()); s.gameSeconds = (unsigned)atoi(jstr(j, "gameSeconds").c_str());
	s.connections = (unsigned)atoi(jstr(j, "connections").c_str()); s.queue = (unsigned)atoi(jstr(j, "queue").c_str()); s.aiActive = (unsigned)atoi(jstr(j, "aiActive").c_str());
	s.humanSlots = (unsigned)atoi(jstr(j, "humanSlots").c_str()); s.humanFree = (unsigned)atoi(jstr(j, "humanFree").c_str()); s.mismatches = (unsigned)atoi(jstr(j, "mismatches").c_str());
	s.maxAi = (unsigned)atoi(jstr(j, "maxAi").c_str()); s.maxPlayers = (unsigned)atoi(jstr(j, "maxPlayers").c_str());
	{	// money pools per faction currency
		size_t ec = j.find("\"economy\":{");
		if (ec != std::string::npos)
		{
			s.pools = jstr(j, "pools", ec) == "true"; s.inflation = jstr(j, "inflation", ec) == "true"; static const char *keys[] = { "usd", "cny", "ils" };
			for (int c = 0; c < 3; ++c)
			{
				size_t o = j.find(std::string("\"") + keys[c] + "\":{", ec); if (o == std::string::npos) continue; size_t e = j.find('}', o); if (e == std::string::npos) continue;
				EcoRow &r = s.eco[c]; r.valid = true; r.pool = atoi(jstr(j, "pool", o, e).c_str()); r.depots = atoi(jstr(j, "depots", o, e).c_str()); r.derricks = atoi(jstr(j, "derricks", o, e).c_str());
				r.other = atoi(jstr(j, "other", o, e).c_str()); r.gdp = atoi(jstr(j, "gdp", o, e).c_str()); r.mult = atof(jstr(j, "priceMultiplier", o, e).c_str());
			}
		}
	}
	if (pl != std::string::npos)
	{
		size_t p = pl;
		for (;;)
		{
			size_t o = j.find('{', p + 1); if (o == std::string::npos) break; size_t e = j.find('}', o); if (e == std::string::npos) break;
			PlayerRow r; r.slot = atoi(jstr(j, "slot", o, e).c_str()); r.name = jstr(j, "name", o, e); r.faction = jstr(j, "faction", o, e); r.online = jstr(j, "online", o, e) == "true";
			r.missed = atoi(jstr(j, "missedRent", o, e).c_str()); r.absentSecs = (unsigned)atoi(jstr(j, "absentSeconds", o, e).c_str()); r.expiring = jstr(j, "expiring", o, e) == "true";
			s.players.push_back(r); p = e;
		}
	}
	return s;
}
static std::string fmtDuration(unsigned s) { char b[48]; sprintf(b, "%ud %02uh %02um %02us", s / 86400, (s / 3600) % 24, (s / 60) % 60, s % 60); if (s < 86400) sprintf(b, "%02uh %02um %02us", s / 3600, (s / 60) % 60, s % 60); return b; }

// ------------------------------------------------------------------------------------------------ status page (HTTP)
static const char *kPage =
"<!doctype html><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\"><title>Living World</title><style>"
":root{--bg:#10151b;--fg:#e6edf3;--mut:#8b98a5;--card:#18202a;--ok:#3fb950;--warn:#d29922;--bad:#f85149}@media(prefers-color-scheme:light){:root{--bg:#f6f8fa;--fg:#1f2328;--mut:#59636e;--card:#fff}}"
"body{margin:0;padding:16px;background:var(--bg);color:var(--fg);font:15px system-ui,sans-serif;max-width:760px;margin:auto}h1{font-size:20px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}"
".c{background:var(--card);border-radius:8px;padding:12px}.c b{display:block;font-size:22px}.c span{color:var(--mut);font-size:13px}table{width:100%;border-collapse:collapse;margin-top:14px}td,th{padding:6px 4px;text-align:left;border-bottom:1px solid #8884}"
".ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}</style><h1 id=nm>Living World <small id=st class=ok></small></h1><div class=grid id=g></div><h2 style=\"font-size:16px;margin:18px 0 8px\">Money pools</h2><div class=grid id=eco></div>"
"<table><thead><tr><th>Seat<th>Player<th>Faction<th>State<th>Rent<th>Absent</thead><tbody id=t></tbody></table><script>"
"const f=n=>n.toLocaleString();const e=s=>String(s).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));"
"async function tick(){try{const s=await (await fetch('status.json',{cache:'no-store'})).json();document.getElementById('st').textContent='online';"
"document.getElementById('nm').firstChild.textContent=(s.name||'Living World')+' ';"
"const u=s.uptimeSeconds,cards=[['Uptime',Math.floor(u/3600)+'h '+Math.floor(u%3600/60)+'m'],['Players',s.players.filter(p=>p.online).length+' / '+s.maxPlayers],['Seats free',s.humanFree],['Waiting',f(s.queue)],['AI',s.aiActive+' / '+s.maxAi],['Desyncs',f(s.mismatches)]];"
"document.getElementById('g').innerHTML=cards.map(c=>'<div class=c><b>'+c[1]+'</b><span>'+c[0]+'</span></div>').join('');"
"const E=s.economy||{},sy={usd:'$',cny:'\\u00a5',ils:'\\u20aa'};document.getElementById('eco').innerHTML=['usd','cny','ils'].filter(k=>E[k]).map(k=>{const q=E[k];return '<div class=c><b>'+sy[k]+f(q.pool+q.depots+q.derricks+q.other)+'</b><span>'+k.toUpperCase()+' pool: depots '+f(q.depots)+', derricks '+f(q.derricks)+', other '+f(q.other)+(E.inflation?'; prices x'+q.priceMultiplier.toFixed(2)+', GDP '+f(q.gdp):'')+'</span></div>'}).join('')||'<div class=c><span>'+(E.pools===false?'Pools are off':'No data yet')+'</span></div>';"
"document.getElementById('t').innerHTML=s.players.map(p=>'<tr><td>#'+p.slot+'<td>'+e(p.name||'?')+'<td>'+e((p.faction||'').replace('Faction',''))+'<td class='+(p.expiring?'bad':p.online?'ok':'warn')+'>'+(p.expiring?'leaving':p.online?'online':'absent')+'<td class='+(p.missedRent?'bad':'')+'>'+(p.missedRent?p.missedRent+' unpaid':'paid')+'<td>'+(p.absentSeconds?Math.floor(p.absentSeconds/60)+'m':'')).join('');"
"}catch(x){const el=document.getElementById('st');el.textContent='offline';el.className='bad'}}tick();setInterval(tick,5000);</script>";

static DWORD WINAPI httpThread(LPVOID)
{
	WSADATA wd; WSAStartup(MAKEWORD(2, 2), &wd);
	int port = num(gRunner.cfg, "status_port", 8080); if (port <= 0) return 0;
	SOCKET ls = socket(AF_INET, SOCK_STREAM, 0); int yes = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
	sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((unsigned short)port);
	if (bind(ls, (sockaddr *)&a, sizeof a) != 0 || listen(ls, 8) != 0) { logLine("Status page: cannot listen on that port."); closesocket(ls); return 0; }
	logLine("Status page on http://localhost:" + gRunner.cfg["status_port"] + "/");
	for (;;)
	{
		SOCKET c = accept(ls, NULL, NULL); if (c == INVALID_SOCKET) break;
		char req[1024]; int n = recv(c, req, sizeof req - 1, 0); if (n <= 0) { closesocket(c); continue; } req[n] = 0;
		std::string body, type = "text/html; charset=utf-8";
		if (strncmp(req, "GET /status.json", 16) == 0) { body = readFile(gRunner.worldDir + "\\status.json"); if (body.empty()) body = "{}"; type = "application/json"; }
		else body = kPage;
		char h[256]; sprintf(h, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", type.c_str(), (unsigned)body.size());
		std::string out = std::string(h) + body; send(c, out.data(), (int)out.size(), 0); closesocket(c);
	}
	return 0;
}
static bool gHttpStarted = false;
static void ensureHttp() { if (!gHttpStarted) { gHttpStarted = true; CreateThread(NULL, 0, httpThread, NULL, 0, NULL); } }

// ------------------------------------------------------------------------------------------------ CLI
static volatile bool gStopCli = false;
static BOOL WINAPI ctrlHandler(DWORD) { gStopCli = true; return TRUE; }

static void usage()
{
	printf("Living World host\n\n  zh_host.exe                 open the window\n  zh_host.exe --cli [options] run the server in this terminal (Ctrl+C stops it)\n  --config <file>             settings file (default um_server.ini next to the program)\n\nOptions (they override the settings file):\n");
	for (int i = 0; i < kNumSettings; ++i) printf("  %-22s %s (default %s)\n", kSettings[i].flag, kSettings[i].label, kSettings[i].def);
}

static int runCli(Config cfg)
{
	std::string why; if (!gRunner.validate(cfg, why)) { printf("%s\n", why.c_str()); return 2; }
	if (!gRunner.start(cfg)) return 2;
	ensureHttp();
	SetConsoleCtrlHandler(ctrlHandler, TRUE);
	unsigned tick = 0;
	while (!gStopCli)
	{
		Sleep(1000); gRunner.supervise();
		if (++tick % 30 == 0)
		{
			Status s = readStatus(gRunner.worldDir);
			if (s.valid) { char b[200]; sprintf(b, "status: up %s, game time %s, %u connected, %u/%u seats free, %u AI, %u desyncs", fmtDuration(s.uptime).c_str(), fmtDuration(s.gameSeconds).c_str(), s.connections, s.humanFree, s.humanSlots, s.aiActive, s.mismatches); logLine(b); }
		}
	}
	gRunner.stop();
	return 0;
}

// ------------------------------------------------------------------------------------------------ window
enum { ID_START = 1000, ID_STOP, ID_SAVE, ID_OPENPAGE, ID_FIREWALL, ID_BROWSE, ID_EDIT0 = 2000, ID_TIMER = 1 };
static HWND gWnd, gStateLbl, gInfoLbl, gEconLbl, gPlayers, gLogBox, gEdits[64], gStartBtn, gStopBtn;
static size_t gLogShown = 0;

static std::string getText(HWND h) { char b[1024]; GetWindowTextA(h, b, sizeof b); return b; }
static Config configFromUi()
{
	Config c; for (int i = 0; i < kNumSettings; ++i) c[kSettings[i].key] = getText(gEdits[i]);
	{ std::string code, sm; resolveMap(c["map"], code, sm); for (int k = 0; k < kNumMaps; ++k) if (code == kMaps[k].code) c["map"] = code; }   // store the codename, not the label
	return c;
}

static void refreshUi()
{
	gRunner.supervise();
	Status s = readStatus(gRunner.worldDir);
	std::string state = !gRunner.wanted ? "Stopped" : (gRunner.running() ? (s.valid ? "Running" : "Starting...") : "Restarting...");
	SetWindowTextA(gStateLbl, ("State: " + state).c_str());
	EnableWindow(gStartBtn, !gRunner.wanted); EnableWindow(gStopBtn, gRunner.wanted);
	char b[400];
	if (gRunner.wanted && s.valid) sprintf(b, "Uptime %s   Game time %s\r\nConnected %u   Seats free %u / %u   Waiting %u\r\nAI active %u / %u   Desyncs %u   Restarts %d", fmtDuration(s.uptime).c_str(), fmtDuration(s.gameSeconds).c_str(), s.connections, s.humanFree, s.humanSlots, s.queue, s.aiActive, s.maxAi, s.mismatches, gRunner.restarts);
	else strcpy(b, "No data yet.");
	SetWindowTextA(gInfoLbl, b);
	{	// money pools: what is in the pool and in this cycle's budgets (depots / derricks / other generators), and the price level when inflation is on
		static const char *names[] = { "USD $", "CNY Y", "ILS S" }; std::string e = "Money pools" + std::string(s.pools ? "" : " (off)") + (s.inflation ? "   inflation on" : "") + "\r\n";
		for (int c = 0; c < 3; ++c)
		{
			char l[200]; if (!s.eco[c].valid) { sprintf(l, "%s: -\r\n", names[c]); e += l; continue; }
			const EcoRow &r = s.eco[c]; sprintf(l, "%s  pool %d   depots %d   derricks %d   other %d   GDP %d   prices x%.2f\r\n", names[c], r.pool, r.depots, r.derricks, r.other, r.gdp, r.mult); e += l;
		}
		SetWindowTextA(gEconLbl, (gRunner.wanted && s.valid) ? e.c_str() : "");
	}
	// players list
	SendMessageA(gPlayers, LB_RESETCONTENT, 0, 0);
	for (size_t i = 0; gRunner.wanted && i < s.players.size(); ++i)
	{
		const PlayerRow &r = s.players[i]; std::string f = r.faction; if (f.compare(0, 7, "Faction") == 0) f = f.substr(7);
		char l[200]; sprintf(l, "#%d  %-16s %-18s %s%s", r.slot, r.name.empty() ? "?" : r.name.c_str(), f.c_str(), r.expiring ? "leaving" : r.online ? "online" : "absent", r.missed ? ("   rent unpaid x" + std::to_string((long long)r.missed)).c_str() : "");
		SendMessageA(gPlayers, LB_ADDSTRING, 0, (LPARAM)l);
	}
	// log
	EnterCriticalSection(&gLogLock);
	if (gLog.size() != gLogShown || gLogShown == 0)
	{
		std::string all; for (size_t i = 0; i < gLog.size(); ++i) { all += gLog[i]; all += "\r\n"; }
		SetWindowTextA(gLogBox, all.c_str()); SendMessageA(gLogBox, EM_SETSEL, (WPARAM)all.size(), (LPARAM)all.size()); SendMessageA(gLogBox, EM_SCROLLCARET, 0, 0); gLogShown = gLog.size();
	}
	LeaveCriticalSection(&gLogLock);
}

// follow the engine's own log file into the window log
static void tailServerLog()
{
	static size_t offset = 0; static std::string lastFile;
	if (gRunner.serverLog.empty()) return;
	if (gRunner.serverLog != lastFile) { lastFile = gRunner.serverLog; offset = 0; }
	FILE *f = fopen(gRunner.serverLog.c_str(), "rb"); if (!f) return;
	fseek(f, 0, SEEK_END); long len = ftell(f); if ((size_t)len < offset) offset = 0;
	if ((size_t)len > offset)
	{
		fseek(f, (long)offset, SEEK_SET); std::string chunk(len - offset, 0); size_t n = fread(&chunk[0], 1, chunk.size(), f); chunk.resize(n); offset += n;
		size_t p = 0; while (p < chunk.size()) { size_t e = chunk.find('\n', p); if (e == std::string::npos) e = chunk.size(); std::string ln = chunk.substr(p, e - p); while (!ln.empty() && (ln[ln.size() - 1] == '\r')) ln.erase(ln.size() - 1); p = e + 1;
			if (ln.empty() || ln.find("dropped locally generated") != std::string::npos || ln.find("[um] frame ") != std::string::npos) continue; logLine(ln); }
	}
	fclose(f);
}

// um_server.ini edited by hand while the window was open: the file wins (the window showed the values it read at start-up and would overwrite the edit)
static unsigned long long gIniStamp = 0;
static unsigned long long iniStamp() { WIN32_FILE_ATTRIBUTE_DATA a; if (!GetFileAttributesExA(gIniPath.c_str(), GetFileExInfoStandard, &a)) return 0; return ((unsigned long long)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime; }
static void fillUiFromConfig(const Config &cfg)
{
	for (int i = 0; i < kNumSettings; ++i)
	{
		Config::const_iterator it = cfg.find(kSettings[i].key); std::string v = it != cfg.end() ? it->second : std::string();
		if (i == kMapSetting) { std::string code, sm; resolveMap(v, code, sm); int sel = -1; for (int k = 0; k < kNumMaps; ++k) if (code == kMaps[k].code) sel = k; if (sel >= 0) SendMessageA(gEdits[i], CB_SETCURSEL, sel, 0); else SetWindowTextA(gEdits[i], v.c_str()); }
		else SetWindowTextA(gEdits[i], v.c_str());
	}
}
static void onStart()
{
	unsigned long long st = iniStamp();
	if (st && gIniStamp && st != gIniStamp) { fillUiFromConfig(loadConfig(gIniPath)); logLine("um_server.ini changed on disk: settings reloaded from the file."); }
	Config c = configFromUi(); saveConfig(c, gIniPath); gIniStamp = iniStamp(); gRunner.restarts = 0; gLogShown = 0; if (gRunner.start(c)) ensureHttp();
}
static void onBrowse()
{
	BROWSEINFOA bi; memset(&bi, 0, sizeof bi); bi.hwndOwner = gWnd; bi.lpszTitle = "Select the Command & Conquer Generals - Zero Hour folder"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	LPITEMIDLIST id = SHBrowseForFolderA(&bi); if (id) { char p[MAX_PATH]; if (SHGetPathFromIDListA(id, p)) SetWindowTextA(gEdits[1], p); CoTaskMemFree(id); }
}
static void onFirewall()
{
	std::string port = getText(gEdits[4]);
	std::string args = "/c netsh advfirewall firewall add rule name=\"Living World server\" dir=in action=allow protocol=TCP localport=" + port + " & netsh advfirewall firewall add rule name=\"Living World status page\" dir=in action=allow protocol=TCP localport=" + getText(gEdits[5]) + " & pause";
	ShellExecuteA(gWnd, "runas", "cmd.exe", args.c_str(), NULL, SW_SHOW);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
	switch (m)
	{
	case WM_CREATE:
	{
		HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT); Config cfg = loadConfig(gIniPath); gIniStamp = iniStamp();
		int y = 10;
		for (int i = 0; i < kNumSettings; ++i)
		{
			HWND lbl = CreateWindowA("STATIC", kSettings[i].label, WS_CHILD | WS_VISIBLE, 10, y + 3, 190, 18, h, NULL, NULL, NULL); SendMessageA(lbl, WM_SETFONT, (WPARAM)font, TRUE);
			if (i == kMapSetting)
			{	// drop-down of the known maps (editable: a .map path can be typed in); each map keeps its own world
				gEdits[i] = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL, 205, y - 1, 170, 200, h, (HMENU)(INT_PTR)(ID_EDIT0 + i), NULL, NULL);
				SendMessageA(gEdits[i], WM_SETFONT, (WPARAM)font, TRUE); SendMessageA(gEdits[i], CB_SETDROPPEDWIDTH, 380, 0);
				std::string cur = cfg[kSettings[i].key], code, sm; resolveMap(cur, code, sm); int sel = -1;
				for (int k = 0; k < kNumMaps; ++k) { std::string item = std::string(kMaps[k].code) + "  - " + kMaps[k].label; SendMessageA(gEdits[i], CB_ADDSTRING, 0, (LPARAM)item.c_str()); if (code == kMaps[k].code) sel = k; }
				if (sel >= 0) SendMessageA(gEdits[i], CB_SETCURSEL, sel, 0); else SetWindowTextA(gEdits[i], cur.c_str());
				SendMessageA(gEdits[i], CB_SETEDITSEL, 0, MAKELPARAM(0, 0));   // show the start (the codename), not the end of a long label
			}
			else
			gEdits[i] = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", cfg[kSettings[i].key].c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 205, y, i == 1 || i == 2 ? 150 : 170, 20, h, (HMENU)(INT_PTR)(ID_EDIT0 + i), NULL, NULL);
			SendMessageA(gEdits[i], WM_SETFONT, (WPARAM)font, TRUE);
			if (i == 1) { HWND b = CreateWindowA("BUTTON", "...", WS_CHILD | WS_VISIBLE, 360, y, 15, 20, h, (HMENU)ID_BROWSE, NULL, NULL); SendMessageA(b, WM_SETFONT, (WPARAM)font, TRUE); }
			y += 24;
		}
		struct B { const char *t; int id; int x; int w; } bs[] = { { "Start", ID_START, 10, 80 }, { "Stop", ID_STOP, 96, 80 }, { "Save settings", ID_SAVE, 182, 90 }, { "Status page", ID_OPENPAGE, 278, 90 } };
		for (int i = 0; i < 4; ++i) { HWND b = CreateWindowA("BUTTON", bs[i].t, WS_CHILD | WS_VISIBLE, bs[i].x, y + 6, bs[i].w, 28, h, (HMENU)(INT_PTR)bs[i].id, NULL, NULL); SendMessageA(b, WM_SETFONT, (WPARAM)font, TRUE); if (i == 0) gStartBtn = b; if (i == 1) gStopBtn = b; }
		{ HWND b = CreateWindowA("BUTTON", "Allow through Windows Firewall", WS_CHILD | WS_VISIBLE, 10, y + 40, 220, 24, h, (HMENU)ID_FIREWALL, NULL, NULL); SendMessageA(b, WM_SETFONT, (WPARAM)font, TRUE); }
		gStateLbl = CreateWindowA("STATIC", "State: Stopped", WS_CHILD | WS_VISIBLE, 400, 10, 400, 20, h, NULL, NULL, NULL); SendMessageA(gStateLbl, WM_SETFONT, (WPARAM)font, TRUE);
		gInfoLbl = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 400, 34, 480, 58, h, NULL, NULL, NULL); SendMessageA(gInfoLbl, WM_SETFONT, (WPARAM)font, TRUE);
		gEconLbl = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 400, 96, 480, 52, h, NULL, NULL, NULL); SendMessageA(gEconLbl, WM_SETFONT, (WPARAM)font, TRUE);
		HWND pl = CreateWindowA("STATIC", "Players", WS_CHILD | WS_VISIBLE, 400, 152, 100, 16, h, NULL, NULL, NULL); SendMessageA(pl, WM_SETFONT, (WPARAM)font, TRUE);
		gPlayers = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL, 400, 170, 480, 100, h, NULL, NULL, NULL);
		{ HFONT mono = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, FIXED_PITCH, "Consolas"); SendMessageA(gPlayers, WM_SETFONT, (WPARAM)mono, TRUE);
		  HWND ll = CreateWindowA("STATIC", "Log", WS_CHILD | WS_VISIBLE, 400, 276, 100, 16, h, NULL, NULL, NULL); SendMessageA(ll, WM_SETFONT, (WPARAM)font, TRUE);
		  gLogBox = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 400, 294, 480, y + 64 - 294 > 120 ? y + 64 - 294 : 240, h, NULL, NULL, NULL); SendMessageA(gLogBox, WM_SETFONT, (WPARAM)mono, TRUE); }
		SetTimer(h, ID_TIMER, 1000, NULL);
		return 0;
	}
	case WM_COMMAND:
		switch (LOWORD(w))
		{
		case ID_START: onStart(); refreshUi(); return 0;
		case ID_STOP: gRunner.stop(); refreshUi(); return 0;
		case ID_SAVE: saveConfig(configFromUi(), gIniPath); gIniStamp = iniStamp(); logLine("Settings saved to " + gIniPath); return 0;
		case ID_BROWSE: onBrowse(); return 0;
		case ID_OPENPAGE: { std::string u = "http://localhost:" + getText(gEdits[5]) + "/"; ShellExecuteA(h, "open", u.c_str(), NULL, NULL, SW_SHOW); return 0; }
		case ID_FIREWALL: onFirewall(); return 0;
		}
		break;
	case WM_TIMER: tailServerLog(); refreshUi(); return 0;
	case WM_CLOSE:
		if (gRunner.wanted && MessageBoxA(h, "The server is running. Stop it and close?", "Living World host", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
		gRunner.stop(); DestroyWindow(h); return 0;
	case WM_DESTROY: PostQuitMessage(0); return 0;
	}
	return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show)
{
	InitializeCriticalSection(&gLogLock); CoInitialize(NULL);
	char self[MAX_PATH]; GetModuleFileNameA(NULL, self, MAX_PATH); gHostDir = dirOf(self); gIniPath = gHostDir + "\\um_server.ini";
	int argc = 0; LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc); std::vector<std::string> args;
	for (int i = 1; i < argc; ++i) { char b[1024]; WideCharToMultiByte(CP_ACP, 0, wargv[i], -1, b, sizeof b, NULL, NULL); args.push_back(b); }
	Config overrides; bool help = false;
	for (size_t i = 0; i < args.size(); ++i)
	{
		if (args[i] == "--cli") gCli = true;
		else if (args[i] == "--help" || args[i] == "-h" || args[i] == "/?") { help = true; gCli = true; }
		else if (args[i] == "--config" && i + 1 < args.size()) gIniPath = absPath(args[++i], gHostDir);
		else for (int k = 0; k < kNumSettings; ++k) if (args[i] == kSettings[k].flag && i + 1 < args.size()) { overrides[kSettings[k].key] = args[++i]; break; }
	}
	if (gCli)
	{
		if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
		freopen("CONOUT$", "w", stdout); freopen("CONOUT$", "w", stderr);
		if (help) { usage(); return 0; }
		Config cfg = loadConfig(gIniPath); for (Config::iterator it = overrides.begin(); it != overrides.end(); ++it) cfg[it->first] = it->second;
		gRunner.cfg = cfg; gRunner.worldDir = worldDirFor(cfg, gHostDir);
		gLogFile = absPath(cfg["world_dir"], gHostDir) + "\\host_log.txt";
		return runCli(cfg);
	}
	{ Config cfg = loadConfig(gIniPath); gRunner.worldDir = worldDirFor(cfg, gHostDir); gLogFile = absPath(cfg["world_dir"], gHostDir) + "\\host_log.txt"; }
	INITCOMMONCONTROLSEX ic = { sizeof ic, ICC_STANDARD_CLASSES }; InitCommonControlsEx(&ic);
	WNDCLASSA wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = proc; wc.hInstance = inst; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(1)); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.lpszClassName = "LivingWorldHost";
	RegisterClassA(&wc);
	RECT r = { 0, 0, 900, 24 * kNumSettings + 90 }; AdjustWindowRect(&r, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
	gWnd = CreateWindowA("LivingWorldHost", "Living World server", WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
	ShowWindow(gWnd, show);
	if (std::find(args.begin(), args.end(), "--start") != args.end()) PostMessageA(gWnd, WM_COMMAND, ID_START, 0);
	MSG msg; while (GetMessageA(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
	return 0;
}
