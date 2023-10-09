#pragma once

#include "cbaseplayerpawn.h"

class CCSPlayerPawnBase : public CBasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawn);
};

class CCSPlayerPawn : public CBasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawn);
	SCHEMA_FIELD(CCSPlayer_MovementServices*, m_pMovementServices)
};