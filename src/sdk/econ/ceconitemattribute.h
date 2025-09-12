#pragma once
#include "utils/schema.h"

typedef uint16 attrib_definition_index_t;

class CEconItemAttribute
{
public:
	DECLARE_SCHEMA_CLASS(CEconItemAttribute)

	SCHEMA_FIELD(attrib_definition_index_t, m_iAttributeDefinitionIndex)
};
