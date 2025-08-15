#include "common.h"
#include "utils.h"
#include "cdetour.h"
#include "detours.h"
#include "kz/kz.h"
#include "sdk/entity/ccsplayerpawn.h"
#include "movement/movement.h"
#include "fmtstr.h"
#include "tier0/memdbgon.h"
#include "sdk/physics/gamesystem.h"
#include "sdk/physics/ivphysics2.h"

CUtlVector<CDetourBase *> g_vecDetours;
extern CGameConfig *g_pGameConfig;

DECLARE_DETOUR(RecvServerBrowserPacket, Detour_RecvServerBrowserPacket);
// Unused
DECLARE_DETOUR(TraceShape, Detour_TraceShape);
DECLARE_DETOUR(CPhysicsGameSystemFrameBoundary, Detour_CPhysicsGameSystemFrameBoundary);
DECLARE_DETOUR(CastBox, Detour_CastBox);

DECLARE_MOVEMENT_DETOUR(PhysicsSimulate);
DECLARE_MOVEMENT_DETOUR(ProcessUsercmds);
DECLARE_MOVEMENT_DETOUR(SetupMove);
DECLARE_MOVEMENT_DETOUR(ProcessMovement);
DECLARE_MOVEMENT_DETOUR(PlayerMove);
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
DECLARE_MOVEMENT_DETOUR(Friction);
DECLARE_MOVEMENT_DETOUR(WalkMove);
DECLARE_MOVEMENT_DETOUR(TryPlayerMove);
DECLARE_MOVEMENT_DETOUR(CategorizePosition);
DECLARE_MOVEMENT_DETOUR(CheckFalling);
DECLARE_MOVEMENT_DETOUR(PostPlayerMove);
DECLARE_MOVEMENT_DETOUR(PostThink);

void InitDetours()
{
	g_vecDetours.RemoveAll();
	INIT_DETOUR(g_pGameConfig, RecvServerBrowserPacket);
	INIT_DETOUR(g_pGameConfig, CPhysicsGameSystemFrameBoundary);
	CastBox.CreateDetour(g_pGameConfig);
	TraceShape.CreateDetour(g_pGameConfig);
#ifdef DEBUG_TPM
	INIT_DETOUR(g_pGameConfig, TraceShape);
	TraceShape.DisableDetour();
#endif
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

bool FASTCALL Detour_CastBox(const void *world, void *results, const Vector &vCenter, const Vector &vDelta, const Vector &vExtents, void *attr)
{
	META_CONPRINTF("CastBox center %s, delta %s, extents %s, ", VecToString(vCenter), VecToString(vDelta), VecToString(vExtents));
	RnQueryShapeAttr_t *pTraceFilter = (RnQueryShapeAttr_t *)attr;
	META_CONPRINTF("RnQueryShapeAttr_t {\n"
				   "  m_nInteractsWith: 0x%016llx\n"
				   "  m_nInteractsExclude: 0x%016llx\n"
				   "  m_nInteractsAs: 0x%016llx\n"
				   "  m_nEntityIdsToIgnore: [%u, %u]\n"
				   "  m_nOwnerIdsToIgnore: [%u, %u]\n"
				   "  m_nHierarchyIds: [%u, %u]\n"
				   "  m_nObjectSetMask: 0x%04x\n"
				   "  m_nCollisionGroup: %u\n"
				   "  m_bHitSolid: %d\n"
				   "  m_bHitSolidRequiresGenerateContacts: %d\n"
				   "  m_bHitTrigger: %d\n"
				   "  m_bShouldIgnoreDisabledPairs: %d\n"
				   "  m_bIgnoreIfBothInteractWithHitboxes: %d\n"
				   "  m_bForceHitEverything: %d\n"
				   "  m_bUnknown: %d\n"
				   "}\n",
				   (unsigned long long)pTraceFilter->m_nInteractsWith, (unsigned long long)pTraceFilter->m_nInteractsExclude,
				   (unsigned long long)pTraceFilter->m_nInteractsAs, pTraceFilter->m_nEntityIdsToIgnore[0], pTraceFilter->m_nEntityIdsToIgnore[1],
				   pTraceFilter->m_nOwnerIdsToIgnore[0], pTraceFilter->m_nOwnerIdsToIgnore[1], pTraceFilter->m_nHierarchyIds[0],
				   pTraceFilter->m_nHierarchyIds[1], pTraceFilter->m_nObjectSetMask, pTraceFilter->m_nCollisionGroup,
				   pTraceFilter->m_bHitSolid ? 1 : 0, pTraceFilter->m_bHitSolidRequiresGenerateContacts ? 1 : 0, pTraceFilter->m_bHitTrigger ? 1 : 0,
				   pTraceFilter->m_bShouldIgnoreDisabledPairs ? 1 : 0, pTraceFilter->m_bIgnoreIfBothInteractWithHitboxes ? 1 : 0,
				   pTraceFilter->m_bForceHitEverything ? 1 : 0, pTraceFilter->m_bUnknown ? 1 : 0);
	bool result = CastBox(world, results, vCenter, vDelta, vExtents, attr);
	CUtlVectorFixedGrowable<PhysicsTrace_t, 128> *res = (CUtlVectorFixedGrowable<PhysicsTrace_t, 128> *)results;
	META_CONPRINTF("\t-> %i results\n", res->Count());
	return result;
}

extern bool RetraceShape(const Ray_t &ray, const Vector &start, const Vector &end, const CTraceFilter &filter, CGameTrace &tr);
static void *g_pPhysicsQuery = nullptr;

bool Detour_TraceShape(const void *physicsQuery, const Ray_t &ray, const Vector &start, const Vector &end, const CTraceFilter *pTraceFilter,
					   trace_t *pm)
{
#ifdef DEBUG_TPM
	META_CONPRINTF("\n\nTrace %s -> %s, ", VecToString(start), VecToString(end));
	META_CONPRINTF("RnQueryShapeAttr_t {\n"
				   "  m_nInteractsWith: 0x%016llx\n"
				   "  m_nInteractsExclude: 0x%016llx\n"
				   "  m_nInteractsAs: 0x%016llx\n"
				   "  m_nEntityIdsToIgnore: [%u, %u]\n"
				   "  m_nOwnerIdsToIgnore: [%u, %u]\n"
				   "  m_nHierarchyIds: [%u, %u]\n"
				   "  m_nObjectSetMask: 0x%04x\n"
				   "  m_nCollisionGroup: %u\n"
				   "  m_bHitSolid: %d\n"
				   "  m_bHitSolidRequiresGenerateContacts: %d\n"
				   "  m_bHitTrigger: %d\n"
				   "  m_bShouldIgnoreDisabledPairs: %d\n"
				   "  m_bIgnoreIfBothInteractWithHitboxes: %d\n"
				   "  m_bForceHitEverything: %d\n"
				   "  m_bUnknown: %d\n"
				   "}\n",
				   (unsigned long long)pTraceFilter->m_nInteractsWith, (unsigned long long)pTraceFilter->m_nInteractsExclude,
				   (unsigned long long)pTraceFilter->m_nInteractsAs, pTraceFilter->m_nEntityIdsToIgnore[0], pTraceFilter->m_nEntityIdsToIgnore[1],
				   pTraceFilter->m_nOwnerIdsToIgnore[0], pTraceFilter->m_nOwnerIdsToIgnore[1], pTraceFilter->m_nHierarchyIds[0],
				   pTraceFilter->m_nHierarchyIds[1], pTraceFilter->m_nObjectSetMask, pTraceFilter->m_nCollisionGroup,
				   pTraceFilter->m_bHitSolid ? 1 : 0, pTraceFilter->m_bHitSolidRequiresGenerateContacts ? 1 : 0, pTraceFilter->m_bHitTrigger ? 1 : 0,
				   pTraceFilter->m_bShouldIgnoreDisabledPairs ? 1 : 0, pTraceFilter->m_bIgnoreIfBothInteractWithHitboxes ? 1 : 0,
				   pTraceFilter->m_bForceHitEverything ? 1 : 0, pTraceFilter->m_bUnknown ? 1 : 0);
	switch (ray.m_eType)
	{
		case RAY_TYPE_LINE:
		{
			META_CONPRINTF("RAY_TYPE_LINE offset %s radius %f, ", VecToString(ray.m_Line.m_vStartOffset), ray.m_Line.m_flRadius);
			break;
		}
		case RAY_TYPE_SPHERE:
		{
			META_CONPRINTF("RAY_TYPE_SPHERE radius %f, center %s, ", ray.m_Sphere.m_flRadius, VecToString(ray.m_Sphere.m_vCenter));
			break;
		}
		case RAY_TYPE_HULL:
		{
			META_CONPRINTF("RAY_TYPE_HULL mins = %s, maxs = %s, ", VecToString(ray.m_Hull.m_vMins), VecToString(ray.m_Hull.m_vMaxs));
			break;
		}
		case RAY_TYPE_CAPSULE:
		{
			META_CONPRINTF("RAY_TYPE_CAPSULE radius %f, center %s %s, ", ray.m_Capsule.m_flRadius, VecToString(ray.m_Capsule.m_vCenter[0]),
						   VecToString(ray.m_Capsule.m_vCenter[1]));
			break;
		}
		case RAY_TYPE_MESH:
		{
			META_CONPRINTF("RAY_TYPE_MESH mins = %s, maxs = %s, numVertice = %i, pVertices = %p, ", VecToString(ray.m_Mesh.m_vMins),
						   VecToString(ray.m_Mesh.m_vMaxs), ray.m_Mesh.m_nNumVertices, ray.m_Mesh.m_pVertices);
			break;
		}
	}
	bool ret = TraceShape(physicsQuery, ray, start, end, pTraceFilter, pm);
	f32 error;
	Vector velocity;
	for (u32 i = 0; i < 2; i++)
	{
		if (g_pKZPlayerManager->ToPlayer(i) && g_pKZPlayerManager->ToPlayer(i)->GetMoveServices())
		{
			error = g_pKZPlayerManager->ToPlayer(i)->GetMoveServices()->m_flAccumulatedJumpError();
			velocity = g_pKZPlayerManager->ToPlayer(i)->currentMoveData->m_vecVelocity;
			break;
		}
	}
	traceHistory.AddToTail({start, end, ray, pm->DidHit(), pm->m_vStartPos, pm->m_vEndPos, pm->m_vHitNormal, pm->m_vHitPoint, pm->m_flHitOffset,
							pm->m_flFraction, error, velocity});
	return ret;
	if (pm->DidHit())
	{
		META_CONPRINTF("hit %s (normal %s, triangle %i, body %p, shape %p)\n", VecToString(pm->m_vEndPos), VecToString(pm->m_vHitNormal),
					   pm->m_nTriangle, pm->m_hBody, pm->m_hShape);
	}
	else
	{
		META_CONPRINT("missed\n");
	}
#else
	g_pPhysicsQuery = (void *)physicsQuery;
	bool ret = TraceShape(physicsQuery, ray, start, end, pTraceFilter, pm);
	CConVarRef<bool> kz_retrace_tpm_enable("kz_retrace_tpm_enable");
	CConVarRef<bool> kz_retrace_cg_enable("kz_retrace_cg_enable");
	if (kz_retrace_tpm_enable.Get() || kz_retrace_cg_enable.Get())
	{
		RetraceShape(ray, start, end, *pTraceFilter, *pm);
	}
#endif
	return ret;
}

#include "utils/simplecmds.h"
#include "sdk/tracefilter.h"

SCMD(kz_drawtriangle, SCFL_HIDDEN)
{
	if (!g_pPhysicsQuery)
	{
		return MRES_SUPERCEDE;
	}
	g_pKZUtils->ClearOverlays();
	auto pawn = controller->GetPlayerPawn();
	Vector origin;
	g_pKZPlayerManager->ToPlayer(controller)->GetEyeOrigin(&origin);
	QAngle angles = pawn->m_angEyeAngles();

	Vector forward;
	AngleVectors(angles, &forward);
	Vector endPos = origin + forward * 32768;
	trace_t tr;

	CTraceFilterPlayerMovementCS filter(pawn);
	Ray_t ray;
	TraceShape.EnableDetour();
	bool result = TraceShape(g_pPhysicsQuery, ray, origin, endPos, &filter, &tr);
	TraceShape.DisableDetour();
	if (tr.DidHit() && tr.m_nTriangle != -1)
	{
		CTransform transform;
		g_pKZUtils->GetPhysicsBodyTransform(tr.m_hBody, transform);
		RnMesh_t *mesh = tr.m_hShape->GetMesh();
		const RnTriangle_t &triangle = mesh->m_Triangles[tr.m_nTriangle];
		Vector v0 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[0]]);
		Vector v1 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[1]]);
		Vector v2 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[2]]);

		META_CONPRINTF("Triangle (%i): normal %f %f %f, endpos %f %f %f, fraction %f\n", tr.m_nTriangle, tr.m_vHitNormal.x, tr.m_vHitNormal.y,
					   tr.m_vHitNormal.z, tr.m_vEndPos.x, tr.m_vEndPos.y, tr.m_vEndPos.z, tr.m_flFraction);

		g_pKZUtils->AddTriangleOverlay(v0, v1, v2, 0, 255, 0, 128, true, -1.0f);
	}
	return MRES_SUPERCEDE;
}

void Detour_CPhysicsGameSystemFrameBoundary(void *pThis)
{
	CPhysicsGameSystemFrameBoundary(pThis);
	KZ::misc::OnPhysicsGameSystemFrameBoundary(pThis);
}
