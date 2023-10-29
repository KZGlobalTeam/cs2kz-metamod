#pragma once
#include <platform.h>

#include "utils/schema.h"
#include "cbaseplayerpawn.h"
#include "cinbuttonstate.h"
class CBaseEntity;
class CBasePlayerPawn;

class CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayerPawnComponent);
public:
	uint8_t vtable[0x8];
	uint8_t chainEntity[0x28]; // Unused
	CBasePlayerPawn *pawn; // 0x16
	uint8_t __pad0030[0x6]; // 0x0
};

class CPlayer_MovementServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices);
	SCHEMA_FIELD_POINTER(CInButtonState, m_nButtons)
};

class CPlayer_MovementServices_Humanoid : public CPlayer_MovementServices
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices_Humanoid);
	SCHEMA_FIELD(bool, m_bDucked)
	SCHEMA_FIELD(float, m_flSurfaceFriction)
};

class CCSPlayer_MovementServices : public CPlayer_MovementServices_Humanoid
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_MovementServices);
	SCHEMA_FIELD(float, m_flJumpUntil)
	SCHEMA_FIELD(Vector, m_vecLadderNormal)
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
	virtual CBaseEntity* _GiveNamedItem(const char* pchName) = 0;
public:
	virtual bool GiveNamedItemBool(const char* pchName) = 0;
	virtual CBaseEntity* GiveNamedItem(const char* pchName) = 0;
};