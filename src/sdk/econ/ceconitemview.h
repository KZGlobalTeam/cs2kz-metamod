#pragma once
#include "utils/schema.h"
#include "cattributelist.h"

class CEconItemView
{
public:
	DECLARE_SCHEMA_CLASS(CEconItemView)

	SCHEMA_FIELD(uint16, m_iItemDefinitionIndex)
	SCHEMA_FIELD(int32, m_iEntityQuality)
	SCHEMA_FIELD(uint32, m_iEntityLevel)
	SCHEMA_FIELD(uint64, m_iItemID)
	SCHEMA_FIELD(uint32, m_iItemIDHigh)
	SCHEMA_FIELD(uint32, m_iItemIDLow)
	SCHEMA_FIELD(uint32, m_iAccountID)
	SCHEMA_FIELD(uint32, m_iInventoryPosition)
	SCHEMA_FIELD(bool, m_bInitialized)
	SCHEMA_FIELD(CAttributeList, m_AttributeList)
	SCHEMA_FIELD(CAttributeList, m_NetworkedDynamicAttributes)
	SCHEMA_FIELD_POINTER(char, m_szCustomName)
	SCHEMA_FIELD_POINTER(char, m_szCustomNameOverride)
};
