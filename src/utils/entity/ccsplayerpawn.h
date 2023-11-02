#pragma once

#include "cbaseplayerpawn.h"

class CCSPlayerPawnBase : public CBasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawnBase);
	SCHEMA_FIELD(float, m_flSlopeDropOffset)
	SCHEMA_FIELD(float, m_flSlopeDropHeight)
	SCHEMA_FIELD(float, m_ignoreLadderJumpTime)
};

class CCSPlayerPawn : public CCSPlayerPawnBase
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawn);
};