#pragma once
#include "cbasetrigger.h"
#include "entity2/ientity2_entityio.h"
#ifndef IDA_IGNORE
class CTriggerMultiple : public CBaseTrigger
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CTriggerMultiple)

	SCHEMA_FIELD_POINTER(CEntityIOOutput, m_OnTrigger)
};
#endif
