#pragma once

#include "utils/schema.h"
#include "utils/datatypes.h"

class CInButtonState
{
	DECLARE_SCHEMA_CLASS(CInButtonState);
public:
	uint64_t unk;
	uint64_t m_pButtonStates[3];
};