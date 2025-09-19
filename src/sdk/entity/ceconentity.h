#pragma once
#include "cbaseanimgraph.h"
#include "sdk/econ/cattributecontainer.h"

class CEconEntity : public CBaseAnimGraph
{
public:
	DECLARE_SCHEMA_CLASS(CEconEntity)
	SCHEMA_FIELD(CAttributeContainer, m_AttributeManager)

	SCHEMA_FIELD(int, m_nFallbackPaintKit)
	SCHEMA_FIELD(int, m_nFallbackSeed)
	SCHEMA_FIELD(float, m_flFallbackWear)
};
