#pragma once

#include "cbaseplayerpawn.h"
#include "sdk/econ/ceconitemview.h"

enum CSPlayerState : u32
{
	PLAYER_STATE_ACTIVE = 0,
	PLAYER_STATE_WELCOME,
	PLAYER_STATE_PICKINGTEAM,
	PLAYER_STATE_PICKINGCLASS,
	PLAYER_STATE_DEATH_ANIM,
	PLAYER_STATE_DEATH_WAIT_FOR_KEY,
	PLAYER_STATE_OBSERVER_MODE,
	PLAYER_STATE_GUNGAME_RESPAWN,
	PLAYER_STATE_DORMANT,
};

class CCSPlayerPawnBase : public CBasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSPlayerPawnBase);
	SCHEMA_FIELD(u32, m_iPlayerState)

	// Leaves the current player state and enters the given one. Unlike Respawn this is not virtual, so it is found by signature.
	void SetPlayerState(CSPlayerState state)
	{
		typedef void StateTransition_t(CCSPlayerPawnBase * pawn, CSPlayerState state);
		static_persist StateTransition_t *stateTransition = (StateTransition_t *)g_pGameConfig->ResolveSignature("CCSPlayerPawnBase_StateTransition");
		stateTransition(this, state);
	}
};

class CCSPlayerPawn : public CCSPlayerPawnBase
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSPlayerPawn);
	SCHEMA_FIELD(float, m_ignoreLadderJumpTime)
	SCHEMA_FIELD(float, m_flVelocityModifier)

	SCHEMA_FIELD(QAngle, m_angEyeAngles)
	SCHEMA_FIELD(bool, m_bLeftHanded)

	SCHEMA_FIELD(CEconItemView, m_EconGloves);
	SCHEMA_FIELD(uint8, m_nEconGlovesChanged);

	SCHEMA_FIELD(float, m_flViewmodelOffsetX);
	SCHEMA_FIELD(float, m_flViewmodelOffsetY);
	SCHEMA_FIELD(float, m_flViewmodelOffsetZ);
	SCHEMA_FIELD(float, m_flViewmodelFOV);

	void Respawn()
	{
		static_persist const int offset = g_pGameConfig->GetOffset("Respawn");
		CALL_VIRTUAL(void, offset, this);
	}
};
