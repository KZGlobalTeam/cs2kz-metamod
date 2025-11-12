#pragma once
#include "utils/schema.h"
#include "ceconitemattribute.h"

class CAttributeList
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CAttributeList, 1)

	SCHEMA_FIELD_POINTER(CUtlVector<CEconItemAttribute>, m_Attributes)
};
