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
		static_persist const int offset = g_pGameConfig->GetOffset("PassesTriggerFilters");
		return CALL_VIRTUAL(bool, offset, this, pOther);
	}
};
