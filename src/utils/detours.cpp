#include "common.h"
#include "utils.h"
#include "cdetour.h"
#include "module.h"
#include "detours.h"
#include "utils/simplecmds.h"

#include "movement/movement.h"

#include "tier0/memdbgon.h"
extern CEntitySystem* g_pEntitySystem;
CUtlVector<CDetourBase *> g_vecDetours;

DECLARE_DETOUR(CBaseTrigger_StartTouch, Detour_CBaseTrigger_StartTouch, &modules::server);
DECLARE_DETOUR(CBaseTrigger_EndTouch, Detour_CBaseTrigger_EndTouch, &modules::server);
DECLARE_DETOUR(RecvServerBrowserPacket, Detour_RecvServerBrowserPacket, &modules::steamnetworkingsockets);
DECLARE_DETOUR(CCSPP_Teleport, Detour_CCSPP_Teleport, &modules::server);

DECLARE_MOVEMENT_DETOUR(ProcessUsercmds);
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
DECLARE_MOVEMENT_DETOUR(PostPlayerMove);
DECLARE_MOVEMENT_DETOUR(PostThink);

void InitDetours()
{
	g_vecDetours.RemoveAll();
	INIT_DETOUR(CBaseTrigger_StartTouch);
	INIT_DETOUR(CBaseTrigger_EndTouch);
	INIT_DETOUR(RecvServerBrowserPacket);
}

void FlushAllDetours()
{
	FOR_EACH_VEC(g_vecDetours, i)
	{
		g_vecDetours[i]->FreeDetour();
	}
	
	g_vecDetours.RemoveAll();
}

bool IsEntTriggerMultiple(CBaseEntity2 *ent)
{
	bool result = false;
	if (ent && ent->m_pEntity)
	{
		const char *classname = ent->m_pEntity->m_designerName.String();
		result = classname && stricmp(classname, "trigger_multiple") == 0;
	}
	return result;
}

bool IsTriggerStartZone(CBaseTrigger *trigger)
{
	bool result = false;
	if (trigger && trigger->m_pEntity)
	{
		const char *targetname = trigger->m_pEntity->m_name.String();
		result = targetname && stricmp(targetname, "timer_startzone") == 0;
	}
	return result;
}

bool IsTriggerEndZone(CBaseTrigger *trigger)
{
	bool result = false;
	if (trigger && trigger->m_pEntity)
	{
		const char *targetname = trigger->m_pEntity->m_name.String();
		result = targetname && stricmp(targetname, "timer_endzone") == 0;
	}
	return result;
}

void FASTCALL Detour_CBaseTrigger_StartTouch(CBaseTrigger *this_, CBaseEntity2 *pOther)
{
	CBaseTrigger_StartTouch(this_, pOther);
	
	if (pOther->IsPawn())
	{
		if (IsEntTriggerMultiple(this_))
		{
			CBasePlayerController *controller = utils::GetController(pOther);
			if (!controller) return;
			MovementPlayer *player = g_pPlayerManager->ToPlayer(controller);
			if (IsTriggerStartZone(this_))
			{
				player->StartZoneStartTouch();
			}
			else if (IsTriggerEndZone(this_))
			{
				player->EndZoneStartTouch();
			}
		}
	}
}

void FASTCALL Detour_CBaseTrigger_EndTouch(CBaseTrigger *this_, CBaseEntity2 *pOther)
{
	CBaseTrigger_EndTouch(this_, pOther);

	if (pOther->IsPawn())
	{
		if (IsEntTriggerMultiple(this_))
		{
			CBasePlayerController *controller = utils::GetController(pOther);
			if (!controller) return;
			MovementPlayer *player = g_pPlayerManager->ToPlayer(controller);
			if (IsTriggerStartZone(this_))
			{
				player->StartZoneEndTouch();
			}
		}
	}
}

int FASTCALL Detour_RecvServerBrowserPacket(RecvPktInfo_t &info, void* pSock)
{
	int retValue = RecvServerBrowserPacket(info, pSock);
	// META_CONPRINTF("Detour_RecvServerBrowserPacket: Message received from %i.%i.%i.%i:%i, returning %i\nPayload: %s\n", 
	// 	info.m_adrFrom.m_IPv4Bytes.b1, info.m_adrFrom.m_IPv4Bytes.b2, info.m_adrFrom.m_IPv4Bytes.b3, info.m_adrFrom.m_IPv4Bytes.b4, 
	// 	info.m_adrFrom.m_usPort, retValue, (char*)info.m_pPkt);
	return retValue;
}

void Detour_CCSPP_Teleport(CCSPlayerPawn *this_, const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
{
	// Just to be sure.
	if (this_->IsPawn())
	{
		MovementPlayer *player = g_pPlayerManager->ToPlayer(this_);
		player->OnTeleport(newPosition, newAngles, newVelocity);
	}
	CCSPP_Teleport(this_, newPosition, newAngles, newVelocity);
}