
#pragma once
#include "entity/cbaseentity.h"

struct GameTime_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(GameTime_t)

	SCHEMA_FIELD(float, m_Value)
};

class CGameRules
{
public:
	DECLARE_SCHEMA_CLASS(CGameRules)
};

class CCSGameRules : public CGameRules
{
public:
	DECLARE_SCHEMA_CLASS(CCSGameRules)

	SCHEMA_FIELD(bool, m_bGameRestart)
};

class CCSGameRulesProxy : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CCSGameRulesProxy)

	SCHEMA_FIELD(CCSGameRules *, m_pGameRules)
};
