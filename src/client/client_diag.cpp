// Diagnostics for the client: who called exit()/abort(), and a crash dump, both written next to the working directory.
#include <windows.h>
#include <stdio.h>
#include <unwind.h>
#include <string.h>
#include <psapi.h>

// Diagnostic files go to a "logs" folder next to zh_client.exe (the mod's own folder, not the Zero Hour folder the game runs in); UM_DIAG_DIR overrides it.
// When that cannot be written either, %TEMP%\\LivingWorld is used, and as a last resort the working directory.
static char gDiagDir[MAX_PATH] = "";
static bool diagUsable(const char *dir)
{
	CreateDirectoryA(dir, NULL);
	char p[MAX_PATH]; wsprintfA(p, "%sum_probe.tmp", dir);
	HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	CloseHandle(h); return true;
}
const char *UmDiagPath(const char *leaf)
{
	static char out[4][MAX_PATH]; static int k = 0; char *o = out[k++ & 3];
	if (!gDiagDir[0])
	{
		char d[MAX_PATH] = ""; const char *ov = getenv("UM_DIAG_DIR");
		if (ov && *ov) { lstrcpynA(d, ov, MAX_PATH - 2); size_t n = strlen(d); if (d[n - 1] != '\\' && d[n - 1] != '/') strcat(d, "\\"); }
		else { GetModuleFileNameA(NULL, d, MAX_PATH); char *sl = strrchr(d, '\\'); if (sl) strcpy(sl + 1, "logs\\"); else d[0] = 0; }
		if (d[0] && diagUsable(d)) strcpy(gDiagDir, d);
		else
		{
			char tmp[MAX_PATH]; DWORD n = GetTempPathA(MAX_PATH, tmp);
			if (n && n < MAX_PATH - 20) { wsprintfA(d, "%sLivingWorld\\", tmp); if (diagUsable(d)) strcpy(gDiagDir, d); }
			if (!gDiagDir[0]) strcpy(gDiagDir, ".\\");
		}
	}
	wsprintfA(o, "%s%s", gDiagDir, leaf); return o;
}


// ---- memory: a big city world needs ~1.8 GB; running out must leave a trace ----
// One line of numbers about the process and the machine (plain Win32, no allocation, safe to call when memory is exhausted).
void UmMemLine(char *out, int n)
{
	PROCESS_MEMORY_COUNTERS_EX pmc; memset(&pmc, 0, sizeof pmc); pmc.cb = sizeof pmc;
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof pmc);
	MEMORYSTATUSEX ms; memset(&ms, 0, sizeof ms); ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms);
	wsprintfA(out, "process: %lu MB in use (peak %lu), %lu MB committed; address space free %lu MB of %lu; machine: %lu MB RAM free of %lu, %lu MB commit free of %lu",
		(unsigned long)(pmc.WorkingSetSize >> 20), (unsigned long)(pmc.PeakWorkingSetSize >> 20), (unsigned long)(pmc.PrivateUsage >> 20),
		(unsigned long)(ms.ullAvailVirtual >> 20), (unsigned long)(ms.ullTotalVirtual >> 20),
		(unsigned long)(ms.ullAvailPhys >> 20), (unsigned long)(ms.ullTotalPhys >> 20), (unsigned long)(ms.ullAvailPageFile >> 20), (unsigned long)(ms.ullTotalPageFile >> 20));
}
static void diagAppend(const char *leaf, const char *text)
{
	HANDLE h = CreateFileA(UmDiagPath(leaf), FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return;
	DWORD w; WriteFile(h, text, lstrlenA(text), &w, NULL); CloseHandle(h);
}
// the engine's allocator calls this just before it throws ERROR_OUT_OF_MEMORY
void UmOutOfMemory(int bytes)
{
	static volatile LONG once = 0; if (InterlockedExchange(&once, 1)) return;
	char line[600], msg[760]; UmMemLine(line, sizeof line);
	wsprintfA(msg, "OUT OF MEMORY: the game asked for %d bytes and Windows said no. %s\r\n", bytes, line);
	diagAppend("um_client_trace.txt", msg); diagAppend("um_client_mem.txt", msg);
}
// the engine's own fatal-error path (ReleaseCrash): record the reason and the memory state, then tell the player
void UmShowFatal(const char *reason)
{
	char mem[600], msg[1700]; UmMemLine(mem, sizeof mem);
	wsprintfA(msg, "ReleaseCrash: %s | %s\r\n", reason ? reason : "(no reason)", mem);
	diagAppend("um_client_trace.txt", msg);
	wsprintfA(msg, "Living World hit a serious error and has to close.\n\nReason: %s\n\nMemory: %s\n\nPlease send the files in %s (um_client_log.txt, um_client_trace.txt, um_client_mem.txt) and ReleaseCrashInfo.txt from your Command and Conquer Generals Zero Hour Data folder (in Documents).",
		reason ? reason : "(none)", mem, gDiagDir[0] == '.' ? "the game's folder" : gDiagDir);
	if (getenv("UM_SERVER") || getenv("UM_NO_DIALOG")) return;   // an unattended server must not wait for a click
	MessageBoxA(NULL, msg, "Living World", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND);
}
// the last known memory figures survive a hard kill: rewrite a one-line file every few seconds
static void diagMemTick(void)
{
	static DWORD t0 = GetTickCount(); char line[600], msg[700]; UmMemLine(line, sizeof line);
	wsprintfA(msg, "t+%lu s  %s\r\n", (unsigned long)((GetTickCount() - t0) / 1000), line);
	HANDLE h = CreateFileA(UmDiagPath("um_client_mem.txt"), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return;
	DWORD w; WriteFile(h, msg, lstrlenA(msg), &w, NULL); CloseHandle(h);
}

static void diagWrite(const char *what, int code)
{
	FILE *f = fopen(UmDiagPath("um_client_trace.txt"), "a"); if (!f) return;
	fprintf(f, "%s(%d) base=%p\n", what, code, (void *)GetModuleHandle(NULL));
	fclose(f);
}
struct UmUnwind { void *f[24]; int n; };
static _Unwind_Reason_Code umUnwindCb(struct _Unwind_Context *c, void *arg)
{
	UmUnwind *u = (UmUnwind *)arg; if (u->n >= 24) return _URC_END_OF_STACK;
	u->f[u->n++] = (void *)_Unwind_GetIP(c); return _URC_NO_REASON;
}
static void diagStack(const char *what, int code)
{
	UmUnwind u; u.n = 0; _Unwind_Backtrace(umUnwindCb, &u);
	FILE *f = fopen(UmDiagPath("um_client_trace.txt"), "a"); if (!f) return;
	fprintf(f, "%s(%d) base=%p frames:", what, code, (void *)GetModuleHandle(NULL));
	for (int i = 0; i < u.n; ++i) fprintf(f, " %p", u.f[i]);
	fprintf(f, "\n"); fclose(f);
}
extern "C" {
void __real_exit(int);
void __real_abort(void);
void __wrap_exit(int c) { diagStack("exit", c); __real_exit(c); }
void __wrap_abort(void) { diagStack("abort", 0); __real_abort(); }
}
static DWORD gCrashRec[6 + 1024 + 8 + 64];
static LONG WINAPI diagCrash(EXCEPTION_POINTERS *ep)
{
	// raw binary record, plain Win32 only (the C runtime or stack may be what is broken): code, address, eip, image base, access type, access address, then stack dwords

	gCrashRec[0] = ep->ExceptionRecord->ExceptionCode; gCrashRec[1] = (DWORD)ep->ExceptionRecord->ExceptionAddress; gCrashRec[2] = ep->ContextRecord->Eip; gCrashRec[3] = (DWORD)GetModuleHandleA(NULL);
	gCrashRec[4] = ep->ExceptionRecord->NumberParameters >= 2 ? (DWORD)ep->ExceptionRecord->ExceptionInformation[0] : 0; gCrashRec[5] = ep->ExceptionRecord->NumberParameters >= 2 ? (DWORD)ep->ExceptionRecord->ExceptionInformation[1] : 0;
	DWORD *esp = (DWORD *)ep->ContextRecord->Esp;
	for (int i = 0; i < 1024; ++i) gCrashRec[6 + i] = IsBadReadPtr(esp + i, 4) ? 0 : esp[i];
	{ HMODULE m = NULL; GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)ep->ExceptionRecord->ExceptionAddress, &m); gCrashRec[6 + 1024] = (DWORD)m; gCrashRec[6 + 1025] = ep->ContextRecord->Ebp; gCrashRec[6 + 1026] = ep->ContextRecord->Eax; gCrashRec[6 + 1027] = ep->ContextRecord->Ecx; gCrashRec[6 + 1028] = ep->ContextRecord->Edx; gCrashRec[6 + 1029] = ep->ContextRecord->Esi; gCrashRec[6 + 1030] = ep->ContextRecord->Edi; gCrashRec[6 + 1031] = ep->ContextRecord->Ebx; }
	{ DWORD *fp = (DWORD *)ep->ContextRecord->Ebp; for (int i = 0; i < 64; ++i) { DWORD ret = 0; if (fp && !IsBadReadPtr(fp, 8)) { ret = fp[1]; fp = (DWORD *)fp[0]; } else fp = NULL; gCrashRec[6 + 1032 + i] = ret; } }
	HANDLE h = CreateFileA(UmDiagPath("um_client_crash.bin"), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, gCrashRec, sizeof gCrashRec, &w, NULL); CloseHandle(h); }
	return EXCEPTION_EXECUTE_HANDLER;
}
// The final, unhandled crash: the record is already written; tell the player instead of vanishing silently.
static LONG WINAPI diagUnhandled(EXCEPTION_POINTERS *ep)
{
	diagCrash(ep);
	char msg[1100], mem[600]; UmMemLine(mem, sizeof mem);
	wsprintfA(msg, "Living World crashed.\n\nError %08lX at %08lX (game base %08lX).\n\nThe game will close. Please send um_client_crash.bin and um_client_log.txt from:\n%s\nso it can be investigated.\n\nMemory: %s",
		(unsigned long)ep->ExceptionRecord->ExceptionCode, (unsigned long)ep->ExceptionRecord->ExceptionAddress, (unsigned long)GetModuleHandleA(NULL), gDiagDir[0] == '.' ? "the game's folder" : gDiagDir, mem);
	MessageBoxA(NULL, msg, "Living World", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
	return EXCEPTION_EXECUTE_HANDLER;
}
static LONG CALLBACK diagVectored(EXCEPTION_POINTERS *ep)
{
	DWORD c = ep->ExceptionRecord->ExceptionCode;
	if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == EXCEPTION_STACK_OVERFLOW) { static volatile LONG once = 0; if (!InterlockedExchange(&once, 1)) diagCrash(ep); InterlockedExchange(&once, 0); }
	return EXCEPTION_CONTINUE_SEARCH;
}
static void __attribute__((constructor(101))) diagEarly(void)
{
	UmDiagPath("");
	SetUnhandledExceptionFilter(diagUnhandled);
	AddVectoredExceptionHandler(1, diagVectored);   // drivers and the runtime may replace the unhandled filter; this one sees every access violation first diagWrite("static init", 0);
}
// a GUI program has no console: keep the engine's printf output (network, sync) in a file next to the game
void UmClientLogInit(void)
{
	const char *lf = getenv("UM_LOGFILE") ? getenv("UM_LOGFILE") : UmDiagPath("um_client_log.txt");
	freopen(lf, "w", stdout); freopen(lf, "a", stderr);
	setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0);
}

// Hang watchdog: if the main thread stops calling UmClientHeartbeat() for 12 s, record where it is (same record format as a crash, file um_client_hang.bin).
static volatile LONG gBeat = 0; static DWORD gMainThreadId = 0; static HANDLE gMainThread = NULL;
void UmClientHeartbeat(void) { InterlockedIncrement(&gBeat); }
static DWORD WINAPI hangWatch(LPVOID)
{
	LONG last = -1; int still = 0;
	for (;;)
	{
		Sleep(2000);
		{ static int memTicks = 0; if ((memTicks++ & 1) == 0) diagMemTick(); }
		LONG now = gBeat;
		static const bool sampling = getenv("UM_SAMPLE") != NULL; static int sampleNo = 0;   // profiling aid: record the main thread's stack every 2 s (um_client_sample_N.bin)
		if (!sampling) {
		if (now != last) { last = now; still = 0; continue; }
		if (++still < 6 || !gMainThread) continue;
		if (still > 6) continue;   // once per hang
		}
		else { static int ticks = 0; static const int delay = atoi(getenv("UM_SAMPLE")); if (++ticks * 2 < delay) continue; static const int maxN = getenv("UM_SAMPLE_N") ? atoi(getenv("UM_SAMPLE_N")) : 12; if (sampleNo >= maxN) continue; }   // UM_SAMPLE=<seconds>: start sampling after that long; UM_SAMPLE_N samples (default 12)
		SuspendThread(gMainThread);
		CONTEXT ctx; memset(&ctx, 0, sizeof ctx); ctx.ContextFlags = CONTEXT_FULL;
		if (GetThreadContext(gMainThread, &ctx))
		{
			static DWORD rec[6 + 1024 + 8 + 64]; memset(rec, 0, sizeof rec);
			rec[0] = 0xDEAD0001; rec[2] = ctx.Eip; rec[3] = (DWORD)GetModuleHandleA(NULL);
			DWORD *esp = (DWORD *)ctx.Esp; for (int i = 0; i < 1024; ++i) rec[6 + i] = IsBadReadPtr(esp + i, 4) ? 0 : esp[i];
			{ HMODULE m = NULL; GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)ctx.Eip, &m); rec[6 + 1024] = (DWORD)m; rec[6 + 1025] = ctx.Ebp; }
			DWORD *fp = (DWORD *)ctx.Ebp; for (int i = 0; i < 64; ++i) { DWORD ret = 0; if (fp && !IsBadReadPtr(fp, 8)) { ret = fp[1]; fp = (DWORD *)fp[0]; } else fp = NULL; rec[6 + 1032 + i] = ret; }
			char fname[MAX_PATH]; if (sampling) { char b[48]; sprintf(b, "um_client_sample_%d.bin", sampleNo++); strcpy(fname, UmDiagPath(b)); } else strcpy(fname, UmDiagPath("um_client_hang.bin"));
			HANDLE h = CreateFileA(fname, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, rec, sizeof rec, &w, NULL); CloseHandle(h); }
		}
		ResumeThread(gMainThread);
	}
	return 0;
}
void UmClientStartHangWatch(void)
{
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &gMainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
	CreateThread(NULL, 0, hangWatch, NULL, 0, NULL);
}
