#pragma once
#include "utils/schema.h"

typedef uint16 attrib_definition_index_t;

// It's really important that this class size is correct, otherwise CUtlVector containing this class will not work properly.
class CEconItemAttribute // : public CNetworkVarChainer
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CEconItemAttribute, 1)

	SCHEMA_FIELD(attrib_definition_index_t, m_iAttributeDefinitionIndex)
	SCHEMA_FIELD(float, m_flValue)
	SCHEMA_FIELD(float, m_flInitialValue)

private:
	uint8 padding[69];
};

static_assert(sizeof(CEconItemAttribute) == 72, "CEconItemAttribute size is incorrect");
