// Things the graphical client does not carry (audio library, embedded browser, benchmark, server-only tracing).
#include "PreRTS.h"
#include "Lib/BaseType.h"

#include "Common/INI.h"
void INI::umSkipBlock(INI *ini)
{
	int depth = 1;
	while (!ini->isEOF() && depth > 0)
	{
		ini->readLine();
		char tmp[INI_MAX_CHARS_PER_LINE + 1];
		strncpy(tmp, ini->m_buffer, INI_MAX_CHARS_PER_LINE); tmp[INI_MAX_CHARS_PER_LINE] = 0;
		char *tok = strtok(tmp, ini->getSeps());
		if (!tok) continue;
		if (stricmp(tok, ini->m_blockEndToken) == 0) --depth;
		else if (!stricmp(tok, "ConditionState") || !stricmp(tok, "DefaultConditionState") || !stricmp(tok, "TransitionState")) ++depth;
	}
}

void UmTraceHere(const char *) {}

extern "C" int RunBenchmark(int, char **, float *floatResult, float *intResult, float *memResult)
{ if (floatResult) *floatResult = 10.0f; if (intResult) *intResult = 10.0f; if (memResult) *memResult = 10.0f; return 0; }

class HAnimClass; class Matrix3D;
class AnimatedSoundMgrClass
{
public:
	static void Initialize(const char *ini_filename = NULL);
	static void Shutdown(void);
	static const char *Get_Embedded_Sound_Name(HAnimClass *anim);
	static float Trigger_Sound(HAnimClass *anim, float old_frame, float new_frame, const Matrix3D &tm);
};
void AnimatedSoundMgrClass::Initialize(const char *) {}
void AnimatedSoundMgrClass::Shutdown(void) {}
const char *AnimatedSoundMgrClass::Get_Embedded_Sound_Name(HAnimClass *) { return NULL; }
float AnimatedSoundMgrClass::Trigger_Sound(HAnimClass *, float, float old_frame, const Matrix3D &) { return old_frame; }

class DX8WebBrowser
{
public:
	static bool Initialize(const char *a, const char *b, const char *c, const char *d);
	static void Shutdown(void);
	static void Update(void);
	static void Render(int backbufferindex);
};
bool DX8WebBrowser::Initialize(const char *, const char *, const char *, const char *) { return false; }
void DX8WebBrowser::Shutdown(void) {}
void DX8WebBrowser::Update(void) {}
void DX8WebBrowser::Render(int) {}
