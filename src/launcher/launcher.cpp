// Living World launcher: pick a name, faction and colour, enter the server address and press Start.
// It starts zh_client.exe (next to this program) inside the player's own Zero Hour folder and hands the choices over in environment variables.
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

enum { ID_NAME = 100, ID_FACTION, ID_COLOR, ID_SERVER, ID_GAMEDIR, ID_BROWSE, ID_WINDOWED, ID_START, ID_STATUS, ID_RES };

struct Faction { const char *label; const char *templ; };
static const Faction kFactions[] = {
	{ "USA", "FactionAmerica" }, { "USA - Air Force General", "FactionAmericaAirForceGeneral" }, { "USA - Laser General", "FactionAmericaLaserGeneral" },
	{ "USA - Superweapon General", "FactionAmericaSuperWeaponGeneral" },
	{ "China", "FactionChina" }, { "China - Tank General", "FactionChinaTankGeneral" }, { "China - Infantry General", "FactionChinaInfantryGeneral" },
	{ "China - Nuke General", "FactionChinaNukeGeneral" },
	{ "GLA", "FactionGLA" }, { "GLA - Toxin General", "FactionGLAToxinGeneral" }, { "GLA - Stealth General", "FactionGLAStealthGeneral" },
	{ "GLA - Demolition General", "FactionGLADemolitionGeneral" },
};
static const char *kResolutions[] = { "1024x768", "1280x720", "1600x900", "1920x1080" };

static HWND gName, gFaction, gColor, gServer, gGameDir, gMode, gStatus, gRes;
static const char *kModes[] = { "Windowed", "Borderless fullscreen", "Exclusive fullscreen" };
static COLORREF gColorValue = RGB(220, 40, 40);
static COLORREF gCustom[16];
static char gIni[MAX_PATH];

static std::string getText(HWND h) { char b[512]; GetWindowTextA(h, b, sizeof b); return b; }
static std::string iniGet(const char *k, const char *d) { char b[512]; GetPrivateProfileStringA("client", k, d, b, sizeof b, gIni); return b; }
static void iniSet(const char *k, const std::string &v) { WritePrivateProfileStringA("client", k, v.c_str(), gIni); }

static std::string detectGameDir()
{
	const char *keys[] = { "SOFTWARE\\Wow6432Node\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour", "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour" };
	for (int i = 0; i < 2; ++i)
	{
		HKEY k; char b[512]; DWORD n = sizeof b, t;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keys[i], 0, KEY_READ, &k) == ERROR_SUCCESS)
		{
			if (RegQueryValueExA(k, "InstallPath", NULL, &t, (LPBYTE)b, &n) == ERROR_SUCCESS) { RegCloseKey(k); return b; }
			RegCloseKey(k);
		}
	}
	const char *guess[] = { "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Generals - Zero Hour", "D:\\SteamLibrary\\steamapps\\common\\Command & Conquer Generals - Zero Hour", "E:\\SteamLibrary\\steamapps\\common\\Command & Conquer Generals - Zero Hour" };
	for (int i = 0; i < 3; ++i) { std::string p = std::string(guess[i]) + "\\INIZH.big"; if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) return guess[i]; }
	return "";
}

static std::string identityKey()
{
	std::string k = iniGet("key", "");
	if (k.size() >= 16) return k;
	unsigned char r[16]; HCRYPTPROV p; k.clear();
	if (CryptAcquireContextA(&p, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) { CryptGenRandom(p, sizeof r, r); CryptReleaseContext(p, 0); }
	else for (int i = 0; i < 16; ++i) r[i] = (unsigned char)(rand() ^ GetTickCount());
	char h[40]; for (int i = 0; i < 16; ++i) sprintf(h + i * 2, "%02x", r[i]);
	k = h; iniSet("key", k); return k;
}

static void setStatus(const char *s) { SetWindowTextA(gStatus, s); }

static void chooseColor(HWND owner)
{
	CHOOSECOLORA cc; memset(&cc, 0, sizeof cc); cc.lStructSize = sizeof cc; cc.hwndOwner = owner; cc.rgbResult = gColorValue; cc.lpCustColors = gCustom; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
	if (ChooseColorA(&cc)) { gColorValue = cc.rgbResult; InvalidateRect(gColor, NULL, TRUE); }
}

static void browse(HWND owner)
{
	BROWSEINFOA bi; memset(&bi, 0, sizeof bi); bi.hwndOwner = owner; bi.lpszTitle = "Select your Command & Conquer Generals - Zero Hour folder"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	LPITEMIDLIST id = SHBrowseForFolderA(&bi);
	if (id) { char p[MAX_PATH]; if (SHGetPathFromIDListA(id, p)) SetWindowTextA(gGameDir, p); CoTaskMemFree(id); }
}

static void start(HWND owner)
{
	std::string name = getText(gName), server = getText(gServer), dir = getText(gGameDir);
	if (name.empty()) { setStatus("Enter a player name."); return; }
	if (name.size() > 16) name.resize(16);
	if (server.empty()) { setStatus("Enter the server address, like 203.0.113.5:7777."); return; }
	if (GetFileAttributesA((dir + "\\INIZH.big").c_str()) == INVALID_FILE_ATTRIBUTES) { setStatus("That folder does not look like Zero Hour (INIZH.big is missing)."); return; }
	int fi = (int)SendMessageA(gFaction, CB_GETCURSEL, 0, 0); if (fi < 0) fi = 0;
	int ri = (int)SendMessageA(gRes, CB_GETCURSEL, 0, 0); if (ri < 0) ri = 1;
	int mode = (int)SendMessageA(gMode, CB_GETCURSEL, 0, 0); if (mode < 0) mode = 1;
	iniSet("name", name); iniSet("server", server); iniSet("gamedir", dir); iniSet("faction", kFactions[fi].templ);
	char c[16]; sprintf(c, "%06x", (unsigned)(GetRValue(gColorValue) << 16 | GetGValue(gColorValue) << 8 | GetBValue(gColorValue))); iniSet("color", c);
	iniSet("mode", mode == 0 ? "windowed" : mode == 1 ? "borderless" : "exclusive"); iniSet("resolution", kResolutions[ri]);

	char self[MAX_PATH]; GetModuleFileNameA(NULL, self, MAX_PATH); char *sl = strrchr(self, '\\'); if (sl) *sl = 0;
	std::string exe = std::string(self) + "\\zh_client.exe";
	if (GetFileAttributesA(exe.c_str()) == INVALID_FILE_ATTRIBUTES) { setStatus("zh_client.exe was not found next to the launcher."); return; }

	SetEnvironmentVariableA("UM_CLIENT", server.c_str());
	SetEnvironmentVariableA("UM_NAME", name.c_str());
	SetEnvironmentVariableA("UM_FACTION", kFactions[fi].templ);
	SetEnvironmentVariableA("UM_COLOR", c);
	SetEnvironmentVariableA("UM_KEY", identityKey().c_str());
	std::string cmd = "\"" + exe + "\"";
	int w = 1280, h = 720; sscanf(kResolutions[ri], "%dx%d", &w, &h);
	if (mode == 0) cmd += " -win";
	else if (mode == 1) { cmd += " -win -borderless"; w = GetSystemMetrics(SM_CXSCREEN); h = GetSystemMetrics(SM_CYSCREEN); }
	char res[64]; sprintf(res, " -xres %d -yres %d", w, h); cmd += res;
	AllowSetForegroundWindow(ASFW_ANY);   // let the game take the foreground (exclusive fullscreen minimises a window that is not active)
	STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si; PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
	if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, dir.c_str(), &si, &pi)) { char m[128]; sprintf(m, "Could not start the game (error %lu).", GetLastError()); setStatus(m); return; }
	CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
	setStatus("Starting... you can close this window.");
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
	switch (m)
	{
	case WM_CREATE:
	{
		HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		struct Row { const char *label; int y; } rows[] = { { "Player name", 14 }, { "Faction", 50 }, { "Colour", 86 }, { "Server (address:port)", 122 }, { "Zero Hour folder", 158 }, { "Resolution / mode", 194 } };
		for (int i = 0; i < 6; ++i) { HWND s = CreateWindowA("STATIC", rows[i].label, WS_CHILD | WS_VISIBLE, 14, rows[i].y + 4, 130, 20, h, NULL, NULL, NULL); SendMessageA(s, WM_SETFONT, (WPARAM)font, TRUE); }
		gName = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", iniGet("name", "Player").c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 150, 14, 250, 24, h, (HMENU)ID_NAME, NULL, NULL);
		SendMessageA(gName, EM_LIMITTEXT, 16, 0);
		gFaction = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 150, 50, 250, 300, h, (HMENU)ID_FACTION, NULL, NULL);
		std::string wantF = iniGet("faction", "FactionAmerica"); int sel = 0;
		for (int i = 0; i < (int)(sizeof kFactions / sizeof *kFactions); ++i) { SendMessageA(gFaction, CB_ADDSTRING, 0, (LPARAM)kFactions[i].label); if (wantF == kFactions[i].templ) sel = i; }
		SendMessageA(gFaction, CB_SETCURSEL, sel, 0);
		unsigned cv = (unsigned)strtoul(iniGet("color", "dc2828").c_str(), NULL, 16); gColorValue = RGB((cv >> 16) & 255, (cv >> 8) & 255, cv & 255);
		gColor = CreateWindowA("BUTTON", "Pick a colour...", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 150, 86, 250, 26, h, (HMENU)ID_COLOR, NULL, NULL);
		gServer = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", iniGet("server", "127.0.0.1:7777").c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 150, 122, 250, 24, h, (HMENU)ID_SERVER, NULL, NULL);
		std::string gd = iniGet("gamedir", ""); if (gd.empty()) gd = detectGameDir();
		gGameDir = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", gd.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 150, 158, 200, 24, h, (HMENU)ID_GAMEDIR, NULL, NULL);
		HWND b = CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 356, 158, 44, 24, h, (HMENU)ID_BROWSE, NULL, NULL); SendMessageA(b, WM_SETFONT, (WPARAM)font, TRUE);
		gRes = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 150, 194, 120, 200, h, (HMENU)ID_RES, NULL, NULL);
		std::string wantR = iniGet("resolution", "1280x720"); int rsel = 1;
		for (int i = 0; i < 4; ++i) { SendMessageA(gRes, CB_ADDSTRING, 0, (LPARAM)kResolutions[i]); if (wantR == kResolutions[i]) rsel = i; }
		SendMessageA(gRes, CB_SETCURSEL, rsel, 0);
		gMode = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 276, 194, 124, 200, h, (HMENU)ID_WINDOWED, NULL, NULL);
		{ std::string wantM = iniGet("mode", "borderless"); int msel = wantM == "windowed" ? 0 : wantM == "exclusive" ? 2 : 1; for (int i = 0; i < 3; ++i) SendMessageA(gMode, CB_ADDSTRING, 0, (LPARAM)kModes[i]); SendMessageA(gMode, CB_SETCURSEL, msel, 0); }
		HWND st = CreateWindowA("BUTTON", "Start", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 150, 236, 250, 34, h, (HMENU)ID_START, NULL, NULL);
		gStatus = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 14, 282, 390, 36, h, (HMENU)ID_STATUS, NULL, NULL);
		HWND kids[] = { gName, gFaction, gColor, gServer, gGameDir, gRes, gMode, st, gStatus };
		for (int i = 0; i < 9; ++i) SendMessageA(kids[i], WM_SETFONT, (WPARAM)font, TRUE);
		return 0;
	}
	case WM_DRAWITEM:
	{
		DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)l;
		if (d->CtlID == ID_COLOR)
		{
			HBRUSH br = CreateSolidBrush(gColorValue); FillRect(d->hDC, &d->rcItem, br); DeleteObject(br);
			FrameRect(d->hDC, &d->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
			int lum = GetRValue(gColorValue) * 30 + GetGValue(gColorValue) * 59 + GetBValue(gColorValue) * 11;
			SetBkMode(d->hDC, TRANSPARENT); SetTextColor(d->hDC, lum > 12000 ? RGB(0, 0, 0) : RGB(255, 255, 255));
			DrawTextA(d->hDC, "Click to change colour", -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			return TRUE;
		}
		break;
	}
	case WM_COMMAND:
		switch (LOWORD(w))
		{
		case ID_COLOR: chooseColor(h); return 0;
		case ID_BROWSE: browse(h); return 0;
		case ID_START: start(h); return 0;
		}
		break;
	case WM_DESTROY: PostQuitMessage(0); return 0;
	}
	return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmdLine, int show)
{
	CoInitialize(NULL);
	char dir[MAX_PATH]; if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, dir) != S_OK) strcpy(dir, ".");
	std::string folder = std::string(dir) + "\\LivingWorld"; CreateDirectoryA(folder.c_str(), NULL); strcpy(gIni, (folder + "\\client.ini").c_str());
	for (int i = 0; i < 16; ++i) gCustom[i] = RGB(i * 16, 255 - i * 16, (i * 37) & 255);
	WNDCLASSA wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = proc; wc.hInstance = inst; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.lpszClassName = "LivingWorldLauncher";
	RegisterClassA(&wc);
	RECT r = { 0, 0, 420, 330 }; AdjustWindowRect(&r, WS_CAPTION | WS_SYSMENU, FALSE);
	HWND w = CreateWindowA("LivingWorldLauncher", "Living World", WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
	ShowWindow(w, show);
	if (cmdLine && strstr(cmdLine, "--start")) PostMessageA(w, WM_COMMAND, ID_START, 0);   // start straight away with the saved choices (for scripts and shortcuts)
	MSG m; while (GetMessageA(&m, NULL, 0, 0)) { if (!IsDialogMessageA(w, &m)) { TranslateMessage(&m); DispatchMessageA(&m); } }
	return 0;
}
