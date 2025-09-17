#pragma once
#include "cbasetrigger.h"
#include "entityio.h"
#ifndef IDA_IGNORE
class CTriggerMultiple : public CBaseTrigger
{
public:
	DECLARE_SCHEMA_CLASS(CTriggerMultiple)

	SCHEMA_FIELD(CEntityIOOutput, m_OnTrigger)
};
#endif
