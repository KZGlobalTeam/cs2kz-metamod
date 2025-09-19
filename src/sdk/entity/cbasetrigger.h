#pragma once
#include "cbaseentity.h"
#include "utlsymbollarge.h"

class CBaseTrigger : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CBaseTrigger)

	SCHEMA_FIELD(CUtlSymbolLarge, m_iFilterName)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hFilter)

	bool PassesTriggerFilters(CBaseEntity *pOther)
	{
		return CALL_VIRTUAL(bool, g_pGameConfig->GetOffset("PassesTriggerFilters"), this, pOther);
	}
};
