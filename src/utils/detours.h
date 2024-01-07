#pragma once
#include "cdetour.h"
#include "irecipientfilter.h"
#include "datatypes.h"
#include "utils.h"
#include "movement/movement.h"
#include "sns.h"

void FASTCALL Detour_CBaseTrigger_StartTouch(CBaseTrigger *this_, CBaseEntity2 *pOther);
extern CDetour<decltype(Detour_CBaseTrigger_StartTouch)> CBaseTrigger_StartTouch;

void FASTCALL Detour_CBaseTrigger_EndTouch(CBaseTrigger *this_, CBaseEntity2 *pOther);
extern CDetour<decltype(Detour_CBaseTrigger_EndTouch)> CBaseTrigger_EndTouch;

int FASTCALL Detour_RecvServerBrowserPacket(RecvPktInfo_t &info, void* pSock);
extern CDetour<decltype(Detour_RecvServerBrowserPacket)> RecvServerBrowserPacket;

void FASTCALL Detour_CCSPP_Teleport(CCSPlayerPawn *this_, const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity);
extern CDetour<decltype(Detour_CCSPP_Teleport)> CCSPP_Teleport;

DECLARE_MOVEMENT_EXTERN_DETOUR(ProcessUsercmds);
DECLARE_MOVEMENT_EXTERN_DETOUR(GetMaxSpeed);
DECLARE_MOVEMENT_EXTERN_DETOUR(ProcessMovement);
DECLARE_MOVEMENT_EXTERN_DETOUR(PlayerMoveNew);
DECLARE_MOVEMENT_EXTERN_DETOUR(CheckParameters);
DECLARE_MOVEMENT_EXTERN_DETOUR(CanMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(FullWalkMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(MoveInit);
DECLARE_MOVEMENT_EXTERN_DETOUR(CheckWater);
DECLARE_MOVEMENT_EXTERN_DETOUR(WaterMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(CheckVelocity);
DECLARE_MOVEMENT_EXTERN_DETOUR(Duck);
DECLARE_MOVEMENT_EXTERN_DETOUR(CanUnduck);
DECLARE_MOVEMENT_EXTERN_DETOUR(LadderMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(CheckJumpButton);
DECLARE_MOVEMENT_EXTERN_DETOUR(OnJump);
DECLARE_MOVEMENT_EXTERN_DETOUR(AirAccelerate);
DECLARE_MOVEMENT_EXTERN_DETOUR(Friction);
DECLARE_MOVEMENT_EXTERN_DETOUR(WalkMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(TryPlayerMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(CategorizePosition);
DECLARE_MOVEMENT_EXTERN_DETOUR(FinishGravity);
DECLARE_MOVEMENT_EXTERN_DETOUR(CheckFalling);
DECLARE_MOVEMENT_EXTERN_DETOUR(PostPlayerMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(PostThink);

void InitDetours();
void FlushAllDetours();
