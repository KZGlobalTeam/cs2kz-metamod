#pragma once

#include "utils/schema.h"
#include "utils/datatypes.h"

struct VPhysicsCollisionAttribute_t
{
	DECLARE_SCHEMA_CLASS_INLINE(VPhysicsCollisionAttribute_t)

	SCHEMA_FIELD(uint8, m_nCollisionGroup)
	SCHEMA_FIELD(uint64, m_nInteractsAs)
	SCHEMA_FIELD(uint64, m_nInteractsWith)
};

class CCollisionProperty
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CCollisionProperty)

	SCHEMA_FIELD(VPhysicsCollisionAttribute_t, m_collisionAttribute)
	SCHEMA_FIELD(SolidType_t, m_nSolidType)
	SCHEMA_FIELD(uint8, m_usSolidFlags)
	SCHEMA_FIELD(uint8, m_CollisionGroup)
};