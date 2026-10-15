// Headless GameEngine: real logic, real data loading, no window/display/audio/video/input.
#include "PreRTS.h"
#include <psapi.h>
#include "Common/GameEngine.h"
#include "net/um_net.h"
#include "Common/GameState.h"
#include "Common/MultiplayerSettings.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "Common/RandomValue.h"
#include "GameNetwork/GameInfo.h"
#include "Common/GameLOD.h"
#include "Common/FunctionLexicon.h"
#include "Common/ThingFactory.h"
#include "GameClient/GameClient.h"
#include "GameClient/Snow.h"
#include "GameClient/Drawable.h"
#include "GameNetwork/NetworkInterface.h"
#include "GameNetwork/GameSpy/PeerDefs.h"
#include "GameNetwork/WOLBrowser/WebBrowser.h"
#include "W3DDevice/GameLogic/W3DGameLogic.h"
#include "W3DDevice/Common/W3DThingFactory.h"
#include "Win32Device/Common/Win32BIGFileSystem.h"
#include "Win32Device/Common/Win32LocalFileSystem.h"
#include "um/um_terrain.h"
#include "Common/MapReaderWriterInfo.h"
#include "null/NullGameEngineBase.h"
#include "null/NullGameClientBase.h"
#include "null/NullAudioManager.h"
#include "null/NullRadar.h"
#include "null/NullParticleSystemManager.h"
#include "null/NullDisplay.h"
#include "null/NullInGameUI.h"
#include "null/NullView.h"
#include "null/NullKeyboard.h"
#include "null/NullMouse.h"
#include "null/NullGameWindowManager.h"
#include "null/NullGameWindow.h"
#include "null/NullFontLibrary.h"
#include "null/NullDisplayStringManager.h"
#include "null/NullDisplayString.h"
#include "null/NullVideoPlayerInterface.h"
#include "null/NullTerrainVisual.h"

extern ModuleFactory *UmCreateModuleFactory();

// Terrain: loads the map's height data for the logic side (no rendering).
class UmTerrainVisual : public NullTerrainVisual
{
public:
	UmTerrainVisual() : m_logicHeightMap(NULL) {}
	virtual ~UmTerrainVisual() { REF_PTR_RELEASE(m_logicHeightMap); delete TheTerrainRenderObject; TheTerrainRenderObject = NULL; }
	virtual Bool load(AsciiString filename)
	{
		if (!TerrainVisual::load(filename)) return FALSE;
		CachedFileInputStream fileStrm;
		if (!fileStrm.open(filename)) return FALSE;
		ChunkInputStream *pStrm = &fileStrm;
		REF_PTR_RELEASE(m_logicHeightMap);
		m_logicHeightMap = NEW WorldHeightMap(pStrm);
		if (!TheTerrainRenderObject) TheTerrainRenderObject = new UmTerrain;
		TheTerrainRenderObject->setMap(m_logicHeightMap);
		return TRUE;
	}
	virtual void reset(void) { TerrainVisual::reset(); REF_PTR_RELEASE(m_logicHeightMap); if (TheTerrainRenderObject) TheTerrainRenderObject->setMap(NULL); }
	virtual WorldHeightMap *getLogicHeightMap(void) { return m_logicHeightMap; }
	virtual WorldHeightMap *getClientHeightMap(void) { return m_logicHeightMap; }
	virtual Int getRawMapHeight(const ICoord2D *g) { if (!m_logicHeightMap) return 0; return m_logicHeightMap->getHeight(g->x + m_logicHeightMap->getBorderSizeInline(), g->y + m_logicHeightMap->getBorderSizeInline()); }
	virtual void setRawMapHeight(const ICoord2D *g, Int h)
	{
		if (!m_logicHeightMap) return;
		Int x = g->x + m_logicHeightMap->getBorderSizeInline(), y = g->y + m_logicHeightMap->getBorderSizeInline();
		if (m_logicHeightMap->getHeight(x, y) > h) m_logicHeightMap->setRawHeight(x, y, h);
	}
private:
	WorldHeightMap *m_logicHeightMap;
};

// UI windows exist as plain objects (so shell/control-bar init does not dereference NULL) but never draw.
class UmGameWindow : public NullGameWindow
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(UmGameWindow, "W3DGameWindow")
public:
	UmGameWindow() {}
};
UmGameWindow::~UmGameWindow() {}
class UmWindowManager : public NullGameWindowManager
{
public:
	virtual GameWindow *allocateNewWindow() { return newInstance(UmGameWindow); }
};

class UmFontLibrary : public NullFontLibrary
{
public:
	virtual Bool loadFontData(GameFont *font) { font->height = font->pointSize; font->fontData = (void *)1; return TRUE; }
};

class UmDisplayString : public NullDisplayString
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(UmDisplayString, "W3DDisplayString")
public:
	UmDisplayString() {}
};
UmDisplayString::~UmDisplayString() {}
class UmDisplayStringManager : public NullDisplayStringManager
{
public:
	virtual DisplayString *newDisplayString(void) { DisplayString *s = newInstance(UmDisplayString); link(s); return s; }
	virtual void freeDisplayString(DisplayString *s) { if (!s) return; unLink(s); s->deleteInstance(); }
};

class UmInGameUI : public NullInGameUI
{
public:
	virtual View *createView(void) { return NEW NullView; }
};

void UmDrawRentHud(void) {}   // the headless build draws nothing
class UmAudio : public NullAudioManager { public: virtual Bool isMusicAlreadyLoaded(void) const { return TRUE; } };  // no CD prompt

class UmSnow : public SnowManager { public: virtual void update(void) {} };

class UmGameClient : public NullGameClientBase
{
public:
	virtual Drawable *friend_createDrawable(const ThingTemplate *t, DrawableStatus s) { return t ? newInstance(Drawable)(t, s) : NULL; }
protected:
	virtual Display *createGameDisplay() { return NEW NullDisplay; }
	virtual InGameUI *createInGameUI() { return NEW UmInGameUI; }
	virtual GameWindowManager *createWindowManager() { return NEW UmWindowManager; }
	virtual FontLibrary *createFontLibrary() { return NEW UmFontLibrary; }
	virtual DisplayStringManager *createDisplayStringManager() { return NEW UmDisplayStringManager; }
	virtual VideoPlayerInterface *createVideoPlayer() { return NEW NullVideoPlayerInterface; }
	virtual TerrainVisual *createTerrainVisual() { return NEW UmTerrainVisual; }
	virtual Keyboard *createKeyboard() { return NEW NullKeyboard; }
	virtual Mouse *createMouse() { return NEW NullMouse; }
	virtual SnowManager *createSnowManager() { return NEW UmSnow; }
};

class UmGameEngine : public NullGameEngineBase
{
public:
	// Stress scenario: UM_SKIRMISH_MAP="Maps\\Tournament Continent\\Tournament Continent.map" UM_SKIRMISH_AI=N starts a skirmish with one
	// observer (slot 0) and N AI players, bypassing the shell UI. UM_SYNTH_STARTS (see TerrainLogic.cpp) supplies start waypoints beyond the map's own.
	virtual void init(int argc, char *argv[])
	{
		GameEngine::init(argc, argv);
		{ extern int UmSynthStarts; if (getenv("UM_LOADSAVE") && getenv("UM_SYNTH_STARTS")) UmSynthStarts = atoi(getenv("UM_SYNTH_STARTS")); }   // world settings normally arrive with the snapshot
		TheMultiplayerSettings->extendColors(MAX_SLOTS + 1);   // every process must agree on the palette (also when a world is restored from a save)
		const char *client = getenv("UM_CLIENT");
		if (client)
		{
			std::string hp(client); size_t colon = hp.find(':');
			UmNetStartClient(colon == std::string::npos ? hp.c_str() : hp.substr(0, colon).c_str(), colon == std::string::npos ? 7777 : atoi(hp.c_str() + colon + 1));
			return;
		}
		const char *map = getenv("UM_SKIRMISH_MAP"); const char *ai = getenv("UM_SKIRMISH_AI");
		if (!map || !ai) return;
		Int n = atoi(ai); if (n < 1) n = 1; if (n > MAX_SLOTS - 1) n = MAX_SLOTS - 1;
		UmWorldDesc w; w.map = map; w.seed = getenv("UM_SEED") ? atoi(getenv("UM_SEED")) : 12345;
		w.synthStarts = getenv("UM_SYNTH_STARTS") ? atoi(getenv("UM_SYNTH_STARTS")) : 0;
		UmSlotDesc obs = { SLOT_PLAYER, PLAYERTEMPLATE_OBSERVER, -1, -1, -1, "Server" }; w.slots.push_back(obs);
		for (Int i = 1; i <= n; ++i) { UmSlotDesc s = { SLOT_MED_AI, PLAYERTEMPLATE_RANDOM, i, i - 1, -1, "" }; w.slots.push_back(s); }
		{ int h = getenv("UM_HUMAN_SLOTS") ? atoi(getenv("UM_HUMAN_SLOTS")) : 0; for (int k = 0; k < h && (int)w.slots.size() < MAX_SLOTS; ++k) { UmSlotDesc s = { SLOT_PLAYER, PLAYERTEMPLATE_RANDOM, (int)w.slots.size(), (int)w.slots.size() - 1, -1, "~free" }; w.slots.push_back(s); } }
		{ int m = getenv("UM_AI_POOL") ? atoi(getenv("UM_AI_POOL")) : 0; for (int k = 0; k < m && (int)w.slots.size() < MAX_SLOTS; ++k) { UmSlotDesc s = { SLOT_MED_AI, PLAYERTEMPLATE_RANDOM, (int)w.slots.size(), (int)w.slots.size() - 1, -1, "~ai" }; w.slots.push_back(s); } }
		printf("[um] scenario: map '%s', %d AI players, %d slots available\n", map, n, MAX_SLOTS);
		if (const char *srv = getenv("UM_SERVER")) UmNetStartServer(atoi(srv), w); else UmStartWorld(w);
	}

	// Per-frame sync log: the logic frame, an object count and the whole-world CRC, so runs can be diffed.
	virtual void update(void)
	{
		static bool loadTried = false;
		if (!loadTried && getenv("UM_LOADSAVE")) {   // snapshot-restore experiment: resume a world from a save written by UM_SAVE_AT
			loadTried = true;
			AvailableGameInfo info; info.filename = getenv("UM_LOADSAVE"); info.next = info.prev = NULL; info.saveGameInfo.saveFileType = SAVE_FILE_TYPE_NORMAL;
			SaveCode rc = TheGameState->loadGame(info); printf("[um] loadGame('%s') -> %d\n", getenv("UM_LOADSAVE"), (int)rc);
		}
		GameEngine::update();
		if (const char *at = getenv("UM_SAVE_AT")) {
			static bool saved = false;
			if (!saved && TheGameLogic && TheGameLogic->isInGame() && TheGameLogic->getFrame() >= (UnsignedInt)atoi(at)) {
				saved = true; DWORD t = GetTickCount();
				SaveCode rc = TheGameState->saveGame(AsciiString("um_snap.sav"), UnicodeString(L"um snapshot"), SAVE_FILE_TYPE_NORMAL);
				printf("[um] saveGame at frame %u -> %d (%lu ms)\n", TheGameLogic->getFrame(), (int)rc, (unsigned long)(GetTickCount() - t));
				if (getenv("UM_RELOAD")) {   // canonical-epoch experiment: the saver also restores its own snapshot
					AvailableGameInfo info; info.filename = "um_snap.sav"; info.next = info.prev = NULL; info.saveGameInfo.saveFileType = SAVE_FILE_TYPE_NORMAL;
					DWORD t2 = GetTickCount(); SaveCode rc2 = TheGameState->loadGame(info); printf("[um] self-reload -> %d (%lu ms), frame now %u\n", (int)rc2, (unsigned long)(GetTickCount() - t2), TheGameLogic->getFrame());
				}
			}
		}
		UmNetPostUpdate();
		if (const char *df = getenv("UM_DEEPCRC_FRAME")) {   // standalone deep dump for snapshot experiments
			static bool dumped = false;
			if (!dumped && !TheUmNet && TheGameLogic && TheGameLogic->isInGame() && TheGameLogic->getFrame() == (UnsignedInt)atoi(df)) {
				dumped = true; char nm[64]; sprintf(nm, "deepcrc_%s_%u.bin", getenv("UM_DEEPCRC_TAG") ? getenv("UM_DEEPCRC_TAG") : "x", (unsigned)atoi(df)); UnsignedInt dc = TheGameLogic->getCRC(CRC_RECALC, AsciiString(nm)); UnsignedInt lc = TheGameLogic->getCRC(CRC_RECALC); UnsignedInt lc2 = TheGameLogic->getCRC(CRC_RECALC);
				printf("[um] frame %u deepcrc=%08x light=%08x light-again=%08x\n", (unsigned)TheGameLogic->getFrame(), (unsigned)dc, (unsigned)lc, (unsigned)lc2);
			}
		}
		static UnsignedInt lastLogged = 0xffffffff; static DWORD t0 = GetTickCount(); static int n = 0;
		if (TheGameLogic && TheGameLogic->isInGame())
		{
			UnsignedInt f = TheGameLogic->getFrame();
			static unsigned every = getenv("UM_CRC_EVERY") ? (unsigned)atoi(getenv("UM_CRC_EVERY")) : 30; static unsigned from = getenv("UM_CRC_FROM") ? (unsigned)atoi(getenv("UM_CRC_FROM")) : 0;
			if (f != lastLogged && f >= from && (f % every) == 0)
			{
				lastLogged = f;
				int objs = 0; for (Object *o = TheGameLogic->getFirstObject(); o; o = o->getNextObject()) ++objs;
				PROCESS_MEMORY_COUNTERS pmc; pmc.WorkingSetSize = 0; GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
				printf("[um] frame %u objects %d crc %08x t=%lums mem=%luMB\n", f, objs, (unsigned)TheGameLogic->getCRC(CRC_RECALC), (unsigned long)(GetTickCount() - t0), (unsigned long)(pmc.WorkingSetSize >> 20));
				if (++n >= 40 && getenv("UM_FRAMES_MAX")) setQuitting(TRUE);
			}
		}
	}
protected:
	virtual LocalFileSystem *createLocalFileSystem() { return NEW Win32LocalFileSystem; }
	virtual ArchiveFileSystem *createArchiveFileSystem() { return NEW Win32BIGFileSystem; }
	virtual GameLogic *createGameLogic() { return NEW W3DGameLogic; }
	virtual GameClient *createGameClient() { return NEW UmGameClient; }
	virtual ModuleFactory *createModuleFactory() { return UmCreateModuleFactory(); }
	virtual ThingFactory *createThingFactory() { return NEW W3DThingFactory; }
	virtual FunctionLexicon *createFunctionLexicon() { return NEW FunctionLexicon; }
	virtual Radar *createRadar() { return NEW NullRadar; }
	virtual WebBrowser *createWebBrowser() { return NEW WebBrowser; }
	virtual ParticleSystemManager *createParticleSystemManager() { return NEW NullParticleSystemManager; }
	virtual AudioManager *createAudioManager() { return NEW UmAudio; }
	virtual NetworkInterface *createNetwork() { return NetworkInterface::createNetwork(); }
};

GameEngine *CreateGameEngine(void) { return NEW UmGameEngine; }
