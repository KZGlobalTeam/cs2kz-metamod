
#pragma once
#include "entity/cbaseentity.h"

class CGameRules
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CGameRules)
};

class CCSGameRules : public CGameRules
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSGameRules)

	SCHEMA_FIELD(bool, m_bGameRestart)
	SCHEMA_FIELD(GameTime_t, m_fRoundStartTime)
	SCHEMA_FIELD(GameTime_t, m_flGameStartTime)
	SCHEMA_FIELD(int, m_iRoundWinStatus);
	SCHEMA_FIELD(int, m_iRoundTime);
};

class CCSGameRulesProxy : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSGameRulesProxy)

	SCHEMA_FIELD(CCSGameRules *, m_pGameRules)
};
