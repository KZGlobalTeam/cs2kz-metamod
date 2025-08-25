#pragma once
#include "gametrace.h"
#include "sdk/entity/cbaseplayerpawn.h"
#include "utils/addresses.h"

struct CTraceFilterPlayerMovementCS : public CTraceFilter
{
	CTraceFilterPlayerMovementCS(CBasePlayerPawn *pawn)
		: CTraceFilter((CEntityInstance *)pawn, pawn->m_hOwnerEntity.Get(), pawn->m_Collision().m_collisionAttribute().m_nHierarchyId(),
					   pawn->m_pCollision()->m_collisionAttribute().m_nInteractsWith, COLLISION_GROUP_PLAYER, true)
	{
		EnableInteractsAsLayer(LAYER_INDEX_CONTENTS_PLAYER);
		m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
		m_bHitSolid = true;
		m_bHitSolidRequiresGenerateContacts = true;
		m_bHitTrigger = false;
		m_bShouldIgnoreDisabledPairs = true;
		m_bIgnoreIfBothInteractWithHitboxes = false;
		m_bForceHitEverything = false;
		m_bUnknown = true;
	}

	virtual bool ShouldHitEntity(CEntityInstance *ent) override
	{
		static void **vtable = (void **)modules::server->FindVirtualTable("CTraceFilterPlayerMovementCS");
		assert(vtable);
		using ShouldHitEntityFn = bool (*)(CTraceFilterPlayerMovementCS *, CEntityInstance *);
#ifdef _WIN32
		return (*reinterpret_cast<ShouldHitEntityFn *>(vtable[1]))(this, ent);
#else
		return (*reinterpret_cast<ShouldHitEntityFn *>(vtable[2]))(this, ent);
#endif
	}
};
