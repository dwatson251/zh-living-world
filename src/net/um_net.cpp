// Server-paced lockstep transport. See um_net.h.
#include "net/um_net.h"
#include <winsock.h>
#include "ZLib/zlib.h"
#include "Common/GameState.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/NameKeyGenerator.h"
#include "Common/ThingTemplate.h"
#include "Common/Money.h"
#include "Common/PlayerTemplate.h"
#include "Common/GameEngine.h"
#include "Common/MessageStream.h"
#include "Common/GlobalData.h"
#include "Common/MultiplayerSettings.h"
#include "Common/RandomValue.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/GameInfo.h"
#include "null/NullNetworkBase.h"

UmNet *TheUmNet = NULL;
static std::map<int, std::pair<int, unsigned> > gLastKill;       // victim player index -> (killer player index, frame)
void UmNoteCombatImpl(Int a, Int b);
void UmNoteCombat(Int a, Int b) { UmNoteCombatImpl(a, b); }
void UmNoteKill(Int victim, Int killer) { gLastKill[victim] = std::make_pair((int)killer, TheGameLogic ? TheGameLogic->getFrame() : 0u); }

// ---------------------------------------------------------------------------------------------------------------------
// byte buffer helpers
namespace {
struct Wr {
	std::string b;
	void u8(unsigned v) { b.push_back((char)v); }
	void u32(unsigned v) { for (int i = 0; i < 4; ++i) b.push_back((char)(v >> (8 * i))); }
	void i32(int v) { u32((unsigned)v); }
	void f32(float f) { unsigned u; memcpy(&u, &f, 4); u32(u); }
	void str(const std::string &s) { u32((unsigned)s.size()); b += s; }
};
struct Rd {
	const char *p; const char *e; bool bad;
	Rd(const char *a, size_t n) : p(a), e(a + n), bad(false) {}
	unsigned u8() { if (p + 1 > e) { bad = true; return 0; } return (unsigned char)*p++; }
	unsigned u32() { if (p + 4 > e) { bad = true; return 0; } unsigned v = 0; for (int i = 0; i < 4; ++i) v |= (unsigned)(unsigned char)p[i] << (8 * i); p += 4; return v; }
	int i32() { return (int)u32(); }
	float f32() { unsigned u = u32(); float f; memcpy(&f, &u, 4); return f; }
	std::string str() { unsigned n = u32(); if (bad || p + n > e) { bad = true; return std::string(); } std::string s(p, n); p += n; return s; }
};

enum { T_HELLO = 1, T_WORLD, T_FRAME, T_ADVANCE, T_CMD, T_CRC, T_CRCRESULT, T_EPOCH, T_WELCOME, T_KICK };
const unsigned UM_MAGIC = 0x574C4D55; // 'UMLW'
const unsigned UM_VERSION = 2;

bool isNetCommand(const GameMessage *m)
{
	return m && m->getType() > GameMessage::MSG_BEGIN_NETWORK_MESSAGES && m->getType() < GameMessage::MSG_END_NETWORK_MESSAGES;
}

// GameMessage <-> bytes (own format: the stock NetPacket carries 8-bit player masks, which cannot address 128 players)
void putMessage(Wr &w, GameMessage *m)
{
	w.u32((unsigned)m->getType()); w.i32(m->getPlayerIndex()); w.u8(m->getArgumentCount());
	for (int i = 0; i < m->getArgumentCount(); ++i)
	{
		GameMessageArgumentDataType t = m->getArgumentDataType(i); const GameMessageArgumentType *a = m->getArgument(i);
		w.u8((unsigned)t);
		switch (t)
		{
		case ARGUMENTDATATYPE_INTEGER: w.i32(a->integer); break;
		case ARGUMENTDATATYPE_REAL: w.f32(a->real); break;
		case ARGUMENTDATATYPE_BOOLEAN: w.u8(a->boolean ? 1 : 0); break;
		case ARGUMENTDATATYPE_OBJECTID: w.u32((unsigned)a->objectID); break;
		case ARGUMENTDATATYPE_DRAWABLEID: w.u32((unsigned)a->drawableID); break;
		case ARGUMENTDATATYPE_TEAMID: w.u32(a->teamID); break;
		case ARGUMENTDATATYPE_LOCATION: w.f32(a->location.x); w.f32(a->location.y); w.f32(a->location.z); break;
		case ARGUMENTDATATYPE_PIXEL: w.i32(a->pixel.x); w.i32(a->pixel.y); break;
		case ARGUMENTDATATYPE_PIXELREGION: w.i32(a->pixelRegion.lo.x); w.i32(a->pixelRegion.lo.y); w.i32(a->pixelRegion.hi.x); w.i32(a->pixelRegion.hi.y); break;
		case ARGUMENTDATATYPE_TIMESTAMP: w.u32(a->timestamp); break;
		case ARGUMENTDATATYPE_WIDECHAR: w.u32((unsigned)a->wChar); break;
		default: break;
		}
	}
}

GameMessage *getMessage(Rd &r)
{
	unsigned type = r.u32(); int player = r.i32(); unsigned argc = r.u8();
	if (r.bad) return NULL;
	GameMessage *m = newInstance(GameMessage)((GameMessage::Type)type);
	m->friend_setPlayerIndex(player);
	for (unsigned i = 0; i < argc && !r.bad; ++i)
	{
		switch ((GameMessageArgumentDataType)r.u8())
		{
		case ARGUMENTDATATYPE_INTEGER: m->appendIntegerArgument(r.i32()); break;
		case ARGUMENTDATATYPE_REAL: m->appendRealArgument(r.f32()); break;
		case ARGUMENTDATATYPE_BOOLEAN: m->appendBooleanArgument(r.u8() != 0); break;
		case ARGUMENTDATATYPE_OBJECTID: m->appendObjectIDArgument((ObjectID)r.u32()); break;
		case ARGUMENTDATATYPE_DRAWABLEID: m->appendDrawableIDArgument((DrawableID)r.u32()); break;
		case ARGUMENTDATATYPE_TEAMID: m->appendTeamIDArgument(r.u32()); break;
		case ARGUMENTDATATYPE_LOCATION: { Coord3D c; c.x = r.f32(); c.y = r.f32(); c.z = r.f32(); m->appendLocationArgument(c); } break;
		case ARGUMENTDATATYPE_PIXEL: { ICoord2D c; c.x = r.i32(); c.y = r.i32(); m->appendPixelArgument(c); } break;
		case ARGUMENTDATATYPE_PIXELREGION: { IRegion2D g; g.lo.x = r.i32(); g.lo.y = r.i32(); g.hi.x = r.i32(); g.hi.y = r.i32(); m->appendPixelRegionArgument(g); } break;
		case ARGUMENTDATATYPE_TIMESTAMP: m->appendTimestampArgument(r.u32()); break;
		case ARGUMENTDATATYPE_WIDECHAR: { WideChar c = (WideChar)r.u32(); m->appendWideCharArgument(c); } break;
		default: r.bad = true; break;
		}
	}
	if (r.bad) { m->deleteInstance(); return NULL; }
	return m;
}

void putWorld(Wr &w, const UmWorldDesc &d)
{
	w.str(d.map); w.i32(d.seed); w.i32(d.synthStarts); w.u32((unsigned)d.slots.size());
	for (size_t i = 0; i < d.slots.size(); ++i) { const UmSlotDesc &s = d.slots[i]; w.i32(s.state); w.i32(s.tmpl); w.i32(s.color); w.i32(s.startPos); w.i32(s.team); w.str(s.name); w.i32(s.profT); w.u32(s.rgb); w.str(s.nick); }
}
bool getWorld(Rd &r, UmWorldDesc &d)
{
	d.map = r.str(); d.seed = r.i32(); d.synthStarts = r.i32(); unsigned n = r.u32(); d.slots.clear();
	for (unsigned i = 0; i < n && !r.bad; ++i) { UmSlotDesc s; s.state = r.i32(); s.tmpl = r.i32(); s.color = r.i32(); s.startPos = r.i32(); s.team = r.i32(); s.name = r.str(); s.profT = r.i32(); s.rgb = r.u32(); s.nick = r.str(); d.slots.push_back(s); }
	return !r.bad;
}

void frameMsg(Wr &out, unsigned type, const std::string &payload)
{
	out.u32((unsigned)payload.size() + 1); out.u8(type); out.b += payload;
}
} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// snapshot files (Zero Hour's own save format: complete logic state, including activities and the AI)
static std::string umSavePath(const char *leaf) { return std::string(TheGameState->getFilePathInSaveDirectory(AsciiString(leaf)).str()); }
static bool umReadFile(const std::string &path, std::string &out)
{
	FILE *f = fopen(path.c_str(), "rb"); if (!f) return false; char buf[65536]; size_t n; out.clear();
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
	fclose(f); return true;
}
static bool umWriteFile(const std::string &path, const std::string &data)
{
	FILE *f = fopen(path.c_str(), "wb"); if (!f) return false; size_t n = fwrite(data.data(), 1, data.size(), f); fclose(f); return n == data.size();
}
static std::string umZip(const std::string &raw)
{
	uLongf zl = compressBound((uLong)raw.size()); std::string z(zl, '\0');
	if (compress2((Bytef *)&z[0], &zl, (const Bytef *)raw.data(), (uLong)raw.size(), 1) != Z_OK) return std::string();
	z.resize(zl); return z;
}
static bool umUnzip(const std::string &z, unsigned rawSize, std::string &raw)
{
	raw.assign(rawSize, '\0'); uLongf rl = rawSize;
	return uncompress((Bytef *)&raw[0], &rl, (const Bytef *)z.data(), (uLong)z.size()) == Z_OK && rl == rawSize;
}
static bool umLoadSnapshotFile(const char *leaf)
{
	AvailableGameInfo info; info.filename = leaf; info.next = info.prev = NULL; info.saveGameInfo.saveFileType = SAVE_FILE_TYPE_NORMAL;
	SaveCode rc = TheGameState->loadGame(info); return rc == SC_OK;
}

int UmSynthStarts = 0;
struct UmProfile { int tmpl; unsigned rgb; std::string nick; UmProfile() : tmpl(-1), rgb(0) {} };
static std::vector<UmProfile> gProfiles;   // per slot: faction override, custom colour and display name chosen by the occupant
void UmSetProfile(int slot, int tmpl, unsigned rgb, const std::string &nick)
{
	if (slot < 0 || slot >= MAX_SLOTS) return;
	if ((int)gProfiles.size() <= slot) gProfiles.resize(slot + 1);
	gProfiles[slot].tmpl = tmpl; gProfiles[slot].rgb = rgb; gProfiles[slot].nick = nick;
}
// Called after the players of a (re)started world exist: put every occupant's faction override, colour and name on its player.
void UmApplyProfiles()
{
	for (size_t i = 0; i < gProfiles.size(); ++i)
	{
		const UmProfile &p = gProfiles[i]; if (p.tmpl < 0 && !p.rgb && p.nick.empty()) continue;
		char nm[32]; sprintf(nm, "player%d", (int)i);
		Player *pl = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(nm)); if (!pl) continue;
		UnicodeString name; for (size_t c = 0; c < p.nick.size(); ++c) name.concat((WideChar)(unsigned char)p.nick[c]);
		pl->umApplyProfile(p.tmpl >= 0 ? ThePlayerTemplateStore->getNthPlayerTemplate(p.tmpl) : NULL, p.rgb, name);
	}
}
std::string gUmKickReason; DWORD gUmKickTick = 0; bool gUmHasUi = false;   // the graphical client shows the reason for a few seconds before it closes
static std::vector<bool> gInertSlots;
bool UmIsInertSlot(int slot) { return slot >= 0 && slot < (int)gInertSlots.size() && gInertSlots[slot]; }
void UmStartWorld(const UmWorldDesc &w)
{
	if (TheGameLogic && TheGameLogic->isInGame()) TheGameLogic->clearGameData(FALSE);   // graphical client: leave the shell map first, like the retail skirmish start does
	gInertSlots.assign(w.slots.size(), false); for (size_t i = 0; i < w.slots.size(); ++i) gInertSlots[i] = !w.slots[i].name.empty() && w.slots[i].name[0] == '~';
	gProfiles.clear(); for (size_t i = 0; i < w.slots.size(); ++i) UmSetProfile((int)i, w.slots[i].profT - 1, w.slots[i].rgb, w.slots[i].nick);
	UmSynthStarts = w.synthStarts;
	TheMultiplayerSettings->extendColors(MAX_SLOTS + 1);
	if (!TheSkirmishGameInfo) TheSkirmishGameInfo = NEW SkirmishGameInfo;
	TheSkirmishGameInfo->init(); TheSkirmishGameInfo->clearSlotList(); TheSkirmishGameInfo->reset(); TheSkirmishGameInfo->enterGame();
	for (size_t i = 0; i < w.slots.size() && (int)i < MAX_SLOTS; ++i)
	{
		const UmSlotDesc &d = w.slots[i]; GameSlot s;
		UnicodeString name; const std::string &shown = d.nick.empty() ? d.name : d.nick; for (size_t c = 0; c < shown.size(); ++c) name.concat((WideChar)(unsigned char)shown[c]);
		s.setState((SlotState)d.state, name); s.setPlayerTemplate(d.tmpl); s.setColor(d.color); s.setStartPos(d.startPos); s.setTeamNumber(d.team);
		TheSkirmishGameInfo->setSlot((Int)i, s);
	}
	TheSkirmishGameInfo->setMap(AsciiString(w.map.c_str()));
	TheSkirmishGameInfo->setSeed(w.seed);
	TheWritableGlobalData->m_mapName = AsciiString(w.map.c_str());
	TheSkirmishGameInfo->startGame(0);
	InitGameLogicRandom(TheSkirmishGameInfo->getSeed());
	GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_NEW_GAME);
	msg->appendIntegerArgument(GAME_SKIRMISH); msg->appendIntegerArgument(DIFFICULTY_NORMAL); msg->appendIntegerArgument(0); msg->appendIntegerArgument(30);
}

// ---------------------------------------------------------------------------------------------------------------------
struct UmConn
{
	SOCKET s; std::string in, out; bool hello; bool live; std::string name; std::string nick, faction; unsigned rgb; int id; int slot; unsigned boundFrame; unsigned cmdsThisTick;
	UmConn() : s(INVALID_SOCKET), hello(false), live(false), rgb(0), id(0), slot(-1), boundFrame(0), cmdsThisTick(0) {}
};

class UmNet : public NullNetworkBase
{
public:
	UmNet(UmRole role) : m_role(role), m_listen(INVALID_SOCKET), m_sock(INVALID_SOCKET), m_ready(FALSE), m_started(false), m_limit(0), m_pendingCount(0),
		m_lastFrame(0xffffffff), m_nextTick(0), m_nextConnId(1), m_mismatches(0), m_crcChecked(0), m_gotWorld(false), m_localDropped(0), m_uncapped(getenv("UM_UNCAPPED") != NULL), m_epochFrame(0), m_epochRaw(0), m_epochInterval(getenv("UM_EPOCH") ? (unsigned)atoi(getenv("UM_EPOCH")) : 0), m_needSnapshot(false), m_mySlot(-1), m_resumePending(false), m_resumeTarget(0), m_testDone(false), m_dropLog(0), m_aiNextJoin(0), m_nextDamage(0), m_startTick(GetTickCount()), m_supplyBucket(0xffffffffu) { const char *d = getenv("UM_WORLD_DIR"); if (d) { m_dir = d; CreateDirectoryA(m_dir.c_str(), NULL); } }

	// --- lifecycle
	virtual void init(void) {}
	virtual void reset(void) {}
	virtual void liteupdate(void) {}
	virtual void startGame(void) {}
	virtual void update(void)
	{
		m_ready = FALSE;
		takeLocalCommands();
		if (m_role == UM_ROLE_SERVER) serverUpdate(); else clientUpdate();
	}
	virtual Bool isFrameDataReady(void) { return m_ready; }
	virtual Int getExecutionFrame(void) { return TheGameLogic ? TheGameLogic->getFrame() : 0; }
	virtual Bool sawCRCMismatch(void) { return FALSE; }
	virtual Bool areAllQueuesEmpty(void) { return TRUE; }
	virtual Bool isPlayerConnected(Int) { return TRUE; }
	virtual UnsignedInt getRunAhead(void) { return 0; }
	virtual UnsignedInt getFrameRate(void) { return 30; }
	virtual Bool isPacketRouter(void) { return m_role == UM_ROLE_SERVER; }
	virtual Int getNumPlayers(void) { return (Int)m_conns.size(); }

	// --- setup
	void startServer(int port, const UmWorldDesc &world)
	{
		m_world = world; m_worldEpoch = world; netInit();
		m_listen = socket(AF_INET, SOCK_STREAM, 0); unsigned long nb = 1; ioctlsocket(m_listen, FIONBIO, &nb);
		int yes = 1; setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
		sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((unsigned short)port);
		if (const char *bindTo = getenv("UM_BIND")) { if (*bindTo && strcmp(bindTo, "0.0.0.0") != 0) a.sin_addr.s_addr = inet_addr(bindTo); }   // host: the address to listen on
		if (bind(m_listen, (sockaddr *)&a, sizeof(a)) != 0 || listen(m_listen, 64) != 0) { printf("[um] server: cannot listen on port %d (err %d)\n", port, WSAGetLastError()); m_listen = INVALID_SOCKET; return; }
		printf("[um] server: listening on port %d\n", port); m_startTick = GetTickCount();
		if (getenv("UM_RESUME") && tryResume()) { m_started = true; return; }
		UmStartWorld(m_world); m_started = true;
	}
	void startClient(const char *host, int port)
	{
		netInit();
		m_sock = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port);
		a.sin_addr.s_addr = inet_addr(host);
		if (connect(m_sock, (sockaddr *)&a, sizeof(a)) != 0) { int err = WSAGetLastError(); printf("[um] client: connect to %s:%d failed (err %d)\n", host, port, err); fflush(stdout); closesocket(m_sock); m_sock = INVALID_SOCKET;
			{	// without a server there is nothing to play: the engine would fall back to its retail main menu, which this mod does not support
				char msg[400]; sprintf(msg, "Could not connect to %s:%d (error %d).\n\nCheck that the server is running, the address and port are right, and that the server's firewall / port forwarding allows it. If the server keeps restarting, antivirus software may be stopping zh_client.exe on the server PC.", host, port, err);
				MessageBoxA(NULL, msg, "Living World", MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND); ExitProcess(0);
			}
			return; }
		unsigned long nb = 1; ioctlsocket(m_sock, FIONBIO, &nb);
		char keybuf[64]; sprintf(keybuf, "key-%lu", (unsigned long)GetTickCount()); const char *key = getenv("UM_KEY") ? getenv("UM_KEY") : keybuf;
		Wr h; h.u32(UM_MAGIC); h.u32(UM_VERSION); h.str(key);
		h.str(getenv("UM_NAME") ? getenv("UM_NAME") : ""); h.str(getenv("UM_FACTION") ? getenv("UM_FACTION") : ""); h.u32(getenv("UM_COLOR") ? (unsigned)strtoul(getenv("UM_COLOR"), NULL, 16) : 0);
		Wr f; frameMsg(f, T_HELLO, h.b); sendAll(m_sock, f.b);
		printf("[um] client: connected to %s:%d\n", host, port);
	}

	// --- per-frame bookkeeping, called after the logic has run
	void postUpdate()
	{
		if (!TheGameLogic || !TheGameLogic->isInGame()) return;
		UnsignedInt f = TheGameLogic->getFrame();
		if (f == m_lastFrame) return;
		m_lastFrame = f;
		if (f % 1500 == 0) status();
		if (const char *df = getenv("UM_DEEPCRC_FRAME"))   // debugging aid: dump every xfer'd value at a frame, diff server vs replica
			if ((unsigned)atoi(df) == f) { char nm[64]; sprintf(nm, "deepcrc_%s_%u.bin", m_role == UM_ROLE_SERVER ? "server" : "client", f); TheGameLogic->getCRC(CRC_RECALC, AsciiString(nm)); printf("[um] deep CRC written to %s\n", nm); char tn[64]; sprintf(tn, "deepobjs_%s_%u.txt", m_role == UM_ROLE_SERVER ? "server" : "client", f); if (FILE *tf = fopen(tn, "w")) { for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject()) { const Coord3D *pp = o->getPosition(); fprintf(tf, "%u %s p%d %.3f %.3f %.3f\n", (unsigned)o->getID(), o->getTemplate()->getName().str(), o->getControllingPlayer() ? o->getControllingPlayer()->getPlayerIndex() : -1, pp->x, pp->y, pp->z); } fclose(tf); } }
		if (m_role == UM_ROLE_SERVER)
		{
			Wr w; w.u32(f); Wr fr; frameMsg(fr, T_ADVANCE, w.b);       // clients may now execute frames < f
			for (size_t i = 0; i < m_conns.size(); ++i) if (m_conns[i].live) m_conns[i].out += fr.b;
			if (m_epochInterval && f > 0 && f % m_epochInterval == 0) serverEpoch(f);
			else if (!m_dir.empty() && m_epochFrame == 0 && f >= 60) serverEpoch(f);      // a persisted world needs a base snapshot early
			if (!m_dir.empty() && f % 150 == 0) { persistFrameMark(f); writeStatus(f); }
			if (f % 15 == 0) census(f);
			for (size_t i = 0; i < m_conns.size(); ++i) m_conns[i].cmdsThisTick = 0;
			if (f % 30 == 0 && f != m_epochFrame) { m_crcHist[f] = TheGameLogic->getCRC(CRC_RECALC); while (m_crcHist.size() > 4096) m_crcHist.erase(m_crcHist.begin()); }
			flushAll();
		}
		else if (m_sock != INVALID_SOCKET && getenv("UM_BOT") && m_mySlot >= 0 && f % 300 == 0)
		{
			// scripted human: toggles retaliation mode (a harmless, state-changing order) to exercise the command path end to end
			static bool flip = false; flip = !flip; GameMessage *m = newInstance(GameMessage)(GameMessage::MSG_ENABLE_RETALIATION_MODE); m->appendIntegerArgument(0); m->appendBooleanArgument(flip);
			Wr w; putMessage(w, m); Wr fr; frameMsg(fr, T_CMD, w.b); sendAll(m_sock, fr.b); m->deleteInstance();
		}
		if (m_role == UM_ROLE_CLIENT && f % 30 == 0 && m_sock != INVALID_SOCKET)
		{
			Wr w; w.u32(f); w.u32((unsigned)TheGameLogic->getCRC(CRC_RECALC)); Wr fr; frameMsg(fr, T_CRC, w.b); sendAll(m_sock, fr.b);
		}
	}

	void status()
	{
		if (m_role == UM_ROLE_SERVER && TheGameLogic && TheGameLogic->isInGame()) {   // who owns what
			std::map<int, int> cnt; for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject()) if (o->getControllingPlayer()) ++cnt[o->getControllingPlayer()->getPlayerIndex()];
			std::string line; char b[48]; for (int sl = 1; sl <= 4; ++sl) if (Player *p = playerForSlot(sl)) { sprintf(b, " base%d:%d", sl, cnt[p->getPlayerIndex()]); line += b; }
			for (std::map<int, unsigned>::const_iterator it = m_aiSlots.begin(); it != m_aiSlots.end(); ++it) if (Player *p = playerForSlot(it->first)) { sprintf(b, " ai%d[%.14s]:%d(bl%d,$%u,%s)", it->first, p->getPlayerTemplate() ? p->getPlayerTemplate()->getName().str() : "?", cnt[p->getPlayerIndex()], p->getBuildList() ? 1 : 0, (unsigned)p->getMoney()->countMoney(), p->getPlayerType() == PLAYER_COMPUTER ? "cpu" : "other"); line += b; }
			for (std::map<std::string, Rec>::const_iterator it = m_recs.begin(); it != m_recs.end(); ++it) if (Player *p = playerForSlot(it->second.slot)) { sprintf(b, " h%d:%d(m%d,$%u)", it->second.slot, cnt[p->getPlayerIndex()], it->second.missed, (unsigned)p->getMoney()->countMoney()); line += b; }
			if (!line.empty()) printf("[um] server holdings:%s\n", line.c_str()); }
		if (m_role == UM_ROLE_SERVER) printf("[um] server frame %u clients %d log %d batches crc-checked %d mismatches %d\n", TheGameLogic ? TheGameLogic->getFrame() : 0, (int)m_conns.size(), (int)m_log.size(), m_crcChecked, m_mismatches);
		else printf("[um] client frame %u limit %u world %d crc-ok %d mismatches %d\n", TheGameLogic ? TheGameLogic->getFrame() : 0, m_limit, m_gotWorld ? 1 : 0, m_crcChecked, m_mismatches);
	}
	int mismatches() const { return m_mismatches; }

private:
	DWORD m_startTick; unsigned m_supplyBucket;
	UmRole m_role; SOCKET m_listen, m_sock; Bool m_ready; bool m_started; unsigned m_limit; unsigned m_lastFrame; DWORD m_nextTick;
	std::vector<UmConn> m_conns; int m_nextConnId; UmWorldDesc m_world; std::map<unsigned, std::string> m_log; std::map<unsigned, std::string> m_inbox;
	std::map<unsigned, UnsignedInt> m_crcHist; std::string m_pending; unsigned m_pendingCount; int m_mismatches, m_crcChecked; bool m_gotWorld, m_uncapped;
	std::string m_recv;
	unsigned m_epochFrame, m_epochRaw, m_epochInterval; std::string m_epochZ;                     // server: latest canonical snapshot
	std::map<unsigned, std::pair<unsigned, std::string> > m_epochInbox;                              // client: snapshots waiting for their frame
	bool m_needSnapshot;                                                                              // client: world will arrive as a snapshot
	int m_mySlot;
public:
	int mySlot() const { return m_mySlot; }
private:
	std::string m_dir; bool m_resumePending; unsigned m_resumeTarget; std::map<unsigned, std::string> m_replay;   // server persistence: world dir, restart state

	// Like the stock Network::GetCommandsFromCommandList: network-range messages sitting in TheCommandList were generated locally
	// (UI/translators). They must NOT run locally; they are forwarded to the server, which decides what executes and when.
	void takeLocalCommands()
	{
		if (!TheCommandList) return;
		GameMessage *msg = TheCommandList->getFirstMessage();
		while (msg)
		{
			GameMessage *next = msg->next();
			if (isNetCommand(msg))
			{
				if (getenv("UM_TRACE_CMD") && msg->getType() != GameMessage::MSG_SET_REPLAY_CAMERA) printf("[um] %s: local command type %d (player %d)%s\n", m_role == UM_ROLE_SERVER ? "server" : "client", (int)msg->getType(), msg->getPlayerIndex(), (m_role == UM_ROLE_CLIENT && m_mySlot >= 0) ? " -> sent" : " -> dropped");
				if (m_role == UM_ROLE_CLIENT && m_sock != INVALID_SOCKET && m_mySlot >= 0 && msg->getType() != GameMessage::MSG_SET_REPLAY_CAMERA) { Wr w; putMessage(w, msg); Wr f; frameMsg(f, T_CMD, w.b); sendAll(m_sock, f.b); }
				else if (m_role == UM_ROLE_SERVER) { /* the server has no local human; nothing may originate here */ }
				if (m_localDropped < 20) { ++m_localDropped; printf("[um] %s: dropped locally generated command type %d (player %d)\n", m_role == UM_ROLE_SERVER ? "server" : "client", (int)msg->getType(), msg->getPlayerIndex()); }
				TheCommandList->removeMessage(msg); msg->deleteInstance();
			}
			msg = next;
		}
	}
	int m_localDropped;
	static void netInit() { static bool done = false; if (!done) { WSADATA d; WSAStartup(MAKEWORD(1, 1), &d); done = true; } }
	static void sendAll(SOCKET s, const std::string &b) { size_t off = 0; while (off < b.size()) { int n = send(s, b.data() + off, (int)(b.size() - off), 0); if (n <= 0) { if (WSAGetLastError() == WSAEWOULDBLOCK) { Sleep(1); continue; } return; } off += n; } }

	void flush(UmConn &c)
	{
		while (!c.out.empty())
		{
			int n = send(c.s, c.out.data(), (int)c.out.size(), 0);
			if (n <= 0) { if (WSAGetLastError() != WSAEWOULDBLOCK) { closesocket(c.s); c.s = INVALID_SOCKET; } return; }
			c.out.erase(0, n);
		}
	}
	void flushAll() { for (size_t i = 0; i < m_conns.size(); ++i) if (m_conns[i].s != INVALID_SOCKET) flush(m_conns[i]); }

	// pull whole frames [len][type][payload] out of a receive buffer
	static bool nextFrame(std::string &buf, unsigned &type, std::string &payload)
	{
		if (buf.size() < 5) return false;
		unsigned len = 0; for (int i = 0; i < 4; ++i) len |= (unsigned)(unsigned char)buf[i] << (8 * i);
		if (len == 0 || len > (64u << 20)) { buf.clear(); return false; }
		if (buf.size() < 4 + len) return false;
		type = (unsigned char)buf[4]; payload.assign(buf, 5, len - 1); buf.erase(0, 4 + len); return true;
	}
	static bool pump(SOCKET s, std::string &buf)    // returns false when the peer is gone
	{
		char tmp[8192];
		for (;;) { int n = recv(s, tmp, sizeof(tmp), 0); if (n > 0) { buf.append(tmp, n); continue; } if (n == 0) return false; return WSAGetLastError() == WSAEWOULDBLOCK; }
	}

	// ---------------------------------------------------------------- persistence (server)
	// <dir>/epoch.sav (canonical snapshot) + world.bin (description) + cmds.log (batches since the epoch) + frame.txt (last frame reached).
	// Everything is written to a temp name and renamed so a crash never leaves a half-written world.
	std::string pathOf(const char *leaf) const { return m_dir + "\\" + leaf; }
	void atomicWrite(const char *leaf, const std::string &data)
	{
		if (m_dir.empty()) return;
		std::string tmp = pathOf(leaf) + ".tmp", dst = pathOf(leaf);
		if (!umWriteFile(tmp, data)) { printf("[um] persist: cannot write %s\n", tmp.c_str()); return; }
		remove(dst.c_str()); rename(tmp.c_str(), dst.c_str());
	}
	void persistEpoch(unsigned f, const std::string &raw)
	{
		if (m_dir.empty()) return;
		CreateDirectoryA(m_dir.c_str(), NULL);
		atomicWrite("epoch.sav", raw);
		Wr w; putWorld(w, m_worldEpoch); w.u32(f); atomicWrite("world.bin", w.b);
		atomicWrite("cmds.log", std::string());
		atomicWrite("frame.txt", std::string());
	}
	void persistBatch(unsigned f, const std::string &body)
	{
		if (m_dir.empty()) return;
		FILE *fp = fopen(pathOf("cmds.log").c_str(), "ab"); if (!fp) return;
		unsigned len = (unsigned)body.size() + 4; fwrite(&len, 4, 1, fp); fwrite(&f, 4, 1, fp); fwrite(body.data(), 1, body.size(), fp); fclose(fp);
	}
	// status.json: what the host's status page shows. No identity keys ever appear here, only slots.
	static std::string jsonEsc(const std::string &t) { std::string o; for (size_t i = 0; i < t.size(); ++i) { char c = t[i]; if (c == '"' || c == '\\') { o += '\\'; o += c; } else if ((unsigned char)c >= 32 && (unsigned char)c < 127) o += c; } return o; }
	void writeStatus(unsigned f)
	{
		std::string o; char b[512];
		{ const char *nm = getenv("UM_SERVER_NAME"); const char *mp = m_world.map.c_str(); unsigned up = (GetTickCount() - m_startTick) / 1000;
		  sprintf(b, "{\"name\":\"%s\",\"map\":\"%s\",\"version\":\"0.0.1\",\"uptimeSeconds\":%u,\"maxAi\":%u,\"maxPlayers\":%u,", jsonEsc(nm ? nm : "Living World").c_str(), jsonEsc(mp).c_str(), up, envU("UM_AI_MAX", 0), (unsigned)countSlots("~free")); o += b; }
		unsigned connected = 0; for (size_t i = 0; i < m_conns.size(); ++i) if (m_conns[i].live) ++connected;
		sprintf(b, "\"frame\":%u,\"gameSeconds\":%u,\"epochFrame\":%u,\"connections\":%u,\"queue\":%u,\"aiActive\":%u,\"humanSlots\":%u,\"humanFree\":%u,\"mismatches\":%u,\"players\":[",
			f, f / 30, m_epochFrame, connected, (unsigned)m_queue.size(), (unsigned)m_aiSlots.size(), (unsigned)countSlots("~free"), (unsigned)(countSlots("~free") - m_slotKey.size()), m_mismatches);
		o += b; bool first = true;
		for (std::map<std::string, Rec>::const_iterator it = m_recs.begin(); it != m_recs.end(); ++it)
		{
			const Rec &r = it->second; bool online = false; for (size_t i = 0; i < m_conns.size(); ++i) if (m_conns[i].live && m_conns[i].name == r.key && m_conns[i].slot == r.slot) online = true;
			unsigned absentFor = (!online && r.left) ? (f - r.left) / 30 : 0;
			std::string nick, fac; if (r.slot >= 0 && r.slot < (int)m_world.slots.size()) { nick = m_world.slots[r.slot].nick; int t = m_world.slots[r.slot].profT - 1; const PlayerTemplate *pt = t >= 0 ? ThePlayerTemplateStore->getNthPlayerTemplate(t) : NULL; if (pt) fac = pt->getName().str(); }
			sprintf(b, "%s{\"slot\":%d,\"name\":\"%s\",\"faction\":\"%s\",\"online\":%s,\"absentSeconds\":%u,\"missedRent\":%d,\"expiring\":%s,\"nextRentFrame\":%u}", first ? "" : ",", r.slot, jsonEsc(nick).c_str(), jsonEsc(fac).c_str(), online ? "true" : "false", absentFor, r.missed, r.expiring ? "true" : "false", r.nextRent);
			o += b; first = false;
		}
		o += "]}";
		atomicWrite("status.json", o);
	}
	size_t countSlots(const char *name) const { size_t n = 0; for (size_t i = 0; i < m_world.slots.size(); ++i) if (m_world.slots[i].name == name) ++n; return n; }
	void persistFrameMark(unsigned f) { char b[32]; sprintf(b, "%u", f); atomicWrite("frame.txt", b); }
	// returns true if a persisted world exists; reads it into m_world / m_replay and queues the restore for the first update
	bool tryResume()
	{
		if (m_dir.empty()) return false;
		std::string wb, raw, mark, log;
		if (!umReadFile(pathOf("world.bin"), wb) || !umReadFile(pathOf("epoch.sav"), raw)) return false;
		Rd r(wb.data(), wb.size()); if (!getWorld(r, m_world)) return false; m_worldEpoch = m_world;
		unsigned ef = r.u32(); umReadFile(pathOf("frame.txt"), mark); umReadFile(pathOf("cmds.log"), log);
		m_epochFrame = ef; m_epochRaw = (unsigned)raw.size(); m_epochZ = umZip(raw);
		m_resumeTarget = mark.empty() ? ef : (unsigned)atoi(mark.c_str());
		for (size_t off = 0; off + 8 <= log.size(); )
		{
			unsigned len, fr; memcpy(&len, log.data() + off, 4); memcpy(&fr, log.data() + off + 4, 4);
			if (len < 4 || off + 4 + len > log.size()) break;     // torn tail record from a crash: ignore
			m_replay[fr] = log.substr(off + 8, len - 4); off += 4 + len;
		}
		loadRecs();
		if (!umWriteFile(umSavePath("um_epoch.sav"), raw)) return false;
		extern int UmSynthStarts; UmSynthStarts = m_world.synthStarts;
		m_resumePending = true;
		printf("[um] server: resuming persisted world from '%s': epoch frame %u, target frame %u, %d logged batches\n", m_dir.c_str(), ef, m_resumeTarget, (int)m_replay.size());
		return true;
	}

	// ---------------------------------------------------------------- world rules (server)
	// Every rule decision is made here, from the server's own copy of the simulation, and reaches the replicas only as explicit commands.
	// A "record" is a human identity (secret key) that owns a slot; it outlives the connection (absent players keep their base).
	struct Rec { std::string key; int slot; unsigned bound, left, lastCombat, nextRent; int missed; bool expiring; Rec() : slot(-1), bound(0), left(0), lastCombat(0), nextRent(0), missed(0), expiring(false) {} };
	UmWorldDesc m_worldEpoch;                    // the world description as of the last epoch (what joiners and a restart start from)
	std::map<std::string, Rec> m_recs;           // human identities by key
	std::map<int, std::string> m_slotKey;        // human slot -> key
	std::map<int, unsigned> m_aiSlots;           // active AI slots -> frame they joined
	std::vector<std::string> m_queue;            // keys waiting for a free human slot
	unsigned m_aiNextJoin; unsigned m_nextDamage;

	static unsigned envU(const char *n, unsigned d) { const char *e = getenv(n); return e ? (unsigned)atoi(e) : d; }
	bool isHumanSlot(int i) const { return i >= 0 && i < (int)m_world.slots.size() && m_world.slots[i].name == "~free"; }
	bool isAiSlot(int i) const { return i >= 0 && i < (int)m_world.slots.size() && m_world.slots[i].name == "~ai"; }
	static Player *playerForSlot(int slot) { char nm[32]; sprintf(nm, "player%d", slot); return ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(nm)); }
	static int slotOfPlayerIndex(int idx) { for (int i = 0; i < MAX_SLOTS; ++i) { Player *p = playerForSlot(i); if (p && p->getPlayerIndex() == idx) return i; } return -1; }
	int freeHumanSlot() const { for (size_t i = 0; i < m_world.slots.size(); ++i) if (isHumanSlot((int)i) && m_slotKey.find((int)i) == m_slotKey.end()) return (int)i; return -1; }
	int freeAiSlot() const { for (size_t i = 0; i < m_world.slots.size(); ++i) if (isAiSlot((int)i) && m_aiSlots.find((int)i) == m_aiSlots.end()) return (int)i; return -1; }

	void saveRecs()
	{
		std::string o; char b[256];
		for (std::map<std::string, Rec>::const_iterator it = m_recs.begin(); it != m_recs.end(); ++it)
		{ const Rec &r = it->second; sprintf(b, " %d %u %u %u %u %d %d\n", r.slot, r.bound, r.left, r.lastCombat, r.nextRent, r.missed, r.expiring ? 1 : 0); o += "P " + r.key + b; }
		for (std::map<int, unsigned>::const_iterator it = m_aiSlots.begin(); it != m_aiSlots.end(); ++it) { sprintf(b, "A %d %u\n", it->first, it->second); o += b; }
		atomicWrite("players.txt", o);
	}
	void loadRecs()
	{
		std::string t; if (!umReadFile(pathOf("players.txt"), t)) return;
		size_t p = 0;
		while (p < t.size())
		{
			size_t e = t.find('\n', p); if (e == std::string::npos) e = t.size(); std::string ln = t.substr(p, e - p); p = e + 1;
			if (ln.size() > 2 && ln[0] == 'P') { char key[200]; Rec r; int ex; if (sscanf(ln.c_str() + 2, "%199s %d %u %u %u %u %d %d", key, &r.slot, &r.bound, &r.left, &r.lastCombat, &r.nextRent, &r.missed, &ex) == 8) { r.key = key; r.expiring = ex != 0; m_recs[r.key] = r; m_slotKey[r.slot] = r.key; } }
			else if (ln.size() > 2 && ln[0] == 'A') { int sl; unsigned fr; if (sscanf(ln.c_str() + 2, "%d %u", &sl, &fr) == 2) m_aiSlots[sl] = fr; }
		}
	}
	void queueCmd(GameMessage *m) { Wr w; putMessage(w, m); m_pending += w.b; ++m_pendingCount; m->deleteInstance(); }
	GameMessage *mk(GameMessage::Type t) { GameMessage *m = newInstance(GameMessage)(t); m->friend_setPlayerIndex(0); return m; }
	void queueBind(int slot, int cash, int tmpl = -1, unsigned rgb = 0, const std::string &nick = std::string())
	{
		GameMessage *m = mk(GameMessage::MSG_UM_BIND_PLAYER); m->appendIntegerArgument(slot); m->appendIntegerArgument(cash); m->appendIntegerArgument(tmpl); m->appendIntegerArgument((int)rgb);
		for (int w = 0; w < 4; ++w) { unsigned v = 0; for (int b = 0; b < 4; ++b) { size_t k = w * 4 + b; if (k < nick.size()) v |= (unsigned)(unsigned char)nick[k] << (8 * b); } m->appendIntegerArgument((int)v); }
		queueCmd(m);
	}
	void queueUnbind(int slot) { GameMessage *m = mk(GameMessage::MSG_UM_UNBIND_PLAYER); m->appendIntegerArgument(slot); queueCmd(m); }
	void queueGrant(int slot, int amount) { GameMessage *m = mk(GameMessage::MSG_UM_GRANT_CASH); m->appendIntegerArgument(slot); m->appendIntegerArgument(amount); queueCmd(m); }
	void queueCharge(int slot, int amount) { GameMessage *m = mk(GameMessage::MSG_UM_CHARGE); m->appendIntegerArgument(slot); m->appendIntegerArgument(amount); queueCmd(m); }
	void queueRefill(int cash) { GameMessage *m = mk(GameMessage::MSG_UM_REFILL_SUPPLY); m->appendIntegerArgument(cash); queueCmd(m); }
	void queueRentDamage(int slot, int permille) { GameMessage *m = mk(GameMessage::MSG_UM_RENT_DAMAGE); m->appendIntegerArgument(slot); m->appendIntegerArgument(permille); queueCmd(m); }
	void queueSelfDestruct(int slot) { Player *p = playerForSlot(slot); if (!p) return; GameMessage *m = mk(GameMessage::MSG_SELF_DESTRUCT); m->friend_setPlayerIndex(p->getPlayerIndex()); m->appendBooleanArgument(FALSE); queueCmd(m); }
	void sendMsg(UmConn &c, unsigned type, const std::string &payload) { Wr f; frameMsg(f, type, payload); c.out += f.b; }
	UmConn *connOfKey(const std::string &key) { for (size_t i = 0; i < m_conns.size(); ++i) if (m_conns[i].live && m_conns[i].name == key) return &m_conns[i]; return NULL; }

	// A connection owns exactly one slot; its commands are re-stamped with that slot's player index and filtered.
	bool admitCommand(UmConn &c, GameMessage *m)
	{
		if (c.slot < 0 || !isNetCommand(m)) return false;
		GameMessage::Type t = m->getType();
		if (t >= GameMessage::MSG_UM_BIND_PLAYER && t <= GameMessage::MSG_UM_REFILL_SUPPLY) return false;      // server-only
		if (t == GameMessage::MSG_LOGIC_CRC || t == GameMessage::MSG_SET_REPLAY_CAMERA || t == GameMessage::MSG_SELF_DESTRUCT || t >= GameMessage::MSG_BEGIN_DEBUG_NETWORK_MESSAGES) return false;
		Player *p = playerForSlot(c.slot); if (!p) return false;
		if (++c.cmdsThisTick > 64) return false;
		m->friend_setPlayerIndex(p->getPlayerIndex());
		if (t == GameMessage::MSG_ENABLE_RETALIATION_MODE && m->getArgumentCount() >= 1 && m->getArgumentDataType(0) == ARGUMENTDATATYPE_INTEGER)
		{ /* the handler trusts argument 0 as the player: pin it to the sender */ GameMessage *r = newInstance(GameMessage)(t); r->friend_setPlayerIndex(p->getPlayerIndex()); r->appendIntegerArgument(p->getPlayerIndex()); r->appendBooleanArgument(m->getArgumentCount() > 1 ? m->getArgument(1)->boolean : FALSE); Wr w; putMessage(w, r); m_pending += w.b; ++m_pendingCount; r->deleteInstance(); return true; }
		Wr w; putMessage(w, m); m_pending += w.b; ++m_pendingCount; return true;
	}

	void freeSlotAndPromote(unsigned f)
	{
		while (!m_queue.empty() && freeHumanSlot() >= 0)
		{
			std::string key = m_queue.front(); m_queue.erase(m_queue.begin());
			UmConn *c = connOfKey(key); if (!c) continue;                 // gave up while waiting
			bindHuman(*c, f);
		}
	}
	void bindHuman(UmConn &c, unsigned f)
	{
		int slot = freeHumanSlot(); if (slot < 0) return;
		Rec r; r.key = c.name; r.slot = slot; r.bound = f; r.nextRent = f + 2 * envU("UM_RENT_FRAMES", 27000);   // the first interval is free
		m_recs[c.name] = r; m_slotKey[slot] = c.name; c.slot = slot; c.boundFrame = f; saveRecs();
		int tmpl = -1;   // the occupant's faction, if it is one the skirmish AI scripts and the starting base support
		if (!c.faction.empty()) { int idx = ThePlayerTemplateStore->getTemplateNumByName(AsciiString(c.faction.c_str())); const PlayerTemplate *pt = idx >= 0 ? ThePlayerTemplateStore->getNthPlayerTemplate(idx) : NULL; if (pt && !pt->isObserver() && (pt->getSide() == "America" || pt->getSide() == "China" || pt->getSide() == "GLA") && !pt->getStartingBuilding().isEmpty()) tmpl = idx; }
		if (slot < (int)m_world.slots.size()) { m_world.slots[slot].profT = tmpl + 1; m_world.slots[slot].rgb = c.rgb; m_world.slots[slot].nick = c.nick; }
		queueBind(slot, (int)envU("UM_START_CASH", 10000), tmpl, c.rgb, c.nick);
		Wr ww; ww.i32(slot); sendMsg(c, T_WELCOME, ww.b);
		printf("[um] server: client %d bound to slot %d (new base)\n", c.id, slot);
	}
	// Knocking out a player's last unit pays the player who dealt the last hit (within the window) a share of the victim's money plus a flat amount.
	std::map<int, int> m_lastMoney;   // slot -> cash seen while the player still had units (a defeated player's cash is cleared by the engine)
	int lastHitBonus(int slot) { std::map<int, int>::iterator mi = m_lastMoney.find(slot); long long money = mi != m_lastMoney.end() ? (long long)mi->second : 0; return (int)(money * (long long)envU("UM_LASTHIT_PCT", 10) / 100) + (int)envU("UM_LASTHIT", 10000); }
	int lastHitKiller(Player *victim, unsigned f)
	{
		if (!victim) return -1; std::map<int, std::pair<int, unsigned> >::iterator k = gLastKill.find(victim->getPlayerIndex());
		if (k != gLastKill.end() && f - k->second.second <= envU("UM_LASTHIT_WINDOW", 300 * 30)) return slotOfPlayerIndex(k->second.first);
		return -1;
	}
	// a human record is gone (eliminated or expired): free the slot, optionally pay the last-hit bonus, tell the connection
	void retire(Rec r, unsigned f, bool bonusAllowed, const char *why)
	{
		Player *p = playerForSlot(r.slot); int killerSlot = bonusAllowed ? lastHitKiller(p, f) : -1; int bonus = killerSlot >= 0 ? lastHitBonus(r.slot) : 0;
		queueUnbind(r.slot); if (killerSlot >= 0 && bonus > 0) queueGrant(killerSlot, bonus);
		printf("[um] server: slot %d %s at frame %u; last-hit bonus %d to slot %d\n", r.slot, why, f, bonus, killerSlot);
		if (UmConn *c = connOfKey(r.key)) { c->slot = -1; Wr w; w.str(why); sendMsg(*c, T_KICK, w.b); }
		m_slotKey.erase(r.slot); m_recs.erase(r.key); saveRecs();
		freeSlotAndPromote(f);
	}
	void onDisconnect(const std::string &key, unsigned f)
	{
		std::map<std::string, Rec>::iterator it = m_recs.find(key);
		if (it != m_recs.end() && it->second.left == 0) { it->second.left = f ? f : 1; saveRecs(); printf("[um] server: %s is now absent (slot %d keeps its base)\n", key.c_str(), it->second.slot); }
		for (size_t i = 0; i < m_queue.size(); ++i) if (m_queue[i] == key) { m_queue.erase(m_queue.begin() + i); break; }
	}

	// rent is the larger of a floor and a share of what the player owns, or a flat amount when UM_RENT_FIXED is set
	int rentAmount(Player *p)
	{
		if (envU("UM_RENT_FIXED", 0) > 0) return (int)envU("UM_RENT_FIXED", 0);
		long long value = 0; for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject()) if (o->getControllingPlayer() == p) value += o->getTemplate()->calcCostToBuild(p);
		int amount = (int)(value * (long long)envU("UM_RENT_PCT", 5) / 100); int floor = (int)envU("UM_RENT_FLOOR", 500);
		return amount < floor ? floor : amount;
	}
	static bool hasCommandCenter(Player *p)
	{
		for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject()) if (o->getControllingPlayer() == p && o->isKindOf(KINDOF_COMMANDCENTER) && !o->isEffectivelyDead() && !o->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION)) return true;
		return false;
	}
	void census(unsigned f)
	{
		rebuildPidSlot();
		const unsigned ABSENT = envU("UM_ABSENT_FRAMES", 54000), COMBAT = envU("UM_COMBAT_FRAMES", 900), RENT = envU("UM_RENT_FRAMES", 27000), DMGINT = envU("UM_RENT_DMG_FRAMES", 300);
		// humans: elimination, absence expiry
		std::vector<std::string> keys; for (std::map<std::string, Rec>::iterator it = m_recs.begin(); it != m_recs.end(); ++it) keys.push_back(it->first);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			std::map<std::string, Rec>::iterator it = m_recs.find(keys[k]); if (it == m_recs.end()) continue; Rec &r = it->second;
			Player *p = playerForSlot(r.slot); if (!p) continue;
			if (p->hasAnyObjects()) m_lastMoney[r.slot] = (int)p->getMoney()->countMoney();
			if (f >= r.bound + 90 && !p->hasAnyObjects()) { retire(r, f, !r.expiring, r.expiring ? "expired" : "eliminated"); continue; }
			if (r.left && !r.expiring)
			{
				unsigned deadline = r.left + ABSENT; if (r.lastCombat + COMBAT > deadline) deadline = r.lastCombat + COMBAT;
				if (f >= deadline) { r.expiring = true; queueSelfDestruct(r.slot); saveRecs(); printf("[um] server: absent base in slot %d expires at frame %u\n", r.slot, f); }
			}
			if (!r.expiring && f >= r.nextRent)   // rent: all or nothing, per interval
			{
				int amount = rentAmount(p);
				if (hasCommandCenter(p) && (int)p->getMoney()->countMoney() >= amount) { /* rent can only be paid through a command center */ queueCharge(r.slot, amount); printf("[um] server: rent %d charged to slot %d\n", amount, r.slot); }
				else { ++r.missed; printf("[um] server: slot %d MISSED rent %d (stack %d)\n", r.slot, amount, r.missed); }
				r.nextRent += RENT; saveRecs();
			}
		}
		// slow restocking of every supply dock (server setting: cash per dock and interval; 0 = never)
		{
			unsigned amount = envU("UM_SUPPLY_AMOUNT", 0), every = envU("UM_SUPPLY_FRAMES", 0);
			if (amount > 0 && every > 0) { unsigned bucket = f / every; if (m_supplyBucket == 0xffffffffu) m_supplyBucket = bucket; if (bucket != m_supplyBucket) { m_supplyBucket = bucket; queueRefill((int)amount); } }
		}
		// rent pressure: pay off one missed cycle when affordable, otherwise damage everything the player owns
		if (f >= m_nextDamage)
		{
			m_nextDamage = f + DMGINT;
			for (std::map<std::string, Rec>::iterator it = m_recs.begin(); it != m_recs.end(); ++it)
			{
				Rec &r = it->second; if (r.missed <= 0 || r.expiring) continue; Player *p = playerForSlot(r.slot); if (!p) continue;
				int amount = (int)envU("UM_RENT_FLOOR", 500); if (envU("UM_RENT_FIXED", 0) > 0) amount = (int)envU("UM_RENT_FIXED", 0);
				if (hasCommandCenter(p) && (int)p->getMoney()->countMoney() >= amount) { queueCharge(r.slot, amount); --r.missed; printf("[um] server: slot %d paid off a missed cycle (stack %d)\n", r.slot, r.missed); saveRecs(); }
				else { unsigned pm = (unsigned)r.missed * envU("UM_RENT_DMG_PERMILLE", 10), cap = envU("UM_RENT_DMG_CAP", 1000); if (pm > cap) pm = cap; queueRentDamage(r.slot, (int)pm); }   // more unpaid stacks, more damage
			}
		}
		// the AI population: join over time up to a cap, leave when wiped out
		for (std::map<int, unsigned>::iterator it = m_aiSlots.begin(); it != m_aiSlots.end(); )
		{
			Player *p = playerForSlot(it->first);
			if (p && p->hasAnyObjects()) m_lastMoney[it->first] = (int)p->getMoney()->countMoney();
			if (p && f >= it->second + 90 && !p->hasAnyObjects()) { int killerSlot = lastHitKiller(p, f), bonus = killerSlot >= 0 ? lastHitBonus(it->first) : 0; queueUnbind(it->first); if (killerSlot >= 0 && bonus > 0) queueGrant(killerSlot, bonus); printf("[um] server: AI slot %d wiped out at frame %u\n", it->first, f); m_aiSlots.erase(it++); }
			else ++it;
		}
		unsigned aiMax = envU("UM_AI_MAX", 0);
		if (aiMax && f >= m_aiNextJoin && (unsigned)m_aiSlots.size() < aiMax) { int sl = freeAiSlot(); if (sl >= 0) { queueBind(sl, (int)envU("UM_AI_CASH", 10000)); m_aiSlots[sl] = f; saveRecs(); printf("[um] server: AI joins in slot %d (%d active)\n", sl, (int)m_aiSlots.size()); } m_aiNextJoin = f + envU("UM_AI_JOIN_FRAMES", 300); }
		// test hook: UM_TEST_SELFDESTRUCT=<frames after binding> makes the server issue a self-destruct for the first bound player
		if (const char *td = getenv("UM_TEST_SELFDESTRUCT"))
			for (std::map<std::string, Rec>::iterator it = m_recs.begin(); it != m_recs.end() && !m_testDone; ++it)
				if (f >= it->second.bound + (unsigned)atoi(td)) { m_testDone = true; Player *p = playerForSlot(it->second.slot); if (p) { if (Player *ai = playerForSlot(1)) gLastKill[p->getPlayerIndex()] = std::make_pair(ai->getPlayerIndex(), f); queueSelfDestruct(it->second.slot); printf("[um] server: test self-destruct for slot %d at frame %u\n", it->second.slot, f); } }
	}
	std::map<int, int> m_pidSlot;   // player index -> slot, refreshed by the census (cheap combat attribution)
	void rebuildPidSlot() { m_pidSlot.clear(); for (int i = 0; i < MAX_SLOTS; ++i) if (Player *p = playerForSlot(i)) m_pidSlot[p->getPlayerIndex()] = i; }
public:
	void noteCombat(int pa, int pb)
	{
		unsigned f = TheGameLogic ? TheGameLogic->getFrame() : 0; int ids[2] = { pa, pb };
		for (int k = 0; k < 2; ++k) { std::map<int, int>::iterator it = m_pidSlot.find(ids[k]); if (it == m_pidSlot.end()) continue; std::map<int, std::string>::iterator ks = m_slotKey.find(it->second); if (ks != m_slotKey.end()) { std::map<std::string, Rec>::iterator rr = m_recs.find(ks->second); if (rr != m_recs.end()) rr->second.lastCombat = f; } }
	}
private:
	bool m_testDone; int m_dropLog;

	// ---------------------------------------------------------------- canonical epoch (server)
	// Restoring a save is deterministic but not identical to the uninterrupted run, so the world must be re-based: the server saves,
	// every participant (server included) loads that same save at the same frame, and the command log restarts there.
	void serverEpoch(unsigned f)
	{
		DWORD t0 = GetTickCount();
		SaveCode rc = TheGameState->saveGame(AsciiString("um_epoch.sav"), UnicodeString(L"um epoch"), SAVE_FILE_TYPE_NORMAL);
		std::string raw; if (rc != SC_OK || !umReadFile(umSavePath("um_epoch.sav"), raw)) { printf("[um] server: epoch save failed (%d)\n", (int)rc); return; }
		std::string z = umZip(raw); if (z.empty()) return;
		m_epochFrame = f; m_epochRaw = (unsigned)raw.size(); m_epochZ = z; m_worldEpoch = m_world;
		persistEpoch(f, raw);
		for (std::map<unsigned, std::string>::iterator it = m_log.begin(); it != m_log.end(); ) { if (it->first < f) m_log.erase(it++); else ++it; }
		Wr w; w.u32(f); w.u32(m_epochRaw); w.b += z; Wr fr; frameMsg(fr, T_EPOCH, w.b);
		for (size_t i = 0; i < m_conns.size(); ++i) if (m_conns[i].live) m_conns[i].out += fr.b;
		flushAll();
		if (!umLoadSnapshotFile("um_epoch.sav")) { printf("[um] server: epoch reload FAILED\n"); return; }
		printf("[um] server: epoch at frame %u: snapshot %u bytes (%u compressed), save+reload %lu ms\n", f, m_epochRaw, (unsigned)z.size(), (unsigned long)(GetTickCount() - t0));
	}

	// ---------------------------------------------------------------- server
	void serverUpdate()
	{
		if (m_listen == INVALID_SOCKET) { m_ready = TRUE; return; }
		// accept
		for (;;)
		{
			SOCKET s = accept(m_listen, NULL, NULL); if (s == INVALID_SOCKET) break;
			unsigned long nb = 1; ioctlsocket(s, FIONBIO, &nb); int nd = 1; setsockopt(s, IPPROTO_TCP, 1 /*TCP_NODELAY*/, (const char *)&nd, sizeof(nd));
			UmConn c; c.s = s; c.id = m_nextConnId++; m_conns.push_back(c); printf("[um] server: client %d connected (%d total)\n", c.id, (int)m_conns.size());
		}
		// read
		for (size_t i = 0; i < m_conns.size(); )
		{
			UmConn &c = m_conns[i];
			bool alive = c.s != INVALID_SOCKET && pump(c.s, c.in);
			unsigned type; std::string payload;
			while (alive && nextFrame(c.in, type, payload)) alive = serverHandle(c, type, payload);
			if (!alive) { if (c.s != INVALID_SOCKET) closesocket(c.s); printf("[um] server: client %d left\n", c.id); if (c.slot >= 0) onDisconnect(c.name, TheGameLogic ? TheGameLogic->getFrame() : 0); else onDisconnect(c.name, 0); m_conns.erase(m_conns.begin() + i); }
			else ++i;
		}
		if (m_resumePending && (!TheGameLogic || !TheGameLogic->isInGame()))
		{
			m_resumePending = false;
			if (umLoadSnapshotFile("um_epoch.sav")) { m_lastFrame = TheGameLogic->getFrame(); printf("[um] server: world restored at frame %u\n", TheGameLogic->getFrame()); }
			else printf("[um] server: RESTORE FAILED\n");
			m_ready = FALSE; return;
		}
		if (!TheGameLogic || !TheGameLogic->isInGame()) { m_ready = TRUE; return; }   // let the engine process MSG_NEW_GAME
		// pace (a restarted server re-simulates up to where it died as fast as it can)
		DWORD now = timeGetTime();
		bool catching = TheGameLogic->getFrame() < m_resumeTarget;
		if (!m_uncapped && !catching && now < m_nextTick) return;
		m_nextTick = (now > m_nextTick + 200 || m_nextTick == 0) ? now + 33 : m_nextTick + 33;
		unsigned f = TheGameLogic->getFrame();
		if (!m_replay.empty())
		{
			std::map<unsigned, std::string>::iterator rp = m_replay.find(f);
			if (rp != m_replay.end()) { Rd rr(rp->second.data(), rp->second.size()); rr.u32(); m_pendingCount = rr.u32(); m_pending.assign(rr.p, rr.e - rr.p); m_replay.erase(rp); }   // logged batch re-enters exactly as before
		}
		if (m_pendingCount)
		{
			if (getenv("UM_TRACE_BATCH")) printf("[um] server: batch of %u command(s) for frame %u\n", m_pendingCount, f);
			Wr body; body.u32(f); body.u32(m_pendingCount); body.b += m_pending;
			m_log[f] = body.b; persistBatch(f, body.b);
			Wr fr; frameMsg(fr, T_FRAME, body.b);
			for (size_t i = 0; i < m_conns.size(); ++i) if (m_conns[i].live) m_conns[i].out += fr.b;
			Rd r(m_pending.data(), m_pending.size());
			for (unsigned k = 0; k < m_pendingCount; ++k) if (GameMessage *m = getMessage(r)) TheCommandList->appendMessage(m);
			m_pending.clear(); m_pendingCount = 0;
		}
		m_ready = TRUE;
	}

	bool serverHandle(UmConn &c, unsigned type, const std::string &payload)
	{
		Rd r(payload.data(), payload.size());
		if (type == T_HELLO)
		{
			if (r.u32() != UM_MAGIC || r.u32() != UM_VERSION) { printf("[um] server: client %d bad hello\n", c.id); return false; }
			c.name = r.str(); c.nick = r.str(); c.faction = r.str(); c.rgb = r.u32() & 0xffffff; c.hello = true;
			{ std::string clean; for (size_t k = 0; k < c.nick.size() && clean.size() < 16; ++k) { unsigned char ch = (unsigned char)c.nick[k]; if (ch >= 32 && ch < 127) clean += (char)ch; } c.nick = clean; }
			Wr w; putWorld(w, m_worldEpoch); w.u8(m_epochFrame ? 1 : 0); Wr f; frameMsg(f, T_WORLD, w.b); c.out += f.b;
			if (m_epochFrame) { Wr e; e.u32(m_epochFrame); e.u32(m_epochRaw); e.b += m_epochZ; Wr fe; frameMsg(fe, T_EPOCH, e.b); c.out += fe.b; }
			for (std::map<unsigned, std::string>::const_iterator it = m_log.begin(); it != m_log.end(); ++it) { Wr fr; frameMsg(fr, T_FRAME, it->second); c.out += fr.b; }
			Wr a; a.u32(TheGameLogic ? TheGameLogic->getFrame() : 0); Wr fa; frameMsg(fa, T_ADVANCE, a.b); c.out += fa.b;
			c.live = true; printf("[um] server: client %d '%s' admitted at frame %u (replaying %d command batches)\n", c.id, c.name.c_str(), TheGameLogic ? TheGameLogic->getFrame() : 0, (int)m_log.size());
			// identity: the hello name is the player's secret key; the same key gets the same slot (and base) back
			if (c.name.size() >= 2 && c.name[0] != '~')
			{
				unsigned f = TheGameLogic ? TheGameLogic->getFrame() : 0;
				std::map<std::string, Rec>::iterator ks = m_recs.find(c.name);
				if (ks != m_recs.end())
				{
					for (size_t j = 0; j < m_conns.size(); ++j) if (&m_conns[j] != &c && m_conns[j].live && m_conns[j].name == c.name && m_conns[j].slot >= 0) { m_conns[j].slot = -1; Wr kw; kw.str("replaced by a new connection"); sendMsg(m_conns[j], T_KICK, kw.b); }
					c.slot = ks->second.slot; c.boundFrame = ks->second.bound; ks->second.left = 0; saveRecs();
					printf("[um] server: client %d bound to slot %d (returning player)\n", c.id, c.slot);
				}
				else if (freeHumanSlot() >= 0) bindHuman(c, f);
				else { m_queue.push_back(c.name); printf("[um] server: client %d queued (position %d, world full)\n", c.id, (int)m_queue.size()); }
			}
			{ Wr ww; ww.i32(c.slot); sendMsg(c, T_WELCOME, ww.b); }
			flush(c);
		}
		else if (type == T_CMD && c.hello)
		{
			GameMessage *m = getMessage(r);
			if (m && getenv("UM_TRACE_CMD")) printf("[um] server: command type %d from client %d slot %d\n", (int)m->getType(), c.id, c.slot);
			if (m) { if (!admitCommand(c, m)) { if (m_dropLog < 20) { ++m_dropLog; printf("[um] server: refused command type %d from client %d\n", (int)m->getType(), c.id); } } m->deleteInstance(); }
		}
		else if (type == T_CRC && c.hello)
		{
			unsigned f = r.u32(), crc = r.u32();
			std::map<unsigned, UnsignedInt>::const_iterator it = m_crcHist.find(f);
			if (it != m_crcHist.end())
			{
				++m_crcChecked; bool ok = it->second == crc;
				if (!ok) { ++m_mismatches; printf("[um] server: CRC MISMATCH client %d frame %u: server %08x client %08x\n", c.id, f, (unsigned)it->second, crc); }
				Wr w; w.u32(f); w.u8(ok ? 1 : 0); Wr fr; frameMsg(fr, T_CRCRESULT, w.b); c.out += fr.b;
			}
		}
		return true;
	}

	// ---------------------------------------------------------------- client
	void clientUpdate()
	{
		if (m_sock != INVALID_SOCKET)
		{
			bool alive = pump(m_sock, m_recv); unsigned type; std::string payload;
			while (nextFrame(m_recv, type, payload)) clientHandle(type, payload);
			if (!alive) { printf("[um] client: server closed the connection\n"); closesocket(m_sock); m_sock = INVALID_SOCKET; fflush(stdout); ExitProcess(0); }
		}
		// snapshots: a joiner resumes from the first one; a live client re-bases when it reaches the epoch frame
		if (!m_epochInbox.empty())
		{
			bool inGame = TheGameLogic && TheGameLogic->isInGame(); unsigned cur = inGame ? TheGameLogic->getFrame() : 0;
			std::map<unsigned, std::pair<unsigned, std::string> >::iterator it = m_epochInbox.begin();
			if ((!inGame && m_needSnapshot) || (inGame && it->first == cur))
			{
				std::string raw;
				if (umUnzip(it->second.second, it->second.first, raw) && umWriteFile(umSavePath("um_epoch_in.sav"), raw) && umLoadSnapshotFile("um_epoch_in.sav"))
				{
					printf("[um] client: loaded epoch snapshot for frame %u (now at frame %u)\n", it->first, TheGameLogic->getFrame()); m_needSnapshot = false;
				}
				else printf("[um] client: epoch snapshot for frame %u could not be loaded\n", it->first);
				m_epochInbox.erase(it); m_ready = FALSE; return;
			}
			else if (inGame && it->first < cur) m_epochInbox.erase(it);   // stale
		}
		if (m_needSnapshot) return;   // waiting for the snapshot message
		if (!TheGameLogic || !TheGameLogic->isInGame()) { m_ready = TRUE; return; }
		unsigned f = TheGameLogic->getFrame();
		// catch-up runs unthrottled; near the live edge fall back to the normal limiter
		TheWritableGlobalData->m_useFpsLimit = (m_limit - f) < 60 && !m_uncapped;
		if (f < m_limit)
		{
			std::map<unsigned, std::string>::iterator it = m_inbox.find(f);
			if (it != m_inbox.end())
			{
				Rd r(it->second.data(), it->second.size()); unsigned n = r.u32();
				if (getenv("UM_TRACE_BATCH")) printf("[um] client: executing %u command(s) at frame %u\n", n, f);
				for (unsigned k = 0; k < n; ++k) if (GameMessage *m = getMessage(r)) TheCommandList->appendMessage(m);
				m_inbox.erase(it);
			}
			m_ready = TRUE;
		}
	}
	void clientHandle(unsigned type, const std::string &payload)
	{
		Rd r(payload.data(), payload.size());
		if (type == T_WORLD && !m_gotWorld)
		{
			if (getWorld(r, m_world))
			{
				m_gotWorld = true; bool snap = r.u8() != 0; extern int UmSynthStarts; UmSynthStarts = m_world.synthStarts;
				printf("[um] client: world '%s', %d slots, seed %d%s\n", m_world.map.c_str(), (int)m_world.slots.size(), m_world.seed, snap ? " (resuming from a snapshot)" : "");
				if (snap) m_needSnapshot = true; else UmStartWorld(m_world);
			}
		}
		else if (type == T_EPOCH) { unsigned f = r.u32(), raw = r.u32(); m_epochInbox[f] = std::make_pair(raw, payload.substr(8)); }
		else if (type == T_WELCOME) { m_mySlot = r.i32(); printf("[um] client: welcome, slot %d\n", m_mySlot); }
		else if (type == T_KICK) { std::string why = r.str(); printf("[um] client: KICKED (%s)\n", why.c_str()); m_mySlot = -1; gUmKickReason = why; gUmKickTick = GetTickCount() | 1; if (!gUmHasUi && !getenv("UM_STAY_AFTER_KICK")) { fflush(stdout); ExitProcess(0); } }
		else if (type == T_FRAME) { unsigned f = r.u32(); m_inbox[f] = payload.substr(4); }   // keep count+messages
		else if (type == T_ADVANCE) { unsigned l = r.u32(); if (l > m_limit) m_limit = l; }
		else if (type == T_CRCRESULT) { r.u32(); if (r.u8()) ++m_crcChecked; else { ++m_mismatches; printf("[um] client: server says our CRC DIVERGED\n"); } }
	}
};

void UmNetStartServer(int port, const UmWorldDesc &world)
{
	TheUmNet = new UmNet(UM_ROLE_SERVER); TheNetwork = TheUmNet; TheUmNet->startServer(port, world);
}
void UmNetStartClient(const char *host, int port)
{
	TheUmNet = new UmNet(UM_ROLE_CLIENT); TheNetwork = TheUmNet; TheUmNet->startClient(host, port);
}
void UmNetPostUpdate() { if (TheUmNet) TheUmNet->postUpdate(); }
int UmNetMySlot() { return TheUmNet ? TheUmNet->mySlot() : -1; }

void UmNoteCombatImpl(Int a, Int b) { if (TheUmNet) TheUmNet->noteCombat(a, b); }
