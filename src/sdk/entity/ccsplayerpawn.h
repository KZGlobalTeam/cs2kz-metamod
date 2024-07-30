#pragma once

class CCSPlayer_ViewModelServices;

#include "cbaseplayerpawn.h"

class CCSPlayerPawnBase : public CBasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawnBase);
	SCHEMA_FIELD(CCSPlayer_ViewModelServices *, m_pViewModelServices)
};

class CCSPlayerPawn : public CCSPlayerPawnBase
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawn);
	SCHEMA_FIELD(float, m_ignoreLadderJumpTime)
	SCHEMA_FIELD(float, m_flSlopeDropOffset)
	SCHEMA_FIELD(float, m_flSlopeDropHeight)

	SCHEMA_FIELD(float, m_flVelocityModifier)

	void Respawn()
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("Respawn"), this);
	}
};
