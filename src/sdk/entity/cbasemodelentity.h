#pragma once

#include "cbaseentity.h"
#include "sdk/ccollisionproperty.h"

class CBaseModelEntity : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CBaseModelEntity);

	SCHEMA_FIELD(CCollisionProperty, m_Collision)
	SCHEMA_FIELD(Color, m_clrRender)
	SCHEMA_FIELD(uint8, m_nRenderMode)
};
