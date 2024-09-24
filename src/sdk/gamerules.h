
#pragma once
#include "entity/cbaseentity.h"

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
	SCHEMA_FIELD(GameTime_t, m_fRoundStartTime)
	SCHEMA_FIELD(GameTime_t, m_flGameStartTime)
};

class CCSGameRulesProxy : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CCSGameRulesProxy)

	SCHEMA_FIELD(CCSGameRules *, m_pGameRules)
};
