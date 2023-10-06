#include "utils.h"
#include "cdetour.h"
#include "module.h"
#include "detours.h"
#include "movement.h"
#include "tier0/memdbgon.h"

extern CEntitySystem* g_pEntitySystem;
CUtlVector<CDetourBase *> g_vecDetours;

DECLARE_DETOUR(Host_Say, Detour_Host_Say, &modules::server);
DECLARE_DETOUR(CBaseTrigger_StartTouch, Detour_CBaseTrigger_StartTouch, &modules::server);
DECLARE_DETOUR(CBaseTrigger_EndTouch, Detour_CBaseTrigger_EndTouch, &modules::server);

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
	INIT_DETOUR(CBaseTrigger_StartTouch);
	INIT_DETOUR(CBaseTrigger_EndTouch);
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

void FASTCALL Detour_CBaseTrigger_StartTouch(CBaseTrigger *this_, CBaseEntity *pOther)
{
	CBaseTrigger_StartTouch(this_, pOther);
#if 1
	if (utils::IsEntityPawn(pOther))
	{
		CGlobalVars *gpGlobals = interfaces::pEngine->GetServerGlobals();
		const char *triggerName = this_->m_pEntity->m_name.String();
		utils::PrintChat(pOther, "[%i] %s started touching %i (%s)", gpGlobals->framecount, this_->m_pEntity->m_designerName.String(), this_->m_pEntity->m_EHandle.GetEntryIndex(), triggerName ? triggerName : "");
	}
#endif
}

void FASTCALL Detour_CBaseTrigger_EndTouch(CBaseTrigger *this_, CBaseEntity *pOther)
{
	CBaseTrigger_EndTouch(this_, pOther);
#if 1
	if (utils::IsEntityPawn(pOther))
	{
		CGlobalVars *gpGlobals = interfaces::pEngine->GetServerGlobals();
		const char *triggerName = this_->m_pEntity->m_name.String();
		utils::PrintChat(pOther, "[%i] %s stopped touching %i (%s)", gpGlobals->framecount, this_->m_pEntity->m_designerName.String(), this_->m_pEntity->m_EHandle.GetEntryIndex(), triggerName ? triggerName : "");
	}
#endif
}
