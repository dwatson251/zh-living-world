// Diagnostics for the client: who called exit()/abort(), and a crash dump, both written next to the working directory.
#include <windows.h>
#include <stdio.h>
#include <unwind.h>

static void diagWrite(const char *what, int code)
{
	FILE *f = fopen("um_client_trace.txt", "a"); if (!f) return;
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
	FILE *f = fopen("um_client_trace.txt", "a"); if (!f) return;
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
	HANDLE h = CreateFileA("um_client_crash.bin", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, gCrashRec, sizeof gCrashRec, &w, NULL); CloseHandle(h); }
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
	SetUnhandledExceptionFilter(diagCrash);
	AddVectoredExceptionHandler(1, diagVectored);   // drivers and the runtime may replace the unhandled filter; this one sees every access violation first diagWrite("static init", 0);
}
// a GUI program has no console: keep the engine's printf output (network, sync) in a file next to the game
void UmClientLogInit(void)
{
	const char *lf = getenv("UM_LOGFILE") ? getenv("UM_LOGFILE") : "um_client_log.txt";
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
			char fname[64]; if (sampling) sprintf(fname, "um_client_sample_%d.bin", sampleNo++); else strcpy(fname, "um_client_hang.bin");
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
