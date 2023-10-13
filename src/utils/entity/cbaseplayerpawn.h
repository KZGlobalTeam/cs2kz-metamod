#pragma once

#include "cbaseentity.h"
#include "cbaseplayercontroller.h"
#include "services.h"


class CBasePlayerPawn : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerPawn);

	SCHEMA_FIELD_POINTER(CPlayer_MovementServices, m_pMovementServices)
	SCHEMA_FIELD(CHandle< CBasePlayerController >, m_hController)
	SCHEMA_FIELD(uint8*, m_pWeaponServices)
	SCHEMA_FIELD(CCSPlayer_ItemServices*, m_pItemServices)
	SCHEMA_FIELD(QAngle, v_angle)
	SCHEMA_FIELD(float, m_ignoreLadderJumpTime)
};