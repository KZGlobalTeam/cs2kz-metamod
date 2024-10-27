#pragma once
#include "cbaseentity.h"
#include "utlsymbollarge.h"

class CBaseTrigger : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBaseTrigger)

	SCHEMA_FIELD(CUtlSymbolLarge, m_iFilterName)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hFilter)
};
