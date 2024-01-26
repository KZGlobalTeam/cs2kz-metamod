#pragma once
#include "cbaseentity.h"

class CBaseTrigger : public CBaseEntity2
{
public:
	DECLARE_SCHEMA_CLASS(CBaseTrigger)

	bool IsStartZone() { return !V_stricmp(this->GetClassname(), "trigger_multiple") && this->m_pEntity->NameMatches("timer_startzone"); }
	bool IsEndZone() { return !V_stricmp(this->GetClassname(), "trigger_multiple") && this->m_pEntity->NameMatches("timer_endzone"); }
};
