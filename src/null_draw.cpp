// Stand-ins for the 19 W3D draw modules. They parse (and discard) their INI blocks so Object.ini loads, and expose
// no model data. NOTE: this is the first-milestone shortcut. Logic queries bone/barrel data through ObjectDrawInterface
// (31 call sites in GameLogic), so a server that must stay in lockstep with real clients needs a real bone provider (see MODLOG).
#include "PreRTS.h"
#include "Common/INI.h"
#include "Common/Xfer.h"
#include "Common/ModuleFactory.h"
#include "null/NullDrawBase.h"

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

class UmNullDrawData : public ModuleData {};

#define UM_NULL_DRAW(NAME) \
class NAME : public NullDrawBase \
{ \
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(NAME, #NAME) \
	MAKE_STANDARD_MODULE_MACRO(NAME) \
public: \
	static ModuleData *friend_newModuleData(INI *ini) { UmNullDrawData *d = MSGNEW("AllModuleData") UmNullDrawData; if (ini) INI::umSkipBlock(ini); return d; } \
	NAME(Thing *thing, const ModuleData *md) : NullDrawBase(thing, md) {} \
	virtual ObjectDrawInterface *getObjectDrawInterface() { return this; } \
	virtual const ObjectDrawInterface *getObjectDrawInterface() const { return this; } \
}; \
NAME::~NAME() {} \
void NAME::crc(Xfer *x) { DrawModule::crc(x); } \
void NAME::xfer(Xfer *x) { XferVersion v = 1; x->xferVersion(&v, 1); DrawModule::xfer(x); } \
void NAME::loadPostProcess() { DrawModule::loadPostProcess(); }

UM_NULL_DRAW(W3DDefaultDraw)
UM_NULL_DRAW(W3DDebrisDraw)
UM_NULL_DRAW(W3DModelDraw)
UM_NULL_DRAW(W3DLaserDraw)
UM_NULL_DRAW(W3DOverlordTankDraw)
UM_NULL_DRAW(W3DOverlordTruckDraw)
UM_NULL_DRAW(W3DOverlordAircraftDraw)
UM_NULL_DRAW(W3DProjectileStreamDraw)
UM_NULL_DRAW(W3DPoliceCarDraw)
UM_NULL_DRAW(W3DRopeDraw)
UM_NULL_DRAW(W3DScienceModelDraw)
UM_NULL_DRAW(W3DSupplyDraw)
UM_NULL_DRAW(W3DDependencyModelDraw)
UM_NULL_DRAW(W3DTankDraw)
UM_NULL_DRAW(W3DTruckDraw)
UM_NULL_DRAW(W3DTracerDraw)
UM_NULL_DRAW(W3DTankTruckDraw)
UM_NULL_DRAW(W3DTreeDraw)
UM_NULL_DRAW(W3DPropDraw)

class UmModuleFactory : public ModuleFactory
{
public:
	virtual void init(void)
	{
		ModuleFactory::init();
#define UM_ADD(N) addModule(N);
		UM_ADD(W3DDefaultDraw) UM_ADD(W3DDebrisDraw) UM_ADD(W3DModelDraw) UM_ADD(W3DLaserDraw) UM_ADD(W3DOverlordTankDraw)
		UM_ADD(W3DOverlordTruckDraw) UM_ADD(W3DOverlordAircraftDraw) UM_ADD(W3DProjectileStreamDraw) UM_ADD(W3DPoliceCarDraw)
		UM_ADD(W3DRopeDraw) UM_ADD(W3DScienceModelDraw) UM_ADD(W3DSupplyDraw) UM_ADD(W3DDependencyModelDraw) UM_ADD(W3DTankDraw)
		UM_ADD(W3DTruckDraw) UM_ADD(W3DTracerDraw) UM_ADD(W3DTankTruckDraw) UM_ADD(W3DTreeDraw) UM_ADD(W3DPropDraw)
	}
};
ModuleFactory *UmCreateModuleFactory() { return MSGNEW("GameEngineSubsystem") UmModuleFactory; }
