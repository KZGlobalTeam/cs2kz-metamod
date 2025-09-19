#pragma once

#include "cbaseplayerpawn.h"

class CCSPlayerPawnBase : public CBasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawnBase);
};

class CCSPlayerPawn : public CCSPlayerPawnBase
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawn);
	SCHEMA_FIELD(float, m_ignoreLadderJumpTime)
	SCHEMA_FIELD(float, m_flSlopeDropOffset)
	SCHEMA_FIELD(float, m_flSlopeDropHeight)

	SCHEMA_FIELD(float, m_flVelocityModifier)

	SCHEMA_FIELD(QAngle, m_angEyeAngles)
	SCHEMA_FIELD(bool, m_bLeftHanded)

	void Respawn()
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("Respawn"), this);
	}
};
