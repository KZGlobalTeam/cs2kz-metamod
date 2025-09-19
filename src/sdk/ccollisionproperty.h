#pragma once
#include "utils/schema.h"

class CBaseModelEntity;

struct VPhysicsCollisionAttribute_t
{
	DECLARE_SCHEMA_CLASS_BASE(VPhysicsCollisionAttribute_t, 1)

	SCHEMA_FIELD(uint8, m_nCollisionGroup)
	SCHEMA_FIELD(uint64, m_nInteractsAs)
	SCHEMA_FIELD(uint64, m_nInteractsWith)
	SCHEMA_FIELD(uint16, m_nHierarchyId)
};

class CCollisionProperty
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CCollisionProperty, 1)

	SCHEMA_FIELD(VPhysicsCollisionAttribute_t, m_collisionAttribute)
	SCHEMA_FIELD(Vector, m_vecMins)
	SCHEMA_FIELD(Vector, m_vecMaxs)
	SCHEMA_FIELD(SolidType_t, m_nSolidType)
	SCHEMA_FIELD(uint8, m_usSolidFlags)
	SCHEMA_FIELD(uint8, m_CollisionGroup)

	CBaseModelEntity *GetOuter()
	{
		return (CBaseModelEntity *)((char *)this + 8);
	}
};
