#pragma once

#include "cbaseentity.h"
#include "utils/entity/ccollisionproperty.h"
class CBaseEntity2;

class CBaseModelEntity : public CBaseEntity2
{
public:
	DECLARE_SCHEMA_CLASS(CBaseModelEntity);

	SCHEMA_FIELD(CCollisionProperty, m_Collision)
	SCHEMA_FIELD(Color, m_clrRender)
};