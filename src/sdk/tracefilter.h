#pragma once
#include "gametrace.h"
#include "sdk/entity/cbaseplayerpawn.h"
#include "utils/addresses.h"
#include "utils/virtual.h"

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

	static inline void **vTable {};

	virtual bool ShouldHitEntity(CEntityInstance *ent) override
	{
		if (!vTable)
		{
			vTable = static_cast<void **>(modules::server->FindVirtualTable("CTraceFilterPlayerMovementCS"));
		}
		assert(vTable);
#ifdef _WIN32
		return CALL_VIRTUAL_OVERRIDE_VTBL(bool, 1, vTable, this, ent);
#else
		return CALL_VIRTUAL_OVERRIDE_VTBL(bool, 2, vTable, this, ent);
#endif
	}
};
