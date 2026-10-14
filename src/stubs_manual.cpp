// Hand-written stand-ins for things the dedicated-server build does not carry:
// GameSpy / lobby UI, W3D rendering hooks, and process-level globals normally defined in Main/WinMain.cpp.
#include "PreRTS.h"
#include <string>
#include "Common/INI.h"
#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
#include "GameClient/GameWindow.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Image.h"
#include "GameClient/Shadow.h"
#include "GameNetwork/GameSpy/PeerDefs.h"
#include "GameNetwork/GameSpy/BuddyThread.h"
#include "GameNetwork/GameSpy/PeerThread.h"
#include "GameNetwork/GameSpy/PersistentStorageThread.h"
#include "GameNetwork/GameSpy/GameResultsThread.h"
#include "GameNetwork/GameSpy/LadderDefs.h"
#include "GameNetwork/GameSpy/StagingRoomGameInfo.h"
#include "GameNetwork/GameSpy/PingThread.h"
#include "GameNetwork/GameSpy/GSConfig.h"
#include "GameNetwork/GameSpyOverlay.h"
#include "GameNetwork/GameSpy/MainMenuUtils.h"
#include "GameNetwork/GameSpy/LobbyUtils.h"
#include "GameNetwork/GameSpy/PersistentStorageDefs.h"
#include "GameNetwork/GameSpy/BuddyDefs.h"
#include "GameNetwork/GameSpy/ThreadUtils.h"
#include "Common/OSDisplay.h"
#include "Common/GameLOD.h"

// ---- explicit instantiation: VC6/STLport shipped std::basic_string<unsigned short> (wchar_t == unsigned short); libc++ does not.
template class std::basic_string<unsigned short>;

// ---- process globals normally owned by WinMain / the W3D device
#ifndef UM_CLIENT
HWND ApplicationHWnd = NULL;
bool DX8Wrapper_IsWindowed = true;
Int DX8Wrapper_PreserveFPU = 0;

#endif
Bool DontShowMainMenu = FALSE;
#ifndef UM_CLIENT
const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";

#endif
int isThreadHosting = 0;
#ifndef UM_CLIENT
ProjectedShadowManager *TheProjectedShadowManager = NULL;

#endif

// ---- GameSpy globals (never created in the dedicated build)
GameSpyInfoInterface *TheGameSpyInfo = NULL;
GameSpyConfigInterface *TheGameSpyConfig = NULL;
GameSpyBuddyMessageQueueInterface *TheGameSpyBuddyMessageQueue = NULL;
GameSpyPeerMessageQueueInterface *TheGameSpyPeerMessageQueue = NULL;
GameSpyPSMessageQueueInterface *TheGameSpyPSMessageQueue = NULL;
GameResultsInterface *TheGameResultsQueue = NULL;
GameSpyStagingRoom *TheGameSpyGame = NULL;
LadderList *TheLadderList = NULL;
PingerInterface *ThePinger = NULL;
Color GameSpyColor[GSCOLOR_MAX];

// ---- GameSpy UI / flow entry points
void CancelPatchCheckCallback(void) {}
void GSMessageBoxOk(UnicodeString, UnicodeString, GameWinMsgBoxFunc) {}
void GameSpyCloseAllOverlays(void) {}
void GameSpyCloseOverlay(GSOverlayType) {}
Bool GameSpyIsOverlayOpen(GSOverlayType) { return FALSE; }
void GameSpyOpenOverlay(GSOverlayType) {}
void GameSpyToggleOverlay(GSOverlayType) {}
void GameSpyUpdateOverlays(void) {}
void HTTPThinkWrapper(void) {}
void HandleQMLadderSelection(Int) {}
void InitBuddyControls(Int) {}
const Image* LookupSmallRankImage(Int, Int) { return NULL; }
std::wstring MultiByteToWideCharSingleLine(const char *orig) { std::wstring w; for (; orig && *orig; ++orig) w.push_back((WideChar)(unsigned char)*orig); return w; }
std::string WideCharStringToMultiByte(const WideChar *orig) { std::string s; for (; orig && *orig; ++orig) s.push_back((char)*orig); return s; }
void PopulateOldBuddyMessages(void) {}
void PopulateQMLadderComboBox(void) {}
void PopulateQMLadderListBox(GameWindow *) {}
void RaiseGSMessageBox(void) {}
void ReOpenPlayerInfo(void) {}
void RefreshGameListBoxes(void) {}
void SetLobbyAttemptHostJoin(Bool) {}
void StartDownloadingPatches(void) {}
void StartPatchCheck(void) {}
void StopAsyncDNSCheck(void) {}
void TearDownGameSpy(void) {}
void UpdateLocalPlayerStats(void) {}
void updateBuddyInfo(void) {}
void gameAcceptTooltip(GameWindow *, WinInstanceData *, UnsignedInt) {}
void playerTemplateComboBoxTooltip(GameWindow *, WinInstanceData *, UnsignedInt) {}
void playerTemplateListBoxTooltip(GameWindow *, WinInstanceData *, UnsignedInt) {}
#ifndef UM_CLIENT
void ReloadAllTextures(void) {}
void doSkyBoxSet(Bool) {}
void oversizeTheTerrain(Int) {}

#endif
void INI::parseOnlineChatColorDefinition(INI *ini) { INI::umSkipBlock(ini); }  // lobby chat colours: parse and discard

#include "null/NullGameResultsInterface.h"
class UmGameResults : public NullGameResultsInterface { public: virtual void init(void) {} virtual void reset(void) {} virtual void update(void) {} };
GameResultsInterface* GameResultsInterface::createNewGameResultsInterface(void) { return NEW UmGameResults; }
std::string GameSpyPSMessageQueueInterface::formatPlayerKVPairs(PSPlayerStats) { return std::string(); }
AsciiString GameSpyStagingRoom::generateGameSpyGameResultsPacket(void) { return AsciiString::TheEmptyString; }
AsciiString GameSpyStagingRoom::generateLadderGameResultsPacket(void) { return AsciiString::TheEmptyString; }
GameSpyGameSlot *GameSpyStagingRoom::getGameSpySlot(Int) { return NULL; }
const LadderInfo* LadderList::findLadder(const AsciiString&, UnsignedShort) { return NULL; }
const LadderInfo* LadderList::findLadderByIndex(Int) { return NULL; }
const LadderInfoList* LadderList::getLocalLadders(void) { return NULL; }
const LadderInfoList* LadderList::getSpecialLadders(void) { return NULL; }
const LadderInfoList* LadderList::getStandardLadders(void) { return NULL; }

// ---- misc
class WWMouseClass; WWMouseClass *MouseCursor = NULL;       // referenced by WWLib keyboard.cpp
#ifndef UM_CLIENT
class W3DShadowManager; W3DShadowManager *TheW3DShadowManager = NULL;

#endif
extern "C" int getQR2HostingStatus(void) { return 0; }
#ifndef UM_CLIENT
OSDisplayButtonType OSDisplayWarningBox(AsciiString p, AsciiString m, UnsignedInt, UnsignedInt)
{ fprintf(stderr, "[warning box] %s / %s\n", p.str(), m.str()); return OSDBT_OK; }

#endif
PSPlayerStats::PSPlayerStats(void) { reset(); }
PSPlayerStats::PSPlayerStats(const PSPlayerStats& o) { *this = o; }
PSRequest::PSRequest() { player.reset(); requestType = PSREQUEST_READPLAYERSTATS; addDiscon = addDesync = FALSE; lastHouse = -1; }
void Register_Thread_ID(unsigned long, char *, bool) {}      // WWLib/Except.cpp is MSVC-only crash-handler code
void Unregister_Thread_ID(unsigned long, char *) {}
void PSPlayerStats::reset(void) { id = 0; locale = 0; gamesAsRandom = 0; lastFPS = 0; lastGeneral = 0; gamesInRowWithLastGeneral = 0; challengeMedals = 0; battleHonors = 0; QMwinsInARow = maxQMwinsInARow = 0; winsInARow = maxWinsInARow = lossesInARow = maxLossesInARow = disconsInARow = maxDisconsInARow = desyncsInARow = maxDesyncsInARow = 0; builtParticleCannon = builtNuke = builtSCUD = 0; lastLadderPort = 0; }
#ifndef UM_CLIENT
Bool testMinimumRequirements(ChipsetType *videoChipType, CpuType *cpuType, Int *cpuFreq, Int *numRAM, Real *intBenchIndex, Real *floatBenchIndex, Real *memBenchIndex)
{ // dedicated server: report a fast machine so GameLOD never down-rates simulation-relevant settings
	if (cpuType) *cpuType = P4; if (cpuFreq) *cpuFreq = 3000; if (numRAM) *numRAM = 1024*1024*1024;
	if (intBenchIndex) *intBenchIndex = 10.0f; if (floatBenchIndex) *floatBenchIndex = 10.0f; if (memBenchIndex) *memBenchIndex = 10.0f;
	return TRUE;
}

#endif

// W3DGhostObject.cpp (kept for its logic-side ghost bookkeeping) references the W3D scene/asset manager statics.
#include "W3DDevice/GameClient/W3DDisplay.h"
#ifndef UM_CLIENT
RTS3DScene *W3DDisplay::m_3DScene = NULL;
W3DAssetManager *W3DDisplay::m_assetManager = NULL;

#endif
