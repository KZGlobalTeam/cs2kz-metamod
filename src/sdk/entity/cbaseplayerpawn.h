#pragma once

class CCSPlayer_WaterServices;
class CPlayer_MovementServices;
class CPlayer_ObserverServices;
class CCSPlayer_ItemServices;
class CPlayer_WeaponServices;

#include "cbasemodelentity.h"

class CBasePlayerPawn : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CBasePlayerPawn);

	SCHEMA_FIELD(CPlayer_MovementServices *, m_pMovementServices)
	SCHEMA_FIELD(CHandle<CBasePlayerController>, m_hController)
	SCHEMA_FIELD(CCSPlayer_ItemServices *, m_pItemServices)
	SCHEMA_FIELD(CPlayer_ObserverServices *, m_pObserverServices)
	SCHEMA_FIELD(CPlayer_WeaponServices *, m_pWeaponServices)
	SCHEMA_FIELD(CCSPlayer_WaterServices *, m_pWaterServices)
	SCHEMA_FIELD(QAngle, v_angle)

	void CommitSuicide(bool bExplode, bool bForce)
	{
		this->m_bTakesDamage(true);
		static_persist const int offset = g_pGameConfig->GetOffset("CommitSuicide");
		CALL_VIRTUAL(void, offset, this, bExplode, bForce);
		this->m_bTakesDamage(false);
	}

	bool IsBot()
	{
		return !!(this->m_fFlags() & FL_BOT);
	}
};
