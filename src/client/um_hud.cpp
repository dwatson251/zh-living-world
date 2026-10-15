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
#include "Common/NameKeyGenerator.h"
#include "GameLogic/GameLogic.h"
#include "net/um_net.h"
#include <stdio.h>
#include <string>
#include <wchar.h>

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
		gLine[i]->setFont(TheFontLibrary->getFont(AsciiString("Arial"), TheGlobalLanguageData->adjustFontSize(18), TRUE));
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

void UmDrawRentHud(void)
{
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
	else swprintf(t, 96, L"RENT  $%d  %ls", ri.amount, c);
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
