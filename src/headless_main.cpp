// Headless entry point: the retail WinMain minus window/D3D/launcher/copy-protection, keeping the engine's own startup order.
#include "PreRTS.h"
#include "Common/GameEngine.h"
#include "Common/Debug.h"
#include "Common/GameMemory.h"
#include "Common/CriticalSection.h"
#include "Common/Version.h"
#include <stdio.h>

extern void GameMain(int argc, char *argv[]);
HINSTANCE ApplicationHInstance = NULL;
char *gAppPrefix = "";
static CriticalSection critSec2, critSec3, critSec4, critSec5;

void UmStartWatchdog();
static LONG WINAPI umCrash(EXCEPTION_POINTERS *ep)
{
	fprintf(stderr, "[um] CRASH code=0x%08lx addr=%p (image base %p)\n", ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress, (void *)GetModuleHandle(NULL));
	DWORD *esp = (DWORD *)ep->ContextRecord->Esp;
	for (int i = 0; i < 24; ++i) fprintf(stderr, "  stack[%d]=%08lx\n", i, (unsigned long)esp[i]);
	fflush(stderr);
	return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char *argv[])
{
	SetUnhandledExceptionFilter(umCrash);
	UmStartWatchdog();
	setvbuf(stdout, NULL, _IONBF, 0);
	ApplicationHInstance = GetModuleHandle(NULL);
	try
	{
		TheUnicodeStringCriticalSection = &critSec2;
		TheDmaCriticalSection = &critSec3;
		TheMemoryPoolCriticalSection = &critSec4;
		TheDebugLogCriticalSection = &critSec5;
		fputs("[um] main entered\n", stderr);
		DEBUG_INIT(DEBUG_FLAGS_DEFAULT);
		fputs("[um] debug init done\n", stderr);
		initMemoryManager();
		fputs("[um] memory manager up\n", stderr);
		TheVersion = NEW Version;
		TheVersion->setVersion(1, 4, 0, 0, AsciiString("livingworld"), AsciiString("headless"), AsciiString(__TIME__), AsciiString(__DATE__));
		printf("[um] starting engine, cwd-relative data\n");
		GameMain(argc, argv);
		printf("[um] engine returned\n");
		delete TheVersion; TheVersion = NULL;
		shutdownMemoryManager();
		DEBUG_SHUTDOWN();
	}
	catch (ErrorCode ec) { printf("[um] ErrorCode exception 0x%08x\n", (unsigned)ec); return 2; }
	catch (...) { printf("[um] unknown exception\n"); return 4; }
	return 0;
}

// The mingw CRT (built against -fms-compatibility headers) still references WinMain; forward it.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return main(__argc, __argv); }
static void __attribute__((constructor(101))) um_early(void) { fputs("[um] static-init phase begun\n", stderr); }

// Diagnostics: report who calls exit()/abort() (release-mode ReleaseCrash paths exit silently).
extern "C" {
void __real_exit(int);
void __real_abort(void);
#include <unwind.h>
struct UmUnwind { void *f[24]; int n; };
static _Unwind_Reason_Code umUnwindCb(struct _Unwind_Context *c, void *arg)
{
	UmUnwind *u = (UmUnwind *)arg; if (u->n >= 24) return _URC_END_OF_STACK;
	u->f[u->n++] = (void *)_Unwind_GetIP(c); return _URC_NO_REASON;
}
static void umTrace(const char *what, int code)
{
	UmUnwind u; u.n = 0; _Unwind_Backtrace(umUnwindCb, &u);
	void **frames = u.f; USHORT n = (USHORT)u.n;
	fprintf(stderr, "[um] %s(%d) called; base=%p; frames:", what, code, (void *)GetModuleHandle(NULL));
	for (USHORT i = 0; i < n; ++i) fprintf(stderr, " %p", frames[i]);
	fprintf(stderr, "\n"); fflush(stderr);
}
void __wrap_exit(int c) { umTrace("exit", c); __real_exit(c); }
void __wrap_abort(void) { umTrace("abort", 0); __real_abort(); }
}

#include <typeinfo>
extern "C" {
void __real___cxa_throw(void *, std::type_info *, void (*)(void *));
void __wrap___cxa_throw(void *obj, std::type_info *ti, void (*dtor)(void *))
{
	umTrace("throw", 0);
	fprintf(stderr, "[um]   exception type: %s\n", ti ? ti->name() : "?");
	fprintf(stderr, "[um]   first word: 0x%08x\n", *(unsigned *)obj);
	fflush(stderr);
	__real___cxa_throw(obj, ti, dtor);
}
}

// Hang diagnostics: UM_WATCHDOG=<seconds> prints where the main thread is stuck (EIP + plausible return addresses on its stack).
static HANDLE gMainThread = NULL;
static DWORD WINAPI umWatchdog(LPVOID arg)
{
	DWORD secs = (DWORD)(size_t)arg; Sleep(secs * 1000);
	SuspendThread(gMainThread);
	CONTEXT c; memset(&c, 0, sizeof(c)); c.ContextFlags = CONTEXT_FULL; GetThreadContext(gMainThread, &c);
	fprintf(stderr, "[um] WATCHDOG after %lus: eip=%08lx base=%p frames:", (unsigned long)secs, (unsigned long)c.Eip, (void *)GetModuleHandle(NULL));
	DWORD *sp = (DWORD *)c.Esp; char *lo = (char *)GetModuleHandle(NULL); char *hi = lo + 0x1800000; int n = 0;
	for (int i = 0; i < 4000 && n < 40; ++i) { if (IsBadReadPtr(sp + i, 4)) break; DWORD v = sp[i]; if ((char *)v >= lo + 0x1000 && (char *)v < hi) { fprintf(stderr, " %08lx", (unsigned long)v); ++n; } }
	fprintf(stderr, "\n"); fflush(stderr); ExitProcess(9); return 0;
}
void UmStartWatchdog() { if (const char *w = getenv("UM_WATCHDOG")) { DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &gMainThread, 0, FALSE, DUPLICATE_SAME_ACCESS); CreateThread(NULL, 0, umWatchdog, (LPVOID)(size_t)atoi(w), 0, NULL); } }

void UmTraceHere(const char *what) { umTrace(what, 0); }
