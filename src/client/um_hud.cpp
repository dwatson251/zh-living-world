// Living World HUD: the rent countdown and spawn protection, drawn on the left of the screen like a special-power cooldown (information only, nothing to click).
// The server sends the numbers (T_RENT, see um_net.cpp); the countdowns are computed from the local logic frame so they tick smoothly.
#include "PreRTS.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/ControlBar.h"
#include "GameClient/InGameUI.h"
#include "Common/NameKeyGenerator.h"
#include "GameLogic/GameLogic.h"
#include "net/um_net.h"
#include "net/um_economy.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include <stdio.h>
#include <string>
#include <wchar.h>
#include <windows.h>
#include "GameClient/GameClient.h"
#include "Common/GameEngine.h"

namespace
{
DisplayString *gLine[3] = { NULL, NULL, NULL };
unsigned gLastPaid = 0, gPaidFlashUntil = 0, gProtMax = 0;
bool gPaidSeen = false;

DisplayString *line(int i)
{
	if (!gLine[i])
	{
		gLine[i] = TheDisplayStringManager->newDisplayString();
		// exactly the font of the game's on-screen messages ("Joining the world: catching up ...")
		gLine[i]->setFont(TheFontLibrary->getFont(TheInGameUI->umMessageFontName(), TheGlobalLanguageData->adjustFontSize(TheInGameUI->umMessagePointSize()), TheInGameUI->umMessageBold()));
	}
	return gLine[i];
}
void clock(wchar_t *out, size_t n, unsigned secs) { swprintf(out, n, L"%u:%02u", secs / 60, secs % 60); }
// a thin bar that fills from the left as the cooldown runs out
void bar(Int x, Int y, Int w, Int h, Real filled, Color fill, Color back)
{
	if (filled < 0) filled = 0; if (filled > 1) filled = 1;
	TheDisplay->drawFillRect(x, y, w, h, back);
	TheDisplay->drawFillRect(x, y, (Int)(w * filled), h, fill);
}
}

// ---- spectating: the permanent top-left note, and the "Defeated" splash a knocked-out player sees for a few seconds ----
extern std::string gUmKickReason; extern DWORD gUmKickTick; extern bool gUmKickFatal;
int gUmTopReserve = 0;   // rows of the top-left message stack taken by the spectator note (InGameUI::postDraw starts its messages below it)
namespace
{
DisplayString *gSpecLine = NULL, *gSplashBig = NULL, *gSplashSmall = NULL;
void drawSpectator(void)
{
	gUmTopReserve = 0;
	if (!TheDisplay || !TheDisplayStringManager || !TheInGameUI || UmNetMySlot() >= 0) return;
	unsigned cf, cl; if (UmNetCatchUp(&cf, &cl)) return;
	const Color shadow = GameMakeColor(0, 0, 0, 255);
	if (!gSpecLine)
	{
		gSpecLine = TheDisplayStringManager->newDisplayString();
		gSpecLine->setFont(TheFontLibrary->getFont(TheInGameUI->umMessageFontName(), TheGlobalLanguageData->adjustFontSize(TheInGameUI->umMessagePointSize()), TheInGameUI->umMessageBold()));
	}
	int pos = 0, total = 0; UmNetQueueInfo(&pos, &total);
	wchar_t t[96];
	if (pos > 0) swprintf(t, 96, L"Spectator view, in the queue (%d of %d). Open Menu (Esc) to continue", pos, total);
	else swprintf(t, 96, L"Spectator view, Open Menu (Esc) to Join");
	gSpecLine->setText(UnicodeString(t));
	Int tw = 0, th = 0; gSpecLine->getSize(&tw, &th);
	gSpecLine->draw(10, 10, GameMakeColor(255, 235, 120, 255), shadow);
	gUmTopReserve = th + 4;

	if (gUmKickTick && !gUmKickFatal)
	{
		const DWORD age = GetTickCount() - gUmKickTick;
		if (age < 4500)
		{
			const Int W = TheDisplay->getWidth(), H = TheDisplay->getHeight();
			Real fade = age < 3800 ? 1.0f : 1.0f - (Real)(age - 3800) / 700.0f;
			const int a = (int)(255 * fade);
			if (!gSplashBig)
			{
				gSplashBig = TheDisplayStringManager->newDisplayString();
				gSplashBig->setFont(TheFontLibrary->getFont(TheInGameUI->umMessageFontName(), TheGlobalLanguageData->adjustFontSize(54), TRUE));
				gSplashSmall = TheDisplayStringManager->newDisplayString();
				gSplashSmall->setFont(TheFontLibrary->getFont(TheInGameUI->umMessageFontName(), TheGlobalLanguageData->adjustFontSize(TheInGameUI->umMessagePointSize()), FALSE));
			}
			gSplashBig->setText(UnicodeString(L"DEFEATED"));
			UnicodeString why; { const std::string &r = gUmKickReason; const char *msg = r == "expired" ? "Your base expired while you were away" : "Your base has fallen"; for (const char *c = msg; *c; ++c) why.concat((WideChar)(unsigned char)*c); }
			gSplashSmall->setText(why);
			Int bw, bh, sw, sh; gSplashBig->getSize(&bw, &bh); gSplashSmall->getSize(&sw, &sh);
			const Int bandH = bh + sh + 40, y0 = H / 2 - bandH / 2;
			TheDisplay->drawFillRect(0, y0, W, bandH, GameMakeColor(0, 0, 0, (int)(170 * fade)));
			gSplashBig->draw((W - bw) / 2, y0 + 12, GameMakeColor(255, 70, 50, a), GameMakeColor(0, 0, 0, a));
			gSplashSmall->draw((W - sw) / 2, y0 + 20 + bh, GameMakeColor(230, 230, 230, a), GameMakeColor(0, 0, 0, a));
		}
	}
}
}

void UmDrawRentHud(void)
{
	drawSpectator();
	if (getenv("UM_TEST_BAR") && TheControlBar && TheGameLogic)   // test hook: cycle the control bar stage (default, low, hidden) every 8 s
	{
		static unsigned lastStep = 0; unsigned stepNo = TheGameLogic->getFrame() / (8 * 30);
		if (stepNo != lastStep)
		{
			lastStep = stepNo; TheControlBar->switchControlBarStage((ControlBarStages)(stepNo % 3));
			const char *names[] = { "ControlBarParent", "LeftHUD", "CommandWindow", "BackgroundMarker", "MoneyDisplay", "RightHUD" };
			for (int i = 0; i < 6; ++i)
			{
				GameWindow *w = TheWindowManager->winGetWindowFromId(NULL, TheNameKeyGenerator->nameToKey(AsciiString((std::string("ControlBar.wnd:") + names[i]).c_str())));
				Int sx = -1, sy = -1, wd = 0, ht = 0; if (w) { w->winGetScreenPosition(&sx, &sy); w->winGetSize(&wd, &ht); }
				printf("[um] hud: stage %u %s %s at (%d,%d) size %dx%d\n", stepNo % 3, names[i], !w ? "missing" : (w->winIsHidden() ? "hidden" : "shown"), sx, sy, wd, ht);
			}
			fflush(stdout);
		}
	}
	UmRentInfo ri;
	unsigned cf = 0, cl = 0;
	if (!TheDisplay || !TheGameLogic || !TheDisplayStringManager || UmNetMySlot() < 0 || UmNetCatchUp(&cf, &cl) || !UmNetRentInfo(&ri) || ri.expiring) return;

	const unsigned speed = UmNetSpeed() ? UmNetSpeed() : 30;
	const unsigned now = TheGameLogic->getFrame();
	const Int W = TheDisplay->getWidth(), H = TheDisplay->getHeight();
	// size the panel from the widest text it can show; the text height drives the row height
	Int w0 = 0, w1 = 0, w2 = 0, hh = 0;
	line(0)->setText(UnicodeString(L"PROTECTED  00:00")); line(0)->getSize(&w0, &hh);
	line(1)->setText(UnicodeString(L"RENT (grace)  $00000  00:00")); line(1)->getSize(&w1, &hh);
	line(2)->setText(UnicodeString(L"UNPAID RENT  x00")); line(2)->getSize(&w2, &hh);
	const Int pad = 8, textW = (w0 > w1 ? (w0 > w2 ? w0 : w2) : (w1 > w2 ? w1 : w2));
	const Int panelW = textW + 2 * pad, x = (Int)(W * 0.010f) + pad + 4;

	// bottom left, just above the control bar; it drops to the screen edge when the bar is hidden or lowered
	Int barTop = H;
	if (TheWindowManager)
	{
		static const NameKeyType barKey = TheNameKeyGenerator->nameToKey(AsciiString("ControlBar.wnd:ControlBarParent"));
		GameWindow *bar = TheWindowManager->winGetWindowFromId(NULL, barKey);
		if (bar && !bar->winIsHidden()) { Int sx = 0, sy = 0; bar->winGetScreenPosition(&sx, &sy); if (sy > 0 && sy < H) barTop = sy; }
	}
	const Int margin = 10;
	if (!gPaidSeen) { gPaidSeen = true; gLastPaid = ri.paid; }
	if (ri.paid != gLastPaid) { gLastPaid = ri.paid; gPaidFlashUntil = now + 2 * speed; }
	const bool paidFlash = now < gPaidFlashUntil;

	const bool protectedNow = ri.immuneUntil > now;
	const unsigned dueFrames = ri.nextRent > now ? ri.nextRent - now : 0;
	const bool grace = now < ri.graceEnd && ri.missed == 0;
	const Int rowH = hh > 0 ? hh : 20, step = rowH + 9;   // text height, then a 4 px bar and some air
	Int rows = 1 + (protectedNow ? 1 : 0) + (ri.missed > 0 ? 1 : 0);
	Int panelH = rows * step + 2;
	Int y = barTop - margin - panelH - 4;
	if (y < 0) y = 0;

	const Color back = GameMakeColor(0, 0, 0, 150);
	const Color border = paidFlash ? GameMakeColor(80, 255, 120, 255) : (ri.missed > 0 ? GameMakeColor(255, 70, 50, 255) : GameMakeColor(200, 200, 200, 200));
	TheDisplay->drawFillRect(x - pad, y - 4, panelW, panelH + 6, back);
	TheDisplay->drawOpenRect(x - pad, y - 4, panelW, panelH + 6, 2.0f, border);

	wchar_t t[96], c[24];
	const Color shadow = GameMakeColor(0, 0, 0, 255);

	if (protectedNow)
	{
		unsigned left = ri.immuneUntil - now;
		if (left > gProtMax) gProtMax = left;
		clock(c, 24, left / speed);
		swprintf(t, 96, L"PROTECTED  %ls", c);
		line(0)->setText(UnicodeString(t));
		line(0)->draw(x, y, GameMakeColor(255, 150, 60, 255), shadow);
		bar(x, y + rowH + 2, textW, 4, gProtMax ? (Real)left / (Real)gProtMax : 0.0f, GameMakeColor(255, 90, 40, 255), GameMakeColor(70, 30, 20, 200));
		y += step;
	}
	else gProtMax = 0;

	// the rent: the bar fills as the due time approaches, like a power recharging in reverse
	clock(c, 24, (dueFrames + speed - 1) / speed);
	if (grace) swprintf(t, 96, L"RENT (grace)  %ls", c);
	else { UnicodeString sym; UmEconSymbol(ThePlayerList ? ThePlayerList->getLocalPlayer() : NULL, &sym); swprintf(t, 96, L"RENT  %ls%d  %ls", sym.str(), ri.amount, c); }
	line(1)->setText(UnicodeString(t));
	const Color rentColor = grace ? GameMakeColor(120, 230, 140, 255) : (dueFrames < 20u * speed ? GameMakeColor(255, 220, 80, 255) : GameMakeColor(255, 255, 255, 255));
	line(1)->draw(x, y, rentColor, shadow);
	Real done = ri.interval ? 1.0f - (Real)dueFrames / (Real)ri.interval : 0.0f;
	bar(x, y + rowH + 2, textW, 4, done, rentColor, GameMakeColor(40, 40, 40, 200));
	y += step;

	if (ri.missed > 0)
	{
		const bool blink = ((now / (speed / 2 ? speed / 2 : 1)) & 1) != 0;
		swprintf(t, 96, L"UNPAID RENT  x%d", ri.missed);
		line(2)->setText(UnicodeString(t));
		line(2)->draw(x, y, blink ? GameMakeColor(255, 60, 40, 255) : GameMakeColor(255, 140, 120, 255), shadow);
	}
}

// ---- the loading overlay: joining a big city world takes minutes on an older PC; say what the client is doing, and log it ----
// The retail main menu stays up until the world is ready (client_engine.cpp hides it), so the menu itself marks how far a start got.
namespace
{
char gStageText[200] = ""; bool gLoadOn = false; DWORD gLoadT0 = 0, gLastDraw = 0; int gLastLogPct = -10;
DisplayString *gLoadLine[2] = { NULL, NULL };
DisplayString *loadLine(int i)
{
	if (!gLoadLine[i])
	{
		gLoadLine[i] = TheDisplayStringManager->newDisplayString();
		gLoadLine[i]->setFont(TheFontLibrary->getFont(TheInGameUI->umMessageFontName(), TheGlobalLanguageData->adjustFontSize(TheInGameUI->umMessagePointSize() + 4), TheInGameUI->umMessageBold()));
	}
	return gLoadLine[i];
}
void loadRedraw(bool force)
{
	DWORD n = GetTickCount(); if (!force && n - gLastDraw < 400) return;
	gLastDraw = n;
	if (!TheDisplay || !TheGameEngine || !TheInGameUI || !TheDisplayStringManager || !TheFontLibrary || !TheGlobalLanguageData) return;
	TheGameEngine->serviceWindowsOS();   // keeps Windows from calling the window "not responding" while it loads
	TheDisplay->draw();
}
}
void UmLoadStage(const char *text)
{
	extern bool gUmServerMode; if (gUmServerMode || getenv("UM_SERVER")) return;
	if (!gLoadOn) { gLoadOn = true; gLoadT0 = GetTickCount(); gLastLogPct = -10; }
	strncpy(gStageText, text, sizeof gStageText - 1); gStageText[sizeof gStageText - 1] = 0;
	{ void UmMemLine(char *, int); char mem[600]; UmMemLine(mem, sizeof mem); printf("[um] load: %s (%lu s) | %s\n", gStageText, (unsigned long)((GetTickCount() - gLoadT0) / 1000), mem); fflush(stdout); }
	loadRedraw(true);
}
void UmLoadTick(const char *what, int pct)
{
	if (!gLoadOn) return;
	if (pct >= 0) _snprintf(gStageText, sizeof gStageText - 1, "%s: %d%%", what, pct); else strncpy(gStageText, what, sizeof gStageText - 1);
	gStageText[sizeof gStageText - 1] = 0;
	if (pct < 0 || pct >= gLastLogPct + 10 || pct < gLastLogPct) { gLastLogPct = pct; printf("[um] load: %s (%lu s)\n", gStageText, (unsigned long)((GetTickCount() - gLoadT0) / 1000)); fflush(stdout); }
	loadRedraw(false);
}
void UmLoadDone(void)
{
	if (!gLoadOn) return;
	gLoadOn = false; printf("[um] load: world ready after %lu s\n", (unsigned long)((GetTickCount() - gLoadT0) / 1000)); fflush(stdout);
}
void UmDrawLoadStatus(void)
{
	if (!gLoadOn || !TheDisplay || !TheDisplayStringManager || !TheInGameUI || !TheFontLibrary || !TheGlobalLanguageData) return;
	unsigned secs = (GetTickCount() - gLoadT0) / 1000;
	wchar_t t0[300], t1[120]; swprintf(t0, 300, L"LIVING WORLD   %hs   %u:%02u", gStageText, secs / 60, secs % 60);
	swprintf(t1, 120, L"Big cities can take minutes on older PCs. Keep this window open.");
	loadLine(0)->setText(UnicodeString(t0)); loadLine(1)->setText(UnicodeString(t1));
	Int w0 = 0, h0 = 0, w1 = 0, h1 = 0; loadLine(0)->getSize(&w0, &h0); loadLine(1)->getSize(&w1, &h1);
	const Int W = TheDisplay->getWidth(), H = TheDisplay->getHeight(), pad = 14, pw = (w0 > w1 ? w0 : w1) + 2 * pad, ph = h0 + h1 + 3 * pad;
	const Int x = (W - pw) / 2, y = H / 2 - ph / 2;
	TheDisplay->drawFillRect(0, 0, W, H, GameMakeColor(0, 0, 0, 170));   // dim the retail main menu behind (its buttons do nothing in this mod while the world loads)
	TheDisplay->drawFillRect(x, y, pw, ph, GameMakeColor(0, 0, 0, 200));
	TheDisplay->drawOpenRect(x, y, pw, ph, 2.0f, GameMakeColor(255, 190, 60, 255));
	const Color shadow = GameMakeColor(0, 0, 0, 255);
	loadLine(0)->draw(x + pad, y + pad, GameMakeColor(255, 220, 120, 255), shadow);
	loadLine(1)->draw(x + pad, y + 2 * pad + h0, GameMakeColor(220, 220, 220, 255), shadow);
}
