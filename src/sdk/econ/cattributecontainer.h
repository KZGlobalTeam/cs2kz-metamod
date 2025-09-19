#pragma once

#include "ceconitemview.h"
#include "cattributemanager.h"

class CAttributeContainer : public CAttributeManager
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CAttributeContainer, 2, false)

	SCHEMA_FIELD(CEconItemView, m_Item)
};
