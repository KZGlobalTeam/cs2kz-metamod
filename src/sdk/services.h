#pragma once
#include <platform.h>

class CBaseEntity;
class CBaseModelEntity;
class CBasePlayerPawn;

#include "utils/schema.h"
#include "entity/cbaseplayerpawn.h"
#include "cinbuttonstate.h"

class CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayerPawnComponent);

public:
	uint8 vtable[0x8];
	uint8 chainEntity[0x28]; // Unused
	CBasePlayerPawn *pawn;   // 0x16
	uint8 __pad0030[0x6];    // 0x0
};

class CCSPlayer_ViewModelServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ViewModelServices)
	SCHEMA_FIELD_POINTER(CHandle<CBaseModelEntity>, m_hViewModel);
};

class CPlayer_ObserverServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_ObserverServices)
	SCHEMA_FIELD(uint8_t, m_iObserverMode)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hObserverTarget)
};

class CPlayer_MovementServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices);
	SCHEMA_FIELD_POINTER(CInButtonState, m_nButtons)
	SCHEMA_FIELD_POINTER(float, m_arrForceSubtickMoveWhen)
};

class CPlayer_MovementServices_Humanoid : public CPlayer_MovementServices
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices_Humanoid);
	SCHEMA_FIELD(bool, m_bDucking)
	SCHEMA_FIELD(bool, m_bDucked)
	SCHEMA_FIELD(float, m_flSurfaceFriction)
};

class CCSPlayer_MovementServices : public CPlayer_MovementServices_Humanoid
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_MovementServices);
	SCHEMA_FIELD(float, m_flJumpUntil)
	SCHEMA_FIELD(Vector, m_vecLadderNormal)
	SCHEMA_FIELD(bool, m_bOldJumpPressed)
	SCHEMA_FIELD(float, m_flJumpPressedTime)
	SCHEMA_FIELD(float, m_flDuckSpeed)
	SCHEMA_FIELD(float, m_flDuckAmount)
	SCHEMA_FIELD(float, m_flStamina)
};

class CCSPlayer_WaterServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_WaterServices);
	SCHEMA_FIELD(float, m_flWaterJumpTime)
	SCHEMA_FIELD(Vector, m_vecWaterJumpVel)
};

class CCSPlayer_ItemServices

{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ItemServices);

	virtual ~CCSPlayer_ItemServices() = 0;

private:
	virtual void unk_01() = 0;
	virtual void unk_02() = 0;
	virtual void unk_03() = 0;
	virtual void unk_04() = 0;
	virtual void unk_05() = 0;
	virtual void unk_06() = 0;
	virtual void unk_07() = 0;
	virtual void unk_08() = 0;
	virtual void unk_09() = 0;
	virtual void unk_10() = 0;
	virtual void unk_11() = 0;
	virtual void unk_12() = 0;
	virtual void unk_13() = 0;
	virtual void unk_14() = 0;
	virtual CBaseEntity *_GiveNamedItem(const char *pchName) = 0;

public:
	virtual bool GiveNamedItemBool(const char *pchName) = 0;
	virtual CBaseEntity *GiveNamedItem(const char *pchName) = 0;
};
