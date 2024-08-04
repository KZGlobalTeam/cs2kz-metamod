#include "common.h"
#include "utils.h"
#include "cdetour.h"
#include "detours.h"

#include "sdk/entity/ccsplayerpawn.h"
#include "movement/movement.h"

#include "tier0/memdbgon.h"

CUtlVector<CDetourBase *> g_vecDetours;
extern CGameConfig *g_pGameConfig;

DECLARE_DETOUR(RecvServerBrowserPacket, Detour_RecvServerBrowserPacket);
// Unused
DECLARE_DETOUR(TracePlayerBBox, Detour_TracePlayerBBox);

DECLARE_MOVEMENT_DETOUR(PhysicsSimulate);
DECLARE_MOVEMENT_DETOUR(ProcessUsercmds);
DECLARE_MOVEMENT_DETOUR(GetMaxSpeed);
DECLARE_MOVEMENT_DETOUR(SetupMove);
DECLARE_MOVEMENT_DETOUR(ProcessMovement);
DECLARE_MOVEMENT_DETOUR(PlayerMoveNew);
DECLARE_MOVEMENT_DETOUR(CheckParameters);
DECLARE_MOVEMENT_DETOUR(CanMove);
DECLARE_MOVEMENT_DETOUR(FullWalkMove);
DECLARE_MOVEMENT_DETOUR(MoveInit);
DECLARE_MOVEMENT_DETOUR(CheckWater);
DECLARE_MOVEMENT_DETOUR(WaterMove);
DECLARE_MOVEMENT_DETOUR(CheckVelocity);
DECLARE_MOVEMENT_DETOUR(Duck);
DECLARE_MOVEMENT_DETOUR(CanUnduck);
DECLARE_MOVEMENT_DETOUR(LadderMove);
DECLARE_MOVEMENT_DETOUR(CheckJumpButton);
DECLARE_MOVEMENT_DETOUR(OnJump);
DECLARE_MOVEMENT_DETOUR(AirMove);
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
	INIT_DETOUR(g_pGameConfig, RecvServerBrowserPacket);
}

void FlushAllDetours()
{
	FOR_EACH_VEC(g_vecDetours, i)
	{
		g_vecDetours[i]->FreeDetour();
	}

	g_vecDetours.RemoveAll();
}

int FASTCALL Detour_RecvServerBrowserPacket(RecvPktInfo_t &info, void *pSock)
{
	int retValue = RecvServerBrowserPacket(info, pSock);
	// META_CONPRINTF("Detour_RecvServerBrowserPacket: Message received from %i.%i.%i.%i:%i, returning %i\nPayload: %s\n",
	// 	info.m_adrFrom.m_IPv4Bytes.b1, info.m_adrFrom.m_IPv4Bytes.b2, info.m_adrFrom.m_IPv4Bytes.b3, info.m_adrFrom.m_IPv4Bytes.b4,
	// 	info.m_adrFrom.m_usPort, retValue, (char*)info.m_pPkt);
	return retValue;
}

void Detour_TracePlayerBBox(const Vector &start, const Vector &end, const bbox_t &bounds, CTraceFilter *filter, trace_t &pm)
{
	TracePlayerBBox(start, end, bounds, filter, pm);
}
