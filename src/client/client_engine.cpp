// Living World graphical client: the retail Win32/W3D engine, started straight into a connection to a Living World server.
// Player choices arrive from the launcher through environment variables (UM_CLIENT=host:port, UM_NAME, UM_FACTION, UM_COLOR, UM_KEY).
#include "PreRTS.h"
#include "Win32Device/Common/Win32GameEngine.h"
#include "GameClient/GameClient.h"
#include "GameClient/Shell.h"
#include "Common/MultiplayerSettings.h"
#include "GameNetwork/GameInfo.h"
#include "Common/GlobalData.h"
#include "net/um_net.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/MessageStream.h"
#include "Common/BuildAssistant.h"
#include "Common/PlayerList.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ControlBar.h"
#include "GameClient/View.h"
#include "GameClient/Mouse.h"
#include "GameClient/GameClient.h"
#include "null/NullAudioManager.h"
#include "null/NullVideoPlayerInterface.h"
#include <string>

bool gUmServerMode = false;

class UmClientAudio : public NullAudioManager { public: virtual Bool isMusicAlreadyLoaded(void) const { return TRUE; } };
AudioManager *UmCreateAudio() { return NEW UmClientAudio; }
VideoPlayerInterface *UmCreateVideo() { return NEW NullVideoPlayerInterface; }

// The seat the server gave this client decides which player the UI belongs to: a seat holder controls its base, anyone else watches as the observer.
// Test hook (UM_TEST_BUILD=1): order the dozer to build a barracks, through the same messages the mouse UI would send.
static void umTestBuild()
{
	static unsigned startedAt = 0; static bool done = false;
	if (done || !getenv("UM_TEST_BUILD") || !ThePlayerList || !ThePlayerList->getLocalPlayer() || UmNetMySlot() < 0) return;
	Player *me = ThePlayerList->getLocalPlayer(); Object *dozer = NULL, *cc = NULL;
	for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject()) if (o->getControllingPlayer() == me) { if (o->isKindOf(KINDOF_DOZER)) dozer = o; if (o->isKindOf(KINDOF_COMMANDCENTER)) cc = o; }
	if (!dozer || !cc) return;
	if (!startedAt) startedAt = TheGameLogic->getFrame();
	if (TheGameLogic->getFrame() < startedAt + 90) return;
	const ThingTemplate *want = NULL;
	for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate()) if (t->isKindOf(KINDOF_STRUCTURE) && t->isKindOf(KINDOF_FS_BARRACKS) && me->canBuild(t) && !t->isKindOf(KINDOF_COMMANDCENTER)) { want = t; break; }
	if (!want) { printf("[um] client: test build found no barracks template\n"); done = true; return; }
	done = true;
	GameMessage *m = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP); m->appendBooleanArgument(TRUE); m->appendObjectIDArgument(dozer->getID());
	m = TheMessageStream->appendMessage(GameMessage::MSG_DOZER_CONSTRUCT);
	Coord3D pos = *cc->getPosition(); bool found = false;
	for (Real r = 140; r <= 520 && !found; r += 30)   // a legal spot for it, nearest first (what the placement ghost would show green)
		for (int k = 0; k < 12 && !found; ++k)
		{
			Coord3D c = *cc->getPosition(); c.x += r * cosf(k * 0.5236f); c.y += r * sinf(k * 0.5236f); c.z = TheTerrainLogic->getGroundHeight(c.x, c.y);
			if (TheBuildAssistant->isLocationLegalToBuild(&c, want, 0.0f, BuildAssistant::CLEAR_PATH | BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP | BuildAssistant::SHROUD_REVEALED, dozer, me) == LBC_OK) { pos = c; found = true; }
		}
	if (!found) { printf("[um] client: test build found no legal place\n"); return; }
	m->appendIntegerArgument(want->getTemplateID()); m->appendLocationArgument(pos); m->appendRealArgument(0.0f);
	printf("[um] client: test build of %s ordered at (%.0f,%.0f)\n", want->getName().str(), pos.x, pos.y);
}

void UmClientHeartbeat(void);
extern std::string gUmKickReason; extern DWORD gUmKickTick; extern bool gUmHasUi;
static void umClientMessages()
{
	static DWORD lastMsg = 0; DWORD now = GetTickCount();
	if (!TheInGameUI || !TheGameLogic || !TheGameLogic->isInGame()) return;
	if (gUmKickTick)
	{
		if (now - lastMsg > 1500) { lastMsg = now; UnicodeString m; std::string why = gUmKickReason; for (size_t i = 0; i < why.size(); ++i) m.concat((WideChar)(unsigned char)why[i]); TheInGameUI->message(UnicodeString(L"You are out of the world (%ls). The game closes in a few seconds; start it again to rejoin."), m.str()); }
		if (now - gUmKickTick > 8000) { fflush(stdout); ExitProcess(0); }
		return;
	}
	if (UmNetMySlot() < 0 && now - lastMsg > 6000) { lastMsg = now; TheInGameUI->message(UnicodeString(L"Watching the world: every seat is taken. You join as soon as one frees up.")); }
}
static void umClientTick()
{
	gUmHasUi = true;
	umClientMessages();
	UmClientHeartbeat();
	{	// exclusive fullscreen minimises a window that was not active when the display mode changed; bring it back (borderless/windowed never need this)
		extern HWND ApplicationHWnd; static unsigned lastTry = 0; unsigned now = GetTickCount();
		if (ApplicationHWnd && !gUmServerMode && TheGlobalData && !TheGlobalData->m_windowed && IsIconic(ApplicationHWnd) && now - lastTry > 1000)
		{ lastTry = now; ShowWindow(ApplicationHWnd, SW_RESTORE); SetForegroundWindow(ApplicationHWnd); }
	}
	static int applied = -2;
	if (gUmServerMode || !TheGameLogic || !TheGameLogic->isInGame() || !ThePlayerList || !TheTacticalView) return;
	if (TheMouse && !TheMouse->getVisibility()) { TheMouse->setVisibility(TRUE); TheMouse->setCursor(Mouse::ARROW); printf("[um] client: cursor made visible\n"); }   // the retail code only does this when a load screen shows
	int slot = UmNetMySlot();
	static int centeredFor = -2;
	if (slot == applied && slot >= 0 && centeredFor != slot)
	{	// the base arrives with the server's bind command, a few frames after the seat: look at it as soon as it exists
		Player *me = ThePlayerList->getLocalPlayer(); Object *first = NULL;
		for (Object *o = TheGameLogic->getFirstObject(); o && me; o = o->getNextObject()) if (o->getControllingPlayer() == me) { if (!first) first = o; if (o->isKindOf(KINDOF_COMMANDCENTER)) { first = o; break; } }
		if (first) { Coord3D pos = *first->getPosition(); TheTacticalView->lookAt(&pos); centeredFor = slot; }
	}
	if (slot == applied) return;
	Player *p = NULL;
	if (slot >= 0) { char nm[32]; sprintf(nm, "player%d", slot); p = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey(nm)); }
	else p = ThePlayerList->findPlayerWithNameKey(TheNameKeyGenerator->nameToKey("ReplayObserver"));
	if (!p) return;
	applied = slot;
	ThePlayerList->setLocalPlayer(p);
	TheInGameUI->deselectAllDrawables();
	ThePartitionManager->refreshShroudForLocalPlayer();
	TheControlBar->initSpecialPowershortcutBar(p);
	TheControlBar->setControlBarSchemeByPlayer(p);
	TheGameClient->updateFakeDrawables();
	if (slot >= 0 && TheGameInfo && TheGameInfo->getSlot(slot))
	{
		AsciiString way; way.format("Player_%d_Start", TheGameInfo->getSlot(slot)->getStartPos() + 1);
		if (Waypoint *w = TheTerrainLogic->getWaypointByName(way)) { Coord3D pos = *w->getLocation(); TheTacticalView->lookAt(&pos); printf("[um] client: camera to %s at (%.0f,%.0f)\n", way.str(), pos.x, pos.y); }
		else printf("[um] client: no waypoint %s\n", way.str());
		for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject()) if (o->getControllingPlayer() == p) printf("[um] client: own %s at (%.0f,%.0f)\n", o->getTemplate()->getName().str(), o->getPosition()->x, o->getPosition()->y);
	}
	printf("[um] client: now playing as %s\n", slot >= 0 ? "a seat holder" : "an observer");
}

class UmClientEngine : public Win32GameEngine
{
public:
	virtual void init(void) { Win32GameEngine::init(); }
	virtual void init(int argc, char *argv[])
	{
		fprintf(stderr, "[um] client engine: init begins\n");
		GameEngine::init(argc, argv);
		fprintf(stderr, "[um] client engine: base init done, UM_CLIENT=%s\n", getenv("UM_CLIENT") ? getenv("UM_CLIENT") : "(unset)");
		TheMultiplayerSettings->extendColors(MAX_SLOTS + 1);   // every process must agree on the palette
		if (const char *srv = getenv("UM_SERVER")) { gUmServerMode = getenv("UM_STANDALONE_WINDOWED") == NULL; startServer(atoi(srv)); return; }
		const char *target = getenv("UM_CLIENT");
		if (!target) return;
		std::string hp(target); size_t colon = hp.find(':');
		UmNetStartClient(colon == std::string::npos ? hp.c_str() : hp.substr(0, colon).c_str(), colon == std::string::npos ? 7777 : atoi(hp.c_str() + colon + 1));
	}
	// Dedicated server (UM_SERVER=<port>): the same engine as the client, window hidden and a null Direct3D, running the world on its own.
	void startServer(int port)
	{
		const char *map = getenv("UM_SKIRMISH_MAP"); const char *ai = getenv("UM_SKIRMISH_AI");
		if (!map) return;
		Int n = ai ? atoi(ai) : 0; if (n < 0) n = 0; if (n > MAX_SLOTS - 1) n = MAX_SLOTS - 1;
		UmWorldDesc w; w.map = map; w.seed = getenv("UM_SEED") ? atoi(getenv("UM_SEED")) : 12345;
		w.synthStarts = getenv("UM_SYNTH_STARTS") ? atoi(getenv("UM_SYNTH_STARTS")) : 0;
		UmSlotDesc obs = { SLOT_PLAYER, PLAYERTEMPLATE_OBSERVER, -1, -1, -1, "Server" }; w.slots.push_back(obs);
		for (Int i = 1; i <= n; ++i) { UmSlotDesc s = { SLOT_MED_AI, PLAYERTEMPLATE_RANDOM, i, i - 1, -1, "" }; w.slots.push_back(s); }
		{ int h = getenv("UM_HUMAN_SLOTS") ? atoi(getenv("UM_HUMAN_SLOTS")) : 0; for (int k = 0; k < h && (int)w.slots.size() < MAX_SLOTS; ++k) { UmSlotDesc s = { SLOT_PLAYER, PLAYERTEMPLATE_RANDOM, (int)w.slots.size(), (int)w.slots.size() - 1, -1, "~free" }; w.slots.push_back(s); } }
		{ int m = getenv("UM_AI_POOL") ? atoi(getenv("UM_AI_POOL")) : 0; for (int k = 0; k < m && (int)w.slots.size() < MAX_SLOTS; ++k) { UmSlotDesc s = { SLOT_MED_AI, PLAYERTEMPLATE_RANDOM, (int)w.slots.size(), (int)w.slots.size() - 1, -1, "~ai" }; w.slots.push_back(s); } }
		printf("[um] server scenario: map '%s', %d AI players, %d slots\n", map, n, (int)w.slots.size());
		if (port > 0) UmNetStartServer(port, w); else UmStartWorld(w);   // port 0: a standalone world with no network (comparison runs)
	}
	virtual void update(void)
	{
		Win32GameEngine::update();
		UmNetPostUpdate();
		umClientTick();
		umTestBuild();
		if (const char *df = getenv("UM_DEEPCRC_FRAME"))   // standalone deep dump of the whole logic state, for comparing two processes
		{
			static bool dumped = false;
			if (!dumped && !TheUmNet && TheGameLogic && TheGameLogic->isInGame() && TheGameLogic->getFrame() == (UnsignedInt)atoi(df))
			{
				dumped = true; char nm[96]; sprintf(nm, "deepcrc_%s_%u.bin", getenv("UM_DEEPCRC_TAG") ? getenv("UM_DEEPCRC_TAG") : "x", (unsigned)atoi(df));
				UnsignedInt dc = TheGameLogic->getCRC(CRC_RECALC, AsciiString(nm));
				printf("[um] frame %u deepcrc=%08x\n", (unsigned)TheGameLogic->getFrame(), (unsigned)dc);
			}
		}
		if (getenv("UM_QUIT_AFTER") && TheGameLogic && TheGameLogic->getFrame() > (UnsignedInt)atoi(getenv("UM_QUIT_AFTER"))) { fflush(stdout); ExitProcess(0); }
	}
};

Win32GameEngine *UmCreateClientEngine(void) { return NEW UmClientEngine; }
