#include "common.h"
#include "utils.h"
#include "cdetour.h"
#include "module.h"
#include "detours.h"
#include "tier0/memdbgon.h"

#include "movement/movement.h"
extern CEntitySystem* g_pEntitySystem;
CUtlVector<CDetourBase *> g_vecDetours;

DECLARE_DETOUR(Host_Say, Detour_Host_Say, &modules::server);
DECLARE_DETOUR(CCSGameRules_ctor, Detour_CCSGameRules_ctor, &modules::server);

DECLARE_MOVEMENT_DETOUR(GetMaxSpeed);
DECLARE_MOVEMENT_DETOUR(ProcessMovement);
DECLARE_MOVEMENT_DETOUR(PlayerMoveNew);
DECLARE_MOVEMENT_DETOUR(CheckParameters);
DECLARE_MOVEMENT_DETOUR(CanMove);
DECLARE_MOVEMENT_DETOUR(FullWalkMove);
DECLARE_MOVEMENT_DETOUR(MoveInit);
DECLARE_MOVEMENT_DETOUR(CheckWater);
DECLARE_MOVEMENT_DETOUR(CheckVelocity);
DECLARE_MOVEMENT_DETOUR(Duck);
DECLARE_MOVEMENT_DETOUR(LadderMove);
DECLARE_MOVEMENT_DETOUR(CheckJumpButton);
DECLARE_MOVEMENT_DETOUR(OnJump);
DECLARE_MOVEMENT_DETOUR(AirAccelerate);
DECLARE_MOVEMENT_DETOUR(Friction);
DECLARE_MOVEMENT_DETOUR(WalkMove);
DECLARE_MOVEMENT_DETOUR(TryPlayerMove);
DECLARE_MOVEMENT_DETOUR(CategorizePosition);
DECLARE_MOVEMENT_DETOUR(FinishGravity);
DECLARE_MOVEMENT_DETOUR(CheckFalling);
DECLARE_MOVEMENT_DETOUR(PlayerMovePost);
DECLARE_MOVEMENT_DETOUR(PostThink);

void InitDetours()
{
	g_vecDetours.RemoveAll();
	INIT_DETOUR(Host_Say);
	INIT_DETOUR(CCSGameRules_ctor);
}

void FlushAllDetours()
{
	FOR_EACH_VEC(g_vecDetours, i)
	{
		g_vecDetours[i]->FreeDetour();
	}

	g_vecDetours.RemoveAll();
}

void Detour_Host_Say(CCSPlayerController *pEntity, const CCommand *args, bool teamonly, uint32_t nCustomModRules, const char *pszCustomModPrepend)
{
	Host_Say(pEntity, args, teamonly, nCustomModRules, pszCustomModPrepend);
}

void *FASTCALL Detour_CCSGameRules_ctor(void *this_)
{
	// this is basically where all the configs get executed
	void *result = CCSGameRules_ctor(this_);
	interfaces::pEngine->ServerCommand("exec cs2kz.cfg");
	return result;
}