#pragma once

#include "cbaseentity.h"
#include "sdk/ccollisionproperty.h"

class CBaseModelEntity : public CBaseEntity2
{
public:
	DECLARE_SCHEMA_CLASS(CBaseModelEntity);

	SCHEMA_FIELD(CCollisionProperty, m_Collision)
	SCHEMA_FIELD(Color, m_clrRender)
	SCHEMA_FIELD(uint8, m_nRenderMode)
};

class CBaseViewModel : public CBaseModelEntity
{
public:

	DECLARE_SCHEMA_CLASS(CBaseViewModel);
	SCHEMA_FIELD(int, m_nViewModelIndex);
};
