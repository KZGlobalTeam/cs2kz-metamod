#pragma once
#include "cdetour.h"
#include "sdk/datatypes.h"
#include "sdk/entity/cbasetrigger.h"
#include "sdk/steamnetworkingsockets.h"

#include "movement/movement.h"
#include "utils/utils.h"

int FASTCALL Detour_RecvServerBrowserPacket(RecvPktInfo_t &info, void *pSock);
extern CDetour<decltype(Detour_RecvServerBrowserPacket)> RecvServerBrowserPacket;

bool FASTCALL Detour_TraceShape(const void *physicsQuery, const Ray_t &ray, const Vector &start, const Vector &end, const CTraceFilter *pTraceFilter,
								trace_t *pm);
extern CDetour<decltype(Detour_TraceShape)> TraceShape;
void FASTCALL Detour_CPhysicsGameSystemFrameBoundary(void *pThis);
extern CDetour<decltype(Detour_CPhysicsGameSystemFrameBoundary)> CPhysicsGameSystemFrameBoundary;

#define DECLARE_MOVEMENT_DETOUR(name)        DECLARE_DETOUR(name, movement::Detour_##name);
#define DECLARE_MOVEMENT_EXTERN_DETOUR(name) extern CDetour<decltype(movement::Detour_##name)> name;

DECLARE_MOVEMENT_EXTERN_DETOUR(PhysicsSimulate);
DECLARE_MOVEMENT_EXTERN_DETOUR(ProcessUsercmds);
DECLARE_MOVEMENT_EXTERN_DETOUR(SetupMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(ProcessMovement);
DECLARE_MOVEMENT_EXTERN_DETOUR(PlayerMove);
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
DECLARE_MOVEMENT_EXTERN_DETOUR(AirMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(AirAccelerate);
DECLARE_MOVEMENT_EXTERN_DETOUR(Friction);
DECLARE_MOVEMENT_EXTERN_DETOUR(WalkMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(TryPlayerMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(CategorizePosition);
DECLARE_MOVEMENT_EXTERN_DETOUR(CheckFalling);
DECLARE_MOVEMENT_EXTERN_DETOUR(PostPlayerMove);
DECLARE_MOVEMENT_EXTERN_DETOUR(PostThink);

void InitDetours();
void FlushAllDetours();
